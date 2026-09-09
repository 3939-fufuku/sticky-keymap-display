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
│   ├── zmk_ble_layer_source.*   Nickey44のBLE通知を受信
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

RGB/RGBA/グレースケールPNGを受け付け、透明部分は白として扱います。表示時に輝度を黒・濃灰・薄灰・白へ量子化します。UP/DOWNボタンでレイヤー0〜3を移動し、中央ボタンで再読込します。存在しない画像や不正サイズの場合は画面上にエラーを表示します。

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

## Nickey44とのBLE接続

起動後、NimBLE centralがデバイス名 `nickey` を検索して接続します。接続はJust Works方式で暗号化・ボンディングされ、次のカスタムGATT CharacteristicをRead/Notify購読します。

- Service: `3a7d9f10-7d8b-4f2c-9a61-6e7e3c5b1a00`
- Characteristic: `3a7d9f11-7d8b-4f2c-9a61-6e7e3c5b1a00`
- Payload: アクティブレイヤー番号を表す符号なし1バイト

通知を受けると `layer_<番号>.png` を表示します。切断後は8秒間スキャンし、見つからない場合は12秒休止して再試行します。BLE接続前やトラブル時も本体ボタンで手動切替できます。

> ZMK v0.3.0は通常、アクティブなBLEプロファイル1台へ接続します。Nickey44をPCへBLE接続中はStickyが同時接続できない場合があります。まずPC側Bluetoothを切ってStickyとの接続・通知を確認してください。PCとStickyの完全同時利用には、今後Nickey側を接続不要の広告通知方式へ変更する必要があります。

## 省電力動作

- 表示更新後はE-Inkコントローラーと表示電源を停止します。画像は無給電でも残り、次のレイヤー通知時に自動復帰します。
- ESP-IDFの動的周波数制御により、待機中はCPUクロックを下げます。
- StickyとNickey間のBLE接続間隔を100〜150msへ延ばし、無線の起動回数を減らします。
- 最後のボタン操作またはレイヤー／電池表示更新から5分経過すると、自動でDeep-sleepへ入ります。
- AI／電源ボタンを2秒長押しするとDeep-sleepへ入ります。表示は残りますが、スリープ中はレイヤー通知を受信しません。AI／電源ボタンをもう一度押すと再起動して接続します。

Deep-sleep中もUSB接続時の充電は有効です。

## 検証

サンプル画像の形式確認:

```bash
python3 tools/validate_keymaps.py sample-keymaps
```

CIはこの検証後にファームウェアをビルドし、merged.binを生成します。本パッケージ作成時にはソース一覧・スクリプト・サンプルPNGのローカル検査まで実施しています。最終コンパイルはGitHub Actionsで確認してください。実機でのE-Ink表示・microSDカード相性・Nickey44とのBLE接続は別途確認が必要です。

## ライセンスと由来

派生コードのライセンスは `LICENSE` を参照してください。`components/` と公式デモ由来ファイルは、それぞれの原著作者・同梱ライセンスおよび上流条件に従います。PNGデコードはESP Component Registryの `espressif/libpng` をビルド時に取得します。
