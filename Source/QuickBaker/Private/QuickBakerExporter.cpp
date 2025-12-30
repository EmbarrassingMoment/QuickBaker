#include "QuickBakerExporter.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FQuickBakerExporter"

bool FQuickBakerExporter::ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG)
{
	if (!RenderTarget)
	{
		return false;
	}

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
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

		// Force Alpha to 255 (Opaque)
		for (FColor& Pixel : Bitmap)
		{
			Pixel.A = 255;
		}

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), RenderTarget->SizeX, RenderTarget->SizeY, ERGBFormat::BGRA, 8))
		{
			CompressedData = ImageWrapper->GetCompressed();
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
	}

	if (CompressedData.Num() > 0)
	{
		return FFileHelper::SaveArrayToFile(CompressedData, *FullPath);
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
