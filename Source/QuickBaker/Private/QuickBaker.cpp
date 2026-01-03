// Copyright Kurorekishi, All Rights Reserved.

#include "QuickBaker.h"
#include "QuickBakerExporter.h" // For LogQuickBaker
#include "SQuickBakerWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"

static const FName QuickBakerTabName("QuickBaker");

#define LOCTEXT_NAMESPACE "FQuickBakerModule"

void FQuickBakerModule::StartupModule()
{
	// Register the tab spawner for the editor window
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(QuickBakerTabName, FOnSpawnTab::CreateRaw(this, &FQuickBakerModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("QuickBakerTabTitle", "QuickBaker"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Register startup callbacks for menu extension
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FQuickBakerModule::RegisterMenus));
}

void FQuickBakerModule::ShutdownModule()
{
	// Clean up menu callbacks
	UToolMenus::UnRegisterStartupCallback(this);

	// Unregister menu extensions
	UToolMenus::UnregisterOwner(this);

	// Unregister the tab spawner
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(QuickBakerTabName);
}

void FQuickBakerModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(QuickBakerTabName);
}

void FQuickBakerModule::OpenQuickBakerWithMaterial(UMaterialInterface* Material)
{
	if (Material)
	{
		UE_LOG(LogQuickBaker, Log, TEXT("Opening QuickBaker with material: %s"), *Material->GetName());
	}

	ContextMenuMaterial = Material;
	FGlobalTabmanager::Get()->TryInvokeTab(QuickBakerTabName);

	// If the widget is already alive (tab was already open), update it immediately
	if (TSharedPtr<SQuickBakerWidget> Widget = QuickBakerWidget.Pin())
	{
		Widget->SetInitialMaterial(Material);
		ContextMenuMaterial.Reset();
	}
}

void FQuickBakerModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// 1. Extend Tools Menu
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Tools");
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

	// 2. Extend Material Context Menu
	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.MaterialInterface");
		FToolMenuSection& Section = Menu->FindOrAddSection("QuickBaker");
		Section.Label = LOCTEXT("QuickBakerContextLabel", "QuickBaker");

		FToolMenuEntry Entry = FToolMenuEntry::InitMenuEntry(
			"QuickBakerContext",
			LOCTEXT("QuickBakerContextCommand", "Quick Baker"),
			LOCTEXT("QuickBakerContextTooltip", "Open Quick Baker with this material"),
			FSlateIcon(),
			FToolMenuExecuteAction::CreateRaw(this, &FQuickBakerModule::OnContextMenuExecute)
		);

		Section.AddEntry(Entry);
	}
}

void FQuickBakerModule::OnContextMenuExecute(const FToolMenuContext& Context)
{
	if (const UContentBrowserAssetContextMenuContext* AssetContext = Context.FindContext<UContentBrowserAssetContextMenuContext>())
	{
		TArray<FAssetData> SelectedAssets = AssetContext->GetSelectedAssetsOfType(UMaterialInterface::StaticClass());
		if (SelectedAssets.Num() > 0)
		{
			// Just take the first valid material
			if (UMaterialInterface* Mat = Cast<UMaterialInterface>(SelectedAssets[0].GetAsset()))
			{
				UE_LOG(LogQuickBaker, Log, TEXT("Context menu executed on material: %s"), *Mat->GetName());
				OpenQuickBakerWithMaterial(Mat);
			}
		}
	}
}

TSharedRef<SDockTab> FQuickBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	TSharedRef<SQuickBakerWidget> NewWidget = SNew(SQuickBakerWidget);
	QuickBakerWidget = NewWidget;

	if (ContextMenuMaterial.IsValid())
	{
		NewWidget->SetInitialMaterial(ContextMenuMaterial.Get());
		ContextMenuMaterial.Reset();
	}

	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			NewWidget
		];
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
