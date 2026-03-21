// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "QuickBakerSettings.h"
#include "QuickBakerAsync.h"
#include "Engine/Texture.h" // For TextureCompressionSettings

class FAssetThumbnailPool;
class FAssetThumbnail;
class SProgressBar;

/**
 * Main Slate Widget for the QuickBaker tool.
 * Provides the UI for selecting materials, configuring bake settings, and executing the bake operation.
 */
class QUICKBAKER_API SQuickBakerWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SQuickBakerWidget) {}
	SLATE_END_ARGS()

	/**
	 * Constructs the widget.
	 *
	 * @param InArgs The declaration data for this widget.
	 */
	void Construct(const FArguments& InArgs);

	/**
	 * Destructor.
	 * Cleans up the thumbnail pool and other resources.
	 */
	virtual ~SQuickBakerWidget();

	/**
	 * Public API to set the material programmatically (e.g. from Context Menu).
	 * Updates the UI to reflect the selected material.
	 *
	 * @param Material The material to select.
	 */
	void SetInitialMaterial(UMaterialInterface* Material);

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

	// Reference to the Compression Row for dynamic visibility control
	TSharedPtr<SHorizontalBox> CompressionRow;

	// Async bake state
	TSharedPtr<FQuickBakerAsyncTask> ActiveBakeTask;
	TSharedPtr<SProgressBar> BakeProgressBar;
	TSharedPtr<STextBlock> BakeStatusText;
	TSharedPtr<SVerticalBox> ProgressSection;

	/**
	 * Initializes the dropdown options for Resolution, Output Type, Bit Depth, etc.
	 */
	void InitializeOptions();

	/**
	 * Load saved settings from EditorPerProjectUserSettings.ini.
	 * Updates the local Settings struct and UI selection.
	 */
	void LoadSavedSettings();

	// UI Callbacks

	/**
	 * Callback when the Output Type selection changes.
	 *
	 * @param NewValue The new output type selected.
	 * @param SelectInfo The method by which the selection was made.
	 */
	void OnOutputTypeChanged(TSharedPtr<EQuickBakerOutputType> NewValue, ESelectInfo::Type SelectInfo);

	/**
	 * Generates the widget for an Output Type option.
	 *
	 * @param InOption The output type option.
	 * @return The widget to display in the dropdown.
	 */
	TSharedRef<SWidget> GenerateOutputTypeWidget(TSharedPtr<EQuickBakerOutputType> InOption);

	/**
	 * Gets the text label for the currently selected Output Type.
	 *
	 * @return The text label.
	 */
	FText GetSelectedOutputTypeText() const;

	/**
	 * Callback when the selected material changes in the asset picker.
	 *
	 * @param AssetData The data of the newly selected asset.
	 */
	void OnMaterialChanged(const struct FAssetData& AssetData);

	/**
	 * Gets the path of the currently selected material as a string.
	 *
	 * @return The material path.
	 */
	FString GetSelectedMaterialPath() const;

	/**
	 * Callback when the Resolution selection changes.
	 *
	 * @param NewValue The new resolution selected.
	 * @param SelectInfo The method by which the selection was made.
	 */
	void OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo);

	/**
	 * Generates the widget for a Resolution option.
	 *
	 * @param InOption The resolution option.
	 * @return The widget to display in the dropdown.
	 */
	TSharedRef<SWidget> GenerateResolutionWidget(TSharedPtr<int32> InOption);

	/**
	 * Gets the text label for the currently selected Resolution.
	 *
	 * @return The text label.
	 */
	FText GetSelectedResolutionText() const;

	/**
	 * Callback when the Bit Depth selection changes.
	 *
	 * @param NewValue The new bit depth selected.
	 * @param SelectInfo The method by which the selection was made.
	 */
	void OnBitDepthChanged(TSharedPtr<EQuickBakerBitDepth> NewValue, ESelectInfo::Type SelectInfo);

	/**
	 * Generates the widget for a Bit Depth option.
	 *
	 * @param InOption The bit depth option.
	 * @return The widget to display in the dropdown.
	 */
	TSharedRef<SWidget> GenerateBitDepthWidget(TSharedPtr<EQuickBakerBitDepth> InOption);

	/**
	 * Gets the text label for the currently selected Bit Depth.
	 *
	 * @return The text label.
	 */
	FText GetSelectedBitDepthText() const;

	/**
	 * Callback when the Compression selection changes.
	 *
	 * @param NewValue The new compression setting selected.
	 * @param SelectInfo The method by which the selection was made.
	 */
	void OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo);

	/**
	 * Generates the widget for a Compression option.
	 *
	 * @param InOption The compression option.
	 * @return The widget to display in the dropdown.
	 */
	TSharedRef<SWidget> GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption);

	/**
	 * Gets the text label for the currently selected Compression setting.
	 *
	 * @return The text label.
	 */
	FText GetSelectedCompressionText() const;

	/**
	 * Callback when the Output Name text changes.
	 *
	 * @param NewText The new text.
	 */
	void OnOutputNameChanged(const FText& NewText);

	/**
	 * Callback when the "Bake" button is clicked.
	 * Triggers the baking process.
	 *
	 * @return Whether the event was handled.
	 */
	FReply OnBakeClicked();

	/**
	 * Callback when the "Browse" button is clicked.
	 * Opens a dialog to select the output path.
	 *
	 * @return Whether the event was handled.
	 */
	FReply OnBrowseClicked();

	/** Called when the async bake task completes. */
	void OnBakeComplete(bool bSuccess, const FText& ResultMessage);

	/** @return True if a bake operation is currently running. */
	bool IsBaking() const { return ActiveBakeTask.IsValid() && ActiveBakeTask->IsRunning(); }
};
