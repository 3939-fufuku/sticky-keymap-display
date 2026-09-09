#include "keymap_page.h"

#include <cstdio>

#include "canvas.h"
#include "sd_keymap_source.h"

esp_err_t keymap_page_render(Canvas &canvas, const char *keyboard_id,
                             uint8_t layer)
{
    const esp_err_t result = sd_keymap_load(keyboard_id, layer, canvas);
    if (result == ESP_OK) return ESP_OK;

    canvas.clear();
    canvas.draw_rect(24, 24, 752, 432, GrayLevel::Black);
    canvas.draw_text(64, 92, "KEYMAP IMAGE NOT AVAILABLE", 3);
    char message[80] = {};
    std::snprintf(message, sizeof(message),
                  "Missing: keymaps/%s/layer_%u.png",
                  keyboard_id != nullptr ? keyboard_id : "default",
                  static_cast<unsigned>(layer));
    canvas.draw_text(64, 190, message, 2);
    std::snprintf(message, sizeof(message), "Error: %s", esp_err_to_name(result));
    canvas.draw_text(64, 250, message, 2);
    canvas.draw_text(64, 344, "Required size: 800 x 480 PNG", 2);
    return result;
}
