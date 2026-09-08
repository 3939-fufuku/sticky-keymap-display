#include "sticky_battery.h"

#include <algorithm>

#include "pin_config.h"

namespace {
i2c_master_dev_handle_t s_device = nullptr;
constexpr uint8_t kStateOfChargeRegister = 0x2c;
}

esp_err_t sticky_battery_init(i2c_master_bus_handle_t bus)
{
    if (bus == nullptr) return ESP_ERR_INVALID_ARG;
    i2c_device_config_t config = {};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = BQ27220_I2C_ADDR;
    config.scl_speed_hz = 400000;
    return i2c_master_bus_add_device(bus, &config, &s_device);
}

esp_err_t sticky_battery_read_percent(int &percent)
{
    if (s_device == nullptr) return ESP_ERR_INVALID_STATE;
    uint8_t value[2] = {};
    esp_err_t result = i2c_master_transmit_receive(
        s_device, &kStateOfChargeRegister, 1, value, sizeof(value), 100);
    if (result == ESP_OK) {
        percent = std::clamp<int>(value[0] | (value[1] << 8), 0, 100);
    }
    return result;
}
