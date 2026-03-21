// Copyright (c) 2026 Kurorekishi (EmbarrassingMoment).

#pragma once

#include "CoreMinimal.h"
#include "QuickBakerSettings.h"
#include "Containers/Ticker.h"

class UTextureRenderTarget2D;

/**
 * Async bake task that processes texture baking without blocking the editor.
 * Uses FTSTicker to advance work each frame: render, read pixels in chunks, then save.
 */
class QUICKBAKER_API FQuickBakerAsyncTask : public TSharedFromThis<FQuickBakerAsyncTask>
{
public:
	DECLARE_DELEGATE_TwoParams(FOnBakeComplete, bool /*bSuccess*/, const FText& /*ResultMessage*/);

	/**
	 * Starts an async bake operation. The returned task must be kept alive until completion.
	 *
	 * @param Settings The bake configuration.
	 * @return A shared reference to the running task.
	 */
	static TSharedRef<FQuickBakerAsyncTask> Start(const FQuickBakerSettings& Settings);

	/** Cancels the bake operation. OnComplete will still fire with bSuccess=false. */
	void Cancel();

	/** @return True if the task is still running. */
	bool IsRunning() const { return CurrentPhase != EPhase::Done; }

	/** @return Progress from 0.0 to 1.0. */
	float GetProgress() const;

	/** @return Localized status text describing the current phase. */
	FText GetStatusText() const;

	/** Called on game thread when the bake completes or is cancelled. */
	FOnBakeComplete OnComplete;

	~FQuickBakerAsyncTask();

private:
	FQuickBakerAsyncTask() = default;

	enum class EPhase : uint8
	{
		Setup,
		Rendering,
		WaitForGPU,
		ReadPixels,
		Saving,
		Done
	};

	/** Tick callback registered with FTSTicker. Returns true to keep ticking. */
	bool Tick(float DeltaTime);

	void PhaseSetup();
	void PhaseRendering();
	void PhaseWaitForGPU();
	bool PhaseReadPixels();
	void PhaseSaving();
	void Finish(bool bSuccess, const FText& Message);
	void Cleanup();

	// Configuration
	FQuickBakerSettings Settings;

	// State
	EPhase CurrentPhase = EPhase::Setup;
	bool bCancelled = false;
	FTSTicker::FDelegateHandle TickHandle;

	// Render Target (transient, added to root during lifetime)
	UTextureRenderTarget2D* RenderTarget = nullptr;

	// ReadPixels chunking
	int32 ChunkRowStart = 0;
	int32 ChunkRowSize = 128; // Number of rows to read per tick
	int32 TotalRows = 0;

	// Accumulated pixel data
	TArray<uint8> PixelBuffer;
	bool bIs16Bit = false;
	int32 BytesPerPixel = 0;
};
