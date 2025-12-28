// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBaker.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "ToolMenus.h"
#include "PropertyCustomizationHelpers.h"
#include "DesktopPlatformModule.h"
#include "Materials/MaterialInterface.h"
#include "Engine/TextureDefines.h"
#include "EditorStyleSet.h"

static const FName QuickBakerTabName("QuickBaker");

#define LOCTEXT_NAMESPACE "FQuickBakerModule"

// ------------------------------------------------------------------------------------------------
// SQuickBakerWidget
// ------------------------------------------------------------------------------------------------

class SQuickBakerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuickBakerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		// Initialize options
		ResolutionOptions = {
			MakeShared<int32>(64), MakeShared<int32>(128), MakeShared<int32>(256),
			MakeShared<int32>(512), MakeShared<int32>(1024), MakeShared<int32>(2048),
			MakeShared<int32>(4096), MakeShared<int32>(8192)
		};
		SelectedResolution = ResolutionOptions[4]; // Default 1024

		BitDepthOptions = {
			MakeShared<FString>(TEXT("8-bit")),
			MakeShared<FString>(TEXT("16-bit"))
		};
		SelectedBitDepth = BitDepthOptions[1]; // Default 16-bit

		// Populate compression settings (simplified set as per requirement usually implies useful ones, but requirement says "Options based on TextureCompressionSettings")
		// We'll add a few common ones.
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Default));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Normalmap));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Masks));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Grayscale));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Displacementmap));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_VectorDisplacementmap));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_HDR));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_EditorIcon));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_Alpha));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_DistanceFieldFont));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_HDR_Compressed));
		CompressionOptions.Add(MakeShared<TextureCompressionSettings>(TC_BC7));

		SelectedCompression = CompressionOptions[0]; // TC_Default

		ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0f)
			[
				SNew(SVerticalBox)

				// Material Selector
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MaterialLabel", "Material"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.7f)
					[
						SNew(SObjectPropertyEntryBox)
						.AllowedClass(UMaterialInterface::StaticClass())
						.ObjectPath(this, &SQuickBakerWidget::GetMaterialPath)
						.OnObjectChanged(this, &SQuickBakerWidget::OnMaterialChanged)
					]
				]

				// Resolution Dropdown
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ResolutionLabel", "Resolution"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.7f)
					[
						SNew(SComboBox<TSharedPtr<int32>>)
						.OptionsSource(&ResolutionOptions)
						.OnGenerateWidget(this, &SQuickBakerWidget::MakeResolutionWidget)
						.OnSelectionChanged(this, &SQuickBakerWidget::OnResolutionChanged)
						.InitiallySelectedItem(SelectedResolution)
						[
							SNew(STextBlock)
							.Text(this, &SQuickBakerWidget::GetResolutionText)
						]
					]
				]

				// Bit Depth Dropdown
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BitDepthLabel", "Bit Depth"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.7f)
					[
						SNew(SComboBox<TSharedPtr<FString>>)
						.OptionsSource(&BitDepthOptions)
						.OnGenerateWidget(this, &SQuickBakerWidget::MakeBitDepthWidget)
						.OnSelectionChanged(this, &SQuickBakerWidget::OnBitDepthChanged)
						.InitiallySelectedItem(SelectedBitDepth)
						[
							SNew(STextBlock)
							.Text(this, &SQuickBakerWidget::GetBitDepthText)
						]
					]
				]

				// Compression Dropdown
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CompressionLabel", "Compression"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.7f)
					[
						SNew(SComboBox<TSharedPtr<TextureCompressionSettings>>)
						.OptionsSource(&CompressionOptions)
						.OnGenerateWidget(this, &SQuickBakerWidget::MakeCompressionWidget)
						.OnSelectionChanged(this, &SQuickBakerWidget::OnCompressionChanged)
						.InitiallySelectedItem(SelectedCompression)
						[
							SNew(STextBlock)
							.Text(this, &SQuickBakerWidget::GetCompressionText)
						]
					]
				]

				// Output Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OutputNameLabel", "Output Name"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.7f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SQuickBakerWidget::GetOutputName)
						.OnTextChanged(this, &SQuickBakerWidget::OnOutputNameChanged)
					]
				]

				// Output Path
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(0.3f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OutputPathLabel", "Output Path"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.55f)
					[
						SNew(SEditableTextBox)
						.Text(this, &SQuickBakerWidget::GetOutputPath)
						.OnTextChanged(this, &SQuickBakerWidget::OnOutputPathChanged)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.15f)
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("BrowseButton", "Browse"))
						.OnClicked(this, &SQuickBakerWidget::OnBrowseClicked)
						.HAlign(HAlign_Center)
					]
				]

				// sRGB Note
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 5)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SRGBNote", "Note: sRGB is fixed to False."))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]

			// Bake Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0f)
			.HAlign(HAlign_Fill)
			[
				SNew(SButton)
				.Text(LOCTEXT("BakeButton", "Bake Texture"))
				.OnClicked(this, &SQuickBakerWidget::OnBakeClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(0, 10))
			]
		];
	}

private:
	// State Variables
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;
	TArray<TSharedPtr<int32>> ResolutionOptions;
	TSharedPtr<int32> SelectedResolution;
	TArray<TSharedPtr<FString>> BitDepthOptions;
	TSharedPtr<FString> SelectedBitDepth;
	TArray<TSharedPtr<TextureCompressionSettings>> CompressionOptions;
	TSharedPtr<TextureCompressionSettings> SelectedCompression;
	FString OutputName;
	FString OutputPath;

	// Callbacks & Helpers
	FString GetMaterialPath() const
	{
		return SelectedMaterial.IsValid() ? SelectedMaterial->GetPathName() : FString();
	}

	void OnMaterialChanged(const FAssetData& AssetData)
	{
		SelectedMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
		if (SelectedMaterial.IsValid())
		{
			// Auto-convert name
			FString MatName = SelectedMaterial->GetName();
			if (MatName.StartsWith("M_"))
			{
				OutputName = MatName.Replace(TEXT("M_"), TEXT("T_"));
			}
			else if (MatName.StartsWith("MI_"))
			{
				OutputName = MatName.Replace(TEXT("MI_"), TEXT("T_"));
			}
			else
			{
				OutputName = FString::Printf(TEXT("T_%s"), *MatName);
			}

			// Default Path: Directory of selected material
			// Path name usually includes the asset name, so we strip it.
			FString PackagePath = AssetData.PackagePath.ToString();
			// Convert package path to system path? Or just keep it as package path?
			// Requirement says: "Text field with a 'Browse' button to pick a folder." which implies filesystem path usually.
			// But UAssets are in package paths.
			// However, usually baking tools output to Content Browser paths (Package paths).
			// If it's a "folder", usually it means OS folder if exporting to disk, or Content Path if saving as asset.
			// Given "Bake Texture" usually creates a texture asset in UE, I'll assume Content Path.
			// But "Browse" button usually opens a directory dialog.
			// If it's creating a UTexture asset, we need a Package Path. If it's exporting a .png, we need a file path.
			// "Bake Texture" implies creating a texture asset.
			// Standard behavior for "Browse" in editor for assets involves picking a path in the content browser.
			// But `IDesktopPlatform::OpenDirectoryDialog` picks a disk path.
			// Let's assume Package Path is preferred for assets, but use a simple text string.
			// If I use `IDesktopPlatform` it will return a disk path.
			// Let's stick to Package Path logic if possible, but the requirement specifically says "Browse button to pick a folder".
			// I'll stick to string handling.
			// Let's set the default to the package path.
			OutputPath = PackagePath;
		}
	}

	TSharedRef<SWidget> MakeResolutionWidget(TSharedPtr<int32> Item)
	{
		return SNew(STextBlock).Text(FText::AsNumber(*Item));
	}

	void OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type)
	{
		SelectedResolution = NewValue;
	}

	FText GetResolutionText() const
	{
		return SelectedResolution.IsValid() ? FText::AsNumber(*SelectedResolution) : FText::GetEmpty();
	}

	TSharedRef<SWidget> MakeBitDepthWidget(TSharedPtr<FString> Item)
	{
		return SNew(STextBlock).Text(FText::FromString(*Item));
	}

	void OnBitDepthChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type)
	{
		SelectedBitDepth = NewValue;
	}

	FText GetBitDepthText() const
	{
		return SelectedBitDepth.IsValid() ? FText::FromString(*SelectedBitDepth) : FText::GetEmpty();
	}

	TSharedRef<SWidget> MakeCompressionWidget(TSharedPtr<TextureCompressionSettings> Item)
	{
		return SNew(STextBlock).Text(UEnum::GetDisplayValueAsText(*Item));
	}

	void OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type)
	{
		SelectedCompression = NewValue;
	}

	FText GetCompressionText() const
	{
		return SelectedCompression.IsValid() ? UEnum::GetDisplayValueAsText(*SelectedCompression) : FText::GetEmpty();
	}

	FText GetOutputName() const
	{
		return FText::FromString(OutputName);
	}

	void OnOutputNameChanged(const FText& NewText)
	{
		OutputName = NewText.ToString();
	}

	FText GetOutputPath() const
	{
		return FText::FromString(OutputPath);
	}

	void OnOutputPathChanged(const FText& NewText)
	{
		OutputPath = NewText.ToString();
	}

	FReply OnBrowseClicked()
	{
		IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
		if (DesktopPlatform)
		{
			FString FolderName;
			const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

			// Note: This opens a system file dialog. If the intention is to pick a Content Browser path,
			// we would typically use a different widget or dialog (SPathPicker).
			// Given "Browse button to pick a folder", system dialog is the literal interpretation.
			// However, for Unreal Assets, it might be better to convert the disk path to a package path later if needed.
			// I will implement the system dialog as requested.
			if (DesktopPlatform->OpenDirectoryDialog(
				ParentWindowWindowHandle,
				TEXT("Select Output Folder"),
				OutputPath,
				FolderName
			))
			{
				OutputPath = FolderName;
			}
		}
		return FReply::Handled();
	}

	FReply OnBakeClicked()
	{
		// Requirement: "Implement the UI... logic".
		// Does not explicitly require the bake logic itself, just the UI for it.
		// I will log the values to verify the UI works.
		UE_LOG(LogTemp, Log, TEXT("Bake Requested: Material=%s, Res=%d, BitDepth=%s, Comp=%d, Name=%s, Path=%s"),
			*GetMaterialPath(),
			*SelectedResolution,
			*SelectedBitDepth,
			(int)*SelectedCompression,
			*OutputName,
			*OutputPath
		);

		// TODO: Implement actual baking logic here.
		return FReply::Handled();
	}
};

// ------------------------------------------------------------------------------------------------
// FQuickBakerModule
// ------------------------------------------------------------------------------------------------

void FQuickBakerModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(QuickBakerTabName, FOnSpawnTab::CreateRaw(this, &FQuickBakerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FQuickBakerTabTitle", "QuickBaker"))
		.SetMenuType(ETabSpawnerMenuType::Always);
}

void FQuickBakerModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(QuickBakerTabName);
}

TSharedRef<SDockTab> FQuickBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SQuickBakerWidget)
		];
}

void FQuickBakerModule::RegisterMenus()
{
	// Not strictly required by the prompt "Implement the UI... specifically where the Tab is spawned",
	// but good practice. The prompt didn't ask for a toolbar button or menu entry specifically,
	// but standard plugins usually have one.
	// However, without a style set setup (QuickBakerStyle), adding a menu entry might be tricky regarding icons.
	// Since I didn't create QuickBakerStyle and QuickBakerCommands, I will skip registering menus to avoid compilation errors due to missing classes.
	// The Tab is registered as "Always" in the menu type, so it should appear in the Windows menu automatically in newer UE versions, or via "Window -> QuickBaker".
}

void FQuickBakerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(QuickBakerTabName);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
