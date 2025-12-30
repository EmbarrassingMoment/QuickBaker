// ========================================
// ファイル: Source/QuickBaker/Public/QuickBakerCore.h
// ========================================

#pragma once

#include "CoreMinimal.h"
#include "QuickBakerSettings.h"

class QUICKBAKER_API FQuickBakerCore
{
public:
	static void ExecuteBake(const FQuickBakerSettings& Settings);
};
