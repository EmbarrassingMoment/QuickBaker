#include "SQuickBakerWidget.h"
#include "QuickBakerEditorSettings.h"
#include "QuickBakerCore.h"
#include "QuickBakerUtils.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "PropertyCustomizationHelpers.h"
#include "AssetThumbnail.h"
#include "Materials/MaterialInterface.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DesktopPlatformModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"

#define LOCTEXT_NAMESPACE "SQuickBakerWidget"

void SQuickBakerWidget::Construct(const FArguments& InArgs)
{
	bShowMaterialWarning = false;
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(24));
	InitializeOptions();

	ChildSlot
	[
		SNew(SVerticalBox)
		// 1. Material Selection
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
				.ObjectPath_Raw(this, &SQuickBakerWidget::GetSelectedMaterialPath)
				.OnObjectChanged_Raw(this, &SQuickBakerWidget::OnMaterialChanged)
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
					(MaterialThumbnail = MakeShareable(new FAssetThumbnail(Settings.SelectedMaterial.Get(), 64, 64, ThumbnailPool)))->MakeThumbnailWidget()
				]
			]
		]

		// 2. Hint Text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5, 0, 5, 5)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaterialHint", "Tip: Use Unlit materials or ensure Emissive (Final Color) is connected."))
			.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.0f)) // Yellow
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
		]

		// 3. Warning Text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5, 0, 5, 5)
		[
			SAssignNew(WarningTextBlock, STextBlock)
			.Text(LOCTEXT("MaterialWarning", "⚠️ This material is not Unlit. Ensure Emissive (Final Color) is connected."))
			.ColorAndOpacity(FLinearColor(1.0f, 0.5f, 0.0f)) // Orange
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			.Visibility(EVisibility::Collapsed)
		]

		// 4. Output Type
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
				.OnGenerateWidget_Raw(this, &SQuickBakerWidget::GenerateOutputTypeWidget)
				.OnSelectionChanged_Raw(this, &SQuickBakerWidget::OnOutputTypeChanged)
				[
					SNew(STextBlock).Text_Raw(this, &SQuickBakerWidget::GetSelectedOutputTypeText)
				]
			]
		]

		// 5. Resolution
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
				.OnGenerateWidget_Raw(this, &SQuickBakerWidget::GenerateResolutionWidget)
				.OnSelectionChanged_Raw(this, &SQuickBakerWidget::OnResolutionChanged)
				[
					SNew(STextBlock).Text_Raw(this, &SQuickBakerWidget::GetSelectedResolutionText)
				]
			]
		]

		// 6. Bit Depth
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
				SNew(SComboBox<TSharedPtr<EQuickBakerBitDepth>>)
				.IsEnabled_Lambda([this]() { return SelectedOutputType.IsValid() && *SelectedOutputType == EQuickBakerOutputType::Asset; })
				.ToolTipText(LOCTEXT("Tooltip_BitDepth", "Choose between 8-bit and 16-bit. 16-bit is highly recommended for Noise and SDF to avoid banding."))
				.OptionsSource(&BitDepthOptions)
				.InitiallySelectedItem(SelectedBitDepth)
				.OnGenerateWidget_Raw(this, &SQuickBakerWidget::GenerateBitDepthWidget)
				.OnSelectionChanged_Raw(this, &SQuickBakerWidget::OnBitDepthChanged)
				[
					SNew(STextBlock).Text_Raw(this, &SQuickBakerWidget::GetSelectedBitDepthText)
				]
			]
		]

		// 7. Compression
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
				.OnGenerateWidget_Raw(this, &SQuickBakerWidget::GenerateCompressionWidget)
				.OnSelectionChanged_Raw(this, &SQuickBakerWidget::OnCompressionChanged)
				[
					SNew(STextBlock).Text_Raw(this, &SQuickBakerWidget::GetSelectedCompressionText)
				]
			]
		]

		// 8. Output Name
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
				.Text_Lambda([this] { return FText::FromString(Settings.OutputName); })
				.OnTextChanged_Raw(this, &SQuickBakerWidget::OnOutputNameChanged)
			]
		]

		// 9. Output Path
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
					.Text_Lambda([this] { return FText::FromString(Settings.OutputPath); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5, 0, 0, 0)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("Tooltip_Browse", "Open the Asset Picker or Folder Dialog to select a destination folder."))
					.Text(LOCTEXT("Browse", "Browse"))
					.OnClicked_Raw(this, &SQuickBakerWidget::OnBrowseClicked)
				]
			]
		]

		// 10. Bake Button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("Tooltip_Bake", "Start the baking process. This will render the material to a temporary Render Target and save it as a Static Texture or File."))
			.ContentPadding(FMargin(10, 5))
			.HAlign(HAlign_Center)
			.Text(LOCTEXT("BakeTexture", "Bake Texture"))
			.OnClicked_Raw(this, &SQuickBakerWidget::OnBakeClicked)
		]
	];
}

SQuickBakerWidget::~SQuickBakerWidget()
{
	if (ThumbnailPool.IsValid())
	{
		ThumbnailPool.Reset();
	}
}

void SQuickBakerWidget::InitializeOptions()
{
	// Output Type
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::Asset));
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::PNG));
	OutputTypeOptions.Add(MakeShared<EQuickBakerOutputType>(EQuickBakerOutputType::EXR));
	if (OutputTypeOptions.Num() > 0)
	{
		SelectedOutputType = OutputTypeOptions[0]; // Asset
		Settings.OutputType = *SelectedOutputType;
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
		Settings.Resolution = *SelectedResolution;
	}
	else if (ResolutionOptions.Num() > 0)
	{
		SelectedResolution = ResolutionOptions[0];
		Settings.Resolution = *SelectedResolution;
	}

	// Bit Depth
	BitDepthOptions.Add(MakeShared<EQuickBakerBitDepth>(EQuickBakerBitDepth::Bit8));
	BitDepthOptions.Add(MakeShared<EQuickBakerBitDepth>(EQuickBakerBitDepth::Bit16));
	if (BitDepthOptions.Num() > 1)
	{
		SelectedBitDepth = BitDepthOptions[1]; // 16-bit
		Settings.BitDepth = *SelectedBitDepth;
	}

	// Compression
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Default));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Normalmap));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Grayscale));
	CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_HDR));

	if (CompressionOptions.Num() > 0)
	{
		SelectedCompression = CompressionOptions[0]; // TC_Default
		Settings.Compression = *SelectedCompression;
	}

	Settings.OutputPath = TEXT("/Game/Textures");

	// Load saved settings
	LoadSavedSettings();
}

void SQuickBakerWidget::LoadSavedSettings()
{
	UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
	if (!EditorSettings)
	{
		return;
	}

	// Restore Resolution
	for (const auto& Option : ResolutionOptions)
	{
		if (*Option == EditorSettings->LastUsedResolution)
		{
			SelectedResolution = Option;
			Settings.Resolution = *SelectedResolution;
			break;
		}
	}

	// Restore Output Type
	EQuickBakerOutputType SavedOutputType = static_cast<EQuickBakerOutputType>(EditorSettings->LastUsedOutputType);
	for (const auto& Option : OutputTypeOptions)
	{
		if (*Option == SavedOutputType)
		{
			SelectedOutputType = Option;
			Settings.OutputType = *SelectedOutputType;
			break;
		}
	}

	// Restore Bit Depth
	EQuickBakerBitDepth SavedBitDepth = static_cast<EQuickBakerBitDepth>(EditorSettings->LastUsedBitDepth);
	for (const auto& Option : BitDepthOptions)
	{
		if (*Option == SavedBitDepth)
		{
			SelectedBitDepth = Option;
			Settings.BitDepth = *SelectedBitDepth;
			break;
		}
	}

	// Restore Compression
	TextureCompressionSettings SavedCompression = static_cast<TextureCompressionSettings>(EditorSettings->LastUsedCompression);
	for (const auto& Option : CompressionOptions)
	{
		if (*Option == SavedCompression)
		{
			SelectedCompression = Option;
			Settings.Compression = *SelectedCompression;
			break;
		}
	}

	// Restore Output Path
	if (!EditorSettings->LastUsedOutputPath.IsEmpty())
	{
		Settings.OutputPath = EditorSettings->LastUsedOutputPath;
	}
}

void SQuickBakerWidget::OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedOutputType = NewValue;
		Settings.OutputType = *SelectedOutputType;

		if (Settings.OutputType == EQuickBakerOutputType::Asset)
		{
			Settings.OutputPath = TEXT("/Game/Textures");
		}
		else
		{
			Settings.OutputPath = FPaths::ProjectSavedDir();
		}

		if (Settings.OutputType == EQuickBakerOutputType::PNG)
		{
			for (const auto& Option : BitDepthOptions)
			{
				if (*Option == EQuickBakerBitDepth::Bit8)
				{
					SelectedBitDepth = Option;
					break;
				}
			}
		}
		else if (Settings.OutputType == EQuickBakerOutputType::EXR)
		{
			for (const auto& Option : BitDepthOptions)
			{
				if (*Option == EQuickBakerBitDepth::Bit16)
				{
					SelectedBitDepth = Option;
					break;
				}
			}
		}
		Settings.BitDepth = *SelectedBitDepth;

		// Save to config
		UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
		if (EditorSettings)
		{
			EditorSettings->LastUsedOutputType = static_cast<uint8>(Settings.OutputType);
			EditorSettings->LastUsedBitDepth = static_cast<uint8>(Settings.BitDepth);
			EditorSettings->LastUsedOutputPath = Settings.OutputPath;
			EditorSettings->SaveConfig();
		}
	}
}

TSharedRef<SWidget> SQuickBakerWidget::GenerateOutputTypeWidget(TSharedPtr<EQuickBakerOutputType> InOption)
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

FText SQuickBakerWidget::GetSelectedOutputTypeText() const
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

void SQuickBakerWidget::OnMaterialChanged(const FAssetData& AssetData)
{
	Settings.SelectedMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	if (MaterialThumbnail.IsValid())
	{
		MaterialThumbnail->SetAsset(AssetData);
	}

	bShowMaterialWarning = false;
	if (Settings.SelectedMaterial.IsValid())
	{
		// Shading Model Check
		FMaterialShadingModelField ShadingModels = Settings.SelectedMaterial->GetShadingModels();

		if (!ShadingModels.HasShadingModel(MSM_Unlit))
		{
			bShowMaterialWarning = true;
			// Log warning if not Unlit
			UE_LOG(LogTemp, Warning,
				TEXT("QuickBaker: Selected material '%s' is not Unlit. "
					"The tool captures Final Color (Emissive) output. "
					"For best results, use Unlit materials or ensure Emissive is connected."),
				*Settings.SelectedMaterial->GetName());
		}

		FString Name = FQuickBakerUtils::GetTextureNameFromMaterial(Settings.SelectedMaterial->GetName());
		Settings.OutputName = Name;
	}

	if (WarningTextBlock.IsValid())
	{
		WarningTextBlock->SetVisibility(bShowMaterialWarning ? EVisibility::Visible : EVisibility::Collapsed);
	}
}

FString SQuickBakerWidget::GetSelectedMaterialPath() const
{
	return Settings.SelectedMaterial.IsValid() ? Settings.SelectedMaterial->GetPathName() : FString();
}

void SQuickBakerWidget::OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedResolution = NewValue;
		Settings.Resolution = *SelectedResolution;

		// Save to config
		UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
		if (EditorSettings)
		{
			EditorSettings->LastUsedResolution = Settings.Resolution;
			EditorSettings->SaveConfig();
		}
	}
}

TSharedRef<SWidget> SQuickBakerWidget::GenerateResolutionWidget(TSharedPtr<int32> InOption)
{
	return SNew(STextBlock).Text(FText::AsNumber(*InOption));
}

FText SQuickBakerWidget::GetSelectedResolutionText() const
{
	return SelectedResolution.IsValid() ? FText::AsNumber(*SelectedResolution) : FText();
}

void SQuickBakerWidget::OnBitDepthChanged(TSharedPtr<EQuickBakerBitDepth> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedBitDepth = NewValue;
		Settings.BitDepth = *SelectedBitDepth;

		// Save to config
		UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
		if (EditorSettings)
		{
			EditorSettings->LastUsedBitDepth = static_cast<uint8>(Settings.BitDepth);
			EditorSettings->SaveConfig();
		}
	}
}

TSharedRef<SWidget> SQuickBakerWidget::GenerateBitDepthWidget(TSharedPtr<EQuickBakerBitDepth> InOption)
{
	FString BitDepthString;
	if (InOption.IsValid())
	{
		BitDepthString = (*InOption == EQuickBakerBitDepth::Bit8) ? TEXT("8-bit") : TEXT("16-bit");
	}
	return SNew(STextBlock).Text(FText::FromString(BitDepthString));
}

FText SQuickBakerWidget::GetSelectedBitDepthText() const
{
	if (!SelectedBitDepth.IsValid())
	{
		return FText();
	}
	FString BitDepthString = (*SelectedBitDepth == EQuickBakerBitDepth::Bit8) ? TEXT("8-bit") : TEXT("16-bit");
	return FText::FromString(BitDepthString);
}

void SQuickBakerWidget::OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedCompression = NewValue;
		Settings.Compression = *SelectedCompression;

		// Save to config
		UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
		if (EditorSettings)
		{
			EditorSettings->LastUsedCompression = static_cast<uint8>(Settings.Compression);
			EditorSettings->SaveConfig();
		}
	}
}

TSharedRef<SWidget> SQuickBakerWidget::GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption)
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

FText SQuickBakerWidget::GetSelectedCompressionText() const
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

void SQuickBakerWidget::OnOutputNameChanged(const FText& NewText)
{
	Settings.OutputName = NewText.ToString();
}

FReply SQuickBakerWidget::OnBakeClicked()
{
	// Validation
	if (!Settings.SelectedMaterial.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoMaterial", "Please select a material to bake."));
		return FReply::Handled();
	}

	if (!Settings.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_InvalidSettings", "Invalid bake settings."));
		return FReply::Handled();
	}

	if (Settings.OutputName.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoName", "Please specify an output name."));
		return FReply::Handled();
	}

	FQuickBakerCore::ExecuteBake(Settings);
	return FReply::Handled();
}

FReply SQuickBakerWidget::OnBrowseClicked()
{
	if (!SelectedOutputType.IsValid())
	{
		return FReply::Handled();
	}

	if (*SelectedOutputType == EQuickBakerOutputType::Asset)
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		FSaveAssetDialogConfig SaveAssetDialogConfig;
		SaveAssetDialogConfig.DefaultPath = Settings.OutputPath;
		SaveAssetDialogConfig.DefaultAssetName = Settings.OutputName;
		SaveAssetDialogConfig.AssetClassNames.Add(UTexture2D::StaticClass()->GetClassPathName());
		SaveAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
		SaveAssetDialogConfig.DialogTitleOverride = LOCTEXT("SaveTextureDialog", "Save Texture");

		FString SaveObjectPath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(SaveAssetDialogConfig);
		if (!SaveObjectPath.IsEmpty())
		{
			FString PackageName = FPackageName::ObjectPathToPackageName(SaveObjectPath);
			Settings.OutputPath = FPackageName::GetLongPackagePath(PackageName);
			Settings.OutputName = FPackageName::GetShortName(PackageName);
		}
	}
	else
	{
		if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
		{
			FString FolderName;
			TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindBestParentWindowForDialogs(nullptr);
			const void* ParentWindowWindowHandle = (ParentWindow.IsValid()) ? ParentWindow->GetNativeWindow()->GetOSWindowHandle() : nullptr;

			if (DesktopPlatform->OpenDirectoryDialog(
				ParentWindowWindowHandle,
				LOCTEXT("SelectFolder", "Select Output Folder").ToString(),
				Settings.OutputPath,
				FolderName))
			{
				Settings.OutputPath = FolderName;
			}
		}
	}

	// Save Output Path
	UQuickBakerEditorSettings* EditorSettings = GetMutableDefault<UQuickBakerEditorSettings>();
	if (EditorSettings)
	{
		EditorSettings->LastUsedOutputPath = Settings.OutputPath;
		EditorSettings->SaveConfig();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
