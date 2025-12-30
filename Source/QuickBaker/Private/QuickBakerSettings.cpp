// ========================================
// ファイル: Source/QuickBaker/Private/QuickBakerSettings.cpp
// ========================================

#include "QuickBakerSettings.h"
#include "Materials/MaterialInterface.h"

FQuickBakerSettings::FQuickBakerSettings()
	: OutputType(EQuickBakerOutputType::Asset)
	, Resolution(1024)
	, bUse16Bit(false)
	, Compression(TC_Default)
	, OutputPath(TEXT("/Game/Textures"))
{
}

bool FQuickBakerSettings::IsValid() const
{
	if (!Material.IsValid())
	{
		return false;
	}

	if (OutputName.IsEmpty())
	{
		return false;
	}

	return true;
}
