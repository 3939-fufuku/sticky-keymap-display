#include "sticky_sdcard.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pin_config.h"
#include "sdmmc_cmd.h"

namespace {

constexpr char kTag[] = "sticky_sdcard";
constexpr char kMountPoint[] = "/sdcard";
constexpr TickType_t kPowerOnDelay = pdMS_TO_TICKS(100);
constexpr TickType_t kPowerOffDelay = pdMS_TO_TICKS(20);

bool s_initialized = false;
sdmmc_card_t *s_card = nullptr;

esp_err_t set_card_power(bool enabled)
{
    return gpio_set_level(
        static_cast<gpio_num_t>(PIN_SD_EN), enabled ? 1 : 0);
}

esp_err_t restore_shared_spi_idle()
{
    // esp_vfs_fat_sdcard_unmount() removes the SDSPI device and leaves its CS
    // pin configured as an input.  With a card inserted, a floating CS can let
    // the card react to the following e-paper transactions on the shared bus.
    // Power the card down, then explicitly drive CS high before the display
    // takes ownership of SPI2 again.
    esp_err_t result = set_card_power(false);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(kPowerOffDelay);

    gpio_config_t cs_config = {};
    cs_config.pin_bit_mask = 1ULL << PIN_SD_CS;
    cs_config.mode = GPIO_MODE_OUTPUT;
    cs_config.pull_up_en = GPIO_PULLUP_DISABLE;
    cs_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cs_config.intr_type = GPIO_INTR_DISABLE;
    result = gpio_config(&cs_config);
    if (result != ESP_OK) {
        return result;
    }
    return gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
}

bool card_is_inserted()
{
    return gpio_get_level(static_cast<gpio_num_t>(PIN_SD_DETECT)) == 0;
}

esp_err_t read_file(const char *path, char *buffer, size_t buffer_size)
{
    FILE *file = std::fopen(path, "rb");
    if (file == nullptr) {
        ESP_LOGE(kTag, "Open %s failed: errno=%d (%s)",
                 path, errno, std::strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }

    const size_t bytes_read =
        std::fread(buffer, 1, buffer_size - 1U, file);
    const bool read_failed = std::ferror(file) != 0;
    std::fclose(file);
    buffer[bytes_read] = '\0';

    if (read_failed) {
        ESP_LOGE(kTag, "Read %s failed", path);
        return ESP_FAIL;
    }

    ESP_LOGI(kTag, "Read %u bytes from %s: %s",
             static_cast<unsigned>(bytes_read), path, buffer);
    return ESP_OK;
}

}  // namespace

esp_err_t sticky_sdcard_init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_config_t output_config = {};
    output_config.pin_bit_mask = (1ULL << PIN_SD_EN) | (1ULL << PIN_SD_CS);
    output_config.mode = GPIO_MODE_OUTPUT;
    output_config.pull_up_en = GPIO_PULLUP_DISABLE;
    output_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output_config.intr_type = GPIO_INTR_DISABLE;
    esp_err_t result = gpio_config(&output_config);
    if (result != ESP_OK) {
        return result;
    }

    result = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
    if (result != ESP_OK) {
        return result;
    }
    result = set_card_power(false);
    if (result != ESP_OK) {
        return result;
    }

    gpio_config_t detect_config = {};
    detect_config.pin_bit_mask = 1ULL << PIN_SD_DETECT;
    detect_config.mode = GPIO_MODE_INPUT;
    detect_config.pull_up_en = GPIO_PULLUP_ENABLE;
    detect_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    detect_config.intr_type = GPIO_INTR_DISABLE;
    result = gpio_config(&detect_config);
    if (result == ESP_OK) {
        s_initialized = true;
        ESP_LOGI(kTag, "Interface ready on shared SPI2");
    }
    return result;
}

esp_err_t sticky_sdcard_read_text(const char *file_name,
                                  char *buffer,
                                  size_t buffer_size)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (file_name == nullptr || buffer == nullptr || buffer_size < 2U) {
        return ESP_ERR_INVALID_ARG;
    }
    buffer[0] = '\0';

    if (!card_is_inserted()) {
        set_card_power(false);
        ESP_LOGW(kTag, "No MicroSD card detected");
        return ESP_ERR_NOT_FOUND;
    }

    // Both devices stay registered on SPI2. Their separate CS pins let the
    // ESP-IDF SPI bus driver serialize SD access and e-paper access safely.
    esp_err_t result = gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1);
    if (result != ESP_OK) {
        return result;
    }
    result = set_card_power(true);
    if (result != ESP_OK) {
        return result;
    }
    vTaskDelay(kPowerOnDelay);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
    slot_config.host_id = SPI2_HOST;
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 2;
    mount_config.allocation_unit_size = 16 * 1024;

    sdmmc_card_t *card = nullptr;
    result = esp_vfs_fat_sdspi_mount(
        kMountPoint, &host, &slot_config, &mount_config, &card);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "MicroSD mount failed: %s", esp_err_to_name(result));
        restore_shared_spi_idle();
        return result;
    }

    char path[128] = {};
    const int path_length =
        std::snprintf(path, sizeof(path), "%s/%s", kMountPoint, file_name);
    if (path_length <= 0 || static_cast<size_t>(path_length) >= sizeof(path)) {
        result = ESP_ERR_INVALID_SIZE;
    } else {
        result = read_file(path, buffer, buffer_size);
    }

    const esp_err_t unmount_result =
        esp_vfs_fat_sdcard_unmount(kMountPoint, card);
    if (result == ESP_OK && unmount_result != ESP_OK) {
        result = unmount_result;
    }

    const esp_err_t idle_result = restore_shared_spi_idle();
    if (result == ESP_OK && idle_result != ESP_OK) {
        result = idle_result;
    }
    ESP_LOGI(kTag, "MicroSD unmounted; shared SPI2 is ready for display refresh");
    return result;
}

esp_err_t sticky_sdcard_mount()
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_card != nullptr) {
        return ESP_OK;
    }
    if (!card_is_inserted()) {
        set_card_power(false);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(
        gpio_set_level(static_cast<gpio_num_t>(PIN_SD_CS), 1),
        kTag, "deselect card");
    ESP_RETURN_ON_ERROR(set_card_power(true), kTag, "power card");
    vTaskDelay(kPowerOnDelay);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = static_cast<gpio_num_t>(PIN_SD_CS);
    slot_config.host_id = SPI2_HOST;
    slot_config.gpio_cd = GPIO_NUM_NC;
    slot_config.gpio_wp = GPIO_NUM_NC;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = 3;
    mount_config.allocation_unit_size = 16 * 1024;

    const esp_err_t result = esp_vfs_fat_sdspi_mount(
        kMountPoint, &host, &slot_config, &mount_config, &s_card);
    if (result != ESP_OK) {
        s_card = nullptr;
        restore_shared_spi_idle();
    }
    return result;
}

esp_err_t sticky_sdcard_unmount()
{
    if (s_card == nullptr) {
        return ESP_OK;
    }
    const esp_err_t result = esp_vfs_fat_sdcard_unmount(kMountPoint, s_card);
    s_card = nullptr;
    const esp_err_t idle_result = restore_shared_spi_idle();
    return result == ESP_OK ? idle_result : result;
}

const char *sticky_sdcard_mount_point()
{
    return kMountPoint;
}
