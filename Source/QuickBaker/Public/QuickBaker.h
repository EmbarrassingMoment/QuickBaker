// ========================================
// ファイル: Source/QuickBaker/Public/QuickBaker.h
// ========================================

#pragma once

#include "Modules/ModuleManager.h"

class FAssetThumbnailPool;
class SDockTab;
class FSpawnTabArgs;

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

	TSharedRef<SDockTab> OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs);

private:
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
};
