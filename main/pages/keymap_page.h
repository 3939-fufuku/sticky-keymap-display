#pragma once

#include <cstdint>

#include "esp_err.h"

class Canvas;

esp_err_t keymap_page_render(Canvas &canvas, uint8_t layer);

