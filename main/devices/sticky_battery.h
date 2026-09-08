#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t sticky_battery_init(i2c_master_bus_handle_t bus);
esp_err_t sticky_battery_read_percent(int &percent);
