#include "app_priv.h"
#include "driver/gpio.h"

// BOOT button on GPIO0 - LOW when pressed
#define BOOT_BUTTON_GPIO GPIO_NUM_0

static void init_boot_button(void)
{
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
}

static bool is_boot_button_pressed(void)
{
    return gpio_get_level(BOOT_BUTTON_GPIO) == 0;
}

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

    init_boot_button();
    xTaskCreatePinnedToCore(weather_task, "weather_task", 1024 * 16, NULL, 3, NULL, 1);

    ESP_LOGI(APP_TAG, "Press BOOT button anytime for config mode");

    while (true)
    {
        // Check BOOT button - enter config mode if pressed
        if (is_boot_button_pressed())
        {
            ESP_LOGI(APP_TAG, "BOOT button pressed - entering config mode");
            app_config_interactive_console();
            ESP_LOGI(APP_TAG, "Exited config mode - resuming normal operation");
        }
        vTaskDelay(pdMS_TO_TICKS(200));  // Check 5 times per second
    }
}
