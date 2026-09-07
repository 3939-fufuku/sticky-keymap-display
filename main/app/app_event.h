#pragma once

#include <cstdint>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

enum class AppEventType {
    PreviousPage,
    NextPage,
    RefreshPage,
    OrientationChanged,
    ClockMinuteTick,
    EnterDeepSleep,
    LayerChanged,
};

struct AppEvent {
    AppEventType type = AppEventType::RefreshPage;
    uint8_t layer = 0;
};

// Small shared queue used by physical buttons and touch input.
esp_err_t app_event_init();
bool app_event_post(AppEventType type);
bool app_event_post_layer(uint8_t layer);
bool app_event_wait(AppEvent &event, TickType_t timeout);
