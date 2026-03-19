// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerUtils.h"
#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "TextureResource.h"

FString FQuickBakerUtils::GetTextureNameFromMaterial(const FString& MaterialName)
{
	FString Name = MaterialName;
	if (Name.StartsWith("M_"))
	{
		Name = "T_" + Name.RightChop(2);
	}
	else if (Name.StartsWith("MI_"))
	{
		Name = "T_" + Name.RightChop(3);
	}
	else
	{
		Name = "T_" + Name;
	}
	return Name;
}

bool FQuickBakerUtils::ReadbackPixels(UTextureRenderTarget2D* RenderTarget, TArray<uint8>& OutData, bool bIsFloat16)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: RenderTarget is null."));
		return false;
	}

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: Could not get render target resource."));
		return false;
	}

	// Ensure all rendering commands (including DrawMaterialToRenderTarget) are complete
	FlushRenderingCommands();

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	if (bIsFloat16)
	{
		TArray<FFloat16Color> SurfaceData;
		SurfaceData.Reserve(Width * Height);
		if (!RTResource->ReadFloat16Pixels(SurfaceData) || SurfaceData.Num() == 0)
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: Could not read Float16 pixels."));
			return false;
		}
		OutData.SetNumUninitialized(SurfaceData.Num() * sizeof(FFloat16Color));
		FMemory::Memcpy(OutData.GetData(), SurfaceData.GetData(), OutData.Num());
	}
	else
	{
		TArray<FColor> SurfaceData;
		SurfaceData.Reserve(Width * Height);
		FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
		ReadPixelFlags.SetLinearToGamma(false);
		if (!RTResource->ReadPixels(SurfaceData, ReadPixelFlags) || SurfaceData.Num() == 0)
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: Could not read pixels."));
			return false;
		}
		OutData.SetNumUninitialized(SurfaceData.Num() * sizeof(FColor));
		FMemory::Memcpy(OutData.GetData(), SurfaceData.GetData(), OutData.Num());
	}

	return true;
}
