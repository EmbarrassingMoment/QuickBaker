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
#include "Async/ParallelFor.h"

#define LOCTEXT_NAMESPACE "FQuickBakerCore"

void FQuickBakerCore::ExecuteBake(const FQuickBakerSettings& Settings)
{
	bool bSuccess = false;
	FText ResultMessage;

	// Scoped block: progress bar is destroyed when this block exits, before any dialog is shown
	{
		FScopedSlowTask Task(4.0f, LOCTEXT("BakingTexture", "Baking Texture..."));
		Task.MakeDialog(true);

		// Phase 1: Render Target Setup
		Task.EnterProgressFrame(1.0f, LOCTEXT("SetupRT", "Setting up Render Target..."));
		if (Task.ShouldCancel())
		{
			return;
		}

		const bool bIsAsset = Settings.OutputType == EQuickBakerOutputType::Asset;
		const bool bIsPNG = Settings.OutputType == EQuickBakerOutputType::PNG;
		const bool bIsEXR = Settings.OutputType == EQuickBakerOutputType::EXR;

		// Determine Format
		ETextureRenderTargetFormat Format = RTF_RGBA16f;
		if (bIsAsset)
		{
			bool bIs16Bit = Settings.BitDepth == EQuickBakerBitDepth::Bit16;
			Format = bIs16Bit ? RTF_RGBA16f : RTF_RGBA8;
		}
		else if (bIsPNG)
		{
			Format = RTF_RGBA8;
		}
		else if (bIsEXR)
		{
			Format = RTF_RGBA16f;
		}

		// Setup Render Target
		// Create a transient render target.
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

		// Validate resolution does not exceed GPU hardware limit
		{
			// Hardcoded to 16384: GMaxTextureDimensions (RHI.h) requires linking RHICore which is
			// unavailable to editor plugins. 16384 is the maximum texture dimension for D3D11/D3D12/Vulkan.
			constexpr int32 MaxDimension = 16384;
			if (Settings.Resolution > MaxDimension)
			{
				UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Resolution %d exceeds maximum supported texture dimension %d."), Settings.Resolution, MaxDimension);
				FMessageDialog::Open(EAppMsgType::Ok, FText::Format(
					LOCTEXT("Error_ResolutionExceedsMax", "Resolution {0} exceeds the maximum supported texture dimension ({1})."),
					FText::AsNumber(Settings.Resolution),
					FText::AsNumber(MaxDimension)));
				return;
			}
		}

		RenderTarget->InitAutoFormat(Settings.Resolution, Settings.Resolution);
		RenderTarget->RenderTargetFormat = Format;
		RenderTarget->bForceLinearGamma = true;
		RenderTarget->SRGB = false;
		RenderTarget->UpdateResourceImmediate(true);

		// Phase 2: Material Rendering
		Task.EnterProgressFrame(2.0f, LOCTEXT("Rendering", "Rendering Material..."));
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

		// Ensure all rendering commands are completed before reading pixels.
		// Note: Caller is responsible for flushing before passing RT to BakeToAsset / ExportToFile
		FlushRenderingCommands();

		// Phase 3: Save
		Task.EnterProgressFrame(1.0f, LOCTEXT("Saving", "Saving..."));
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
			// External Export
			FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
			FString FullPath = FPaths::Combine(Settings.OutputPath, Settings.OutputName + Extension);

			bSuccess = FQuickBakerExporter::ExportToFile(RenderTarget, FullPath, bIsPNG);
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
	} // FScopedSlowTask is destroyed here — progress bar reaches 100% and closes

	// Show result dialog after the progress bar has fully completed
	FMessageDialog::Open(EAppMsgType::Ok, ResultMessage);
}

bool FQuickBakerCore::BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings, FText& OutResultMessage)
{
	// Nested progress: 3 sub-phases (Setup, Read Pixels, Save to Disk)
	FScopedSlowTask SubTask(3.0f, LOCTEXT("BakeToAsset", "Creating Texture Asset..."));

	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: RenderTarget is null."));
		OutResultMessage = LOCTEXT("Error_NullRT", "Render Target is null.");
		return false;
	}

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
	bool bIs16Bit = (Settings.BitDepth == EQuickBakerBitDepth::Bit16);
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

	// Sub-phase 2: Read pixels from render target
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingPixels", "Reading pixels..."));

	// Lock the texture mip for editing
	uint8* MipData = NewTexture->Source.LockMip(0);
	int64 TextureDataSize = 0;
	bool bReadSuccess = false;

	const int64 NumPixels = (int64)Settings.Resolution * Settings.Resolution;

	if (bIs16Bit)
	{
		TArray<FFloat16Color> SurfaceData;
		if (RenderTargetResource->ReadFloat16Pixels(SurfaceData))
		{
			if (SurfaceData.Num() > 0)
			{
				if (SurfaceData.Num() != NumPixels)
				{
					UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Pixel count mismatch. Expected %lld, got %d"), NumPixels, SurfaceData.Num());
				}
				else
				{
					// FMemory::Memcpy is retained for 16-bit path: contiguous bulk copy is faster than
					// ParallelFor for FFloat16Color since no per-pixel transformation is needed.
					TextureDataSize = (int64)SurfaceData.Num() * sizeof(FFloat16Color);
					FMemory::Memcpy(MipData, SurfaceData.GetData(), TextureDataSize);
					bReadSuccess = true;
				}
			}
		}
	}
	else
	{
		TArray<FColor> SurfaceData;
		FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
		ReadPixelFlags.SetLinearToGamma(false);

		if (RenderTargetResource->ReadPixels(SurfaceData, ReadPixelFlags))
		{
			if (SurfaceData.Num() > 0)
			{
				if (SurfaceData.Num() != NumPixels)
				{
					UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Pixel count mismatch. Expected %lld, got %d"), NumPixels, SurfaceData.Num());
				}
				else
				{
					// ParallelFor pixel copy to allow future per-pixel operations (swizzle, etc.)
					const FColor* SrcData = SurfaceData.GetData();
					FColor* DstData = reinterpret_cast<FColor*>(MipData);
					ParallelFor(NumPixels, [SrcData, DstData](int64 Index)
					{
						DstData[Index] = SrcData[Index];
					});
					TextureDataSize = NumPixels * sizeof(FColor);
					bReadSuccess = true;
				}
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