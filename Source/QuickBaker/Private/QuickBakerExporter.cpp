// Copyright (c) 2025 Kurorekishi (EmbarrassingMoment). Licensed under MIT License.

#include "QuickBakerExporter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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

	FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
	ReadPixelFlags.SetLinearToGamma(false);

	TArray<uint8> CompressedData;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	if (bIsPNG)
	{
		// PNG Export (8-bit)
		TArray<FColor> Bitmap;
		RTResource->ReadPixels(Bitmap, ReadPixelFlags);

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
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
		TArray<FLinearColor> LinearBitmap;
		RTResource->ReadLinearColorPixels(LinearBitmap, ReadPixelFlags);

		// Convert FLinearColor to FFloat16Color for EXR export
		TArray<FFloat16Color> Bitmap;
		Bitmap.Reserve(LinearBitmap.Num());
		for (const FLinearColor& Color : LinearBitmap)
		{
			Bitmap.Add(FFloat16Color(Color));
		}

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::RGBAF, 16))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
		else
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: EXR Image compression failed for %s."), *FullPath);
		}
	}

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
