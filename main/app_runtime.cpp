#include "app_priv.h"

void app_run_i2c_scan(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL)
    {
        snprintf(g_app.i2c_scan_text, sizeof(g_app.i2c_scan_text), "I2C bus not initialized");
        app_mark_dirty(false, true, false, false);
        return;
    }

    char text[640] = {0};
    char found_line[192] = {0};
    size_t used = 0;
    size_t found_used = 0;
    int found_count = 0;
    bool found_bme_addr = false;

    used += snprintf(text + used, sizeof(text) - used,
                     "I2C Scan (0x03-0x77)\n"
                     "SDA=%d SCL=%d\n",
                     (int)EXAMPLE_PIN_I2C_SDA, (int)EXAMPLE_PIN_I2C_SCL);

    for (uint8_t addr = 0x03; addr <= 0x77; ++addr)
    {
        esp_err_t ret = ESP_FAIL;
        if (bsp_i2c_lock(50))
        {
            ret = i2c_master_probe(bus_handle, addr, 20);
            bsp_i2c_unlock();
        }

        if (ret == ESP_OK)
        {
            if (found_count == 0)
            {
                used += snprintf(text + used, sizeof(text) - used, "Found:\n");
            }
            if (found_count > 0 && (found_count % 8) == 0)
            {
                used += snprintf(text + used, sizeof(text) - used, "\n");
            }
            used += snprintf(text + used, sizeof(text) - used, "0x%02X ", addr);
            if (found_used + 6 < sizeof(found_line))
            {
                found_used += snprintf(found_line + found_used, sizeof(found_line) - found_used, "0x%02X ", addr);
            }
            found_count++;
            if (addr == 0x76 || addr == 0x77)
            {
                found_bme_addr = true;
            }
        }
    }

    if (found_count == 0)
    {
        snprintf(text, sizeof(text),
                 "I2C Scan (0x03-0x77)\n"
                 "SDA=%d SCL=%d\n"
                 "No devices found.\n\n"
                 "Check sensor power, GND, SDA, SCL.\n"
                 "BME280 should appear at 0x76 or 0x77.",
                 (int)EXAMPLE_PIN_I2C_SDA, (int)EXAMPLE_PIN_I2C_SCL);
    }
    else
    {
        (void)snprintf(text + used, sizeof(text) - used,
                       "\n\nTotal: %d\nBME280 addr: %s\nDriver: %s",
                       found_count,
                       found_bme_addr ? "present" : "missing",
                       bsp_bme280_is_available() ? "initialized" : "not initialized");
    }

    ESP_LOGI(APP_TAG, "i2c scan: found=%d bme_addr=%s driver=%s addrs=[%s]",
             found_count,
             found_bme_addr ? "yes" : "no",
             bsp_bme280_is_available() ? "ready" : "not-ready",
             (found_line[0] != '\0') ? found_line : "(none)");

    snprintf(g_app.i2c_scan_text, sizeof(g_app.i2c_scan_text), "%s", text);
    app_mark_dirty(false, true, false, false);
}

static const char *wifi_auth_mode_name(wifi_auth_mode_t authmode)
{
    switch (authmode)
    {
    case WIFI_AUTH_OPEN:
        return "Open";
    case WIFI_AUTH_WEP:
        return "WEP";
    case WIFI_AUTH_WPA_PSK:
        return "WPA";
    case WIFI_AUTH_WPA2_PSK:
        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "WPA/WPA2";
    case WIFI_AUTH_WPA3_PSK:
        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "WPA2/WPA3";
    case WIFI_AUTH_OWE:
        return "OWE";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "WPA2-ENT";
    default:
        break;
    }
    return "?";
}

void app_run_wifi_scan(void)
{
    wifi_ap_record_t ap_info[APP_WIFI_SCAN_MAX_APS] = {};
    uint16_t ap_count = 0;

    bool ok = bsp_wifi_scan(ap_info, &ap_count, APP_WIFI_SCAN_MAX_APS);
    if (!ok)
    {
        snprintf(g_app.wifi_scan_text, sizeof(g_app.wifi_scan_text),
                 "Wi-Fi scan failed or timed out.\n"
                 "Make sure station mode is initialized.");
        app_mark_dirty(false, true, false, false);
        return;
    }

    char text[1024] = {0};
    size_t used = 0;
    uint16_t shown = (ap_count > APP_WIFI_SCAN_VISIBLE_APS) ? APP_WIFI_SCAN_VISIBLE_APS : ap_count;
    used += snprintf(text + used, sizeof(text) - used, "Found %u APs\n", (unsigned)ap_count);

    if (shown == 0)
    {
        (void)snprintf(text + used, sizeof(text) - used, "No networks in range.");
    }
    else
    {
        for (uint16_t i = 0; i < shown && used < sizeof(text); ++i)
        {
            const wifi_ap_record_t *ap = &ap_info[i];
            const char *ssid = ((const char *)ap->ssid)[0] != '\0' ? (const char *)ap->ssid : "<hidden>";
            used += snprintf(text + used, sizeof(text) - used,
                             "%u) %.16s  %d dBm  ch%u  %s\n",
                             (unsigned)(i + 1),
                             ssid,
                             (int)ap->rssi,
                             (unsigned)ap->primary,
                             wifi_auth_mode_name(ap->authmode));
        }
        if (ap_count > shown && used < sizeof(text))
        {
            (void)snprintf(text + used, sizeof(text) - used, "...and %u more",
                           (unsigned)(ap_count - shown));
        }
    }

    snprintf(g_app.wifi_scan_text, sizeof(g_app.wifi_scan_text), "%s", text);
    app_mark_dirty(false, true, false, false);
}

void io_expander_init(i2c_master_bus_handle_t bus_handle)
{
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander_handle));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(200));
}

void lv_port_init_local(void)
{
    lvgl_port_cfg_t port_cfg = {};
    port_cfg.task_priority = 4;
    port_cfg.task_stack = 1024 * 5;
    port_cfg.task_affinity = 1;
    port_cfg.task_max_sleep_ms = 500;
    port_cfg.timer_period_ms = 5;
    lvgl_port_init(&port_cfg);

    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = io_handle;
    disp_cfg.panel_handle = panel_handle;
    disp_cfg.buffer_size = LCD_BUFFER_SIZE;
    disp_cfg.sw_rotate = EXAMPLE_DISPLAY_ROTATION;
    disp_cfg.hres = EXAMPLE_LCD_H_RES;
    disp_cfg.vres = EXAMPLE_LCD_V_RES;
    disp_cfg.trans_size = LCD_BUFFER_SIZE / 10;
    disp_cfg.draw_wait_cb = NULL;
    disp_cfg.flags.buff_dma = false;
    disp_cfg.flags.buff_spiram = true;

    if (disp_cfg.sw_rotate == LV_DISP_ROT_180 || disp_cfg.sw_rotate == LV_DISP_ROT_NONE)
    {
        disp_cfg.hres = EXAMPLE_LCD_H_RES;
        disp_cfg.vres = EXAMPLE_LCD_V_RES;
    }
    else
    {
        disp_cfg.hres = EXAMPLE_LCD_V_RES;
        disp_cfg.vres = EXAMPLE_LCD_H_RES;
    }

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    (void)lvgl_disp;
}

bool wait_for_wifi_ip(const char *ssid, char *ip_out, size_t ip_out_size)
{
    const int poll_ms = 500;
    int waited_ms = 0;

    while (waited_ms < WIFI_WAIT_TIMEOUT_MS)
    {
        char ip[32] = {0};
        bsp_wifi_get_ip(ip);
        if (ip[0] != '\0' && strcmp(ip, "0.0.0.0") != 0)
        {
            snprintf(ip_out, ip_out_size, "%s", ip);
            return true;
        }

        if ((waited_ms % 5000) == 0)
        {
            app_set_status_fmt("wifi: connecting... %d s", waited_ms / 1000);
            app_set_bottom_fmt("ssid: %s", (ssid != NULL && ssid[0] != '\0') ? ssid : "(unset)");
            app_render_if_dirty();
        }

        vTaskDelay(pdMS_TO_TICKS(poll_ms));
        waited_ms += poll_ms;
    }

    return false;
}

void weather_task(void *arg)
{
    (void)arg;

    TickType_t loop_tick = xTaskGetTickCount();
    uint32_t last_time_sec = UINT32_MAX;
    uint32_t next_weather_sync_ms = 0;
    uint32_t next_indoor_sample_ms = 0;
    uint32_t next_i2c_scan_ms = 0;
    uint32_t next_wifi_scan_ms = 0;

    const char *wifi_ssid = app_config_wifi_ssid();
    const char *wifi_pass = app_config_wifi_pass();

    if (wifi_ssid == NULL || wifi_pass == NULL || strlen(wifi_ssid) == 0 || strlen(WEATHER_API_KEY_LOCAL) == 0)
    {
        app_set_status_fmt("config: missing Wi-Fi or API key");
        snprintf(g_app.weather_text, sizeof(g_app.weather_text),
                 "set WEATHER_API_KEY_LOCAL and Wi-Fi credentials");
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("offline config error");
        app_render_if_dirty();
        vTaskDelete(NULL);
        return;
    }

    app_set_status_fmt("wifi: init");
    app_set_bottom_fmt("network bring-up");
    app_render_if_dirty();

    bsp_wifi_init(wifi_ssid, wifi_pass);

    app_set_status_fmt("wifi: connect -> %s", wifi_ssid);
    app_render_if_dirty();

    char ip[32] = {0};
    if (!wait_for_wifi_ip(wifi_ssid, ip, sizeof(ip)))
    {
        app_set_status_fmt("wifi: timeout waiting for IP");
        snprintf(g_app.weather_text, sizeof(g_app.weather_text), "weather skipped (no network)");
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("offline timeout");
        app_render_if_dirty();
        vTaskDelete(NULL);
        return;
    }

    app_set_status_fmt("wifi: connected ip %s", ip);
    app_set_bottom_fmt("online %s (%s)",
                       WEATHER_QUERY_LOCAL,
                       app_config_wifi_override_active() ? "saved Wi-Fi" : "default Wi-Fi");
    app_render_if_dirty();

    g_wifi_connected = true;
    g_wifi_connected_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    app_update_connect_time(g_wifi_connected_ms);

    bool ntp_synced = app_sync_time_with_ntp();
    app_update_local_time();
    app_set_bottom_fmt("%s | %s",
                       ntp_synced ? "time: synced" : "time: pending",
                       WEATHER_QUERY_LOCAL);
    app_render_if_dirty();

    next_weather_sync_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    next_indoor_sample_ms = next_weather_sync_ms;
    next_i2c_scan_ms = next_weather_sync_ms;
    next_wifi_scan_ms = next_weather_sync_ms;

    while (true)
    {
        uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t now_sec = now_ms / 1000U;

        if (now_sec != last_time_sec)
        {
            last_time_sec = now_sec;
            app_update_connect_time(now_ms);
            app_update_local_time();
        }

        if ((int32_t)(now_ms - next_indoor_sample_ms) >= 0)
        {
            if (!bsp_bme280_is_available())
            {
                // Startup does multi-attempt init. Avoid repeated runtime re-init loops:
                // they can wedge I2C if the sensor/bus is not healthy.
                app_set_indoor_placeholders();
                app_mark_dirty(false, true, true, false);
                next_indoor_sample_ms = now_ms + 30000;
            }
            else
            {
                bsp_bme280_data_t indoor = {};
                bool indoor_ok = false;
                for (int attempt = 0; attempt < 2; ++attempt)
                {
                    if (bsp_bme280_read(&indoor) == ESP_OK)
                    {
                        indoor_ok = true;
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(30));
                }

                if (indoor_ok)
                {
                    app_apply_indoor_data(&indoor);
                    next_indoor_sample_ms = now_ms + BME280_REFRESH_MS;
                }
                else
                {
                    app_set_indoor_placeholders();
                    app_mark_dirty(false, true, true, false);
                    next_indoor_sample_ms = now_ms + BME280_RETRY_MS;
                }
            }
        }

        if (g_app.view == DRAWING_SCREEN_VIEW_I2C_SCAN && (int32_t)(now_ms - next_i2c_scan_ms) >= 0)
        {
            app_run_i2c_scan(g_i2c_bus_handle);
            next_i2c_scan_ms = now_ms + I2C_SCAN_REFRESH_MS;
        }

        if (g_app.view == DRAWING_SCREEN_VIEW_WIFI_SCAN && (int32_t)(now_ms - next_wifi_scan_ms) >= 0)
        {
            app_run_wifi_scan();
            next_wifi_scan_ms = now_ms + WIFI_SCAN_REFRESH_MS;
        }

        if ((int32_t)(now_ms - next_weather_sync_ms) >= 0)
        {
            char local_time[16] = {0};
            if (!app_format_local_time(local_time, sizeof(local_time)))
            {
                app_set_status_fmt("time: waiting for NTP");
                app_set_bottom_fmt("HTTPS blocked until clock sync");
                next_weather_sync_ms = now_ms + 10000;
            }
            else
            {
                bool ok = weather_fetch_once();
                next_weather_sync_ms = now_ms + (ok ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS);
            }
        }

        app_poll_touch_swipe(now_ms);

        app_render_if_dirty();
        vTaskDelayUntil(&loop_tick, pdMS_TO_TICKS(UI_TICK_MS));
    }
}
