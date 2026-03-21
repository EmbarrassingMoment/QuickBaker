// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerAsync.h"
#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Editor.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Misc/ScopeExit.h"

#define LOCTEXT_NAMESPACE "FQuickBakerAsyncTask"

TSharedRef<FQuickBakerAsyncTask> FQuickBakerAsyncTask::Start(const FQuickBakerSettings& InSettings)
{
	TSharedRef<FQuickBakerAsyncTask> Task = MakeShareable(new FQuickBakerAsyncTask());
	Task->Settings = InSettings;
	Task->TotalRows = InSettings.Resolution;

	// Determine bit depth
	const bool bIsAsset = InSettings.OutputType == EQuickBakerOutputType::Asset;
	const bool bIsPNG = InSettings.OutputType == EQuickBakerOutputType::PNG;
	if (bIsAsset)
	{
		Task->bIs16Bit = (InSettings.BitDepth == EQuickBakerBitDepth::Bit16);
	}
	else if (bIsPNG)
	{
		Task->bIs16Bit = false;
	}
	else // EXR
	{
		Task->bIs16Bit = true;
	}
	Task->BytesPerPixel = Task->bIs16Bit ? 8 : 4; // FFloat16Color=8, FColor=4

	// Pre-allocate pixel buffer for the full texture
	const int64 TotalBytes = static_cast<int64>(InSettings.Resolution) * InSettings.Resolution * Task->BytesPerPixel;
	Task->PixelBuffer.SetNumZeroed(TotalBytes);

	// Adjust chunk size based on resolution to balance responsiveness and overhead
	// Aim for ~16MB per chunk
	const int64 TargetChunkBytes = 16 * 1024 * 1024; // 16 MB
	Task->ChunkRowSize = FMath::Max(1, static_cast<int32>(TargetChunkBytes / (static_cast<int64>(InSettings.Resolution) * Task->BytesPerPixel)));
	Task->ChunkRowSize = FMath::Min(Task->ChunkRowSize, InSettings.Resolution);

	// Register tick - must capture a shared ref to keep the task alive
	TSharedRef<FQuickBakerAsyncTask> PinnedTask = Task;
	Task->TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(PinnedTask, &FQuickBakerAsyncTask::Tick),
		0.0f // Tick every frame
	);

	return Task;
}

FQuickBakerAsyncTask::~FQuickBakerAsyncTask()
{
	Cleanup();
}

void FQuickBakerAsyncTask::Cancel()
{
	bCancelled = true;
}

float FQuickBakerAsyncTask::GetProgress() const
{
	// Weight: Setup=5%, Rendering=10%, WaitForGPU=5%, ReadPixels=60%, Saving=20%
	switch (CurrentPhase)
	{
	case EPhase::Setup:
		return 0.0f;
	case EPhase::Rendering:
		return 0.05f;
	case EPhase::WaitForGPU:
		return 0.15f;
	case EPhase::ReadPixels:
		{
			float ReadProgress = (TotalRows > 0) ? static_cast<float>(ChunkRowStart) / TotalRows : 0.0f;
			return 0.20f + ReadProgress * 0.60f;
		}
	case EPhase::Saving:
		return 0.80f;
	case EPhase::Done:
		return 1.0f;
	}
	return 0.0f;
}

FText FQuickBakerAsyncTask::GetStatusText() const
{
	switch (CurrentPhase)
	{
	case EPhase::Setup:
		return LOCTEXT("Status_Setup", "Setting up Render Target...");
	case EPhase::Rendering:
		return LOCTEXT("Status_Rendering", "Rendering Material...");
	case EPhase::WaitForGPU:
		return LOCTEXT("Status_WaitGPU", "Waiting for GPU...");
	case EPhase::ReadPixels:
		{
			int32 Percent = (TotalRows > 0) ? FMath::RoundToInt(static_cast<float>(ChunkRowStart) / TotalRows * 100.0f) : 0;
			return FText::Format(LOCTEXT("Status_ReadPixels", "Reading pixels... ({0}%)"), FText::AsNumber(Percent));
		}
	case EPhase::Saving:
		return LOCTEXT("Status_Saving", "Saving...");
	case EPhase::Done:
		return LOCTEXT("Status_Done", "Done");
	}
	return FText();
}

bool FQuickBakerAsyncTask::Tick(float DeltaTime)
{
	if (bCancelled)
	{
		Finish(false, LOCTEXT("Cancelled", "Bake cancelled."));
		return false; // Remove ticker
	}

	switch (CurrentPhase)
	{
	case EPhase::Setup:
		PhaseSetup();
		break;
	case EPhase::Rendering:
		PhaseRendering();
		break;
	case EPhase::WaitForGPU:
		PhaseWaitForGPU();
		break;
	case EPhase::ReadPixels:
		if (!PhaseReadPixels())
		{
			return false;
		}
		break;
	case EPhase::Saving:
		PhaseSaving();
		return false; // Done after saving
	case EPhase::Done:
		return false;
	}

	return CurrentPhase != EPhase::Done; // Keep ticking unless done
}

void FQuickBakerAsyncTask::PhaseSetup()
{
	// Determine render target format
	ETextureRenderTargetFormat Format;
	if (bIs16Bit)
	{
		Format = RTF_RGBA16f;
	}
	else
	{
		Format = RTF_RGBA8;
	}

	// Create transient render target
	RenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient
	);

	if (!RenderTarget)
	{
		Finish(false, LOCTEXT("Error_RTCreate", "Failed to create render target."));
		return;
	}

	RenderTarget->AddToRoot();
	RenderTarget->InitAutoFormat(Settings.Resolution, Settings.Resolution);
	RenderTarget->RenderTargetFormat = Format;
	RenderTarget->bForceLinearGamma = true;
	RenderTarget->SRGB = false;
	RenderTarget->UpdateResourceImmediate(true);

	CurrentPhase = EPhase::Rendering;
}

void FQuickBakerAsyncTask::PhaseRendering()
{
	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	if (!World)
	{
		Finish(false, LOCTEXT("Error_NoWorld", "No valid editor world found."));
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, RenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RenderTarget, Settings.SelectedMaterial.Get());

	CurrentPhase = EPhase::WaitForGPU;
}

void FQuickBakerAsyncTask::PhaseWaitForGPU()
{
	// Flush rendering commands to ensure the GPU has finished drawing
	FlushRenderingCommands();
	CurrentPhase = EPhase::ReadPixels;
}

bool FQuickBakerAsyncTask::PhaseReadPixels()
{
	FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		Finish(false, LOCTEXT("Error_RTResource", "Failed to access render target resource."));
		return false;
	}

	// Calculate rows to read this tick
	const int32 RowsRemaining = TotalRows - ChunkRowStart;
	const int32 RowsThisTick = FMath::Min(ChunkRowSize, RowsRemaining);

	if (RowsThisTick <= 0)
	{
		// All rows read, move to saving
		CurrentPhase = EPhase::Saving;
		return true;
	}

	// Define the region to read: full width, partial height
	const FIntRect ReadRegion(0, ChunkRowStart, Settings.Resolution, ChunkRowStart + RowsThisTick);

	// Calculate offset into the pixel buffer
	const int64 RowBytes = static_cast<int64>(Settings.Resolution) * BytesPerPixel;
	const int64 BufferOffset = static_cast<int64>(ChunkRowStart) * RowBytes;

	if (bIs16Bit)
	{
		TArray<FFloat16Color> ChunkData;
		RTResource->ReadFloat16Pixels(ChunkData, ReadRegion);

		if (ChunkData.Num() > 0)
		{
			const int64 CopySize = static_cast<int64>(ChunkData.Num()) * sizeof(FFloat16Color);
			FMemory::Memcpy(PixelBuffer.GetData() + BufferOffset, ChunkData.GetData(), CopySize);
		}
	}
	else
	{
		TArray<FColor> ChunkData;
		FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
		ReadFlags.SetLinearToGamma(false);
		RTResource->ReadPixels(ChunkData, ReadFlags, ReadRegion);

		if (ChunkData.Num() > 0)
		{
			const int64 CopySize = static_cast<int64>(ChunkData.Num()) * sizeof(FColor);
			FMemory::Memcpy(PixelBuffer.GetData() + BufferOffset, ChunkData.GetData(), CopySize);
		}
	}

	ChunkRowStart += RowsThisTick;

	// Check if all rows have been read
	if (ChunkRowStart >= TotalRows)
	{
		CurrentPhase = EPhase::Saving;
	}

	return true;
}

void FQuickBakerAsyncTask::PhaseSaving()
{
	bool bSuccess = false;
	FText ResultMessage;

	const bool bIsAsset = Settings.OutputType == EQuickBakerOutputType::Asset;
	const bool bIsPNG = Settings.OutputType == EQuickBakerOutputType::PNG;

	if (bIsAsset)
	{
		// --- Save as Texture Asset ---
		FString PackagePath = Settings.OutputPath;
		if (!PackagePath.StartsWith(TEXT("/Game/")))
		{
			PackagePath = TEXT("/Game/") + PackagePath;
		}
		while (PackagePath.Len() > 1 && PackagePath[PackagePath.Len() - 1] == TEXT('/'))
		{
			PackagePath.RemoveAt(PackagePath.Len() - 1, 1, EAllowShrinking::No);
		}

		FString FullPackageName = FPaths::Combine(PackagePath, Settings.OutputName);

		// Ensure physical directory exists
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(FullPackageName, PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FString PackageDirectory = FPaths::GetPath(PackageFilename);
			if (!IFileManager::Get().DirectoryExists(*PackageDirectory))
			{
				if (!IFileManager::Get().MakeDirectory(*PackageDirectory, true))
				{
					Finish(false, LOCTEXT("Error_MakeDirectory", "Failed to create output directory."));
					return;
				}
			}
		}

		UPackage* Package = CreatePackage(*FullPackageName);
		if (!Package)
		{
			Finish(false, LOCTEXT("Error_CreatePackage", "Failed to create package."));
			return;
		}
		Package->FullyLoad();

		UTexture2D* NewTexture = FindObject<UTexture2D>(Package, *Settings.OutputName);
		if (!NewTexture)
		{
			NewTexture = NewObject<UTexture2D>(
				Package,
				*Settings.OutputName,
				RF_Public | RF_Standalone
			);
		}

		if (!NewTexture)
		{
			Finish(false, LOCTEXT("Error_CreateTexture", "Failed to create texture object."));
			return;
		}

		ETextureSourceFormat SourceFormat = bIs16Bit ? TSF_RGBA16F : TSF_BGRA8;
		NewTexture->Source.Init(Settings.Resolution, Settings.Resolution, 1, 1, SourceFormat);
		NewTexture->CompressionSettings = Settings.Compression;
		NewTexture->SRGB = false;
		NewTexture->MipGenSettings = TMGS_NoMipmaps;

		// Copy accumulated pixels into the texture mip
		uint8* MipData = NewTexture->Source.LockMip(0);
		const int64 TextureDataSize = static_cast<int64>(Settings.Resolution) * Settings.Resolution * BytesPerPixel;
		FMemory::Memcpy(MipData, PixelBuffer.GetData(), TextureDataSize);
		NewTexture->Source.UnlockMip(0);

		NewTexture->UpdateResource();
		Package->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewTexture);

		FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackageName, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.SaveFlags = SAVE_NoError;
		SaveArgs.Error = GError;

		bSuccess = UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);
		if (bSuccess)
		{
			ResultMessage = FText::Format(LOCTEXT("Success_Bake", "Texture baked successfully!\nSaved to: {0}"),
				FText::FromString(FullPackageName));
		}
		else
		{
			ResultMessage = LOCTEXT("Error_SavePackage", "Failed to save package to disk.");
		}
	}
	else
	{
		// --- Export as PNG or EXR ---
		FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
		FString FullPath = FPaths::Combine(Settings.OutputPath, Settings.OutputName + Extension);

		bSuccess = FQuickBakerExporter::ExportFromBuffer(
			PixelBuffer,
			Settings.Resolution,
			Settings.Resolution,
			FullPath,
			bIsPNG,
			bIs16Bit
		);

		if (bSuccess)
		{
			ResultMessage = FText::Format(LOCTEXT("Success_Export", "Saved to {0}"), FText::FromString(FullPath));
		}
		else
		{
			ResultMessage = LOCTEXT("Error_SaveFile", "Failed to save file to disk or convert image.");
		}
	}

	Finish(bSuccess, ResultMessage);
}

void FQuickBakerAsyncTask::Finish(bool bSuccess, const FText& Message)
{
	CurrentPhase = EPhase::Done;

	// Free pixel buffer memory
	PixelBuffer.Empty();

	Cleanup();

	if (OnComplete.IsBound())
	{
		OnComplete.Execute(bSuccess, Message);
	}
}

void FQuickBakerAsyncTask::Cleanup()
{
	// Remove ticker
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	// Release render target
	if (RenderTarget)
	{
		RenderTarget->RemoveFromRoot();
		RenderTarget = nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
