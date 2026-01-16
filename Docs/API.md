# QuickBaker API Documentation

This document describes the internal API of the **QuickBaker** module and provides guidelines for extending its functionality.

## Overview

QuickBaker is an Editor module designed to bake Material output (Emissive Color) into static 2D Textures or external files (PNG, EXR). It uses the Unreal Engine rendering pipeline (`UKismetRenderingLibrary`) to draw materials onto Render Targets and then processes the results.

The module is exposed via the `FQuickBakerModule` class, but the core logic resides in `FQuickBakerCore`.

## Core API

### `FQuickBakerCore`

The `FQuickBakerCore` class contains the main logic for baking. It is a static class.

*   **Header:** `Source/QuickBaker/Public/QuickBakerCore.h`

#### `ExecuteBake`

```cpp
static void ExecuteBake(const FQuickBakerSettings& Settings);
```

Executes the bake process based on the provided settings. This function handles:
1.  Setting up a temporary Render Target.
2.  Drawing the material to the Render Target.
3.  Saving the output (creating an Asset or writing to a file).
4.  Cleaning up resources.

**Parameters:**
*   `Settings`: An `FQuickBakerSettings` struct containing all necessary configuration.

### `FQuickBakerSettings`

The configuration structure for the baking process.

*   **Header:** `Source/QuickBaker/Public/QuickBakerSettings.h`

**Key Properties:**
*   `SelectedMaterial` (`TWeakObjectPtr<UMaterialInterface>`): The material to bake.
*   `OutputType` (`EQuickBakerOutputType`): `Asset`, `PNG`, or `EXR`.
*   `Resolution` (`int32`): Output resolution (e.g., 512, 1024, 2048).
*   `BitDepth` (`EQuickBakerBitDepth`): `Bit8` or `Bit16` (Float).
*   `OutputName` (`FString`): The name of the resulting file or asset.
*   `OutputPath` (`FString`): The directory path (Package path for Assets, OS path for files).
*   `Compression` (`TextureCompressionSettings`): Compression settings (only for Assets).

### `FQuickBakerModule`

The module interface implementation.

*   **Header:** `Source/QuickBaker/Public/QuickBakerModule.h`

#### `OpenQuickBakerWithMaterial`

```cpp
void OpenQuickBakerWithMaterial(class UMaterialInterface* Material);
```

Opens the QuickBaker editor window and automatically selects the specified material. This is useful for integrating QuickBaker into other editor tools or context menus.

## Helper API

### `FQuickBakerExporter`

Helper class for handling file exports.

*   **Header:** `Source/QuickBaker/Public/QuickBakerExporter.h`

#### `ExportToFile`

```cpp
static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);
```

Exports the contents of a Render Target to a disk file.

**Parameters:**
*   `RenderTarget`: The source render target.
*   `FullPath`: The absolute file path including extension.
*   `bIsPNG`: `true` for PNG (8-bit), `false` for EXR (16-bit).

## Usage Example

The following example demonstrates how to invoke QuickBaker programmatically to bake a material into a Texture Asset.

```cpp
#include "QuickBakerCore.h"
#include "QuickBakerSettings.h"
#include "Materials/MaterialInterface.h"

void BakeMaterialCode(UMaterialInterface* MyMaterial)
{
    if (!MyMaterial) return;

    FQuickBakerSettings Settings;
    Settings.SelectedMaterial = MyMaterial;
    Settings.Resolution = 1024;
    Settings.OutputType = EQuickBakerOutputType::Asset;
    Settings.BitDepth = EQuickBakerBitDepth::Bit8;
    Settings.OutputName = "T_MyBakedTexture";
    Settings.OutputPath = "/Game/Textures/Baked";
    Settings.Compression = TC_Default;

    if (Settings.IsValid())
    {
        FQuickBakerCore::ExecuteBake(Settings);
    }
}
```

## Extension Guide

### Adding Support for New File Formats

To add support for a new file format (e.g., JPEG or TGA):

1.  **Update Enum:**
    *   Add a new entry to `EQuickBakerOutputType` in `QuickBakerSettings.h`.
2.  **Update UI:**
    *   Modify `SQuickBakerWidget::Construct` to include the new option in the Output Type selector.
    *   Update `HandleOutputTypeChanged` if specific logic (like hiding compression settings) is needed.
3.  **Update Exporter:**
    *   Modify `FQuickBakerExporter::ExportToFile` or create a new method to handle the specific format using `IImageWrapper`.
4.  **Update Core Logic:**
    *   In `FQuickBakerCore::ExecuteBake`, add a case for the new enum value in the "Save" phase to call your exporter.

### Customizing Rendering

The rendering logic uses `UKismetRenderingLibrary::DrawMaterialToRenderTarget`. If you need custom rendering parameters (e.g., specific view transforms):

1.  Modify `FQuickBakerCore::ExecuteBake`.
2.  Instead of `UKismetRenderingLibrary`, you may need to manually set up a `FCanvas` or use `UCanvas::K2_DrawMaterial` with custom parameters before resolving the render target.
