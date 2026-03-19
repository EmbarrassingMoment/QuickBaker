// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#include "QuickBakerExporter.h"
#include "QuickBakerUtils.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "RenderingThread.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY(LogQuickBaker);

#define LOCTEXT_NAMESPACE "FQuickBakerExporter"

bool FQuickBakerExporter::ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: RenderTarget is null."));
		return false;
	}

	// Nested progress: 3 sub-phases (Read Pixels, Compress, Write File)
	FScopedSlowTask SubTask(3.0f, bIsPNG
		? LOCTEXT("ExportPNG", "Exporting PNG...")
		: LOCTEXT("ExportEXR", "Exporting EXR..."));

	// Sub-phase 1: Read pixels from render target using async GPU readback
	SubTask.EnterProgressFrame(1.0f, LOCTEXT("ReadingPixels_Export", "Reading pixels..."));

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	TArray<uint8> CompressedData;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

	// Sub-phase 2: Compress image
	if (bIsPNG)
	{
		TArray<FColor> PixelData;
		if (!FQuickBakerUtils::ReadbackColorPixels(RenderTarget, PixelData))
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: Could not read pixels from render target."));
			return false;
		}

		SubTask.EnterProgressFrame(1.0f, LOCTEXT("Compressing_PNG", "Compressing PNG..."));

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PixelData.GetData(), (int64)PixelData.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
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
		TArray<FFloat16Color> PixelData;
		if (!FQuickBakerUtils::ReadbackFloat16Pixels(RenderTarget, PixelData))
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFile failed: Could not read pixels from render target."));
			return false;
		}

		SubTask.EnterProgressFrame(1.0f, LOCTEXT("Compressing_EXR", "Compressing EXR..."));

		TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PixelData.GetData(), (int64)PixelData.Num() * sizeof(FFloat16Color), Width, Height, ERGBFormat::RGBAF, 16))
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

void FQuickBakerExporter::ExportToFileAsync(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG,
	TFunction<void(bool bSuccess, const FString& ResultPath)> OnComplete)
{
	if (!RenderTarget)
	{
		UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: RenderTarget is null."));
		if (OnComplete)
		{
			OnComplete(false, FullPath);
		}
		return;
	}

	const int32 Width = RenderTarget->SizeX;
	const int32 Height = RenderTarget->SizeY;

	if (bIsPNG)
	{
		// Read pixels on the game thread (requires GPU access)
		TArray<FColor> PixelData;
		if (!FQuickBakerUtils::ReadbackColorPixels(RenderTarget, PixelData))
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: Could not read pixels from render target."));
			if (OnComplete)
			{
				OnComplete(false, FullPath);
			}
			return;
		}

		// Offload compression and disk I/O to a background thread
		Async(EAsyncExecution::ThreadPool, [PixelData = MoveTemp(PixelData), FullPath, Width, Height, OnComplete]()
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			TArray<uint8> CompressedData;

			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PixelData.GetData(), (int64)PixelData.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
			else
			{
				UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: PNG compression failed for %s."), *FullPath);
			}

			bool bSuccess = false;
			if (CompressedData.Num() > 0)
			{
				bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *FullPath);
				if (bSuccess)
				{
					UE_LOG(LogQuickBaker, Log, TEXT("Successfully exported texture to %s"), *FullPath);
				}
				else
				{
					UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: Could not write file to %s"), *FullPath);
				}
			}

			// Callback on the game thread
			if (OnComplete)
			{
				AsyncTask(ENamedThreads::GameThread, [OnComplete, bSuccess, FullPath]()
				{
					OnComplete(bSuccess, FullPath);
				});
			}
		});
	}
	else
	{
		// Read pixels on the game thread (requires GPU access)
		TArray<FFloat16Color> PixelData;
		if (!FQuickBakerUtils::ReadbackFloat16Pixels(RenderTarget, PixelData))
		{
			UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: Could not read pixels from render target."));
			if (OnComplete)
			{
				OnComplete(false, FullPath);
			}
			return;
		}

		// Offload compression and disk I/O to a background thread
		Async(EAsyncExecution::ThreadPool, [PixelData = MoveTemp(PixelData), FullPath, Width, Height, OnComplete]()
		{
			IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			TArray<uint8> CompressedData;

			TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);
			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(PixelData.GetData(), (int64)PixelData.Num() * sizeof(FFloat16Color), Width, Height, ERGBFormat::RGBAF, 16))
			{
				CompressedData = ImageWrapper->GetCompressed();
			}
			else
			{
				UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: EXR compression failed for %s."), *FullPath);
			}

			bool bSuccess = false;
			if (CompressedData.Num() > 0)
			{
				bSuccess = FFileHelper::SaveArrayToFile(CompressedData, *FullPath);
				if (bSuccess)
				{
					UE_LOG(LogQuickBaker, Log, TEXT("Successfully exported texture to %s"), *FullPath);
				}
				else
				{
					UE_LOG(LogQuickBaker, Error, TEXT("ExportToFileAsync failed: Could not write file to %s"), *FullPath);
				}
			}

			// Callback on the game thread
			if (OnComplete)
			{
				AsyncTask(ENamedThreads::GameThread, [OnComplete, bSuccess, FullPath]()
				{
					OnComplete(bSuccess, FullPath);
				});
			}
		});
	}
}

#undef LOCTEXT_NAMESPACE
