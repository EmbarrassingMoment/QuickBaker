#pragma once

#include "CoreMinimal.h"
#include "QuickBakerSettings.h"

class QUICKBAKER_API FQuickBakerCore
{
public:
	/**
	 * Executes the bake process based on the provided settings.
	 * @param Settings The bake configuration.
	 */
	static void ExecuteBake(const FQuickBakerSettings& Settings);

private:
	static void BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings);
};
