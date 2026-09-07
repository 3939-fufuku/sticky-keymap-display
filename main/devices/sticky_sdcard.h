#pragma once

#include <cstddef>

#include "esp_err.h"

// Configures card detection, power, and chip-select GPIOs. SPI2 must already
// be initialized by the display/shared bus owner.
esp_err_t sticky_sdcard_init();

// Mounts the FAT32 card at /sdcard. The display and card share SPI2; ESP-IDF
// serializes their transactions through separate chip-select lines.
esp_err_t sticky_sdcard_mount();

// Unmounts the card and powers it down. Safe to call when already unmounted.
esp_err_t sticky_sdcard_unmount();

// Stable path used by data-source modules after sticky_sdcard_mount().
const char *sticky_sdcard_mount_point();

// Mounts the card on the existing shared SPI2 bus, reads one text file, then
// unmounts it. The output is always null-terminated when buffer_size > 0.
esp_err_t sticky_sdcard_read_text(const char *file_name,
                                  char *buffer,
                                  size_t buffer_size);
