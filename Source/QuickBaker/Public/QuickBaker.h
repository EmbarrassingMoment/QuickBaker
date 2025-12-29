// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Engine/Texture.h"

class FAssetThumbnailPool;
class FAssetThumbnail;

enum class EQuickBakerOutputType
{
	Asset,
	PNG,
	EXR
};

class FQuickBakerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();

private:

	void RegisterMenus();

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

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

	TWeakObjectPtr<class UMaterialInterface> SelectedMaterial;
	FText OutputName;
	FString OutputPath;

	TSharedPtr<class FAssetThumbnailPool> ThumbnailPool;
	TSharedPtr<class FAssetThumbnail> MaterialThumbnail;

	// Helper to initialize options
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

	void OnBitDepthChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateBitDepthWidget(TSharedPtr<FString> InOption);
	FText GetSelectedBitDepthText() const;

	void OnCompressionChanged(TSharedPtr<TextureCompressionSettings> NewValue, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateCompressionWidget(TSharedPtr<TextureCompressionSettings> InOption);
	FText GetSelectedCompressionText() const;

	void OnOutputNameChanged(const FText& NewText);

	FReply OnBakeClicked();
	FReply OnBrowseClicked();

	// Core baking logic
	void ExecuteBake();
};
