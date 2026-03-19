// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"

class UTextureRenderTarget2D;

/**
 * Utility class for QuickBaker.
 * Contains static helper functions.
 */
class FQuickBakerUtils
{
public:
	/**
	 * Generates a default texture name based on the material name.
	 * Typically appends or replaces prefixes/suffixes (e.g., M_Name -> T_Name_Baked).
	 *
	 * @param MaterialName The name of the source material.
	 * @return The generated texture name.
	 */
	static FString GetTextureNameFromMaterial(const FString& MaterialName);

	/**
	 * Reads pixel data from a render target using FRHIGPUTextureReadback for efficient GPU-to-CPU transfer.
	 * Handles row pitch alignment differences between GPU and CPU memory layouts.
	 *
	 * @param RenderTarget The source render target to read pixels from.
	 * @param OutData Output buffer that will contain the raw pixel data as FFloat16Color (8 bytes/pixel).
	 * @return True if the readback was successful, false otherwise.
	 */
	static bool ReadbackFloat16Pixels(UTextureRenderTarget2D* RenderTarget, TArray<FFloat16Color>& OutData);

	/**
	 * Reads pixel data from a render target using FRHIGPUTextureReadback for efficient GPU-to-CPU transfer.
	 * Handles row pitch alignment differences between GPU and CPU memory layouts.
	 *
	 * @param RenderTarget The source render target to read pixels from.
	 * @param OutData Output buffer that will contain the raw pixel data as FColor (4 bytes/pixel).
	 * @return True if the readback was successful, false otherwise.
	 */
	static bool ReadbackColorPixels(UTextureRenderTarget2D* RenderTarget, TArray<FColor>& OutData);
};
