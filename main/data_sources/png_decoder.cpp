#include "png_decoder.h"

#include <png.h>

#include <cstddef>
#include <cstdint>

#include "canvas.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

namespace {
GrayLevel quantize(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha)
{
    if (alpha < 128) {
        return GrayLevel::White;
    }
    const uint16_t luminance = static_cast<uint16_t>(red) * 77U +
                               static_cast<uint16_t>(green) * 150U +
                               static_cast<uint16_t>(blue) * 29U;
    const uint8_t value = static_cast<uint8_t>(luminance >> 8U);
    if (value < 43) return GrayLevel::Black;
    if (value < 128) return GrayLevel::DarkGray;
    if (value < 213) return GrayLevel::LightGray;
    return GrayLevel::White;
}
}  // namespace

esp_err_t png_decode_to_canvas(const char *path, Canvas &canvas)
{
    if (path == nullptr) return ESP_ERR_INVALID_ARG;

    png_image image = {};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, path)) {
        ESP_LOGE("png", "Cannot open/decode %s: %s", path, image.message);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (image.width != canvas.width() || image.height != canvas.height()) {
        ESP_LOGE("png", "%s is %lux%lu; expected %ux%u", path,
                 static_cast<unsigned long>(image.width),
                 static_cast<unsigned long>(image.height),
                 canvas.width(), canvas.height());
        png_image_free(&image);
        return ESP_ERR_INVALID_SIZE;
    }

    image.format = PNG_FORMAT_RGBA;
    const size_t byte_count = PNG_IMAGE_SIZE(image);
    auto *rgba = static_cast<uint8_t *>(
        heap_caps_malloc(byte_count, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (rgba == nullptr) {
        png_image_free(&image);
        return ESP_ERR_NO_MEM;
    }
    if (!png_image_finish_read(&image, nullptr, rgba, 0, nullptr)) {
        ESP_LOGE("png", "Decode failed for %s: %s", path, image.message);
        heap_caps_free(rgba);
        png_image_free(&image);
        return ESP_ERR_INVALID_RESPONSE;
    }

    for (uint32_t y = 0; y < image.height; ++y) {
        for (uint32_t x = 0; x < image.width; ++x) {
            const size_t offset = (static_cast<size_t>(y) * image.width + x) * 4U;
            canvas.draw_pixel(static_cast<int>(x), static_cast<int>(y),
                              quantize(rgba[offset], rgba[offset + 1],
                                       rgba[offset + 2], rgba[offset + 3]));
        }
    }
    heap_caps_free(rgba);
    png_image_free(&image);
    return ESP_OK;
}

