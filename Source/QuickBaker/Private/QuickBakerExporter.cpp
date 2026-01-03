#include "QuickBakerExporter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY(LogQuickBaker);

#define LOCTEXT_NAMESPACE "FQuickBakerExporter"

bool FQuickBakerExporter::ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, EQuickBakerOutputType OutputType, EQuickBakerBitDepth BitDepth)
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
	TSharedPtr<IImageWrapper> ImageWrapper;

	if (OutputType == EQuickBakerOutputType::PNG)
	{
		// PNG Export (8-bit)
		TArray<FColor> Bitmap;
		RTResource->ReadPixels(Bitmap, ReadPixelFlags);

		// Force Alpha to 255 (Opaque)
		for (FColor& Pixel : Bitmap)
		{
			Pixel.A = 255;
		}

		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
	}
	else if (OutputType == EQuickBakerOutputType::EXR)
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

		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::RGBAF, 16))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
	}
	else if (OutputType == EQuickBakerOutputType::TGA)
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::TGA);

		if (BitDepth == EQuickBakerBitDepth::Bit16)
		{
			// TGA 16-bit
			TArray<FLinearColor> LinearBitmap;
			RTResource->ReadLinearColorPixels(LinearBitmap, ReadPixelFlags);

			TArray<FFloat16Color> Bitmap;
			Bitmap.Reserve(LinearBitmap.Num());
			for (const FLinearColor& Color : LinearBitmap)
			{
				Bitmap.Add(FFloat16Color(Color));
			}

			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::RGBAF, 16))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
		}
		else
		{
			// TGA 8-bit
			TArray<FColor> Bitmap;
			RTResource->ReadPixels(Bitmap, ReadPixelFlags);

			// Preserve Alpha
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
		}
	}
	else if (OutputType == EQuickBakerOutputType::TIFF)
	{
		ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::TIFF);

		if (BitDepth == EQuickBakerBitDepth::Bit16)
		{
			// TIFF 16-bit
			TArray<FLinearColor> LinearBitmap;
			RTResource->ReadLinearColorPixels(LinearBitmap, ReadPixelFlags);

			TArray<FFloat16Color> Bitmap;
			Bitmap.Reserve(LinearBitmap.Num());
			for (const FLinearColor& Color : LinearBitmap)
			{
				Bitmap.Add(FFloat16Color(Color));
			}

			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::RGBAF, 16))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
		}
		else
		{
			// TIFF 8-bit
			TArray<FColor> Bitmap;
			RTResource->ReadPixels(Bitmap, ReadPixelFlags);

			// Preserve Alpha
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
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
	else
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: Image compression failed for %s."), *FullPath);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
