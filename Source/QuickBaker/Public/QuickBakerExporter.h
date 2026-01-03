// Copyright Kurorekishi, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Engine/TextureRenderTarget2D.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuickBaker, Log, All);

/**
 * Helper class for exporting Render Targets to external files (PNG, EXR).
 */
class QUICKBAKER_API FQuickBakerExporter
{
public:
	/**
	 * Exports the content of a RenderTarget to a file on disk (PNG or EXR).
	 *
	 * @param RenderTarget The source Render Target to read pixels from.
	 * @param FullPath The full file system path where the file should be saved, including the extension.
	 * @param bIsPNG Set to true to export as PNG (8-bit, BGRA). Set to false to export as EXR (16-bit float, RGBA).
	 * @return True if the file was successfully saved, false otherwise.
	 */
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);
};
