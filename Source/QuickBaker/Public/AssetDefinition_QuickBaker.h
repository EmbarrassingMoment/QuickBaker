// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetDefinition.h"
#include "AssetDefinitionDefault.h"
#include "AssetDefinition_QuickBaker.generated.h"

/**
 * Asset definition for QuickBaker integration with Materials.
 */
UCLASS()
class QUICKBAKER_API UAssetDefinition_QuickBaker : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Implementation
	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	virtual TConstArrayView<FAssetActionContribution> GetAssetActions(const FAssetActionContributionContext& Context) const override;

private:
	void ExecuteQuickBaker(const FToolMenuContext& InContext);
};
