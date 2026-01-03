#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Engine/TextureRenderTarget2D.h"
#include "QuickBakerSettings.h"

DECLARE_LOG_CATEGORY_EXTERN(LogQuickBaker, Log, All);

/**
 * Helper class for exporting Render Targets to external files (PNG, EXR, TGA, TIFF).
 */
class QUICKBAKER_API FQuickBakerExporter
{
public:
	/**
	 * Exports the content of a RenderTarget to a file on disk.
	 *
	 * @param RenderTarget The source Render Target to read pixels from.
	 * @param FullPath The full file system path where the file should be saved, including the extension.
	 * @param OutputType The format to export to (PNG, EXR, TGA, TIFF).
	 * @param BitDepth The desired bit depth (8-bit or 16-bit).
	 * @return True if the file was successfully saved, false otherwise.
	 */
	static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, EQuickBakerOutputType OutputType, EQuickBakerBitDepth BitDepth);
};
