#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Engine/TextureRenderTarget2D.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuickBaker, Log, All);

class QUICKBAKER_API FQuickBakerExporter
{
public:
	/**
	 * Exports the RenderTarget to a file (PNG or EXR).
	 * @param RenderTarget The Render Target to read from.
	 * @param FullPath The full file path including extension.
	 * @param bIsPNG True if exporting to PNG (8-bit), False for EXR (16-bit float).
	 * @return True if successful.
	 */
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);
};
