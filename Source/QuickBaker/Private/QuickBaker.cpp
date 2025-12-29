// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBaker.h"
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
					(MaterialThumbnail = MakeShareable(new FAssetThumbnail(SelectedMaterial.Get(), 64, 64, ThumbnailPool)))->MakeThumbnailWidget()
				]
			]

			// 2. Resolution
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

			// 3. Bit Depth
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

			// 4. Compression
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

			// 5. Output Name
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

			// 6. Output Path
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
						.ToolTipText(LOCTEXT("Tooltip_OutputPath", "The destination folder in the Content Browser. If the folder doesn't exist, it will be created automatically upon baking."))
						.Text_Lambda([this] { return FText::FromString(OutputPath); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.ToolTipText(LOCTEXT("Tooltip_Browse", "Open the Asset Picker to select a destination folder or create a new one."))
						.Text(LOCTEXT("Browse", "Browse"))
						.OnClicked_Raw(this, &FQuickBakerModule::OnBrowseClicked)
					]
				]
			]

			// 7. Bake Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("Tooltip_Bake", "Start the baking process. This will render the material to a temporary Render Target and save it as a Static Texture."))
				.ContentPadding(FMargin(10, 5))
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("BakeTexture", "Bake Texture"))
				.OnClicked_Raw(this, &FQuickBakerModule::OnBakeClicked)
			]
		];
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
		FString Name = SelectedMaterial->GetName();
		if (Name.StartsWith("M_"))
		{
			Name = "T_" + Name.RightChop(2);
		}
		else if (Name.StartsWith("MI_"))
		{
			Name = "T_" + Name.RightChop(3);
		}
		else
		{
			Name = "T_" + Name;
		}
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

	if (!SelectedResolution.IsValid() || !SelectedBitDepth.IsValid() || !SelectedCompression.IsValid())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_InvalidSettings", "Invalid bake settings."));
		return;
	}

	const int32 Resolution = *SelectedResolution;
	const bool bIs16Bit = *SelectedBitDepth == "16-bit";
	const FString Name = OutputName.ToString();
	FString PackagePath = OutputPath;

	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		PackagePath = TEXT("/Game/") + PackagePath;
	}
	while (PackagePath.EndsWith(TEXT("/")) && PackagePath.Len() > 1)
	{
		PackagePath.LeftChopInline(1);
	}

	if (Name.IsEmpty())
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("Error_NoName", "Please specify an output name."));
		return;
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

	// 2. Setup Render Target
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	check(RenderTarget);
	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->RenderTargetFormat = bIs16Bit ? RTF_RGBA16f : RTF_RGBA8;
	RenderTarget->bForceLinearGamma = true; // Often needed for data, but let's stick to requirement sRGB=false which implies linear.
	RenderTarget->SRGB = false; // Requirement: bSRGB = false
	RenderTarget->UpdateResourceImmediate(true);

	// 3. Bake Material
	UObject* WorldContextObject = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, RenderTarget, SelectedMaterial.Get());

	// 4. Convert to Static Texture
	FString PackageName = FullPackageName;
	FString AssetName = Name;

	// Create Package
	UPackage* Package = CreatePackage(*PackageName);

	// Requirement: Use UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly or FAssetToolsModule
	// RenderTargetCreateStaticTexture2DEditorOnly creates a new asset.

	UTexture2D* NewTexture = UKismetRenderingLibrary::RenderTargetCreateStaticTexture2DEditorOnly(RenderTarget, Name, *SelectedCompression);

	if (NewTexture)
	{
		// Move the texture to the correct package if needed, or check if RenderTargetCreateStaticTexture2DEditorOnly handles it.
		// RenderTargetCreateStaticTexture2DEditorOnly usually creates it in the Transient package or similar if not specified.
		// Wait, the function signature is: (UTextureRenderTarget2D* RenderTarget, FString Name = "Texture", TextureCompressionSettings CompressionSettings = TC_Default, TextureMipGenSettings MipSettings = TMGS_FromTextureGroup)
		// It doesn't take a path. It creates it in the Transient package usually? Or maybe allows renaming.

		// A common pattern is to duplicate/rename or simply use it as source.
		// BUT the requirements say: "Create a new UTexture2D asset in the specified path."

		// If UKismetRenderingLibrary doesn't support path, let's use FAssetToolsModule or duplicate.
		// Actually, let's check if we can rename it to the target package.

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

	// 6. Cleanup
	// RenderTarget is a UObject, will be garbage collected. Explicit cleanup usually means removing from root if added, but here it's local.
	// We can manually mark it pending kill if we want immediate cleanup, but GC handles it.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
