// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerExporter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY(LogQuickBaker);

#define LOCTEXT_NAMESPACE "FQuickBakerExporter"

bool FQuickBakerExporter::ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: RenderTarget is null."));
		return false;
	}

	// Ensure all rendering commands are completed before reading pixels
	FlushRenderingCommands();

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: Could not get RenderTarget Resource from %s."), *RenderTarget->GetName());
		return false;
	}

	// Nested progress: 3 sub-phases (Read Pixels, Compress, Write File)
	FScopedSlowTask SubTask(3.0f, bIsPNG
		? LOCTEXT("ExportPNG", "Exporting PNG...")
		: LOCTEXT("ExportEXR", "Exporting EXR..."));

	FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
	ReadPixelFlags.SetLinearToGamma(false);

	TArray<uint8> CompressedData;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Sub-phase 1: Read pixels from render target
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingPixels_Export", "Reading pixels..."));

	if (bIsPNG)
	{
		// PNG Export (8-bit)
		TArray<FColor> Bitmap;
		RTResource->ReadPixels(Bitmap, ReadPixelFlags);

		// Sub-phase 2: Compress image
		SubTask.EnterProgressFrame(1.0f, LOCTEXT("Compressing_PNG", "Compressing PNG..."));

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), (int64)Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
		else
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: PNG Image compression failed for %s."), *FullPath);
		}
	}
	else
	{
		// EXR Export (16-bit float, Linear color space)
		// Read directly as FFloat16Color to avoid intermediate FLinearColor allocation (saves ~50% memory)
		TArray<FFloat16Color> Bitmap;
		RTResource->ReadFloat16Pixels(Bitmap);

		// Sub-phase 2: Compress image
		SubTask.EnterProgressFrame(1.0f, LOCTEXT("Compressing_EXR", "Compressing EXR..."));

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), (int64)Bitmap.Num() * sizeof(FFloat16Color), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::RGBAF, 16))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
		else
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: EXR Image compression failed for %s."), *FullPath);
		}
	}

	// Sub-phase 3: Write file to disk
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("WritingFile", "Writing file to disk..."));

	if (CompressedData.Num() > 0)
	{
		if (FFileHelper::SaveArrayToFile(CompressedData, *FullPath))
		{
			UE_LOG(LogQuickBaker, Log, TEXT("Successfully exported texture to %s"), *FullPath);
			return true;
		}
		else
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: Could not write file to %s"), *FullPath);
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
