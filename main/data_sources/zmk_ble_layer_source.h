#pragma once

#include <cstdint>

#include "layer_source.h"

// NimBLE central that connects to the Nickey ZMK custom GATT service and
// forwards its one-byte active-layer notifications to the application.
class ZmkBleLayerSource final : public LayerSource {
public:
    esp_err_t start(LayerChangedCallback callback, void *context) override;
    uint8_t initial_layer() const override { return current_layer_; }
    int battery_percent() const { return battery_percent_; }

    void handle_layer(uint8_t layer);
    void handle_battery(uint8_t percent);

private:
    LayerChangedCallback callback_ = nullptr;
    void *context_ = nullptr;
    volatile uint8_t current_layer_ = 0;
    volatile int battery_percent_ = -1;
};
