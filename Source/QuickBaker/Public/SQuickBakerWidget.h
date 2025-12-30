// ========================================
// ファイル: Source/QuickBaker/Public/SQuickBakerWidget.h
// ========================================

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "QuickBakerSettings.h"

class FAssetThumbnailPool;
class FAssetThumbnail;
class UMaterialInterface;
struct FAssetData;

class QUICKBAKER_API SQuickBakerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuickBakerWidget) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FAssetThumbnailPool> InThumbnailPool);

private:
	// UI State
	TSharedPtr<EQuickBakerOutputType> SelectedOutputType;
	TArray<TSharedPtr<EQuickBakerOutputType>> OutputTypeOptions;

	TSharedPtr<int32> SelectedResolution;
	TArray<TSharedPtr<int32>> ResolutionOptions;

	TSharedPtr<FString> SelectedBitDepth;
	TArray<TSharedPtr<FString>> BitDepthOptions;

	TSharedPtr<TextureCompressionSettings> SelectedCompression;
	TArray<TSharedPtr<TextureCompressionSettings>> CompressionOptions;

	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;
	FText OutputName;
	FString OutputPath;

	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<FAssetThumbnail> MaterialThumbnail;

	// Helper to initialize options
	void InitializeOptions();

	// UI Callbacks
	void OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateOutputTypeWidget(TSharedPtr<EQuickBakerOutputType> InOption);
	FText GetSelectedOutputTypeText() const;

	void OnMaterialChanged(const FAssetData& AssetData);
	FString GetSelectedMaterialPath() const;

	void OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateResolutionWidget(TSharedPtr<int32> InOption);
	FText GetSelectedResolutionText() const;

	void OnBitDepthChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateBitDepthWidget(TSharedPtr<FString> InOption);
	FText GetSelectedBitDepthText() const;

	void OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption);
	FText GetSelectedCompressionText() const;

	void OnOutputNameChanged(const FText& NewText);

	FReply OnBakeClicked();
	FReply OnBrowseClicked();
};
