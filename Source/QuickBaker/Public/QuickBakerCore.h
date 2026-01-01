#pragma once

#include "CoreMinimal.h"
#include "QuickBakerSettings.h"

/**
 * Core logic class for QuickBaker.
 * Handles the rendering of materials to render targets and dispatching the save operation.
 */
class QUICKBAKER_API FQuickBakerCore
{
public:
	/**
	 * Executes the bake process based on the provided settings.
	 *
	 * @param Settings The configuration for the bake operation, including material, resolution, and output path.
	 */
	static void ExecuteBake(const FQuickBakerSettings& Settings);

private:
	/**
	 * Internal helper to bake the RenderTarget to a static Texture Asset.
	 *
	 * @param RenderTarget The temporary render target containing the baked result.
	 * @param Settings The bake settings containing output path and compression options.
	 */
	static void BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings);
};
