// Copyright (c) 2025 Kurorekishi (EmbarrassingMoment). Licensed under MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FQuickBakerModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command (by default it will bring up plugin window) */
	void PluginButtonClicked();

	/** Opens QuickBaker tab and selects the specified material */
	void OpenQuickBakerWithMaterial(class UMaterialInterface* Material);

private:

	void RegisterMenus();
	void OnContextMenuExecute(const struct FToolMenuContext& Context);

	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Holds the material selected from the context menu, to be passed to the widget on spawn */
	TWeakObjectPtr<class UMaterialInterface> ContextMenuMaterial;

	/** Weak pointer to the active widget */
	TWeakPtr<class SQuickBakerWidget> QuickBakerWidget;
};
