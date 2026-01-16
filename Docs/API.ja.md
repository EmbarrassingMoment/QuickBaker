# QuickBaker API ドキュメント

このドキュメントでは、**QuickBaker** モジュールの内部 API と、その機能拡張方法について説明します。

## 概要

QuickBaker は、マテリアルの出力 (エミッシブカラー) を静的な 2D テクスチャや外部ファイル (PNG, EXR) にベイクするために設計されたエディタモジュールです。Unreal Engine のレンダリングパイプライン (`UKismetRenderingLibrary`) を使用してマテリアルをレンダーターゲットに描画し、その結果を処理します。

モジュール自体は `FQuickBakerModule` クラスを介して公開されていますが、コアロジックは `FQuickBakerCore` に実装されています。

## コア API

### `FQuickBakerCore`

`FQuickBakerCore` クラスには、ベイクの主要なロジックが含まれています。これは静的クラスです。

*   **ヘッダ:** `Source/QuickBaker/Public/QuickBakerCore.h`

#### `ExecuteBake`

```cpp
static void ExecuteBake(const FQuickBakerSettings& Settings);
```

指定された設定に基づいてベイク処理を実行します。この関数は以下を処理します。
1.  一時的なレンダーターゲットのセットアップ。
2.  レンダーターゲットへのマテリアルの描画。
3.  出力の保存 (アセットの作成またはファイルへの書き込み)。
4.  リソースのクリーンアップ。

**パラメータ:**
*   `Settings`: 必要なすべての設定を含む `FQuickBakerSettings` 構造体。

### `FQuickBakerSettings`

ベイク処理の設定構造体です。

*   **ヘッダ:** `Source/QuickBaker/Public/QuickBakerSettings.h`

**主要プロパティ:**
*   `SelectedMaterial` (`TWeakObjectPtr<UMaterialInterface>`): ベイク対象のマテリアル。
*   `OutputType` (`EQuickBakerOutputType`): `Asset`, `PNG`, または `EXR`。
*   `Resolution` (`int32`): 出力解像度 (例: 512, 1024, 2048)。
*   `BitDepth` (`EQuickBakerBitDepth`): `Bit8` または `Bit16` (Float)。
*   `OutputName` (`FString`): 結果のファイル名またはアセット名。
*   `OutputPath` (`FString`): ディレクトリパス (アセットの場合はパッケージパス、ファイルの場合は OS パス)。
*   `Compression` (`TextureCompressionSettings`): 圧縮設定 (アセットのみ)。

### `FQuickBakerModule`

モジュールインターフェースの実装です。

*   **ヘッダ:** `Source/QuickBaker/Public/QuickBakerModule.h`

#### `OpenQuickBakerWithMaterial`

```cpp
void OpenQuickBakerWithMaterial(class UMaterialInterface* Material);
```

QuickBaker エディタウィンドウを開き、指定されたマテリアルを自動的に選択します。これは、QuickBaker を他のエディタツールやコンテキストメニューに統合する場合に便利です。

## ヘルパー API

### `FQuickBakerExporter`

ファイルのエクスポートを処理するヘルパークラスです。

*   **ヘッダ:** `Source/QuickBaker/Public/QuickBakerExporter.h`

#### `ExportToFile`

```cpp
static bool ExportToFile(UTextureRenderTarget2D* RenderTarget, const FString& FullPath, bool bIsPNG);
```

レンダーターゲットの内容をディスク上のファイルにエクスポートします。

**パラメータ:**
*   `RenderTarget`: ソースとなるレンダーターゲット。
*   `FullPath`: 拡張子を含む絶対ファイルパス。
*   `bIsPNG`: PNG (8-bit) の場合は `true`、EXR (16-bit) の場合は `false`。

## 使用例

以下の例は、プログラムから QuickBaker を呼び出して、マテリアルをテクスチャアセットにベイクする方法を示しています。

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

## 拡張ガイド

### 新しいファイル形式のサポート追加

新しいファイル形式 (例: JPEG や TGA) のサポートを追加するには:

1.  **Enum の更新:**
    *   `QuickBakerSettings.h` の `EQuickBakerOutputType` に新しいエントリを追加します。
2.  **UI の更新:**
    *   `SQuickBakerWidget::Construct` を変更し、出力タイプセレクタに新しいオプションを含めます。
    *   特定のロジック (圧縮設定の非表示など) が必要な場合は、`HandleOutputTypeChanged` を更新します。
3.  **Exporter の更新:**
    *   `FQuickBakerExporter::ExportToFile` を変更するか、`IImageWrapper` を使用して特定の形式を処理する新しいメソッドを作成します。
4.  **コアロジックの更新:**
    *   `FQuickBakerCore::ExecuteBake` の「保存」フェーズで、新しい Enum 値のケースを追加し、Exporter を呼び出します。

### レンダリングのカスタマイズ

レンダリングロジックは `UKismetRenderingLibrary::DrawMaterialToRenderTarget` を使用しています。カスタムのレンダリングパラメータ (例: 特定のビュー変換) が必要な場合:

1.  `FQuickBakerCore::ExecuteBake` を変更します。
2.  `UKismetRenderingLibrary` の代わりに、レンダーターゲットを解決する前に手動で `FCanvas` をセットアップするか、カスタムパラメータを持つ `UCanvas::K2_DrawMaterial` を使用する必要があるかもしれません。
