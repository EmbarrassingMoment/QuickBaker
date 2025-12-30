// ========================================
// ファイル: Source/QuickBaker/Private/QuickBakerCore.cpp
// ========================================

#include "QuickBakerCore.h"
#include "QuickBakerExporter.h"
#include "Editor.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopeExit.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "FQuickBakerCore"

void FQuickBakerCore::ExecuteBake(const FQuickBakerSettings& Settings)
{
	if (!Settings.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_InvalidSettings", "Invalid bake settings."));
		return;
	}

	const int32 Resolution = Settings.Resolution;
	const bool bIsAsset = Settings.OutputType == EQuickBakerOutputType::Asset;
	const bool bIsPNG = Settings.OutputType == EQuickBakerOutputType::PNG;
	const bool bIsEXR = Settings.OutputType == EQuickBakerOutputType::EXR;

	ETextureRenderTargetFormat Format = RTF_RGBA16f;
	if (bIsAsset)
	{
		Format = Settings.bUse16Bit ? RTF_RGBA16f : RTF_RGBA8;
	}
	else if (bIsPNG)
	{
		Format = RTF_RGBA8;
	}
	else if (bIsEXR)
	{
		Format = RTF_RGBA16f;
	}

	const FString Name = Settings.OutputName;
	FString Path = Settings.OutputPath;

	// Initialize progress display (3 phases)
	FScopedSlowTask Task(3.0f, LOCTEXT("BakingTexture", "Baking Texture..."));
	Task.MakeDialog();

	// Phase 1: Render Target Setup
	Task.EnterProgressFrame(1.0f, LOCTEXT("SetupRT", "Setting up Render Target..."));

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	if (!RenderTarget)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_RTCreate", "Failed to create render target."));
		return;
	}

	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->RenderTargetFormat = Format;
	RenderTarget->bForceLinearGamma = true;
	RenderTarget->SRGB = false;
	RenderTarget->UpdateResourceImmediate(true);

	// GC Protection
	RenderTarget->AddToRoot();

	// Ensure cleanup at end of scope
	ON_SCOPE_EXIT
	{
		if (RenderTarget)
		{
			RenderTarget->RemoveFromRoot();
			RenderTarget->MarkAsGarbage();
		}
	};

	// Phase 2: Material Rendering
	Task.EnterProgressFrame(1.0f, LOCTEXT("Rendering", "Rendering Material..."));

	UWorld* World = nullptr;
	if (GEditor)
	{
		World = GEditor->GetEditorWorldContext().World();
	}

	if (!World)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoWorld", "No valid editor world found."));
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, RenderTarget, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(World, RenderTarget, Settings.Material.Get());

	// Phase 3: Save
	Task.EnterProgressFrame(1.0f, LOCTEXT("Saving", "Saving Asset..."));

	if (bIsAsset)
	{
		FString PackagePath = Path;
		if (!PackagePath.StartsWith(TEXT("/Game/")))
		{
			PackagePath = TEXT("/Game/") + PackagePath;
		}
		while (PackagePath.EndsWith(TEXT("/")) && PackagePath.Len() > 1)
		{
			PackagePath.LeftChopInline(1);
		}

		// Ensure directory exists
		FString FullPackageName = FPaths::Combine(PackagePath, Name);
		FString AssetPackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(FullPackageName, AssetPackageFilename))
		{
			FString PackageDirectory = FPaths::GetPath(AssetPackageFilename);
			if (!IFileManager::Get().DirectoryExists(*PackageDirectory))
			{
				if (!IFileManager::Get().MakeDirectory(*PackageDirectory, true))
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_MakeDirectory", "Failed to create output directory."));
					return;
				}
			}
		}

		// Use transaction only when creating Asset
		FScopedTransaction Transaction(LOCTEXT("BakeTextureTransaction", "Bake Texture"));

		// Create Package
		FString PackageName = FullPackageName;
		FString AssetName = Name;
		UPackage* Package = CreatePackage(*PackageName);

		UTexture2D* NewTexture = UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(RenderTarget, Name, Settings.Compression);

		if (NewTexture)
		{
			NewTexture->Rename(*AssetName, Package);

			// Texture Settings
			NewTexture->CompressionSettings = Settings.Compression;
			NewTexture->SRGB = false; // Requirement: sRGB = false

			// Save
			NewTexture->MarkPackageDirty();
			NewTexture->PostEditChange();

			// Notify Asset Registry
			FAssetRegistryModule::AssetCreated(NewTexture);

			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Success_Bake", "Texture baked successfully!"));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_BakeFailed", "Failed to create texture from render target."));
		}
	}
	else
	{
		if (FQuickBakerExporter::ExportToFile(RenderTarget, Path, Name, Settings.OutputType))
		{
			FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
			FString FullPath = FPaths::Combine(Path, Name + Extension);
			FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Success_Export", "Saved to {0}"), FText::FromString(FullPath)));
		}
		else
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_SaveFile", "Failed to save file to disk or convert image."));
		}
	}
}

#undef LOCTEXT_NAMESPACE
