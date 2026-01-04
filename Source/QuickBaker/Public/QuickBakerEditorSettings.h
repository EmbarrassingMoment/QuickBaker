// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "QuickBakerSettings.h" // For EQuickBakerOutputType, EQuickBakerBitDepth
#include "Engine/Texture.h"     // For TextureCompressionSettings
#include "QuickBakerEditorSettings.generated.h"

/**
 * Editor settings for QuickBaker that persist across sessions.
 * Stored in: Saved/Config/<Platform>/EditorPerProjectUserSettings.ini
 */
UCLASS(config=EditorPerProjectUserSettings)
class QUICKBAKER_API UQuickBakerEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Last used resolution (width and height) */
	UPROPERTY(Config)
	int32 LastUsedResolution = 1024;

	/** Last used output path (Content Browser path or disk path) */
	UPROPERTY(Config)
	FString LastUsedOutputPath = TEXT("/Game/Textures");

	/** Last used bit depth */
	UPROPERTY(Config)
	uint8 LastUsedBitDepth = static_cast<uint8>(EQuickBakerBitDepth::Bit16);

	/** Last used output type */
	UPROPERTY(Config)
	uint8 LastUsedOutputType = static_cast<uint8>(EQuickBakerOutputType::Asset);

	/** Last used texture compression */
	UPROPERTY(Config)
	uint8 LastUsedCompression = static_cast<uint8>(TC_Default);
};
