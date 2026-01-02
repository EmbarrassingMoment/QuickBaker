// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_QuickBaker.h"
#include "Materials/MaterialInterface.h"
#include "QuickBaker.h"
#include "ContentBrowserMenuContexts.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_QuickBaker"

FText UAssetDefinition_QuickBaker::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDisplayName", "Material Interface");
}

FLinearColor UAssetDefinition_QuickBaker::GetAssetColor() const
{
	return FLinearColor(0.0f, 0.8f, 0.0f);
}

TSoftClassPtr<UObject> UAssetDefinition_QuickBaker::GetAssetClass() const
{
	return UMaterialInterface::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_QuickBaker::GetAssetCategories() const
{
	static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Material) };
	return Categories;
}

TConstArrayView<FAssetActionContribution> UAssetDefinition_QuickBaker::GetAssetActions(const FAssetActionContributionContext& Context) const
{
	static const FAssetActionContribution Actions[] =
	{
		FAssetActionContribution
		{
			"QuickBaker",
			LOCTEXT("QuickBakerAction", "Quick Baker"),
			FAssetActionContribution::FExecuteAction::CreateUObject(this, &UAssetDefinition_QuickBaker::ExecuteQuickBaker)
		}
	};
	return Actions;
}

void UAssetDefinition_QuickBaker::ExecuteQuickBaker(const FToolMenuContext& InContext)
{
	if (const UContentBrowserAssetContextMenuContext* Context = InContext.FindContext<UContentBrowserAssetContextMenuContext>())
	{
		if (Context->SelectedAssets.Num() > 0)
		{
			if (UMaterialInterface* Material = Cast<UMaterialInterface>(Context->SelectedAssets[0].GetAsset()))
			{
				FQuickBakerModule& QuickBakerModule = FModuleManager::LoadModuleChecked<FQuickBakerModule>("QuickBaker");
				QuickBakerModule.OpenQuickBakerWithMaterial(Material);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
