#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t keymap_page_render(Canvas &canvas, const char *keyboard_id,
                             uint8_t layer);
