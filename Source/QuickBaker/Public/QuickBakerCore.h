// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

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
	 * For Asset output, the operation is synchronous.
	 * For PNG/EXR output, compression and disk I/O run on a background thread.
	 *
	 * @param Settings The configuration for the bake operation, including material, resolution, and output path.
	 * @param OnComplete Optional callback invoked on the game thread when the bake operation is fully complete.
	 */
	static void ExecuteBake(const FQuickBakerSettings& Settings, TFunction<void()> OnComplete = nullptr);

private:
	/**
	 * Internal helper to bake the RenderTarget to a static Texture Asset.
	 *
	 * @param RenderTarget The temporary render target containing the baked result.
	 * @param Settings The bake settings containing output path and compression options.
	 * @param OutResultMessage The result message to display to the user after the progress bar completes.
	 * @return True if the asset was saved successfully, false otherwise.
	 */
	static bool BakeToAsset(UTextureRenderTarget2D* RenderTarget, const FQuickBakerSettings& Settings, FText& OutResultMessage);
};
