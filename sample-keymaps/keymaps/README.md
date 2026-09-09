# Multi-keyboard SD images

Copy the `keymaps` directory itself to the root of the Sticky microSD card.
Each directory name matches the keyboard ID reported over BLE.

- `mona2`: 7 layers generated from `zmk-config-moNa2-v2/keymap-drawer/mona2_01.svg`
- `roba`: 7 layers generated from `zmk-config-roBa/keymap-drawer/roBa.svg`
- `litom`: 7 layers generated from `LiTom/keymap-svg/LiTom.svg`
- `torabo_tsuki_lp`: 5 layers generated from `zmk-keyboard-torabo-tsuki-lp/config/keymap.keymap`

No standalone logo assets were present in these four repositories, so the
images use a compact text wordmark in the header and do not invent a logo.
All output PNG files are 800 x 480 and use a high-contrast four-level grayscale
palette for the reTerminal Sticky E-Ink display.
