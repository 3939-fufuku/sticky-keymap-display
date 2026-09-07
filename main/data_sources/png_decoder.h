#pragma once

#include "esp_err.h"

class Canvas;

// Decodes an 800x480 PNG file into Sticky's native four-level Canvas.
esp_err_t png_decode_to_canvas(const char *path, Canvas &canvas);

