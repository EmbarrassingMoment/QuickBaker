// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_QuickBaker.h"
#include "QuickBaker.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_QuickBaker"

FText UAssetDefinition_QuickBaker::GetAssetDisplayName() const
{
	return LOCTEXT("AssetTypeActions_QuickBaker", "Material");
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

EAssetCommandResult UAssetDefinition_QuickBaker::PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const
{
	// Default behavior
	return UAssetDefinitionDefault::PerformAssetDiff(DiffArgs);
}

TConstArrayView<FAssetActionContribution> UAssetDefinition_QuickBaker::GetAssetActions(const FAssetActionContributionArgs& Args) const
{
	static const FAssetActionContribution Actions[] = {
		FAssetActionContribution
		{
			"QuickBaker",
			LOCTEXT("QuickBakerAction", "Quick Baker"),
			FAssetActionContribution::FExecuteAction::CreateLambda([](const FAssetActionContributionArgs& Args)
			{
				FQuickBakerModule& QuickBakerModule = FModuleManager::LoadModuleChecked<FQuickBakerModule>("QuickBaker");

				// Get the first selected material
				for (const FAssetData& AssetData : Args.Assets)
				{
					UMaterialInterface* Material = Cast<UMaterialInterface>(AssetData.GetAsset());
					if (Material)
					{
						QuickBakerModule.OpenQuickBakerWithMaterial(Material);
						// Only open for the first valid one
						return;
					}
				}
			})
		}
	};

	return Actions;
}

#undef LOCTEXT_NAMESPACE
