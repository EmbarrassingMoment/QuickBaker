// ========================================
// ファイル: Source/QuickBaker/Public/QuickBakerExporter.h
// ========================================

#pragma once

#include "CoreMinimal.h"
#include "QuickBakerSettings.h"

class UTextureRenderTarget2D;

class QUICKBAKER_API FQuickBakerExporter
{
public:
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& OutputPath, const FString& OutputName, EQuickBakerOutputType OutputType);
};
