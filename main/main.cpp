#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"

#include "cJSON.h"

#include "bsp_axp2101.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_wifi.h"
#include "drawing_screen.h"
#include "lv_port.h"

#define EXAMPLE_DISPLAY_ROTATION LV_DISP_ROT_90
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES)

#define WEATHER_HTTP_TIMEOUT_MS 15000
#define WEATHER_HTTP_BUFFER_SIZE 6144
#define WIFI_WAIT_TIMEOUT_MS 30000

#define WEATHER_REFRESH_MS (10 * 60 * 1000)
#define WEATHER_RETRY_MS (30 * 1000)
#define SCREEN_SWITCH_MS (12 * 1000)
#define UI_TICK_MS 250

#if __has_include("wifi_local.h")
#include "wifi_local.h"
#endif

#ifndef WIFI_SSID_LOCAL
#define WIFI_SSID_LOCAL ""
#endif

#ifndef WIFI_PASS_LOCAL
#define WIFI_PASS_LOCAL ""
#endif

#ifndef WEATHER_API_KEY_LOCAL
#define WEATHER_API_KEY_LOCAL ""
#endif

#ifndef WEATHER_QUERY_LOCAL
#define WEATHER_QUERY_LOCAL "q=New York,US"
#endif

static const char *TAG = "app_main";

static esp_io_expander_handle_t expander_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;

typedef struct {
    float temp_f;
    float feels_f;
    float wind_mph;
    int humidity;
    int pressure_hpa;
    char city[48];
    char country[8];
    char condition[96];
} weather_payload_t;

typedef struct {
    drawing_screen_view_t view;
    uint8_t forecast_page;
    bool has_weather;
    char time_text[16];
    char status_text[96];
    char temp_text[24];
    char condition_text[96];
    char weather_text[192];
    char stats_line_1[64];
    char stats_line_2[64];
    char stats_line_3[64];
    char forecast_title_text[96];
    char forecast_body_text[220];
    char bottom_text[96];
    drawing_screen_dirty_t dirty;
} app_state_t;

static app_state_t g_app = {};

static bool lvgl_lock_with_retry(TickType_t timeout_ticks, int max_attempts, const char *reason)
{
    for (int attempt = 1; attempt <= max_attempts; ++attempt)
    {
        if (lvgl_port_lock(timeout_ticks))
        {
            return true;
        }
        ESP_LOGW(TAG, "LVGL lock timeout while %s (attempt %d/%d)", reason, attempt, max_attempts);
    }
    ESP_LOGE(TAG, "Failed to acquire LVGL lock while %s", reason);
    return false;
}

static void app_mark_dirty(bool header, bool main, bool stats, bool bottom)
{
    if (header)
    {
        g_app.dirty.header = true;
    }
    if (main)
    {
        g_app.dirty.main = true;
    }
    if (stats)
    {
        g_app.dirty.stats = true;
    }
    if (bottom)
    {
        g_app.dirty.bottom = true;
    }
}

static bool app_has_dirty(void)
{
    return g_app.dirty.header || g_app.dirty.main || g_app.dirty.stats || g_app.dirty.bottom;
}

static void app_render_if_dirty(void)
{
    if (!app_has_dirty())
    {
        return;
    }

    drawing_screen_data_t data = {};
    data.view = g_app.view;
    data.forecast_page = g_app.forecast_page;
    data.has_weather = g_app.has_weather;
    data.time_text = g_app.time_text;
    data.status_text = g_app.status_text;
    data.temp_text = g_app.temp_text;
    data.condition_text = g_app.condition_text;
    data.weather_text = g_app.weather_text;
    data.stats_line_1 = g_app.stats_line_1;
    data.stats_line_2 = g_app.stats_line_2;
    data.stats_line_3 = g_app.stats_line_3;
    data.forecast_title_text = g_app.forecast_title_text;
    data.forecast_body_text = g_app.forecast_body_text;
    data.bottom_text = g_app.bottom_text;

    drawing_screen_dirty_t dirty = g_app.dirty;

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 6, "rendering state"))
    {
        drawing_screen_render(&data, &dirty);
        lvgl_port_unlock();
        memset(&g_app.dirty, 0, sizeof(g_app.dirty));
    }
}

static void app_set_status_fmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_app.status_text, sizeof(g_app.status_text), fmt, args);
    va_end(args);

    ESP_LOGI(TAG, "%s", g_app.status_text);
    app_mark_dirty(true, false, false, false);
}

static void app_set_bottom_fmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_app.bottom_text, sizeof(g_app.bottom_text), fmt, args);
    va_end(args);

    app_mark_dirty(false, false, false, true);
}

static void app_update_uptime_time(uint32_t uptime_seconds)
{
    char next[16] = {0};
    uint32_t hours = (uptime_seconds / 3600U) % 24U;
    uint32_t minutes = (uptime_seconds / 60U) % 60U;
    uint32_t seconds = uptime_seconds % 60U;
    snprintf(next, sizeof(next), "%02u:%02u:%02u", (unsigned)hours, (unsigned)minutes, (unsigned)seconds);

    if (strcmp(next, g_app.time_text) != 0)
    {
        snprintf(g_app.time_text, sizeof(g_app.time_text), "%s", next);
        app_mark_dirty(true, false, false, false);
    }
}

static void app_set_screen(drawing_screen_view_t view)
{
    if (g_app.view != view)
    {
        g_app.view = view;
        app_mark_dirty(true, true, true, true);
    }
}

static void app_build_forecast_skeleton(void)
{
    snprintf(g_app.forecast_title_text, sizeof(g_app.forecast_title_text),
             "Forecast Page %u (Skeleton)", (unsigned)g_app.forecast_page + 1U);

    if (g_app.has_weather)
    {
        snprintf(g_app.forecast_body_text, sizeof(g_app.forecast_body_text),
                 "Forecast API pending.\n"
                 "Now: %.10s %.48s\n"
                 "Loc: %.56s\n"
                 "Next: 3 hourly cards + icons.",
                 g_app.temp_text, g_app.condition_text, g_app.weather_text);
    }
    else
    {
        snprintf(g_app.forecast_body_text, sizeof(g_app.forecast_body_text),
                 "No live weather yet.\n"
                 "Bring up Wi-Fi and fetch current conditions.\n"
                 "This page will become hourly forecast cards.");
    }

    app_mark_dirty(false, true, false, false);
}

static void app_apply_weather(const weather_payload_t *wx)
{
    snprintf(g_app.temp_text, sizeof(g_app.temp_text), "%d\xC2\xB0" "F", (int)lroundf(wx->temp_f));
    snprintf(g_app.condition_text, sizeof(g_app.condition_text), "%s", wx->condition);
    snprintf(g_app.weather_text, sizeof(g_app.weather_text), "%s, %s", wx->city, wx->country);

    snprintf(g_app.stats_line_1, sizeof(g_app.stats_line_1), "Feels %.0fF   Wind %.1f mph", wx->feels_f, wx->wind_mph);
    snprintf(g_app.stats_line_2, sizeof(g_app.stats_line_2), "Humidity %d%%", wx->humidity);
    snprintf(g_app.stats_line_3, sizeof(g_app.stats_line_3), "Pressure %d hPa", wx->pressure_hpa);

    g_app.has_weather = true;
    app_build_forecast_skeleton();
    app_mark_dirty(false, true, true, false);
}

static void app_state_init_defaults(void)
{
    memset(&g_app, 0, sizeof(g_app));

    g_app.view = DRAWING_SCREEN_VIEW_NOW;
    g_app.forecast_page = 0;
    g_app.has_weather = false;

    snprintf(g_app.time_text, sizeof(g_app.time_text), "00:00:00");
    snprintf(g_app.status_text, sizeof(g_app.status_text), "status: boot complete");
    snprintf(g_app.temp_text, sizeof(g_app.temp_text), "--\xC2\xB0" "F");
    snprintf(g_app.condition_text, sizeof(g_app.condition_text), "Waiting for weather");
    snprintf(g_app.weather_text, sizeof(g_app.weather_text), "Network fetch pending");
    snprintf(g_app.stats_line_1, sizeof(g_app.stats_line_1), "Feels --");
    snprintf(g_app.stats_line_2, sizeof(g_app.stats_line_2), "Humidity --");
    snprintf(g_app.stats_line_3, sizeof(g_app.stats_line_3), "Pressure --");
    snprintf(g_app.bottom_text, sizeof(g_app.bottom_text), "Landscape weather test");

    app_build_forecast_skeleton();
    app_mark_dirty(true, true, true, true);
}

static void io_expander_init(i2c_master_bus_handle_t bus_handle)
{
    ESP_ERROR_CHECK(esp_io_expander_new_i2c_tca9554(bus_handle, ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000, &expander_handle));
    ESP_ERROR_CHECK(esp_io_expander_set_dir(expander_handle, IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 0));
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_ERROR_CHECK(esp_io_expander_set_level(expander_handle, IO_EXPANDER_PIN_NUM_1, 1));
    vTaskDelay(pdMS_TO_TICKS(200));
}

static void lv_port_init_local(void)
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

static bool wait_for_wifi_ip(char *ip_out, size_t ip_out_size)
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
            app_set_bottom_fmt("ssid: %s", WIFI_SSID_LOCAL);
            app_render_if_dirty();
        }

        vTaskDelay(pdMS_TO_TICKS(poll_ms));
        waited_ms += poll_ms;
    }

    return false;
}

static esp_err_t http_get_text(const char *url, char *response_buf, size_t response_buf_size, int *status_code, int *bytes_read)
{
    if (response_buf_size == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    response_buf[0] = '\0';
    if (status_code)
    {
        *status_code = 0;
    }
    if (bytes_read)
    {
        *bytes_read = 0;
    }

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = WEATHER_HTTP_TIMEOUT_MS;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.user_agent = "waveshare-s3-weather-test/1.0";

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    if (status_code)
    {
        *status_code = esp_http_client_get_status_code(client);
    }

    int total = 0;
    while (total < (int)response_buf_size - 1)
    {
        int n = esp_http_client_read(client, response_buf + total, (int)response_buf_size - 1 - total);
        if (n < 0)
        {
            err = ESP_FAIL;
            break;
        }
        if (n == 0)
        {
            break;
        }
        total += n;
    }
    response_buf[total] = '\0';

    if (bytes_read)
    {
        *bytes_read = total;
    }

    if (total >= (int)response_buf_size - 1)
    {
        err = ESP_ERR_NO_MEM;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static bool parse_weather_json(const char *json_text, weather_payload_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL)
    {
        return false;
    }

    cJSON *main_obj = cJSON_GetObjectItemCaseSensitive(root, "main");
    cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON *wind_obj = cJSON_GetObjectItemCaseSensitive(root, "wind");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *sys_obj = cJSON_GetObjectItemCaseSensitive(root, "sys");

    cJSON *temp = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "temp") : NULL;
    cJSON *feels = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "feels_like") : NULL;
    cJSON *humidity = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "humidity") : NULL;
    cJSON *pressure = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "pressure") : NULL;

    cJSON *wind_speed = (wind_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(wind_obj, "speed") : NULL;

    cJSON *country = (sys_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(sys_obj, "country") : NULL;
    cJSON *weather0 = (weather_arr != NULL && cJSON_IsArray(weather_arr)) ? cJSON_GetArrayItem(weather_arr, 0) : NULL;
    cJSON *desc = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "description") : NULL;

    if (!cJSON_IsNumber(temp))
    {
        cJSON_Delete(root);
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->temp_f = (float)temp->valuedouble;
    out->feels_f = cJSON_IsNumber(feels) ? (float)feels->valuedouble : out->temp_f;
    out->wind_mph = cJSON_IsNumber(wind_speed) ? (float)wind_speed->valuedouble : 0.0f;
    out->humidity = cJSON_IsNumber(humidity) ? humidity->valueint : -1;
    out->pressure_hpa = cJSON_IsNumber(pressure) ? pressure->valueint : -1;

    snprintf(out->city, sizeof(out->city), "%s", cJSON_IsString(name) ? name->valuestring : "?");
    snprintf(out->country, sizeof(out->country), "%s", cJSON_IsString(country) ? country->valuestring : "");
    snprintf(out->condition, sizeof(out->condition), "%s", cJSON_IsString(desc) ? desc->valuestring : "(unknown)");

    cJSON_Delete(root);
    return true;
}

static bool weather_fetch_once(void)
{
    char url[512] = {0};
    int url_len = snprintf(url, sizeof(url),
                           "https://api.openweathermap.org/data/2.5/weather?%s&units=imperial&appid=%s",
                           WEATHER_QUERY_LOCAL, WEATHER_API_KEY_LOCAL);
    if (url_len <= 0 || url_len >= (int)sizeof(url))
    {
        app_set_status_fmt("http: url build failed");
        app_set_bottom_fmt("weather URL error");
        return false;
    }

    app_set_status_fmt("http: GET weather (%s)", WEATHER_QUERY_LOCAL);
    app_set_bottom_fmt("fetching current conditions...");
    app_render_if_dirty();

    char response[WEATHER_HTTP_BUFFER_SIZE] = {0};
    int http_status = 0;
    int http_bytes = 0;
    esp_err_t err = http_get_text(url, response, sizeof(response), &http_status, &http_bytes);

    if (err != ESP_OK)
    {
        app_set_status_fmt("http: transport error %s", esp_err_to_name(err));
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    app_set_status_fmt("http: status %d bytes %d", http_status, http_bytes);

    if (http_status != 200)
    {
        snprintf(g_app.weather_text, sizeof(g_app.weather_text), "API returned status %d", http_status);
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    weather_payload_t wx = {};
    if (!parse_weather_json(response, &wx))
    {
        app_set_status_fmt("json: parse failed");
        snprintf(g_app.weather_text, sizeof(g_app.weather_text), "weather JSON parse failed");
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
        return false;
    }

    app_apply_weather(&wx);
    app_set_status_fmt("sync: ok %s %s", wx.city, wx.country);
    app_set_bottom_fmt("next sync in %u min", (unsigned)(WEATHER_REFRESH_MS / 60000));
    return true;
}

static void weather_task(void *arg)
{
    (void)arg;

    TickType_t loop_tick = xTaskGetTickCount();
    uint32_t last_time_sec = UINT32_MAX;
    uint32_t last_screen_switch_ms = 0;
    uint32_t next_weather_sync_ms = 0;

    if (strlen(WIFI_SSID_LOCAL) == 0 || strlen(WIFI_PASS_LOCAL) == 0 || strlen(WEATHER_API_KEY_LOCAL) == 0)
    {
        app_set_status_fmt("config: missing wifi_local.h values");
        snprintf(g_app.weather_text, sizeof(g_app.weather_text),
                 "set WIFI_SSID_LOCAL, WIFI_PASS_LOCAL, WEATHER_API_KEY_LOCAL");
        app_mark_dirty(false, true, false, false);
        app_set_bottom_fmt("offline config error");
        app_render_if_dirty();
        vTaskDelete(NULL);
        return;
    }

    app_set_status_fmt("wifi: init");
    app_set_bottom_fmt("network bring-up");
    app_render_if_dirty();

    bsp_wifi_init(WIFI_SSID_LOCAL, WIFI_PASS_LOCAL);

    app_set_status_fmt("wifi: connect -> %s", WIFI_SSID_LOCAL);
    app_render_if_dirty();

    char ip[32] = {0};
    if (!wait_for_wifi_ip(ip, sizeof(ip)))
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
    app_set_bottom_fmt("online %s", WEATHER_QUERY_LOCAL);
    app_render_if_dirty();

    next_weather_sync_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
    last_screen_switch_ms = next_weather_sync_ms;

    while (true)
    {
        uint32_t now_ms = (uint32_t)xTaskGetTickCount() * portTICK_PERIOD_MS;
        uint32_t now_sec = now_ms / 1000U;

        if (now_sec != last_time_sec)
        {
            last_time_sec = now_sec;
            app_update_uptime_time(now_sec);
        }

        if ((uint32_t)(now_ms - last_screen_switch_ms) >= SCREEN_SWITCH_MS)
        {
            last_screen_switch_ms = now_ms;
            if (g_app.view == DRAWING_SCREEN_VIEW_NOW)
            {
                g_app.forecast_page = (uint8_t)((g_app.forecast_page + 1U) % 3U);
                app_set_screen(DRAWING_SCREEN_VIEW_FORECAST);
            }
            else
            {
                app_set_screen(DRAWING_SCREEN_VIEW_NOW);
            }
            app_build_forecast_skeleton();
        }

        if ((int32_t)(now_ms - next_weather_sync_ms) >= 0)
        {
            bool ok = weather_fetch_once();
            next_weather_sync_ms = now_ms + (ok ? WEATHER_REFRESH_MS : WEATHER_RETRY_MS);
        }

        app_render_if_dirty();
        vTaskDelayUntil(&loop_tick, pdMS_TO_TICKS(UI_TICK_MS));
    }
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

    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_init();

    bsp_axp2101_init(i2c_bus_handle);
    io_expander_init(i2c_bus_handle);

    bsp_display_init(&io_handle, &panel_handle, LCD_BUFFER_SIZE);
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

    ESP_LOGI(TAG, "State-driven weather UI initialized");

    xTaskCreatePinnedToCore(weather_task, "weather_task", 1024 * 12, NULL, 3, NULL, 1);

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
