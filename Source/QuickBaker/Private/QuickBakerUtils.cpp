// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerUtils.h"
#include "QuickBakerExporter.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RHIGPUReadback.h"
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

	FRHITexture* RHITexture = RTResource->GetRenderTargetTexture();
	if (!RHITexture)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: Could not get RHI texture."));
		return false;
	}

	const int32 Width = RHITexture->GetSizeX();
	const int32 Height = RHITexture->GetSizeY();
	const int32 BytesPerPixel = bIsFloat16 ? sizeof(FFloat16Color) : sizeof(FColor);

	FRHIGPUTextureReadback Readback(TEXT("QuickBakerReadback"));

	ENQUEUE_RENDER_COMMAND(QuickBakerEnqueueReadback)(
		[&Readback, RHITexture, Width, Height](FRHICommandListImmediate& RHICmdList)
		{
			Readback.EnqueueCopy(RHICmdList, RHITexture, FResolveRect(0, 0, Width, Height));
		});

	FlushRenderingCommands();

	if (!Readback.IsReady())
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: GPU readback not ready after flush."));
		return false;
	}

	int32 RowPitchInPixels = 0;
	const void* MappedData = Readback.Lock(RowPitchInPixels);
	if (!MappedData)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ReadbackPixels failed: Could not lock readback buffer."));
		return false;
	}

	const int32 RowBytes = Width * BytesPerPixel;
	const int32 SrcRowPitch = RowPitchInPixels * BytesPerPixel;
	OutData.SetNumUninitialized(Width * Height * BytesPerPixel);

	const uint8* Src = static_cast<const uint8*>(MappedData);
	uint8* Dst = OutData.GetData();

	for (int32 Row = 0; Row < Height; ++Row)
	{
		FMemory::Memcpy(Dst + Row * RowBytes, Src + Row * SrcRowPitch, RowBytes);
	}

	Readback.Unlock();
	return true;
}
