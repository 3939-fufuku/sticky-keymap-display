#include "sd_keymap_source.h"

#include <cstdio>

#include "canvas.h"
#include "png_decoder.h"
#include "sticky_sdcard.h"

esp_err_t sd_keymap_load(const char *keyboard_id, uint8_t layer, Canvas &canvas)
{
    esp_err_t result = sticky_sdcard_mount();
    if (result != ESP_OK) return result;

    char path[128] = {};
    std::snprintf(path, sizeof(path), "%s/keymaps/%s/layer_%u.png",
                  sticky_sdcard_mount_point(),
                  keyboard_id != nullptr ? keyboard_id : "default",
                  static_cast<unsigned>(layer));
    result = png_decode_to_canvas(path, canvas);
    if (result != ESP_OK) {
        // Preserve compatibility with existing Nickey SD cards.
        std::snprintf(path, sizeof(path), "%s/layer_%u.png",
                      sticky_sdcard_mount_point(),
                      static_cast<unsigned>(layer));
        result = png_decode_to_canvas(path, canvas);
    }
    const esp_err_t unmount_result = sticky_sdcard_unmount();
    return result == ESP_OK ? unmount_result : result;
}
