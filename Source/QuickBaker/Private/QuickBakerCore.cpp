// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerCore.h"
#include "QuickBakerExporter.h"
#include "QuickBakerUtils.h"
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
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Async/Async.h"

#define LOCTEXT_NAMESPACE "FQuickBakerCore"

void FQuickBakerCore::ExecuteBake(const FQuickBakerSettings& Settings, TFunction<void()> OnComplete)
{
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
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient
	);

	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Failed to create render target."));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_RTCreate", "Failed to create render target."));
		if (OnComplete) { OnComplete(); }
		return;
	}

	// GC Protection
	RenderTarget->AddToRoot();

	RenderTarget->InitAutoFormat(Settings.Resolution, Settings.Resolution);
	RenderTarget->RenderTargetFormat = Format;
	RenderTarget->bForceLinearGamma = true;
	RenderTarget->SRGB = false;
	RenderTarget->UpdateResourceImmediate(true);

	// Render Material
	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	if (!World)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: No valid editor world found."));
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoWorld", "No valid editor world found."));
		RenderTarget->RemoveFromRoot();
		if (OnComplete) { OnComplete(); }
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, RenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RenderTarget, Settings.SelectedMaterial.Get());

	if (bIsAsset)
	{
		// Asset path: synchronous (SavePackage requires game thread)
		FText ResultMessage;
		BakeToAsset(RenderTarget, Settings, ResultMessage);
		RenderTarget->RemoveFromRoot();
		FMessageDialog::Open(EAppMsgType::Ok, ResultMessage);
		if (OnComplete) { OnComplete(); }
	}
	else
	{
		// PNG/EXR path: async compression and disk I/O
		FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
		FString FullPath = FPaths::Combine(Settings.OutputPath, Settings.OutputName + Extension);

		// Show "baking..." notification
		FNotificationInfo Info(FText::Format(LOCTEXT("BakingNotification", "Baking to {0}..."), FText::FromString(Settings.OutputName + Extension)));
		Info.bFireAndForget = false;
		Info.FadeOutDuration = 0.5f;
		Info.ExpireDuration = 0.0f;
		TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		if (NotificationPtr.IsValid())
		{
			NotificationPtr->SetCompletionState(SNotificationItem::CS_Pending);
		}

		FQuickBakerExporter::ExportToFileAsync(RenderTarget, FullPath, bIsPNG,
			[RenderTarget, NotificationPtr, OnComplete](bool bSuccess, const FString& ResultPath)
			{
				// Cleanup render target (game thread)
				RenderTarget->RemoveFromRoot();

				if (NotificationPtr.IsValid())
				{
					if (bSuccess)
					{
						NotificationPtr->SetText(FText::Format(
							LOCTEXT("BakeSuccess", "Bake complete: {0}"), FText::FromString(FPaths::GetCleanFilename(ResultPath))));
						NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
					}
					else
					{
						NotificationPtr->SetText(LOCTEXT("BakeFailed", "Bake failed. Check the Output Log for details."));
						NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
					}
					NotificationPtr->ExpireAndFadeout();
				}

				if (OnComplete) { OnComplete(); }
			});
	}
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

	// Sub-phase 2: Read pixels from render target using async GPU readback
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingPixels", "Reading pixels..."));

	TArray<uint8> PixelData;
	if (!FQuickBakerUtils::ReadbackPixels(RenderTarget, PixelData, bIs16Bit))
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: No pixel data read from render target."));
		OutResultMessage = LOCTEXT("Error_NoPixels", "Failed to read pixels from render target.");
		return false;
	}

	// Lock the texture mip and copy pixel data
	uint8* MipData = NewTexture->Source.LockMip(0);
	FMemory::Memcpy(MipData, PixelData.GetData(), PixelData.Num());
	NewTexture->Source.UnlockMip(0);

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