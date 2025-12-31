// Copyright Epic Games, Inc. All Rights Reserved.

#include "QuickBaker.h"
#include "SQuickBakerWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"

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
}

TSharedRef<SDockTab> FQuickBakerModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SQuickBakerWidget)
		];
}

#undef LOCTEXT_NAMESPACE
	
#ifndef UE_MODULE_NAME
	#define UE_MODULE_NAME "QuickBaker"
#endif

IMPLEMENT_MODULE(FQuickBakerModule, QuickBaker)
