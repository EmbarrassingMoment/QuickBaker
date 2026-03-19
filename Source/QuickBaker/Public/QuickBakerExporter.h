// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

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
	 * Exports the content of a RenderTarget to a file on disk (PNG or EXR). Synchronous.
	 *
	 * @param RenderTarget The source Render Target to read pixels from.
	 * @param FullPath The full file system path where the file should be saved, including the extension.
	 * @param bIsPNG Set to true to export as PNG (8-bit, BGRA). Set to false to export as EXR (16-bit float, RGBA).
	 * @return True if the file was successfully saved, false otherwise.
	 */
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);

	/**
	 * Exports the content of a RenderTarget to a file on disk asynchronously.
	 * Pixel readback happens on the game thread, then compression and disk I/O are offloaded to a background thread.
	 *
	 * @param RenderTarget The source Render Target to read pixels from.
	 * @param FullPath The full file system path where the file should be saved, including the extension.
	 * @param bIsPNG Set to true to export as PNG (8-bit, BGRA). Set to false to export as EXR (16-bit float, RGBA).
	 * @param OnComplete Callback invoked on the game thread when the export is complete.
	 */
	static void ExportToFileAsync(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG,
		TFunction<void(bool bSuccess, const FString& ResultPath)> OnComplete);
};
