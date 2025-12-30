// ========================================
// ファイル: Source/QuickBaker/Private/QuickBakerExporter.cpp
// ========================================

#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"

bool FQuickBakerExporter::ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& OutputPath, const FString& OutputName, EQuickBakerOutputType OutputType)
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

	int32 Width = RenderTarget->SizeX;
	int32 Height = RenderTarget->SizeY;

	const bool bIsPNG = OutputType == EQuickBakerOutputType::PNG;
	const bool bIsEXR = OutputType == EQuickBakerOutputType::EXR;

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
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
	}
	else if (bIsEXR)
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
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FFloat16Color), Width, Height, ERGBFormat::RGBAF, 16))
		{
			CompressedData = ImageWrapper->GetCompressed();
		}
	}

	if (CompressedData.Num() > 0)
	{
		FString Extension = bIsPNG ? TEXT(".png") : TEXT(".exr");
		FString FullPath = FPaths::Combine(OutputPath, OutputName + Extension);

		return FFileHelper::SaveArrayToFile(CompressedData, *FullPath);
	}

	return false;
}
