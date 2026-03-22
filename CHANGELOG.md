# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-03-22
### Added
- PNG transparency and alpha channel support for exported images.
- Persistent editor settings that are preserved between sessions.
- Progress bar (`FScopedSlowTask`) for BakeToAsset and ExportToFile operations.
- Resolution safety checks and pixel count validation to prevent overflow.
- Memory optimization with `ParallelFor` in the baking pipeline.
- Material selection UI using `SObjectPropertyEntryBox` with 64x64 thumbnail preview.
- Dynamic visibility for Compression settings based on the selected Output Type.
- Content Browser context menu integration for quick access via right-click on Materials.
- Fab Marketplace integration (`MarketplaceURL` in `.uplugin`).
- Localization support (English and Japanese).
- API documentation (`Docs/API.md`, `Docs/API.ja.md`).
- Quick Start guides (`QUICKSTART.md`, `QUICKSTART.ja.md`).

### Fixed
- GC protection for Render Target in `ExecuteBake` to prevent garbage collection during bake.
- `UPackage::SavePackage` implementation in `BakeToAsset` for reliable asset saving.
- TArray int32 overflow guard for large resolution and 16-bit depth combinations.
- Critical bugs in core baking logic and UI widget.
- `LogQuickBaker` compilation error.

### Changed
- Updated copyright headers and year to 2026.
- Added English Doxygen comments to all public header files.

## [1.0.0] - 2026-01-01
### Added
- Initial release of QuickBaker.
- Core functionality to bake Material Emissive output into Static Textures.
- Support for multiple output formats:
  - **Texture Asset** (`.uasset`): Saves directly to the Content Browser.
  - **PNG**: Exports to disk as 8-bit images.
  - **EXR**: Exports to disk as 16-bit float images (ideal for high precision).
- Dedicated Editor Window with:
  - Material selection with 64x64 Thumbnail Preview.
  - Customizable Resolution (64x64 to 8192x8192).
  - Bit Depth selection (8-bit / 16-bit), with smart locking for external formats.
  - Texture Compression settings.
- Automatic asset naming convention (converts `M_` or `MI_` prefixes to `T_`).
- Content Browser context menu integration for quick access via right-click on Materials.
- Automatic directory creation for output paths.
- Validation warnings for non-Unlit shading models.
- Support for Unreal Engine 5.5+.
