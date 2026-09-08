idf_component_register(
    SRCS
        "main.cpp"
        "app/app_event.cpp"
        "board/board.cpp"
        "data_sources/zmk_ble_layer_source.cpp"
        "data_sources/png_decoder.cpp"
        "data_sources/sd_keymap_source.cpp"
        "devices/sticky_buttons.cpp"
        "devices/sticky_battery.cpp"
        "devices/sticky_display.cpp"
        "devices/sticky_sdcard.cpp"
        "pages/keymap_page.cpp"
        "ui/canvas.cpp"
        "ui/font.cpp"
    INCLUDE_DIRS "." "app" "board" "data_sources" "devices" "pages" "ui"
    REQUIRES
        bt nvs_flash
        esp_driver_gpio esp_driver_i2c esp_driver_spi esp_hw_support
        button fatfs libpng sdmmc seeed_epaper
)
