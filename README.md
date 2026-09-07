# reTerminal Sticky SD Keymap Display

Seeed Studio公式 `Sticky_dashboard_demo` の現行構成をベースにした、reTerminal Sticky用キーマップ画像ビューアです。microSD直下の `layer_0.png`, `layer_1.png`, ... を読み、3.97インチ 800×480・4階調E-Inkへ表示します。

> この派生物はSeeed Studio公式ファームウェアではありません。実機へ書き込むと既存ファームウェアを置き換えます。

## ベースと設計

2026-09-07時点の[公式ESP-IDF開発ガイド](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-basics/)から配布される `Sticky_dashboard_demo.zip` を基準にしています。公式デモと同じESP-IDF v5.4、ESP32-S3、`board / devices / app / pages / ui` 分離、800×480論理キャンバス、SSD1677表示ドライバー、SPI2共有microSD構成です。

追加した責務は次の通りです。

```text
main/
├── app/
│   ├── app_event.*              ボタン/BLEから表示処理へのイベント境界
│   └── layer_source.h           レイヤー通知元の抽象インターフェース
├── data_sources/
│   ├── ble_layer_source_stub.*  将来のZMK BLE実装との交換点
│   ├── png_decoder.*            libpng → 4階調Canvas
│   └── sd_keymap_source.*       layer_<n>.pngの取得
├── devices/                     公式デモ由来の実機ドライバー
├── pages/keymap_page.*          画像ページとエラー画面
└── ui/                          公式デモ由来のCanvas/フォント
```

## microSDの準備

1. 32GB以下のmicroSDをFAT32でフォーマットします。
2. `sample-keymaps/` 内のPNG、または自作PNGをカード直下へコピーします。
3. ファイル名は `layer_0.png`, `layer_1.png`, ...、画像は必ず800×480にします。

RGB/RGBA/グレースケールPNGを受け付け、透明部分は白として扱います。表示時に輝度を黒・濃灰・薄灰・白へ量子化します。UP/DOWNボタンでレイヤー0〜9を移動し、中央ボタンで再読込します。存在しない画像や不正サイズの場合は画面上にエラーを表示します。

## ローカルビルド（ESP-IDF v5.4）

```bash
idf.py set-target esp32s3
idf.py build
tools/make_merged.sh
```

個別書き込みとモニター:

```bash
idf.py -p PORT flash monitor
```

単一ファイルを書き込む場合（`merged.bin` はオフセット0）:

```bash
esptool.py --chip esp32s3 -p PORT write_flash 0x0 build/merged.bin
```

`tools/make_merged.sh` はESP-IDFが生成した `build/flash_args` をそのまま使うため、ブートローダー・パーティション・アプリのオフセットを手書きしません。

## GitHub Actions

`.github/workflows/build.yml` はpush、Pull Request、手動実行でESP-IDF v5.4.4を使ってビルドします。成功後、Actions実行画面のArtifactsから `sticky-keymap-firmware` をダウンロードできます。中には `merged.bin`、個別bin、`flasher_args.json`、`flash_args` が含まれます。

## ZMK BLEレイヤー通知を追加する場所

現在の `BleLayerSourceStub` は通信を行わず、ボタン操作だけを有効にします。BLEを追加するときは `LayerSource` を実装するクラス（例: `ZmkBleLayerSource`）を `main/data_sources/` に追加し、通知で得たレイヤー番号を登録済みコールバックへ渡します。その後 `main.cpp` の生成クラスを差し替え、必要なNimBLE依存を `main/CMakeLists.txt` と `sdkconfig.defaults` へ追加します。

この境界により、GATT UUIDやZMK側通知方式が変わっても、PNG、microSD、ページ、表示ドライバーは変更不要です。なお、ZMK標準HIDだけではアクティブレイヤー番号は通知されないため、ZMK側にも専用GATTサービス等の実装が必要です。

## 検証

サンプル画像の形式確認:

```bash
python3 tools/validate_keymaps.py sample-keymaps
```

CIはこの検証後にファームウェアをビルドし、merged.binを生成します。本パッケージ作成時にはソース一覧・スクリプト・サンプルPNGのローカル検査まで実施しています。最終コンパイルはGitHub Actionsで確認してください。実機でのE-Ink表示・microSDカード相性・BLE（未実装）は別途確認が必要です。

## ライセンスと由来

派生コードのライセンスは `LICENSE` を参照してください。`components/` と公式デモ由来ファイルは、それぞれの原著作者・同梱ライセンスおよび上流条件に従います。PNGデコードはESP Component Registryの `espressif/libpng` をビルド時に取得します。
