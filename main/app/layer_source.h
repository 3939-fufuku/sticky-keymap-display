#pragma once

#include <cstdint>

#include "esp_err.h"

using LayerChangedCallback = void (*)(uint8_t layer, void *context);

// Transport-neutral producer of ZMK layer changes. The shipped implementation
// is a no-op stub; a future BLE client only needs to implement this contract.
class LayerSource {
public:
    virtual ~LayerSource() = default;
    virtual esp_err_t start(LayerChangedCallback callback, void *context) = 0;
    virtual uint8_t initial_layer() const = 0;
};

