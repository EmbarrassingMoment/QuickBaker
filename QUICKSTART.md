# QuickBaker QuickStart Guide

## 🇯🇵 日本語 (Japanese)

**QuickBaker** は、Unreal Engine 5.5+ 向けの軽量なマテリアルベイクツールです。マテリアルの「エミッシブカラー（最終カラー）」出力を、ワンクリックで静的なテクスチャアセット（.uasset）や画像ファイル（.png, .exr）に変換します。ノイズパターンやSDF（Signed Distance Fields）、プロシージャルな模様をテクスチャ化するのに最適です。

### 1. インストールと有効化
1. **Fabランチャー（またはEpic Games Launcher）** から **QuickBaker** をエンジンにインストールします。
2. Unreal Engine プロジェクトを開きます。
3. メニューバーの **Edit (編集)** > **Plugins (プラグイン)** を開きます。
4. 検索バーに「QuickBaker」と入力し、チェックを入れて有効化します（再起動が必要な場合はエディタを再起動してください）。

### 2. 基本的な使い方
**ステップ 1: ウィンドウを開く**
メニューバーの **Tools (ツール)** > **QuickBaker** > **Quick Baker** を選択してツールウィンドウを開きます。

**ステップ 2: マテリアルの選択**
ベイクしたい **Material (マテリアル)** または **Material Instance (マテリアルインスタンス)** を選択します。
* 重要: このツールはマテリアルの **Emissive Color (エミッシブカラー)** ピンの出力をキャプチャします。プレビュー用のサムネイルが表示されます。

**ステップ 3: 設定の構成**
用途に合わせて設定を調整します。
* **Output Type**: プロジェクト内に保存する場合は `Asset`、外部ファイルとして保存する場合は `PNG` または `EXR` を選択します。
* **Resolution**: 解像度を選択します（64x64 ～ 8192x8192）。
* **Bit Depth**: 通常は `8-bit` ですが、滑らかなグラデーションが必要な場合（SDFやノイズなど）は `16-bit` を推奨します。

**ステップ 4: ベイク実行**
1. **Output Path** の `Browse` ボタンを押し、保存先フォルダを指定します。
2. **Bake Texture** ボタンをクリックします。指定した場所にテクスチャが保存されます。

### 3. トラブルシューティング
**Q: ベイクしたテクスチャが真っ黒になります。**
A: マテリアルの **Emissive Color (エミッシブカラー)** にノードが接続されているか確認してください。
* **Lit (ライティングあり)** マテリアルを使用している場合も、Base ColorではなくEmissive Colorに接続する必要があります。
* ベイク専用のマテリアルを作る場合は、Shading Modelを **Unlit** に設定することを推奨します。

---

## 🇺🇸 English

**QuickBaker** is a lightweight material baking tool for Unreal Engine 5.5+. It allows you to convert any material's "Emissive Color" (Final Color) output into a static Texture Asset (.uasset) or an image file (.png, .exr) with a single click. Ideally suited for baking noise patterns, Signed Distance Fields (SDF), and procedural textures.

### 1. Installation & Setup
1. Install **QuickBaker** to your engine via the **Fab Launcher (or Epic Games Launcher)**.
2. Open your Unreal Engine project.
3. Go to **Edit** > **Plugins**.
4. Search for "QuickBaker" and check the box to enable it (restart the editor if prompted).

### 2. Getting Started
**Step 1: Open the Window**
Go to **Tools** > **QuickBaker** > **Quick Baker** in the menu bar to open the tool window.

**Step 2: Select a Material**
Select the **Material** or **Material Instance** you want to bake.
* Note: This tool captures the output connected to the **Emissive Color** pin. A thumbnail preview will appear.

**Step 3: Configure Settings**
Adjust the settings based on your needs:
* **Output Type**: Choose `Asset` to save within the project, or `PNG` / `EXR` to export to disk.
* **Resolution**: Select a resolution ranging from 64x64 up to 8192x8192.
* **Bit Depth**: Use `8-bit` for standard textures. `16-bit` is recommended for high-precision data like SDFs or noise to avoid banding.

**Step 4: Bake**
1. Click `Browse` next to **Output Path** to select the destination folder.
2. Click the **Bake Texture** button. Your texture will be generated and saved instantly.

### 3. Troubleshooting
**Q: The baked texture is completely black.**
A: Please ensure your logic is connected to the **Emissive Color** pin in your material.
* Even if you are using a **Lit** material, you must connect your graph to Emissive Color for the baker to see it.
* For best results, we recommend setting your material's Shading Model to **Unlit**.
