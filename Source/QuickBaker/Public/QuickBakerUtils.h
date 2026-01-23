// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"

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
};
