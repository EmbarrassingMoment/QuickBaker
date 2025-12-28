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
#include "EditorStyleSet.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/AssetData.h"

static const FName QuickBakerTabName("QuickBaker");

#define LOCTEXT_NAMESPACE "FQuickBakerModule"

void FQuickBakerModule::StartupModule()
{
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
				.VAlign(VAlign_Center)
				.Padding(0, 0, 10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("Label_Material", "Material"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UMaterialInterface::StaticClass())
					.ObjectPath(this, &FQuickBakerModule::GetSelectedMaterialPath)
					.OnObjectChanged(this, &FQuickBakerModule::OnMaterialChanged)
					.AllowClear(true)
					.DisplayUseSelected(true)
					.DisplayBrowse(true)
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
					.OptionsSource(&ResolutionOptions)
					.InitiallySelectedItem(SelectedResolution)
					.OnGenerateWidget(this, &FQuickBakerModule::GenerateResolutionWidget)
					.OnSelectionChanged(this, &FQuickBakerModule::OnResolutionChanged)
					[
						SNew(STextBlock).Text(this, &FQuickBakerModule::GetSelectedResolutionText)
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
					.OptionsSource(&BitDepthOptions)
					.InitiallySelectedItem(SelectedBitDepth)
					.OnGenerateWidget(this, &FQuickBakerModule::GenerateBitDepthWidget)
					.OnSelectionChanged(this, &FQuickBakerModule::OnBitDepthChanged)
					[
						SNew(STextBlock).Text(this, &FQuickBakerModule::GetSelectedBitDepthText)
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
					.OptionsSource(&CompressionOptions)
					.InitiallySelectedItem(SelectedCompression)
					.OnGenerateWidget(this, &FQuickBakerModule::GenerateCompressionWidget)
					.OnSelectionChanged(this, &FQuickBakerModule::OnCompressionChanged)
					[
						SNew(STextBlock).Text(this, &FQuickBakerModule::GetSelectedCompressionText)
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
					.Text_Lambda([this] { return OutputName; })
					.OnTextChanged(this, &FQuickBakerModule::OnOutputNameChanged)
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
						SNew(STextBlock).Text_Lambda([this] { return FText::FromString(OutputPath); })
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(5, 0, 0, 0)
					[
						SNew(SButton)
						.Text(LOCTEXT("Browse", "Browse"))
						.OnClicked_Lambda([]() {
							// Placeholder for browse functionality
							return FReply::Handled();
						})
					]
				]
			]

			// 7. Bake Button
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(5)
			[
				SNew(SButton)
				.ContentPadding(FMargin(10, 5))
				.HAlign(HAlign_Center)
				.Text(LOCTEXT("BakeTexture", "Bake Texture"))
				.OnClicked(this, &FQuickBakerModule::OnBakeClicked)
			]
		];
}

void FQuickBakerModule::OnMaterialChanged(const FAssetData& AssetData)
{
	SelectedMaterial = Cast<UMaterialInterface>(AssetData.GetAsset());
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
	// Implementation of bake functionality would go here
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
