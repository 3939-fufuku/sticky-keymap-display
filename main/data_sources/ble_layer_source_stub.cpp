#include "ble_layer_source_stub.h"

#include "esp_log.h"

esp_err_t BleLayerSourceStub::start(LayerChangedCallback callback, void *context)
{
    (void)callback;
    (void)context;
    ESP_LOGI("layer_source", "BLE source stub active; buttons select layers");
    return ESP_OK;
}

