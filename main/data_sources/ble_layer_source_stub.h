#pragma once

#include "layer_source.h"

class BleLayerSourceStub final : public LayerSource {
public:
    esp_err_t start(LayerChangedCallback callback, void *context) override;
    uint8_t initial_layer() const override { return 0; }
};

