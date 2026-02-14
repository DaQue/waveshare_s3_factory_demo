#include "app_priv.h"

extern "C" void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    app_config_load_from_nvs();

    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_init();
    g_i2c_bus_handle = i2c_bus_handle;

    bsp_axp2101_init(i2c_bus_handle);
    io_expander_init(i2c_bus_handle);

    bsp_display_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
    uint16_t touch_w = EXAMPLE_LCD_H_RES;
    uint16_t touch_h = EXAMPLE_LCD_V_RES;
    if (EXAMPLE_DISPLAY_ROTATION != LV_DISP_ROT_180 && EXAMPLE_DISPLAY_ROTATION != LV_DISP_ROT_NONE)
    {
        touch_w = EXAMPLE_LCD_V_RES;
        touch_h = EXAMPLE_LCD_H_RES;
    }
    bsp_touch_init(i2c_bus_handle, touch_w, touch_h, display_rotation_to_touch_rotation(EXAMPLE_DISPLAY_ROTATION));

    // Give sensor rail time to settle and retry BME280 init to avoid sporadic boot-time misses.
    esp_err_t bme_err = ESP_FAIL;
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        bme_err = bsp_bme280_init(i2c_bus_handle);
        if (bme_err == ESP_OK)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }
    if (bme_err == ESP_OK)
    {
        ESP_LOGI(APP_TAG, "Indoor sensor ready (BME280)");
    }
    else
    {
        ESP_LOGW(APP_TAG, "Indoor sensor not found: %s", esp_err_to_name(bme_err));
    }

    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

    lv_port_init_local();

    app_state_init_defaults();

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 8, "initializing drawing screen"))
    {
        drawing_screen_init();
        lvgl_port_unlock();
    }
    app_render_if_dirty();

    ESP_LOGI(APP_TAG, "State-driven weather UI initialized");

    app_config_boot_console_window(8000);
    xTaskCreatePinnedToCore(weather_task, "weather_task", 1024 * 16, NULL, 3, NULL, 1);

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
