#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"

// Output Type Enum
enum class EQuickBakerOutputType : uint8
{
	Asset,
	PNG,
	EXR
};

enum class EQuickBakerBitDepth : uint8
{
	Bit8,
	Bit16
};

// Data Structure for Bake Settings
struct QUICKBAKER_API FQuickBakerSettings
{
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;
	EQuickBakerOutputType OutputType = EQuickBakerOutputType::Asset;
	int32 Resolution = 1024;
	EQuickBakerBitDepth BitDepth = EQuickBakerBitDepth::Bit16;
	TextureCompressionSettings Compression = TC_Default;
	FString OutputName;
	FString OutputPath;

	// Constructor
	FQuickBakerSettings() {}

	// Validation
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
