// ========================================
// ファイル: Source/QuickBaker/Private/SQuickBakerWidget.cpp
// ========================================

#include "SQuickBakerWidget.h"
#include "QuickBakerCore.h"
#include "QuickBakerUtils.h"
#include "Editor.h"
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
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "DesktopPlatformModule.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "SQuickBakerWidget"

void SQuickBakerWidget::Construct(const FArguments& InArgs, TSharedPtr<FAssetThumbnailPool> InThumbnailPool)
{
	ThumbnailPool = InThumbnailPool;
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
				.ObjectPath(this, &SQuickBakerWidget::GetSelectedMaterialPath)
				.OnObjectChanged(this, &SQuickBakerWidget::OnMaterialChanged)
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

		// Hint Text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5, 0, 5, 5)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MaterialHint", "Tip: Use Unlit materials or ensure Emissive (Final Color) is connected."))
			.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.0f)) // Yellow
			.Font(FCoreStyle::GetDefaultFontStyle("Italic", 8))
		]

		// 2. Output Type
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
				.OnGenerateWidget(this, &SQuickBakerWidget::GenerateOutputTypeWidget)
				.OnSelectionChanged(this, &SQuickBakerWidget::OnOutputTypeChanged)
				[
					SNew(STextBlock).Text(this, &SQuickBakerWidget::GetSelectedOutputTypeText)
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
				.OnGenerateWidget(this, &SQuickBakerWidget::GenerateResolutionWidget)
				.OnSelectionChanged(this, &SQuickBakerWidget::OnResolutionChanged)
				[
					SNew(STextBlock).Text(this, &SQuickBakerWidget::GetSelectedResolutionText)
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
				.IsEnabled_Lambda([this]() { return SelectedOutputType.IsValid() && *SelectedOutputType == EQuickBakerOutputType::Asset; })
				.ToolTipText(LOCTEXT("Tooltip_BitDepth", "Choose between 8-bit and 16-bit. 16-bit is highly recommended for Noise and SDF to avoid banding."))
				.OptionsSource(&BitDepthOptions)
				.InitiallySelectedItem(SelectedBitDepth)
				.OnGenerateWidget(this, &SQuickBakerWidget::GenerateBitDepthWidget)
				.OnSelectionChanged(this, &SQuickBakerWidget::OnBitDepthChanged)
				[
					SNew(STextBlock).Text(this, &SQuickBakerWidget::GetSelectedBitDepthText)
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
				.OnGenerateWidget(this, &SQuickBakerWidget::GenerateCompressionWidget)
				.OnSelectionChanged(this, &SQuickBakerWidget::OnCompressionChanged)
				[
					SNew(STextBlock).Text(this, &SQuickBakerWidget::GetSelectedCompressionText)
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
				.OnTextChanged(this, &SQuickBakerWidget::OnOutputNameChanged)
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
					.OnClicked(this, &SQuickBakerWidget::OnBrowseClicked)
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
			.OnClicked(this, &SQuickBakerWidget::OnBakeClicked)
		]
	];
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

void SQuickBakerWidget::OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo)
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

		if (*SelectedOutputType == EQuickBakerOutputType::PNG)
		{
			for (const auto& Option : BitDepthOptions)
			{
				if (*Option == TEXT("8-bit"))
				{
					SelectedBitDepth = Option;
					break;
				}
			}
		}
		else if (*SelectedOutputType == EQuickBakerOutputType::EXR)
		{
			for (const auto& Option : BitDepthOptions)
			{
				if (*Option == TEXT("16-bit"))
				{
					SelectedBitDepth = Option;
					break;
				}
			}
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
	SelectedMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
	if (MaterialThumbnail.IsValid())
	{
		MaterialThumbnail->SetAsset(AssetData);
	}

	if (SelectedMaterial.IsValid())
	{
		// Shading Model Check
		FMaterialShadingModelField ShadingModels = SelectedMaterial->GetShadingModels();

		if (!ShadingModels.HasShadingModel(MSM_Unlit))
		{
			// Log warning if not Unlit
			UE_LOG(LogTemp, Warning,
				TEXT("QuickBaker: Selected material '%s' is not Unlit. "
					"The tool captures Final Color (Emissive) output. "
					"For best results, use Unlit materials or ensure Emissive is connected."),
				*SelectedMaterial->GetName());
		}

		FString Name = FQuickBakerUtils::GetTextureNameFromMaterial(SelectedMaterial->GetName());
		OutputName = FText::FromString(Name);
	}
}

FString SQuickBakerWidget::GetSelectedMaterialPath() const
{
	return SelectedMaterial.IsValid() ? SelectedMaterial->GetPathName() : FString();
}

void SQuickBakerWidget::OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedResolution = NewValue;
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

void SQuickBakerWidget::OnBitDepthChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedBitDepth = NewValue;
	}
}

TSharedRef<SWidget> SQuickBakerWidget::GenerateBitDepthWidget(TSharedPtr<FString> InOption)
{
	return SNew(STextBlock).Text(FText::FromString(*InOption));
}

FText SQuickBakerWidget::GetSelectedBitDepthText() const
{
	return SelectedBitDepth.IsValid() ? FText::FromString(*SelectedBitDepth) : FText();
}

void SQuickBakerWidget::OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		SelectedCompression = NewValue;
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
	OutputName = NewText;
}

FReply SQuickBakerWidget::OnBakeClicked()
{
	FQuickBakerSettings Settings;
	if (SelectedOutputType.IsValid())
	{
		Settings.OutputType = *SelectedOutputType;
	}
	if (SelectedResolution.IsValid())
	{
		Settings.Resolution = *SelectedResolution;
	}
	if (SelectedBitDepth.IsValid())
	{
		Settings.bUse16Bit = *SelectedBitDepth == TEXT("16-bit");
	}
	if (SelectedCompression.IsValid())
	{
		Settings.Compression = *SelectedCompression;
	}
	Settings.Material = SelectedMaterial;
	Settings.OutputName = OutputName.ToString();
	Settings.OutputPath = OutputPath;

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

#undef LOCTEXT_NAMESPACE
