// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * The main module class for QuickBaker.
 * Handles module startup, shutdown, and UI command registration.
 */
class FQuickBakerModule : public IModuleInterface
{
public:

	/**
	 * Called when the module is started up.
	 * Registers styles, commands, and menus.
	 */
	virtual void StartupModule() override;

	/**
	 * Called when the module is shut down.
	 * Unregisters styles and commands.
	 */
	virtual void ShutdownModule() override;

	/**
	 * This function will be bound to Command (by default it will bring up plugin window).
	 * Toggles the visibility of the QuickBaker tab.
	 */
	void PluginButtonClicked();

	/**
	 * Opens the QuickBaker tab and selects the specified material.
	 * If the tab is already open, it brings it to front and updates the selection.
	 *
	 * @param Material The material to pre-select in the tool.
	 */
	void OpenQuickBakerWithMaterial(class UMaterialInterface* Material);

private:

	/**
	 * Registers the plugin's menu extensions.
	 */
	void RegisterMenus();

	/**
	 * Callback for the context menu action on a Material asset.
	 *
	 * @param Context The tool menu context containing the selected assets.
	 */
	void OnContextMenuExecute(const struct FToolMenuContext& Context);

	/**
	 * Callback to spawn the plugin tab.
	 *
	 * @param SpawnTabArgs Arguments for spawning the tab.
	 * @return The created dock tab containing the SQuickBakerWidget.
	 */
	TSharedRef<class SDockTab> OnSpawnPluginTab(const class FSpawnTabArgs& SpawnTabArgs);

	/** Holds the material selected from the context menu, to be passed to the widget on spawn */
	TWeakObjectPtr<class UMaterialInterface> ContextMenuMaterial;

	/** Weak pointer to the active widget */
	TWeakPtr<class SQuickBakerWidget> QuickBakerWidget;
};
