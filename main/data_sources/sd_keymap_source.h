#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

// Loads /sdcard/layer_<n>.png. Mount/decode/unmount is one transaction so the
// shared SPI bus is released before the e-paper refresh starts.
esp_err_t sd_keymap_load(uint8_t layer, Canvas &canvas);

