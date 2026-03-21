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
	 * Exports the content of a RenderTarget to a file on disk (PNG or EXR).
	 *
	 * @param RenderTarget The source Render Target to read pixels from.
	 * @param FullPath The full file system path where the file should be saved, including the extension.
	 * @param bIsPNG Set to true to export as PNG (8-bit, BGRA). Set to false to export as EXR (16-bit float, RGBA).
	 * @return True if the file was successfully saved, false otherwise.
	 */
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);

	/**
	 * Exports a raw pixel buffer to a file on disk (PNG or EXR).
	 * Used by the async bake path where pixels have already been read into a buffer.
	 *
	 * @param PixelData Raw pixel data (FColor for 8-bit, FFloat16Color for 16-bit).
	 * @param Width Image width in pixels.
	 * @param Height Image height in pixels.
	 * @param FullPath The full file system path including extension.
	 * @param bIsPNG True for PNG (8-bit), false for EXR (16-bit float).
	 * @param bIs16Bit True if PixelData contains FFloat16Color, false for FColor.
	 * @return True if the file was successfully saved.
	 */
	static bool ExportFromBuffer(const TArray<uint8>& PixelData, int32 Width, int32 Height, const FString& FullPath, bool bIsPNG, bool bIs16Bit);
};
