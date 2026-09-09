#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

// Loads /sdcard/keymaps/<keyboard_id>/layer_<n>.png and falls back to the
// legacy /sdcard/layer_<n>.png path. Mount/decode/unmount is one transaction.
esp_err_t sd_keymap_load(const char *keyboard_id, uint8_t layer, Canvas &canvas);
