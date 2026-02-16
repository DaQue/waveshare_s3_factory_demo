#pragma once

#include <math.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_io_expander_tca9554.h"
#include "esp_log.h"
#include "esp_sntp.h"

#include "cJSON.h"

#include "bsp_axp2101.h"
#include "bsp_bme280.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_touch.h"
#include "bsp_wifi.h"
#include "drawing_screen.h"
#include "lv_port.h"

#define EXAMPLE_DISPLAY_ROTATION LV_DISP_ROT_90
#define EXAMPLE_LCD_H_RES 320
#define EXAMPLE_LCD_V_RES 480
#define LCD_BUFFER_SIZE (EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES)

#define WEATHER_HTTP_TIMEOUT_MS 15000
#define WEATHER_HTTP_BUFFER_SIZE 6144
#define WEATHER_FORECAST_HTTP_BUFFER_SIZE 20000
#define WIFI_WAIT_TIMEOUT_MS 30000

#define WEATHER_REFRESH_MS (10 * 60 * 1000)
#define WEATHER_RETRY_MS (30 * 1000)
#define NTP_SYNC_TIMEOUT_MS 20000
#define NTP_SYNC_POLL_MS 250
#define BME280_REFRESH_MS 5000
#define BME280_RETRY_MS 5000
#define I2C_SCAN_REFRESH_MS 10000
#define WIFI_SCAN_REFRESH_MS 15000
#define UI_TICK_MS 100

#define TOUCH_SWIPE_MIN_X_PX 64
#define TOUCH_SWIPE_MAX_Y_PX 80
#define TOUCH_SWIPE_MIN_Y_PX 48
#define TOUCH_SWIPE_MAX_X_PX 96
#define TOUCH_SWIPE_COOLDOWN_MS 300
#define TOUCH_TAP_MAX_MOVE_PX 18

#define APP_FORECAST_ROWS DRAWING_SCREEN_FORECAST_ROWS
#define APP_PREVIEW_DAYS 3
#define APP_FORECAST_MAX_DAYS 8
#define APP_FORECAST_HOURLY_MAX 12
#define APP_WIFI_SCAN_MAX_APS 12
#define APP_WIFI_SCAN_VISIBLE_APS 8
#define APP_WIFI_SSID_MAX_LEN 32
#define APP_WIFI_PASS_MAX_LEN 64
#define APP_WEATHER_API_KEY_MAX_LEN 96
#define APP_WEATHER_QUERY_MAX_LEN 96

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

#ifndef LOCAL_TIMEZONE_TZ
#define LOCAL_TIMEZONE_TZ "CST6CDT,M3.2.0/2,M11.1.0/2"
#endif

#define APP_TAG "app_main"

extern "C" {
extern const uint8_t openweather_trust_chain_pem_start[] asm("_binary_openweather_trust_chain_pem_start");
extern const uint8_t openweather_trust_chain_pem_end[] asm("_binary_openweather_trust_chain_pem_end");
}

typedef struct {
    float temp_f;
    float feels_f;
    float wind_mph;
    int humidity;
    int pressure_hpa;
    drawing_weather_icon_t icon;
    char city[48];
    char country[8];
    char condition[96];
} weather_payload_t;

typedef struct {
    int temp_f;
    int feels_f;
    int wind_mph;
    drawing_weather_icon_t icon;
    char title[24];
    char detail[48];
    char temp_text[12];
} forecast_row_payload_t;

typedef struct {
    int temp_f;
    int feels_f;
    int wind_mph;
    drawing_weather_icon_t icon;
    char time_text[24];
    char detail[48];
    char temp_text[12];
} forecast_hourly_payload_t;

typedef struct {
    uint8_t count;
    forecast_hourly_payload_t entries[APP_FORECAST_HOURLY_MAX];
} forecast_day_payload_t;

typedef struct {
    uint8_t row_count;
    forecast_row_payload_t rows[APP_FORECAST_ROWS];
    forecast_day_payload_t days[APP_FORECAST_ROWS];
    char preview_text[96];
} forecast_payload_t;

typedef struct {
    char wifi_ssid[APP_WIFI_SSID_MAX_LEN + 1];
    char wifi_pass[APP_WIFI_PASS_MAX_LEN + 1];
    bool wifi_override_active;
    char weather_api_key[APP_WEATHER_API_KEY_MAX_LEN + 1];
    char weather_query[APP_WEATHER_QUERY_MAX_LEN + 1];
    bool weather_api_override_active;
    bool weather_query_override_active;
} app_wifi_config_t;

typedef struct {
    drawing_screen_view_t view;
    uint8_t forecast_page;
    bool has_weather;
    char time_text[16];
    char now_time_text[16];
    char status_text[96];
    char temp_text[24];
    char condition_text[96];
    char weather_text[192];
    char stats_line_1[64];
    char stats_line_2[64];
    char stats_line_3[64];
    char indoor_line_1[64];
    char indoor_line_2[64];
    char indoor_line_3[64];
    drawing_weather_icon_t now_icon;
    char forecast_title_text[96];
    char forecast_body_text[220];
    char forecast_preview_text[96];
    uint8_t forecast_preview_count;
    char forecast_preview_day[APP_PREVIEW_DAYS][8];
    char forecast_preview_hi[APP_PREVIEW_DAYS][8];
    char forecast_preview_low[APP_PREVIEW_DAYS][8];
    drawing_weather_icon_t forecast_preview_icon[APP_PREVIEW_DAYS];
    uint8_t forecast_row_count;
    char forecast_row_title[APP_FORECAST_ROWS][24];
    char forecast_row_detail[APP_FORECAST_ROWS][48];
    char forecast_row_temp[APP_FORECAST_ROWS][12];
    drawing_weather_icon_t forecast_row_icon[APP_FORECAST_ROWS];
    bool forecast_hourly_open;
    uint8_t forecast_hourly_day;
    uint8_t forecast_hourly_offset;
    uint8_t forecast_hourly_count;
    char forecast_hourly_day_title[24];
    char forecast_hourly_time[APP_FORECAST_ROWS][24];
    char forecast_hourly_detail[APP_FORECAST_ROWS][48];
    char forecast_hourly_temp[APP_FORECAST_ROWS][12];
    drawing_weather_icon_t forecast_hourly_icon[APP_FORECAST_ROWS];
    char i2c_scan_text[640];
    char wifi_scan_text[1024];
    char bottom_text[96];
    drawing_screen_dirty_t dirty;
} app_state_t;

typedef struct {
    bool pressed;
    int16_t start_x;
    int16_t start_y;
    int16_t last_x;
    int16_t last_y;
    uint32_t last_swipe_ms;
} touch_swipe_state_t;

extern const char *OPENWEATHER_CA_CERT_PEM;

extern esp_io_expander_handle_t expander_handle;
extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;
extern lv_disp_t *lvgl_disp;
extern i2c_master_bus_handle_t g_i2c_bus_handle;

extern app_state_t g_app;
extern forecast_payload_t g_forecast_cache;
extern touch_swipe_state_t g_touch_swipe;
extern app_wifi_config_t g_wifi_config;
extern bool g_wifi_connected;
extern uint32_t g_wifi_connected_ms;

extern const char *WEEKDAY_SHORT[7];

bool lvgl_lock_with_retry(TickType_t timeout_ticks, int max_attempts, const char *reason);
void app_mark_dirty(bool header, bool main, bool stats, bool bottom);
void app_render_if_dirty(void);
void app_set_status_fmt(const char *fmt, ...);
void app_set_bottom_fmt(const char *fmt, ...);

bool app_format_local_time(char *out, size_t out_size);
void app_update_local_time(void);
void app_update_connect_time(uint32_t now_ms);
bool app_sync_time_with_ntp(void);

void app_set_screen(drawing_screen_view_t view);
void app_build_forecast_hourly_visible(void);
void app_close_forecast_hourly(void);
void app_open_forecast_hourly(uint8_t day_row);
void app_scroll_forecast_hourly(int dir);
uint16_t display_rotation_to_touch_rotation(lv_disp_rot_t display_rotation);
void app_poll_touch_swipe(uint32_t now_ms);

const char *weekday_name(int wday);
void format_hour_label(int hour24, char *out, size_t out_size);
drawing_weather_icon_t map_owm_condition_to_icon(int weather_id, const char *icon_code);
bool parse_weather_json(const char *json_text, weather_payload_t *out);
bool parse_forecast_json(const char *json_text, forecast_payload_t *out);

void app_set_forecast_placeholders(void);
void app_set_indoor_placeholders(void);
void app_set_i2c_scan_placeholder(void);
void app_set_wifi_scan_placeholder(void);
void app_run_i2c_scan(i2c_master_bus_handle_t bus_handle);
void app_run_wifi_scan(void);

void app_apply_indoor_data(const bsp_bme280_data_t *indoor);
void app_apply_forecast_payload(const forecast_payload_t *fc);
void app_apply_weather(const weather_payload_t *wx);
void app_state_init_defaults(void);
void app_config_load_from_nvs(void);
const char *app_config_wifi_ssid(void);
const char *app_config_wifi_pass(void);
bool app_config_wifi_override_active(void);
esp_err_t app_config_set_wifi_override(const char *ssid, const char *pass);
esp_err_t app_config_clear_wifi_override(void);
const char *app_config_weather_api_key(void);
const char *app_config_weather_query(void);
bool app_config_weather_api_override_active(void);
bool app_config_weather_query_override_active(void);
esp_err_t app_config_set_weather_api_key(const char *api_key);
esp_err_t app_config_set_weather_query(const char *query);
esp_err_t app_config_clear_weather_override(void);
void app_config_boot_console_window(uint32_t timeout_ms);
void app_config_interactive_console(void);

void io_expander_init(i2c_master_bus_handle_t bus_handle);
void lv_port_init_local(void);
bool wait_for_wifi_ip(const char *ssid, char *ip_out, size_t ip_out_size);
bool weather_fetch_once(void);
void weather_task(void *arg);
