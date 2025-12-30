// ========================================
// ファイル: Source/QuickBaker/Public/QuickBakerSettings.h
// ========================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture.h"

enum class EQuickBakerOutputType
{
	Asset,
	PNG,
	EXR
};

class UMaterialInterface;

struct QUICKBAKER_API FQuickBakerSettings
{
	EQuickBakerOutputType OutputType;
	int32 Resolution;
	bool bUse16Bit;
	TextureCompressionSettings Compression;
	TWeakObjectPtr<UMaterialInterface> Material;
	FString OutputName;
	FString OutputPath;

	FQuickBakerSettings();

	bool IsValid() const;
};
