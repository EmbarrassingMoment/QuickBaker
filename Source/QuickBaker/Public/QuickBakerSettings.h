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

// Data Structure for Bake Settings
struct QUICKBAKER_API FQuickBakerSettings
{
	TWeakObjectPtr<UMaterialInterface> SelectedMaterial;
	EQuickBakerOutputType OutputType = EQuickBakerOutputType::Asset;
	int32 Resolution = 1024;
	FString BitDepth = TEXT("8-bit"); // Using String to match existing UI logic logic, or we could enum it. Existing logic uses FString check "8-bit"/"16-bit".
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
