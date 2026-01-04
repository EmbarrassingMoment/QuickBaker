// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"

/**
 * Enum defining the output file type.
 */
enum class EQuickBakerOutputType : uint8
{
	Asset, /**< Create a Texture Asset in the Content Browser */
	PNG,   /**< Export as a PNG file (8-bit) */
	EXR    /**< Export as an EXR file (16-bit float) */
};

/**
 * Enum defining the bit depth for the output texture.
 */
enum class EQuickBakerBitDepth : uint8
{
	Bit8,  /**< 8-bit per channel */
	Bit16  /**< 16-bit float per channel (Recommended for Noise/SDF) */
};

/**
 * Structure to hold all configuration settings for the baking process.
 */
struct QUICKBAKER_API FQuickBakerSettings
{
	/** The material to be baked. */
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;

	/** The desired output format (Asset, PNG, EXR). */
	EQuickBakerOutputType OutputType = EQuickBakerOutputType::Asset;

	/** The resolution of the output texture (width and height). */
	int32 Resolution = 1024;

	/** The bit depth of the output texture. */
	EQuickBakerBitDepth BitDepth = EQuickBakerBitDepth::Bit16;

	/** Compression settings for the texture asset. */
	TextureCompressionSettings Compression = TC_Default;

	/** The name of the output file or asset. */
	FString OutputName;

	/** The path where the output will be saved. */
	FString OutputPath;

	/** Default constructor. */
	FQuickBakerSettings() {}

	/**
	 * Validates the settings.
	 * @return True if the settings are valid, false otherwise.
	 */
	bool IsValid() const
	{
		if (!SelectedMaterial.IsValid())
		{
			return false;
		}

		if (Resolution <= 0)
		{
			return false;
		}

		if (OutputName.IsEmpty())
		{
			return false;
		}

		return true;
	}
};
