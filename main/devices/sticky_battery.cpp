#include "sticky_battery.h"

#include <algorithm>

#include "driver/gpio.h"
#include "pin_config.h"

namespace {
i2c_master_dev_handle_t s_device = nullptr;
constexpr uint8_t kStateOfChargeRegister = 0x2c;
}

esp_err_t sticky_battery_init(i2c_master_bus_handle_t bus)
{
    if (bus == nullptr) return ESP_ERR_INVALID_ARG;

    // The BQ25616 charger enable input is active-low. Without this setup the
    // USB supply can power Sticky while the battery itself remains uncharged.
    gpio_config_t charger_config = {};
    charger_config.pin_bit_mask = 1ULL << PIN_BAT_CHG_EN;
    charger_config.mode = GPIO_MODE_OUTPUT;
    charger_config.pull_up_en = GPIO_PULLUP_DISABLE;
    charger_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    charger_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t result = gpio_config(&charger_config);
    if (result != ESP_OK) return result;
    result = gpio_set_level(static_cast<gpio_num_t>(PIN_BAT_CHG_EN), 0);
    if (result != ESP_OK) return result;

    gpio_config_t power_detect_config = {};
    power_detect_config.pin_bit_mask = 1ULL << PIN_EXTERNAL_POWER;
    power_detect_config.mode = GPIO_MODE_INPUT;
    power_detect_config.pull_up_en = GPIO_PULLUP_DISABLE;
    power_detect_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    power_detect_config.intr_type = GPIO_INTR_DISABLE;
    result = gpio_config(&power_detect_config);
    if (result != ESP_OK) return result;

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
