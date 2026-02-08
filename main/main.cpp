#include <math.h>
#include <stdarg.h>
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
#define WEATHER_HTTP_BUFFER_SIZE 4096
#define WIFI_WAIT_TIMEOUT_MS 30000

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

static void ui_set_status(const char *fmt, ...)
{
    char text[192] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    ESP_LOGI(TAG, "%s", text);
    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 6, "updating status"))
    {
        drawing_screen_set_status(text);
        lvgl_port_unlock();
    }
}

static void ui_set_weather(const char *fmt, ...)
{
    char text[320] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 6, "updating weather text"))
    {
        drawing_screen_set_weather_text(text);
        lvgl_port_unlock();
    }
}

static void ui_set_temp_f(float temp_f)
{
    char temp_text[24] = {0};
    snprintf(temp_text, sizeof(temp_text), "%d\xC2\xB0""F", (int)lroundf(temp_f));

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 6, "updating temp text"))
    {
        drawing_screen_set_temp_text(temp_text);
        lvgl_port_unlock();
    }
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
            ui_set_status("wifi: connecting... %d s", waited_ms / 1000);
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

static bool parse_weather_json(const char *json_text, char *weather_text, size_t weather_text_size, float *temp_f_out)
{
    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL)
    {
        return false;
    }

    cJSON *main_obj = cJSON_GetObjectItemCaseSensitive(root, "main");
    cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(root, "weather");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *sys_obj = cJSON_GetObjectItemCaseSensitive(root, "sys");

    cJSON *temp = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "temp") : NULL;
    cJSON *feels = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "feels_like") : NULL;
    cJSON *humidity = (main_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(main_obj, "humidity") : NULL;

    cJSON *country = (sys_obj != NULL) ? cJSON_GetObjectItemCaseSensitive(sys_obj, "country") : NULL;
    cJSON *weather0 = (weather_arr != NULL && cJSON_IsArray(weather_arr)) ? cJSON_GetArrayItem(weather_arr, 0) : NULL;
    cJSON *desc = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "description") : NULL;

    if (!cJSON_IsNumber(temp))
    {
        cJSON_Delete(root);
        return false;
    }

    float temp_f = (float)temp->valuedouble;
    float feels_f = cJSON_IsNumber(feels) ? (float)feels->valuedouble : temp_f;
    int hum = cJSON_IsNumber(humidity) ? humidity->valueint : -1;

    const char *city = cJSON_IsString(name) ? name->valuestring : "?";
    const char *country_code = cJSON_IsString(country) ? country->valuestring : "";
    const char *cond = cJSON_IsString(desc) ? desc->valuestring : "(no condition)";

    if (hum >= 0)
    {
        snprintf(weather_text, weather_text_size,
                 "weather: %s %s\ncond: %s\ntemp: %.1fF feels %.1fF hum:%d%%",
                 city, country_code, cond, temp_f, feels_f, hum);
    }
    else
    {
        snprintf(weather_text, weather_text_size,
                 "weather: %s %s\ncond: %s\ntemp: %.1fF feels %.1fF",
                 city, country_code, cond, temp_f, feels_f);
    }

    if (temp_f_out)
    {
        *temp_f_out = temp_f;
    }

    cJSON_Delete(root);
    return true;
}

static void weather_task(void *arg)
{
    (void)arg;

    if (strlen(WIFI_SSID_LOCAL) == 0 || strlen(WIFI_PASS_LOCAL) == 0 || strlen(WEATHER_API_KEY_LOCAL) == 0)
    {
        ui_set_status("config: missing wifi_local.h values");
        ui_set_weather("weather: set WIFI_SSID_LOCAL, WIFI_PASS_LOCAL, WEATHER_API_KEY_LOCAL");
        vTaskDelete(NULL);
        return;
    }

    ui_set_status("wifi: init");
    bsp_wifi_init(WIFI_SSID_LOCAL, WIFI_PASS_LOCAL);
    ui_set_status("wifi: connect -> %s", WIFI_SSID_LOCAL);

    char ip[32] = {0};
    if (!wait_for_wifi_ip(ip, sizeof(ip)))
    {
        ui_set_status("wifi: timeout waiting for IP");
        ui_set_weather("weather: skipped (no network)");
        vTaskDelete(NULL);
        return;
    }

    ui_set_status("wifi: connected ip %s", ip);
    ui_set_weather("weather: requesting...");

    char url[512] = {0};
    int url_len = snprintf(url, sizeof(url),
                           "https://api.openweathermap.org/data/2.5/weather?%s&units=imperial&appid=%s",
                           WEATHER_QUERY_LOCAL, WEATHER_API_KEY_LOCAL);
    if (url_len <= 0 || url_len >= (int)sizeof(url))
    {
        ui_set_status("http: url build failed");
        ui_set_weather("weather: internal URL error");
        vTaskDelete(NULL);
        return;
    }

    ui_set_status("http: GET weather (%s)", WEATHER_QUERY_LOCAL);

    char response[WEATHER_HTTP_BUFFER_SIZE] = {0};
    int http_status = 0;
    int http_bytes = 0;
    esp_err_t err = http_get_text(url, response, sizeof(response), &http_status, &http_bytes);

    if (err != ESP_OK)
    {
        ui_set_status("http: transport error %s", esp_err_to_name(err));
        ui_set_weather("weather: transport failed");
        vTaskDelete(NULL);
        return;
    }

    ui_set_status("http: status %d bytes %d", http_status, http_bytes);
    if (http_status != 200)
    {
        ui_set_weather("weather: API status %d", http_status);
        vTaskDelete(NULL);
        return;
    }

    char weather_text[320] = {0};
    float temp_f = 0.0f;
    if (!parse_weather_json(response, weather_text, sizeof(weather_text), &temp_f))
    {
        ui_set_status("json: parse failed");
        ui_set_weather("weather: JSON parse failed");
        vTaskDelete(NULL);
        return;
    }

    ui_set_weather("%s", weather_text);
    ui_set_temp_f(temp_f);
    ui_set_status("done: weather updated from network");

    vTaskDelete(NULL);
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

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 8, "initializing drawing screen"))
    {
        drawing_screen_init();
        drawing_screen_set_status("status: boot complete");
        drawing_screen_set_weather_text("weather: waiting for WiFi...");
        ESP_LOGI(TAG, "Drawing scene initialized");
        lvgl_port_unlock();
    }

    xTaskCreatePinnedToCore(weather_task, "weather_task", 1024 * 10, NULL, 3, NULL, 1);

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
