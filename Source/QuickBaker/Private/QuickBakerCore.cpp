// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerCore.h"
#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopeExit.h"
#include "Editor.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "RenderingThread.h"
#include "UObject/SavePackage.h"
#include "TextureResource.h"

#define LOCTEXT_NAMESPACE "FQuickBakerCore"

namespace QuickBakerInternal
{
	/** Calculate the number of rows to read per chunk, targeting ~16MB per read. */
	static int32 CalcChunkRowSize(int32 Resolution, int32 BytesPerPixel)
	{
		const int64 TargetChunkBytes = 16 * 1024 * 1024; // 16 MB
		int32 Rows = FMath::Max(1, static_cast<int32>(TargetChunkBytes / (static_cast<int64>(Resolution) * BytesPerPixel)));
		return FMath::Min(Rows, Resolution);
	}

	/** Calculate the number of chunks for a given resolution. */
	static int32 CalcNumChunks(int32 Resolution, int32 ChunkRowSize)
	{
		return FMath::DivideAndRoundUp(Resolution, ChunkRowSize);
	}
}

void FQuickBakerCore::ExecuteBake(const FQuickBakerSettings& Settings)
{
	bool bSuccess = false;
	FText ResultMessage;

	const bool bIsAsset = Settings.OutputType == EQuickBakerOutputType::Asset;
	const bool bIsPNG = Settings.OutputType == EQuickBakerOutputType::PNG;
	const bool bIsEXR = Settings.OutputType == EQuickBakerOutputType::EXR;

	// Determine bit depth and format
	bool bIs16Bit = false;
	ETextureRenderTargetFormat Format = RTF_RGBA16f;
	if (bIsAsset)
	{
		bIs16Bit = (Settings.BitDepth == EQuickBakerBitDepth::Bit16);
		Format = bIs16Bit ? RTF_RGBA16f : RTF_RGBA8;
	}
	else if (bIsPNG)
	{
		bIs16Bit = false;
		Format = RTF_RGBA8;
	}
	else if (bIsEXR)
	{
		bIs16Bit = true;
		Format = RTF_RGBA16f;
	}

	const int32 BytesPerPixel = bIs16Bit ? 8 : 4;
	const int32 ChunkRowSize = QuickBakerInternal::CalcChunkRowSize(Settings.Resolution, BytesPerPixel);
	const int32 NumChunks = QuickBakerInternal::CalcNumChunks(Settings.Resolution, ChunkRowSize);

	// Progress weights: Setup(1) + Rendering(1) + ReadPixels(NumChunks) + Saving(1)
	const float TotalWork = 3.0f + static_cast<float>(NumChunks);

	// Scoped block: progress bar is destroyed when this block exits, before any dialog is shown
	{
		FScopedSlowTask Task(TotalWork, LOCTEXT("BakingTexture", "Baking Texture..."));
		Task.MakeDialog(true);

		// Phase 1: Render Target Setup
		Task.EnterProgressFrame(1.0f, LOCTEXT("SetupRT", "Setting up Render Target..."));
		if (Task.ShouldCancel())
		{
			return;
		}

		// Setup Render Target
		UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
			GetTransientPackage(),
			NAME_None,
			RF_Transient
		);

		// GC Protection: Add to root to prevent garbage collection during the baking process.
		if (RenderTarget)
		{
			RenderTarget->AddToRoot();
		}

		// Ensure RemoveFromRoot is called when the function exits (even if early return occurs).
		ON_SCOPE_EXIT
		{
			if (RenderTarget)
			{
				RenderTarget->RemoveFromRoot();
			}
		};

		if (!RenderTarget)
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Failed to create render target."));
			ResultMessage = LOCTEXT("Error_RTCreate", "Failed to create render target.");
			FMessageDialog::Open(EAppMsgType::Ok, ResultMessage);
			return;
		}

		RenderTarget->InitAutoFormat(Settings.Resolution, Settings.Resolution);
		RenderTarget->RenderTargetFormat = Format;
		RenderTarget->bForceLinearGamma = true;
		RenderTarget->SRGB = false;
		RenderTarget->UpdateResourceImmediate(true);

		// Phase 2: Material Rendering
		Task.EnterProgressFrame(1.0f, LOCTEXT("Rendering", "Rendering Material..."));
		if (Task.ShouldCancel())
		{
			return;
		}

		UWorld* World = nullptr;
		if (GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}

		if (!World)
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: No valid editor world found."));
			ResultMessage = LOCTEXT("Error_NoWorld", "No valid editor world found.");
			FMessageDialog::Open(EAppMsgType::Ok, ResultMessage);
			return;
		}

		UKismetRenderingLibrary::ClearRenderTarget2D(World, RenderTarget, FLinearColor::Black);
		UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RenderTarget, Settings.SelectedMaterial.Get());

		// Ensure all rendering commands are completed before reading pixels
		FlushRenderingCommands();

		// Phase 3: Save (includes chunked ReadPixels)
		if (Task.ShouldCancel())
		{
			return;
		}

		if (bIsAsset)
		{
			bSuccess = BakeToAsset(RenderTarget, Settings, ResultMessage);
		}
		else
		{
			// External Export with chunked ReadPixels
			FRenderTarget* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
			if (!RTResource)
			{
				UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Could not get render target resource."));
				ResultMessage = LOCTEXT("Error_RTResource", "Failed to access render target resource.");
			}
			else
			{
				// Pre-allocate buffer for all pixels
				const int64 TotalBytes = static_cast<int64>(Settings.Resolution) * Settings.Resolution * BytesPerPixel;
				TArray<uint8> PixelBuffer;
				PixelBuffer.SetNumZeroed(TotalBytes);

				bool bReadSuccess = true;
				const int64 RowBytes = static_cast<int64>(Settings.Resolution) * BytesPerPixel;

				// Chunked ReadPixels with progress
				for (int32 ChunkStart = 0; ChunkStart < Settings.Resolution; ChunkStart += ChunkRowSize)
				{
					const int32 RowsThisChunk = FMath::Min(ChunkRowSize, Settings.Resolution - ChunkStart);
					Task.EnterProgressFrame(1.0f, FText::Format(
						LOCTEXT("ReadingPixels_Chunk", "Reading pixels... ({0}%)"),
						FText::AsNumber(FMath::RoundToInt(static_cast<float>(ChunkStart) / Settings.Resolution * 100.0f))
					));

					if (Task.ShouldCancel())
					{
						return;
					}

					const FIntRect ReadRegion(0, ChunkStart, Settings.Resolution, ChunkStart + RowsThisChunk);
					const int64 BufferOffset = static_cast<int64>(ChunkStart) * RowBytes;

					if (bIs16Bit)
					{
						TArray<FFloat16Color> ChunkData;
						FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
						ReadFlags.SetLinearToGamma(false);
						RTResource->ReadFloat16Pixels(ChunkData, ReadFlags, ReadRegion);

						if (ChunkData.Num() > 0)
						{
							FMemory::Memcpy(PixelBuffer.GetData() + BufferOffset, ChunkData.GetData(),
								static_cast<int64>(ChunkData.Num()) * sizeof(FFloat16Color));
						}
						else
						{
							bReadSuccess = false;
							break;
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
							FMemory::Memcpy(PixelBuffer.GetData() + BufferOffset, ChunkData.GetData(),
								static_cast<int64>(ChunkData.Num()) * sizeof(FColor));
						}
						else
						{
							bReadSuccess = false;
							break;
						}
					}
				}

				// Save phase
				Task.EnterProgressFrame(1.0f, LOCTEXT("Saving", "Saving..."));

				if (bReadSuccess)
				{
					FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
					FString FullPath = FPaths::Combine(Settings.OutputPath, Settings.OutputName + Extension);

					bSuccess = FQuickBakerExporter::ExportFromBuffer(PixelBuffer, Settings.Resolution, Settings.Resolution, FullPath, bIsPNG, bIs16Bit);
					if (bSuccess)
					{
						UE_LOG(LogQuickBaker, Log, TEXT("ExecuteBake success: Saved to %s"), *FullPath);
						ResultMessage = FText::Format(LOCTEXT("Success_Export", "Saved to {0}"), FText::FromString(FullPath));
					}
					else
					{
						UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Failed to save file to disk or convert image at %s"), *FullPath);
						ResultMessage = LOCTEXT("Error_SaveFile", "Failed to save file to disk or convert image.");
					}
				}
				else
				{
					UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: No pixel data read from render target."));
					ResultMessage = LOCTEXT("Error_NoPixels", "Failed to read pixels from render target.");
				}
			}
		}
	} // FScopedSlowTask is destroyed here — progress bar reaches 100% and closes

	// Show result dialog after the progress bar has fully completed
	FMessageDialog::Open(EAppMsgType::Ok, ResultMessage);
}

bool FQuickBakerCore::BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings, FText& OutResultMessage)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: RenderTarget is null."));
		OutResultMessage = LOCTEXT("Error_NullRT", "Render Target is null.");
		return false;
	}

	bool bIs16Bit = (Settings.BitDepth == EQuickBakerBitDepth::Bit16);
	const int32 BytesPerPixel = bIs16Bit ? 8 : 4;
	const int32 ChunkRowSize = QuickBakerInternal::CalcChunkRowSize(Settings.Resolution, BytesPerPixel);
	const int32 NumChunks = QuickBakerInternal::CalcNumChunks(Settings.Resolution, ChunkRowSize);

	// Nested progress: Setup(1) + ReadPixels(NumChunks) + Save(1)
	FScopedSlowTask SubTask(2.0f + static_cast<float>(NumChunks), LOCTEXT("BakeToAsset", "Creating Texture Asset..."));

	// Sub-phase 1: Package & texture setup
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("AssetSetup", "Setting up package..."));

	// Normalize package path
	FString PackagePath = Settings.OutputPath;
	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		PackagePath = TEXT("/Game/") + PackagePath;
	}
	// Remove trailing slashes in a single pass
	while (PackagePath.Len() > 1 && PackagePath[PackagePath.Len() - 1] == TEXT('/'))
	{
		PackagePath.RemoveAt(PackagePath.Len() - 1, 1, EAllowShrinking::No);
	}

	// Build full package name
	FString FullPackageName = FPaths::Combine(PackagePath, Settings.OutputName);

	UE_LOG(LogQuickBaker, Log, TEXT("BakeToAsset: Creating texture at package: %s"), *FullPackageName);

	// Ensure physical directory exists
	FString PackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(FullPackageName, PackageFilename, FPackageName::GetAssetPackageExtension()))
	{
		FString PackageDirectory = FPaths::GetPath(PackageFilename);
		if (!IFileManager::Get().DirectoryExists(*PackageDirectory))
		{
			if (!IFileManager::Get().MakeDirectory(*PackageDirectory, true))
			{
				UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to create output directory at %s"), *PackageDirectory);
				OutResultMessage = LOCTEXT("Error_MakeDirectory", "Failed to create output directory.");
				return false;
			}
		}
	}

	// Create package
	UPackage* Package = CreatePackage(*FullPackageName);
	if (!Package)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to create package: %s"), *FullPackageName);
		OutResultMessage = LOCTEXT("Error_CreatePackage", "Failed to create package.");
		return false;
	}

	Package->FullyLoad();

	// Check if asset already exists
	UTexture2D* NewTexture = FindObject<UTexture2D>(Package, *Settings.OutputName);
	if (NewTexture)
	{
		UE_LOG(LogQuickBaker, Log, TEXT("BakeToAsset: Updating existing asset %s"), *FullPackageName);
	}
	else
	{
		NewTexture = NewObject<UTexture2D>(
			Package,
			*Settings.OutputName,
			RF_Public | RF_Standalone
		);
	}

	if (!NewTexture)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to create UTexture2D object."));
		OutResultMessage = LOCTEXT("Error_CreateTexture", "Failed to create texture object.");
		return false;
	}

	// Determine pixel format based on bit depth
	ETextureSourceFormat SourceFormat = bIs16Bit ? TSF_RGBA16F : TSF_BGRA8;

	// Initialize texture properties
	NewTexture->Source.Init(Settings.Resolution, Settings.Resolution, 1, 1, SourceFormat);
	NewTexture->CompressionSettings = Settings.Compression;
	NewTexture->SRGB = false;
	NewTexture->MipGenSettings = TMGS_NoMipmaps;

	// Read pixels from RenderTarget
	FRenderTarget* RenderTargetResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RenderTargetResource)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Could not get render target resource."));
		OutResultMessage = LOCTEXT("Error_RTResource", "Failed to access render target resource.");
		return false;
	}

	// Sub-phase 2: Chunked ReadPixels directly into texture mip
	uint8* MipData = NewTexture->Source.LockMip(0);
	bool bReadSuccess = true;
	const int64 RowBytes = static_cast<int64>(Settings.Resolution) * BytesPerPixel;

	for (int32 ChunkStart = 0; ChunkStart < Settings.Resolution; ChunkStart += ChunkRowSize)
	{
		const int32 RowsThisChunk = FMath::Min(ChunkRowSize, Settings.Resolution - ChunkStart);
		SubTask.EnterProgressFrame(1.0f, FText::Format(
			LOCTEXT("ReadingPixels_Chunk", "Reading pixels... ({0}%)"),
			FText::AsNumber(FMath::RoundToInt(static_cast<float>(ChunkStart) / Settings.Resolution * 100.0f))
		));

		const FIntRect ReadRegion(0, ChunkStart, Settings.Resolution, ChunkStart + RowsThisChunk);
		const int64 BufferOffset = static_cast<int64>(ChunkStart) * RowBytes;

		if (bIs16Bit)
		{
			TArray<FFloat16Color> ChunkData;
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
			ReadFlags.SetLinearToGamma(false);
			RenderTargetResource->ReadFloat16Pixels(ChunkData, ReadFlags, ReadRegion);

			if (ChunkData.Num() > 0)
			{
				FMemory::Memcpy(MipData + BufferOffset, ChunkData.GetData(),
					static_cast<int64>(ChunkData.Num()) * sizeof(FFloat16Color));
			}
			else
			{
				bReadSuccess = false;
				break;
			}
		}
		else
		{
			TArray<FColor> ChunkData;
			FReadSurfaceDataFlags ReadFlags(RCM_MinMax);
			ReadFlags.SetLinearToGamma(false);
			RenderTargetResource->ReadPixels(ChunkData, ReadFlags, ReadRegion);

			if (ChunkData.Num() > 0)
			{
				FMemory::Memcpy(MipData + BufferOffset, ChunkData.GetData(),
					static_cast<int64>(ChunkData.Num()) * sizeof(FColor));
			}
			else
			{
				bReadSuccess = false;
				break;
			}
		}
	}

	// Unlock the mip
	NewTexture->Source.UnlockMip(0);

	if (!bReadSuccess)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: No pixel data read from render target."));
		OutResultMessage = LOCTEXT("Error_NoPixels", "Failed to read pixels from render target.");
		return false;
	}

	// Sub-phase 3: Save to disk
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("SavingAsset", "Saving asset to disk..."));

	// Update texture
	NewTexture->UpdateResource();

	// Mark package as dirty
	Package->MarkPackageDirty();

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewTexture);

	// Save package to disk
	FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackageName, FPackageName::GetAssetPackageExtension());

	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	SaveArgs.Error = GError;

	bool bSaved = UPackage::SavePackage(Package, NewTexture, *PackageFileName, SaveArgs);

	if (bSaved)
	{
		UE_LOG(LogQuickBaker, Log, TEXT("BakeToAsset success: Texture saved to %s"), *PackageFileName);
		OutResultMessage = FText::Format(LOCTEXT("Success_Bake", "Texture baked successfully!\nSaved to: {0}"),
			FText::FromString(FullPackageName));
		return true;
	}
	else
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to save package to %s"), *PackageFileName);
		OutResultMessage = LOCTEXT("Error_SavePackage", "Failed to save package to disk.");
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
