# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]
### Added
- TGA export support (8-bit and 16-bit).
- TIFF export support (8-bit and 16-bit float).

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
