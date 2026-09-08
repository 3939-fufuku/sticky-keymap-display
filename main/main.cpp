#include <cstdint>
#include <cstdio>

#include "app_event.h"
#include "board.h"
#include "canvas.h"
#include "esp_log.h"
#include "keymap_page.h"
#include "sticky_buttons.h"
#include "sticky_battery.h"
#include "sticky_display.h"
#include "sticky_sdcard.h"
#include "zmk_ble_layer_source.h"

namespace {
constexpr char kTag[] = "sticky_keymap";
constexpr uint8_t kMaxLayer = 3;

void layer_changed(uint8_t layer, void *context)
{
    (void)context;
    app_event_post_layer(layer);
}

void show_layer(Canvas &canvas, uint8_t layer, int nickey_battery)
{
    const esp_err_t load_result = keymap_page_render(canvas, layer);
    if (load_result != ESP_OK) {
        ESP_LOGW(kTag, "Showing fallback for layer %u: %s",
                 layer, esp_err_to_name(load_result));
    }
    int sticky_battery = -1;
    sticky_battery_read_percent(sticky_battery);
    char status[32] = {};
    if (nickey_battery >= 0) {
        std::snprintf(status, sizeof(status), "STK:%d%% NKY:%d%%",
                      sticky_battery, nickey_battery);
    } else {
        std::snprintf(status, sizeof(status), "STK:%d%% NKY:--",
                      sticky_battery);
    }
    canvas.fill_rect(520, 8, 272, 30, GrayLevel::White);
    canvas.draw_rect(520, 8, 272, 30, GrayLevel::Black);
    canvas.draw_text(532, 16, status, 2);
    ESP_ERROR_CHECK(sticky_display_refresh());
}
}  // namespace

extern "C" void app_main()
{
    ESP_LOGI(kTag, "Starting reTerminal Sticky SD keymap display");
    ESP_ERROR_CHECK(board_init());
    ESP_ERROR_CHECK(sticky_display_init());
    ESP_ERROR_CHECK(sticky_sdcard_init());
    ESP_ERROR_CHECK(app_event_init());
    ESP_ERROR_CHECK(sticky_buttons_init());
    ESP_ERROR_CHECK(sticky_battery_init(board_sensor_i2c_bus()));

    Canvas *canvas = sticky_display_canvas();
    ESP_ERROR_CHECK(canvas != nullptr ? ESP_OK : ESP_ERR_INVALID_STATE);

    ZmkBleLayerSource layer_source;
    ESP_ERROR_CHECK(layer_source.start(layer_changed, nullptr));
    uint8_t current_layer = layer_source.initial_layer();
    show_layer(*canvas, current_layer, layer_source.battery_percent());

    while (true) {
        AppEvent event;
        if (!app_event_wait(event, portMAX_DELAY)) continue;
        switch (event.type) {
            case AppEventType::PreviousPage:
                current_layer = current_layer == 0 ? kMaxLayer : current_layer - 1;
                show_layer(*canvas, current_layer, layer_source.battery_percent());
                break;
            case AppEventType::NextPage:
                current_layer = current_layer == kMaxLayer ? 0 : current_layer + 1;
                show_layer(*canvas, current_layer, layer_source.battery_percent());
                break;
            case AppEventType::RefreshPage:
                show_layer(*canvas, current_layer, layer_source.battery_percent());
                break;
            case AppEventType::LayerChanged:
                if (event.layer <= kMaxLayer) {
                    current_layer = event.layer;
                    show_layer(*canvas, current_layer, layer_source.battery_percent());
                }
                break;
            default:
                break;
        }
    }
}
