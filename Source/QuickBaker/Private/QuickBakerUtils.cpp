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

bool FQuickBakerUtils::ReadbackFloat16Pixels(UTextureRenderTarget2D* RenderTarget, TArray<FFloat16Color>& OutData)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackFloat16Pixels failed: RenderTarget is null."));
		return false;
	}

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackFloat16Pixels failed: Could not get render target resource."));
		return false;
	}

	// Ensure all rendering commands (including DrawMaterialToRenderTarget) are complete
	FlushRenderingCommands();

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	OutData.Reserve(Width * Height);
	if (!RTResource->ReadFloat16Pixels(OutData) || OutData.Num() == 0)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackFloat16Pixels failed: Could not read Float16 pixels."));
		return false;
	}

	return true;
}

bool FQuickBakerUtils::ReadbackColorPixels(UTextureRenderTarget2D* RenderTarget, TArray<FColor>& OutData)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackColorPixels failed: RenderTarget is null."));
		return false;
	}

	FTextureRenderTargetResource* RTResource = RenderTarget->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackColorPixels failed: Could not get render target resource."));
		return false;
	}

	// Ensure all rendering commands (including DrawMaterialToRenderTarget) are complete
	FlushRenderingCommands();

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	OutData.Reserve(Width * Height);
	FReadSurfaceDataFlags ReadPixelFlags(RCM_MinMax);
	ReadPixelFlags.SetLinearToGamma(false);
	if (!RTResource->ReadPixels(OutData, ReadPixelFlags) || OutData.Num() == 0)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackColorPixels failed: Could not read pixels."));
		return false;
	}

	return true;
}
