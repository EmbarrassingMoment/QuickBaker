// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBaker.h"
#include "SQuickBakerWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "QuickBakerExporter.h" // For LogQuickBaker
#include "Materials/MaterialInterface.h"

static const FName QuickBakerTabName("QuickBaker");

#define LOCTEXT_NAMESPACE "FQuickBakerModule"

void FQuickBakerModule::StartupModule()
{
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

	// Register Material Context Menu
	UToolMenu* AssetMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.MaterialInterface");
	{
		FToolMenuSection& Section = AssetMenu->FindOrAddSection("QuickBakerActions");
		Section.Label = LOCTEXT("QuickBakerActionsLabel", "Quick Baker Actions");

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"QuickBakerContext",
			LOCTEXT("QuickBakerContext", "Quick Baker"),
			LOCTEXT("QuickBakerContextTooltip", "Open Quick Baker with this material selected"),
			FSlateIcon(),
			FToolMenuExecuteAction::CreateLambda([this](const FToolMenuContext& MenuContext)
			{
				if (const UContentBrowserAssetContextMenuContext* Context = MenuContext.FindContext<UContentBrowserAssetContextMenuContext>())
				{
					TArray<UMaterialInterface*> SelectedMaterials = Context->LoadSelectedObjects<UMaterialInterface>();
					if (SelectedMaterials.Num() > 0)
					{
						OpenQuickBakerWithMaterial(SelectedMaterials[0]);
					}
				}
			})
		);
		Section.AddEntry(Entry);
	}
}

void FQuickBakerModule::OpenQuickBakerWithMaterial(UMaterialInterface* Material)
{
	if (Material)
	{
		UE_LOG(LogQuickBaker, Log, TEXT("Opening QuickBaker with material: %s"), *Material->GetName());

		// Store the material to be used in OnSpawnPluginTab if the tab is not yet created
		TargetMaterialToLoad = Material;

		// Invoke the tab (this will call OnSpawnPluginTab if not open, or bring it to front if it is)
		FGlobalTabmanager::Get()->TryInvokeTab(QuickBakerTabName);

		// If the widget is already alive (tab was already open) and OnSpawnPluginTab didn't consume the target, update it directly
		if (PluginWidget.IsValid() && TargetMaterialToLoad.IsValid())
		{
			PluginWidget.Pin()->SetInitialMaterial(Material);
			TargetMaterialToLoad.Reset();
		}
	}
}

TSharedRef<SDockTab> FQuickBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SQuickBakerWidget> NewWidget = SNew(SQuickBakerWidget);
	PluginWidget = NewWidget;

	// If we have a pending material (from OpenQuickBakerWithMaterial), set it now
	if (TargetMaterialToLoad.IsValid())
	{
		NewWidget->SetInitialMaterial(TargetMaterialToLoad.Get());
		TargetMaterialToLoad.Reset();
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			NewWidget
		];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
