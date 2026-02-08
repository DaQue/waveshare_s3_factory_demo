idf_component_register(
    SRCS "drawing_screen.c" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_bsp
        esp_lv_port
        esp_http_client
        json
        mbedtls
        espressif__esp_io_expander_tca9554
        lvgl__lvgl
    EMBED_FILES
        "assets/clear_day_128.rgb565"
)
