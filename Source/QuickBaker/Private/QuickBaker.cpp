// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBaker.h"
#include "QuickBakerUtils.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetThumbnail.h"
#include "EditorStyleSet.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/AssetData.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/TextureRenderTarget2D.h"
#include "AssetToolsModule.h"
#include "ScopedTransaction.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor/EditorEngine.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "HAL/FileManager.h"
#include "DesktopPlatformModule.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"

static const FName QuickBakerTabName("QuickBaker");

#define LOCTEXT_NAMESPACE "FQuickBakerModule"

void FQuickBakerModule::StartupModule()
{
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));

	InitializeOptions();

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(QuickBakerTabName, FOnSpawnTab::CreateRaw(this, &FQuickBakerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("QuickBakerTabTitle", "QuickBaker"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FQuickBakerModule::RegisterMenus));
}

void FQuickBakerModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	if (ThumbnailPool.IsValid())
	{
		ThumbnailPool.Reset();
	}

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(QuickBakerTabName);
}

void FQuickBakerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(QuickBakerTabName);
}

void FQuickBakerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
	{
		FToolMenuSection& Section = Menu->FindOrAddSection("QuickBaker");
		Section.Label = LOCTEXT("QuickBakerLabel", "QuickBaker");

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"QuickBaker",
			LOCTEXT("QuickBakerCommand", "Quick Baker"),
			LOCTEXT("QuickBakerTooltip", "Opens the Quick Baker window"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FQuickBakerModule::PluginButtonClicked))
		);

		Section.AddEntry(Entry);
	}
}

void FQuickBakerModule::InitializeOptions()
{
	// Output Type
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::Asset));
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::PNG));
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::EXR));
	if (OutputTypeOptions.Num() > 0)
	{
		SelectedOutputType = OutputTypeOptions[0]; // Asset
	}

	// Resolution
	TArray<int32> Resolutions = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };
	for (int32 Res : Resolutions)
	{
		ResolutionOptions.Add(MakeShared<int32>(Res));
	}
	if (ResolutionOptions.Num() > 4)
	{
		SelectedResolution = ResolutionOptions[4]; // 1024
	}
	else if (ResolutionOptions.Num() > 0)
	{
		SelectedResolution = ResolutionOptions[0];
	}

	// Bit Depth
	BitDepthOptions.Add(MakeShared<FString>("8-bit"));
	BitDepthOptions.Add(MakeShared<FString>("16-bit"));
	if (BitDepthOptions.Num() > 1)
	{
		SelectedBitDepth = BitDepthOptions[1]; // 16-bit
	}

	// Compression
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Default));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Normalmap));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Grayscale));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_HDR));

	if (CompressionOptions.Num() > 0)
	{
		SelectedCompression = CompressionOptions[0]; // TC_Default
	}

	OutputPath = TEXT("/Game/Textures");
}

TSharedRef<SDockTab> FQuickBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SVerticalBox)
			// 1. Output Type
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_OutputType", "Output Type"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<EQuickBakerOutputType>>)
					.ToolTipText(LOCTEXT("Tooltip_OutputType", "Select the output format: Asset, PNG, or EXR."))
					.OptionsSource(&OutputTypeOptions)
					.InitiallySelectedItem(SelectedOutputType)
					.OnGenerateWidget_Raw(this, &FQuickBakerModule::GenerateOutputTypeWidget)
					.OnSelectionChanged_Raw(this, &FQuickBakerModule::OnOutputTypeChanged)
					[
						SNew(STextBlock).Text_Raw(this, &FQuickBakerModule::GetSelectedOutputTypeText)
					]
				]
			]

			// 2. Material Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(0, 0, 10, 4)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_Material", "Material"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Bottom)
				[
					SNew(SObjectPropertyEntryBox)
					.ToolTipText(LOCTEXT("Tooltip_Material", "Select the Material or Material Instance to bake. The 'Final Color' (Emissive) will be captured."))
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath_Raw(this, &FQuickBakerModule::GetSelectedMaterialPath)
					.OnObjectChanged_Raw(this, &FQuickBakerModule::OnMaterialChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
					.DisplayThumbnail(false)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Bottom)
				.Padding(5, 0, 0, 0)
				[
					SNew(SBox)
					.WidthOverride(64)
					.HeightOverride(64)
					[
						(MaterialThumbnail = MakeShareable(new FAssetThumbnail(SelectedMaterial.Get(), 64, 64, ThumbnailPool)))->MakeThumbnailWidget()
					]
				]
			]

			// 3. Resolution
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_Resolution", "Resolution"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<int32>>)
					.ToolTipText(LOCTEXT("Tooltip_Resolution", "Set the width and height of the output texture. Higher values provide more detail but use more memory."))
					.OptionsSource(&ResolutionOptions)
					.InitiallySelectedItem(SelectedResolution)
					.OnGenerateWidget_Raw(this, &FQuickBakerModule::GenerateResolutionWidget)
					.OnSelectionChanged_Raw(this, &FQuickBakerModule::OnResolutionChanged)
					[
						SNew(STextBlock).Text_Raw(this, &FQuickBakerModule::GetSelectedResolutionText)
					]
				]
			]

			// 4. Bit Depth
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_BitDepth", "Bit Depth"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.ToolTipText(LOCTEXT("Tooltip_BitDepth", "Choose between 8-bit and 16-bit. 16-bit is highly recommended for Noise and SDF to avoid banding."))
					.OptionsSource(&BitDepthOptions)
					.InitiallySelectedItem(SelectedBitDepth)
					.OnGenerateWidget_Raw(this, &FQuickBakerModule::GenerateBitDepthWidget)
					.OnSelectionChanged_Raw(this, &FQuickBakerModule::OnBitDepthChanged)
					[
						SNew(STextBlock).Text_Raw(this, &FQuickBakerModule::GetSelectedBitDepthText)
					]
				]
			]

			// 5. Compression
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_Compression", "Compression"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SComboBox<TSharedPtr<TextureCompressionSettings>>)
					.ToolTipText(LOCTEXT("Tooltip_Compression", "Select the compression format for the resulting texture. TC_Default is standard for most cases."))
					.OptionsSource(&CompressionOptions)
					.InitiallySelectedItem(SelectedCompression)
					.OnGenerateWidget_Raw(this, &FQuickBakerModule::GenerateCompressionWidget)
					.OnSelectionChanged_Raw(this, &FQuickBakerModule::OnCompressionChanged)
					[
						SNew(STextBlock).Text_Raw(this, &FQuickBakerModule::GetSelectedCompressionText)
					]
				]
			]

			// 6. Output Name
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_OutputName", "Output Name"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SEditableTextBox)
					.ToolTipText(LOCTEXT("Tooltip_OutputName", "The name of the generated Texture asset. Automatically prefixed with T_ based on naming conventions."))
					.Text_Lambda([this] { return OutputName; })
					.OnTextChanged_Raw(this, &FQuickBakerModule::OnOutputNameChanged)
				]
			]

			// 7. Output Path
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_OutputPath", "Output Path"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.ToolTipText(LOCTEXT("Tooltip_OutputPath", "The destination folder in the Content Browser or on disk."))
						.Text_Lambda([this] { return FText::FromString(OutputPath); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("Tooltip_Browse", "Open the Asset Picker or Folder Dialog to select a destination folder."))
						.Text(LOCTEXT("Browse", "Browse"))
						.OnClicked_Raw(this, &FQuickBakerModule::OnBrowseClicked)
					]
				]
			]

			// 8. Bake Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("Tooltip_Bake", "Start the baking process. This will render the material to a temporary Render Target and save it as a Static Texture or File."))
				.ContentPadding(FMargin(10, 5))
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("BakeTexture", "Bake Texture"))
				.OnClicked_Raw(this, &FQuickBakerModule::OnBakeClicked)
			]
		];
}

void FQuickBakerModule::OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedOutputType = NewValue;
		if (*SelectedOutputType == EQuickBakerOutputType::Asset)
		{
			OutputPath = TEXT("/Game/Textures");
		}
		else
		{
			OutputPath = FPaths::ProjectSavedDir();
		}
	}
}

TSharedRef<SWidget> FQuickBakerModule::GenerateOutputTypeWidget(TSharedPtr<EQuickBakerOutputType> InOption)
{
	FString OutputTypeString;
	switch (*InOption)
	{
	case EQuickBakerOutputType::Asset:
		OutputTypeString = "Texture Asset";
		break;
	case EQuickBakerOutputType::PNG:
		OutputTypeString = "PNG";
		break;
	case EQuickBakerOutputType::EXR:
		OutputTypeString = "EXR";
		break;
	}
	return SNew(STextBlock).Text(FText::FromString(OutputTypeString));
}

FText FQuickBakerModule::GetSelectedOutputTypeText() const
{
	if (!SelectedOutputType.IsValid())
	{
		return FText();
	}
	FString OutputTypeString;
	switch (*SelectedOutputType)
	{
	case EQuickBakerOutputType::Asset:
		OutputTypeString = "Texture Asset";
		break;
	case EQuickBakerOutputType::PNG:
		OutputTypeString = "PNG";
		break;
	case EQuickBakerOutputType::EXR:
		OutputTypeString = "EXR";
		break;
	}
	return FText::FromString(OutputTypeString);
}

void FQuickBakerModule::OnMaterialChanged(const FAssetData& AssetData)
{
	SelectedMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	if (MaterialThumbnail.IsValid())
	{
		MaterialThumbnail->SetAsset(AssetData);
	}

	if (SelectedMaterial.IsValid())
	{
		FString Name = FQuickBakerUtils::GetTextureNameFromMaterial(SelectedMaterial->GetName());
		OutputName = FText::FromString(Name);
	}
}

FString FQuickBakerModule::GetSelectedMaterialPath() const
{
	return SelectedMaterial.IsValid() ? SelectedMaterial->GetPathName() : FString();
}

void FQuickBakerModule::OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedResolution = NewValue;
	}
}

TSharedRef<SWidget> FQuickBakerModule::GenerateResolutionWidget(TSharedPtr<int32> InOption)
{
	return SNew(STextBlock).Text(FText::AsNumber(*InOption));
}

FText FQuickBakerModule::GetSelectedResolutionText() const
{
	return SelectedResolution.IsValid() ? FText::AsNumber(*SelectedResolution) : FText();
}

void FQuickBakerModule::OnBitDepthChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedBitDepth = NewValue;
	}
}

TSharedRef<SWidget> FQuickBakerModule::GenerateBitDepthWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption));
}

FText FQuickBakerModule::GetSelectedBitDepthText() const
{
	return SelectedBitDepth.IsValid() ? FText::FromString(*SelectedBitDepth) : FText();
}

void FQuickBakerModule::OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedCompression = NewValue;
	}
}

TSharedRef<SWidget> FQuickBakerModule::GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption)
{
	FString EnumName = TEXT("Unknown");
	if (InOption.IsValid())
	{
		if (UEnum* Enum = StaticEnum<TextureCompressionSettings>())
		{
			EnumName = Enum->GetNameStringByValue((int64)*InOption);
		}
	}
	return SNew(STextBlock).Text(FText::FromString(EnumName));
}

FText FQuickBakerModule::GetSelectedCompressionText() const
{
	if (!SelectedCompression.IsValid())
	{
		return FText();
	}
	FString EnumName = TEXT("Unknown");
	if (UEnum* Enum = StaticEnum<TextureCompressionSettings>())
	{
		EnumName = Enum->GetNameStringByValue((int64)*SelectedCompression);
	}
	return FText::FromString(EnumName);
}

void FQuickBakerModule::OnOutputNameChanged(const FText& NewText)
{
	OutputName = NewText;
}

FReply FQuickBakerModule::OnBakeClicked()
{
	ExecuteBake();
	return FReply::Handled();
}

FReply FQuickBakerModule::OnBrowseClicked()
{
	if (!SelectedOutputType.IsValid())
	{
		return FReply::Handled();
	}

	if (*SelectedOutputType == EQuickBakerOutputType::Asset)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DefaultPath = OutputPath;
		SaveAssetDialogConfig.DefaultAssetName = OutputName.ToString();
		SaveAssetDialogConfig.AssetClassNames.Add(UTexture2D::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveTextureDialog", "Save Texture");

		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			OutputPath = FPackageName::GetLongPackagePath(PackageName);
			OutputName = FText::FromString(FPackageName::GetShortName(PackageName));
		}
	}
	else
	{
		if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
		{
			FString FolderName;
			const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

			if (DesktopPlatform->OpenDirectoryDialog(
				ParentWindowWindowHandle,
				LOCTEXT("SelectFolder", "Select Output Folder").ToString(),
				OutputPath,
				FolderName))
			{
				OutputPath = FolderName;
			}
		}
	}

	return FReply::Handled();
}

void FQuickBakerModule::ExecuteBake()
{
	// 1. Validation
	if (!SelectedMaterial.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoMaterial", "Please select a material to bake."));
		return;
	}

	if (!SelectedResolution.IsValid() || !SelectedBitDepth.IsValid() || !SelectedCompression.IsValid() || !SelectedOutputType.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_InvalidSettings", "Invalid bake settings."));
		return;
	}

	const int32 Resolution = *SelectedResolution;
	// For PNG force 8bit, for EXR force 16bit float
	const bool bIsAsset = *SelectedOutputType == EQuickBakerOutputType::Asset;
	const bool bIsPNG = *SelectedOutputType == EQuickBakerOutputType::PNG;
	const bool bIsEXR = *SelectedOutputType == EQuickBakerOutputType::EXR;

	// If Asset, use UI selection. If PNG -> 8bit. If EXR -> 16bit float.
	ETextureRenderTargetFormat Format = RTF_RGBA16f;
	if (bIsAsset)
	{
		bool bIs16Bit = *SelectedBitDepth == "16-bit";
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

	const FString Name = OutputName.ToString();
	FString Path = OutputPath;

	if (Name.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoName", "Please specify an output name."));
		return;
	}

	// 2. Setup Render Target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	check(RenderTarget);
	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->RenderTargetFormat = Format;
	RenderTarget->bForceLinearGamma = true;
	RenderTarget->SRGB = false;
	RenderTarget->UpdateResourceImmediate(true);

	// 3. Bake Material
	UObject* WorldContextObject = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, RenderTarget, SelectedMaterial.Get());

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

		FScopedTransaction Transaction(LOCTEXT("BakeTextureTransaction", "Bake Texture"));

		// 4. Convert to Static Texture
		FString PackageName = FullPackageName;
		FString AssetName = Name;

		// Create Package
		UPackage* Package = CreatePackage(*PackageName);

		UTexture2D* NewTexture = UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(RenderTarget, Name, *SelectedCompression);

		if (NewTexture)
		{
			NewTexture->Rename(*AssetName, Package);

			// 5. Texture Settings
			NewTexture->CompressionSettings = *SelectedCompression;
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
		// External Export
		FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
		if (RTResource)
		{
			FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
			ReadPixelFlags.SetLinearToGamma(false);

			TArray<uint8> CompressedData;
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

			if (bIsPNG)
			{
				// PNG Export (8-bit)
				TArray<FColor> Bitmap;
				RTResource->ReadPixels(Bitmap, ReadPixelFlags);

				TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
				if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), Resolution, Resolution, ERGBFormat::BGRA, 8))
				{
					CompressedData = ImageWrapper->GetCompressed();
				}
			}
			else if (bIsEXR)
			{
				// EXR Export (16-bit float)
				TArray<FFloat16Color> Bitmap;
				RTResource->ReadFloat16Pixels(Bitmap); // ReadFloat16Pixels does not take flags usually, checking docs or assuming standard behavior

				TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
				if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), Resolution, Resolution, ERGBFormat::RGBAF, 16))
				{
					CompressedData = ImageWrapper->GetCompressed();
				}
			}

			if (CompressedData.Num() > 0)
			{
				FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
				FString FullPath = FPaths::Combine(Path, Name + Extension);

				if (FFileHelper::SaveArrayToFile(CompressedData, *FullPath))
				{
					FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("Success_Export", "Saved to {0}"), FText::FromString(FullPath)));
				}
				else
				{
					FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_SaveFile", "Failed to save file to disk."));
				}
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_ImageConversion", "Failed to convert image."));
			}
		}
	}

	// 6. Cleanup handled by GC
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
