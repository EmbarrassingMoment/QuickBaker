// Copyright (c) 2025 Kurorekishi (EmbarrassingMoment). Licensed under MIT License.

#include "QuickBakerCore.h"
#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
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

#define LOCTEXT_NAMESPACE "FQuickBakerCore"

void FQuickBakerCore::ExecuteBake(const FQuickBakerSettings& Settings)
{
	// Initialize progress display (3 phases)
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
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_RTCreate", "Failed to create render target."));
		return;
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
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoWorld", "No valid editor world found."));
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, RenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RenderTarget, Settings.SelectedMaterial.Get());

	// Ensure all rendering commands are completed before reading pixels
	FlushRenderingCommands();

	// Phase 3: Save
	Task.EnterProgressFrame(1.0f, LOCTEXT("Saving", "Saving..."));
	if (Task.ShouldCancel())
	{
		return;
	}

	if (bIsAsset)
	{
		BakeToAsset(RenderTarget, Settings);
	}
	else
	{
		// External Export
		FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
		FString FullPath = FPaths::Combine(Settings.OutputPath, Settings.OutputName + Extension);

		if (FQuickBakerExporter::ExportToFile(RenderTarget, FullPath, bIsPNG))
		{
			UE_LOG(LogQuickBaker, Log, TEXT("ExecuteBake success: Saved to %s"), *FullPath);
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Success_Export", "Saved to {0}"), FText::FromString(FullPath)));
		}
		else
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExecuteBake failed: Failed to save file to disk or convert image at %s"), *FullPath);
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_SaveFile", "Failed to save file to disk or convert image."));
		}
	}
}

void FQuickBakerCore::BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings)
{
	FString PackagePath = Settings.OutputPath;
	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		PackagePath = TEXT("/Game/") + PackagePath;
	}
	while (PackagePath.EndsWith(TEXT("/")) && PackagePath.Len() > 1)
	{
		PackagePath.LeftChopInline(1);
	}

	// Ensure directory exists
	FString FullPackageName = FPaths::Combine(PackagePath, Settings.OutputName);
	FString AssetPackageFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(FullPackageName, AssetPackageFilename))
	{
		FString PackageDirectory = FPaths::GetPath(AssetPackageFilename);
		if (!IFileManager::Get().DirectoryExists(*PackageDirectory))
		{
			if (!IFileManager::Get().MakeDirectory(*PackageDirectory, true))
			{
				UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to create output directory at %s"), *PackageDirectory);
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_MakeDirectory", "Failed to create output directory."));
				return;
			}
		}
	}

	// Use transaction only when creating Asset
	FScopedTransaction Transaction(LOCTEXT("BakeTextureTransaction", "Bake Texture"));

	// Create Package
	UPackage* Package = CreatePackage(*FullPackageName);

	UTexture2D* NewTexture = UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(RenderTarget, Settings.OutputName, Settings.Compression);

	if (NewTexture)
	{
		NewTexture->Rename(*Settings.OutputName, Package);

		// Texture Settings
		NewTexture->CompressionSettings = Settings.Compression;
		NewTexture->SRGB = false; // Requirement: sRGB = false

		// Save
		NewTexture->MarkPackageDirty();
		NewTexture->PostEditChange();

		// Notify Asset Registry
		FAssetRegistryModule::AssetCreated(NewTexture);

		UE_LOG(LogQuickBaker, Log, TEXT("BakeToAsset success: Texture baked successfully at %s"), *FullPackageName);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Success_Bake", "Texture baked successfully!"));
	}
	else
	{
		UE_LOG(LogQuickBaker, Error, TEXT("BakeToAsset failed: Failed to create texture from render target at %s"), *FullPackageName);
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_BakeFailed", "Failed to create texture from render target."));
	}
}

#undef LOCTEXT_NAMESPACE
