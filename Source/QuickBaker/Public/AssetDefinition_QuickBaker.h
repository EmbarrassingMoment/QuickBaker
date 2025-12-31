// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinitionDefault.h"
#include "Materials/MaterialInterface.h"
#include "AssetDefinition_QuickBaker.generated.h"

UCLASS()
class UAssetDefinition_QuickBaker : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition overrides
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual EAssetCommandResult PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const override;

	// This is the key function for context menus in UE 5.x
	virtual TConstArrayView<FAssetActionContribution> GetAssetActions(const FAssetActionContributionArgs& Args) const override;
};
