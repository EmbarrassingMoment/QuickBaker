#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "QuickBakerSettings.h"
#include "Engine/Texture.h" // For TextureCompressionSettings

class FAssetThumbnailPool;
class FAssetThumbnail;

class QUICKBAKER_API SQuickBakerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuickBakerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SQuickBakerWidget();

private:
	// Settings Data
	FQuickBakerSettings Settings;

	// Options for ComboBoxes
	TArray<TSharedPtr<EQuickBakerOutputType>> OutputTypeOptions;
	TArray<TSharedPtr<int32>> ResolutionOptions;
	TArray<TSharedPtr<EQuickBakerBitDepth>> BitDepthOptions;
	TArray<TSharedPtr<TextureCompressionSettings>> CompressionOptions;

	// Current Selections (matching Settings logic but as pointers for ComboBox)
	TSharedPtr<EQuickBakerOutputType> SelectedOutputType;
	TSharedPtr<int32> SelectedResolution;
	TSharedPtr<EQuickBakerBitDepth> SelectedBitDepth;
	TSharedPtr<TextureCompressionSettings> SelectedCompression;

	// Thumbnail Helpers
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> MaterialThumbnail;

	// Warning UI
	bool bShowMaterialWarning;
	TSharedPtr<STextBlock> WarningTextBlock;

	// Initialization
	void InitializeOptions();

	// UI Callbacks
	void OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateOutputTypeWidget(TSharedPtr<EQuickBakerOutputType> InOption);
	FText GetSelectedOutputTypeText() const;

	void OnMaterialChanged(const struct FAssetData& AssetData);
	FString GetSelectedMaterialPath() const;

	void OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateResolutionWidget(TSharedPtr<int32> InOption);
	FText GetSelectedResolutionText() const;

	void OnBitDepthChanged(TSharedPtr<EQuickBakerBitDepth> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateBitDepthWidget(TSharedPtr<EQuickBakerBitDepth> InOption);
	FText GetSelectedBitDepthText() const;

	void OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption);
	FText GetSelectedCompressionText() const;

	void OnOutputNameChanged(const FText& NewText);

	FReply OnBakeClicked();
	FReply OnBrowseClicked();

public:
	void SetInitialMaterial(class UMaterialInterface* Material);
};
