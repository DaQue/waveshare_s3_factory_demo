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

#include "esp_err.h"
#include "esp_crt_bundle.h"
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

extern "C" {
extern const uint8_t openweather_trust_chain_pem_start[] asm("_binary_openweather_trust_chain_pem_start");
extern const uint8_t openweather_trust_chain_pem_end[] asm("_binary_openweather_trust_chain_pem_end");
}

static const char *TAG = "app_main";
static const char *OPENWEATHER_CA_CERT_PEM = reinterpret_cast<const char *>(openweather_trust_chain_pem_start);

static esp_io_expander_handle_t expander_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_disp_t *lvgl_disp = NULL;
static i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

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

static app_state_t g_app = {};
static forecast_payload_t g_forecast_cache = {};

typedef struct {
    bool pressed;
    int16_t start_x;
    int16_t start_y;
    int16_t last_x;
    int16_t last_y;
    uint32_t last_swipe_ms;
} touch_swipe_state_t;

static touch_swipe_state_t g_touch_swipe = {};
static bool g_wifi_connected = false;
static uint32_t g_wifi_connected_ms = 0;

static const char *WEEKDAY_SHORT[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

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
    data.forecast_hourly_open = g_app.forecast_hourly_open;
    data.forecast_hourly_offset = g_app.forecast_hourly_offset;
    data.forecast_hourly_count = g_app.forecast_hourly_count;
    data.has_weather = g_app.has_weather;
    data.time_text = g_app.time_text;
    data.now_time_text = g_app.now_time_text;
    data.status_text = g_app.status_text;
    data.temp_text = g_app.temp_text;
    data.condition_text = g_app.condition_text;
    data.weather_text = g_app.weather_text;
    data.stats_line_1 = g_app.stats_line_1;
    data.stats_line_2 = g_app.stats_line_2;
    data.stats_line_3 = g_app.stats_line_3;
    data.indoor_line_1 = g_app.indoor_line_1;
    data.indoor_line_2 = g_app.indoor_line_2;
    data.indoor_line_3 = g_app.indoor_line_3;
    data.now_icon = g_app.now_icon;
    data.forecast_title_text = g_app.forecast_title_text;
    data.forecast_body_text = g_app.forecast_body_text;
    data.forecast_preview_text = g_app.forecast_preview_text;
    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        data.forecast_row_title[i] = g_app.forecast_row_title[i];
        data.forecast_row_detail[i] = g_app.forecast_row_detail[i];
        data.forecast_row_temp[i] = g_app.forecast_row_temp[i];
        data.forecast_row_icon[i] = g_app.forecast_row_icon[i];
        data.forecast_hourly_time[i] = g_app.forecast_hourly_time[i];
        data.forecast_hourly_detail[i] = g_app.forecast_hourly_detail[i];
        data.forecast_hourly_temp[i] = g_app.forecast_hourly_temp[i];
        data.forecast_hourly_icon[i] = g_app.forecast_hourly_icon[i];
    }
    data.forecast_hourly_day_title = g_app.forecast_hourly_day_title;
    data.i2c_scan_text = g_app.i2c_scan_text;
    data.wifi_scan_text = g_app.wifi_scan_text;
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

static bool app_format_local_time(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return false;
    }

    time_t now = 0;
    time(&now);

    struct tm tm_local = {};
    localtime_r(&now, &tm_local);
    if (tm_local.tm_year < (2024 - 1900))
    {
        snprintf(out, out_size, "--:--");
        return false;
    }

    if (strftime(out, out_size, "%I:%M %p", &tm_local) == 0)
    {
        snprintf(out, out_size, "--:--");
        return false;
    }

    if (out[0] == '0')
    {
        memmove(out, out + 1, strlen(out));
    }

    return true;
}

static void app_update_local_time(void)
{
    char next[16] = {0};
    (void)app_format_local_time(next, sizeof(next));

    if (strcmp(next, g_app.now_time_text) != 0)
    {
        snprintf(g_app.now_time_text, sizeof(g_app.now_time_text), "%s", next);
        app_mark_dirty(true, false, false, false);
    }
}

static void app_update_connect_time(uint32_t now_ms)
{
    uint32_t elapsed_sec = 0;
    if (g_wifi_connected)
    {
        elapsed_sec = (now_ms - g_wifi_connected_ms) / 1000U;
    }

    uint32_t hours = (elapsed_sec / 3600U) % 100U;
    uint32_t minutes = (elapsed_sec / 60U) % 60U;
    uint32_t seconds = elapsed_sec % 60U;

    char next[16] = {0};
    snprintf(next, sizeof(next), "%02u:%02u:%02u",
             (unsigned)hours, (unsigned)minutes, (unsigned)seconds);

    if (strcmp(next, g_app.time_text) != 0)
    {
        snprintf(g_app.time_text, sizeof(g_app.time_text), "%s", next);
        app_mark_dirty(true, false, false, false);
    }
}

static bool app_sync_time_with_ntp(void)
{
    setenv("TZ", LOCAL_TIMEZONE_TZ, 1);
    tzset();

    if (!esp_sntp_enabled())
    {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
    }
    else
    {
        esp_sntp_restart();
    }

    int waited_ms = 0;
    while (waited_ms < NTP_SYNC_TIMEOUT_MS)
    {
        char local_time[16] = {0};
        if (app_format_local_time(local_time, sizeof(local_time)))
        {
            ESP_LOGI(TAG, "time: synced via NTP (%s, %s)", local_time, LOCAL_TIMEZONE_TZ);
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_POLL_MS));
        waited_ms += NTP_SYNC_POLL_MS;
    }

    ESP_LOGW(TAG, "time: NTP sync pending after %d ms", NTP_SYNC_TIMEOUT_MS);
    return false;
}

static void app_set_screen(drawing_screen_view_t view)
{
    if (g_app.view != view)
    {
        if (g_app.view == DRAWING_SCREEN_VIEW_FORECAST && view != DRAWING_SCREEN_VIEW_FORECAST)
        {
            g_app.forecast_hourly_open = false;
            g_app.forecast_hourly_offset = 0;
            g_app.forecast_hourly_count = 0;
            g_app.forecast_hourly_day_title[0] = '\0';
        }
        g_app.view = view;
        app_mark_dirty(true, true, true, true);
    }
}

static int app_forecast_row_from_y(int16_t y)
{
    const int row_top = 52;
    const int row_stride = 64;
    const int row_card_h = 56;
    if (y < row_top || y >= row_top + APP_FORECAST_ROWS * row_stride)
    {
        return -1;
    }
    int rel_y = y - row_top;
    int row = rel_y / row_stride;
    int row_y = rel_y % row_stride;
    if (row_y > row_card_h)
    {
        return -1;
    }
    return row;
}

static void app_build_forecast_hourly_visible(void)
{
    if (!g_app.forecast_hourly_open || g_app.forecast_hourly_day >= g_app.forecast_row_count)
    {
        g_app.forecast_hourly_count = 0;
        return;
    }

    uint8_t day = g_app.forecast_hourly_day;
    uint8_t count = g_forecast_cache.days[day].count;
    g_app.forecast_hourly_count = count;

    uint8_t max_start = 0;
    if (count > APP_FORECAST_ROWS)
    {
        max_start = (uint8_t)(count - APP_FORECAST_ROWS);
    }
    if (g_app.forecast_hourly_offset > max_start)
    {
        g_app.forecast_hourly_offset = max_start;
    }

    snprintf(g_app.forecast_hourly_day_title, sizeof(g_app.forecast_hourly_day_title),
             "%.16s Hourly", g_app.forecast_row_title[day]);

    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        int src = (int)g_app.forecast_hourly_offset + i;
        if (src < count)
        {
            const forecast_hourly_payload_t *entry = &g_forecast_cache.days[day].entries[src];
            snprintf(g_app.forecast_hourly_time[i], sizeof(g_app.forecast_hourly_time[i]), "%s", entry->time_text);
            snprintf(g_app.forecast_hourly_detail[i], sizeof(g_app.forecast_hourly_detail[i]), "%s", entry->detail);
            snprintf(g_app.forecast_hourly_temp[i], sizeof(g_app.forecast_hourly_temp[i]), "%s", entry->temp_text);
            g_app.forecast_hourly_icon[i] = entry->icon;
        }
        else
        {
            snprintf(g_app.forecast_hourly_time[i], sizeof(g_app.forecast_hourly_time[i]), "--");
            g_app.forecast_hourly_detail[i][0] = '\0';
            snprintf(g_app.forecast_hourly_temp[i], sizeof(g_app.forecast_hourly_temp[i]), "--°");
            g_app.forecast_hourly_icon[i] = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        }
    }
}

static void app_close_forecast_hourly(void)
{
    if (!g_app.forecast_hourly_open)
    {
        return;
    }
    g_app.forecast_hourly_open = false;
    g_app.forecast_hourly_offset = 0;
    g_app.forecast_hourly_count = 0;
    g_app.forecast_hourly_day_title[0] = '\0';
    app_mark_dirty(true, true, false, true);
}

static void app_open_forecast_hourly(uint8_t day_row)
{
    if (day_row >= g_app.forecast_row_count)
    {
        return;
    }
    if (g_forecast_cache.days[day_row].count == 0)
    {
        return;
    }

    g_app.forecast_hourly_open = true;
    g_app.forecast_hourly_day = day_row;
    g_app.forecast_hourly_offset = 0;
    app_build_forecast_hourly_visible();
    app_mark_dirty(true, true, false, true);
}

static void app_scroll_forecast_hourly(int dir)
{
    if (!g_app.forecast_hourly_open || g_app.forecast_hourly_day >= g_app.forecast_row_count)
    {
        return;
    }

    uint8_t day = g_app.forecast_hourly_day;
    uint8_t count = g_forecast_cache.days[day].count;
    if (count <= APP_FORECAST_ROWS)
    {
        return;
    }

    int max_start = (int)count - APP_FORECAST_ROWS;
    int next_offset = (int)g_app.forecast_hourly_offset + (dir * APP_FORECAST_ROWS);
    if (next_offset < 0)
    {
        next_offset = 0;
    }
    else if (next_offset > max_start)
    {
        next_offset = max_start;
    }

    if (next_offset == (int)g_app.forecast_hourly_offset)
    {
        return;
    }

    g_app.forecast_hourly_offset = (uint8_t)next_offset;
    app_build_forecast_hourly_visible();
    app_mark_dirty(false, true, false, true);
}

static void app_handle_touch_tap(int16_t x, int16_t y)
{
    (void)x;
    if (g_app.view != DRAWING_SCREEN_VIEW_FORECAST)
    {
        return;
    }

    if (g_app.forecast_hourly_open)
    {
        // Tap near top bar to return to day list.
        if (y < 42)
        {
            app_close_forecast_hourly();
        }
        return;
    }

    int row = app_forecast_row_from_y(y);
    if (row < 0 || row >= g_app.forecast_row_count)
    {
        return;
    }
    app_open_forecast_hourly((uint8_t)row);
}

static uint16_t display_rotation_to_touch_rotation(lv_disp_rot_t display_rotation)
{
    switch (display_rotation)
    {
    case LV_DISP_ROT_90:
        return 1;
    case LV_DISP_ROT_180:
        return 2;
    case LV_DISP_ROT_270:
        return 3;
    case LV_DISP_ROT_NONE:
    default:
        return 0;
    }
}

static void app_poll_touch_swipe(uint32_t now_ms)
{
    touch_data_t touch_data = {};
    bsp_touch_read();
    bool is_pressed = bsp_touch_get_coordinates(&touch_data);

    if (is_pressed)
    {
        int16_t x = (int16_t)touch_data.coords[0].x;
        int16_t y = (int16_t)touch_data.coords[0].y;

        if (!g_touch_swipe.pressed)
        {
            g_touch_swipe.pressed = true;
            g_touch_swipe.start_x = x;
            g_touch_swipe.start_y = y;
        }

        g_touch_swipe.last_x = x;
        g_touch_swipe.last_y = y;
        return;
    }

    if (!g_touch_swipe.pressed)
    {
        return;
    }

    int delta_x = (int)g_touch_swipe.last_x - (int)g_touch_swipe.start_x;
    int delta_y = (int)g_touch_swipe.last_y - (int)g_touch_swipe.start_y;
    int abs_delta_x = (delta_x >= 0) ? delta_x : -delta_x;
    int abs_delta_y = (delta_y >= 0) ? delta_y : -delta_y;
    g_touch_swipe.pressed = false;

    if (abs_delta_x <= TOUCH_TAP_MAX_MOVE_PX && abs_delta_y <= TOUCH_TAP_MAX_MOVE_PX)
    {
        app_handle_touch_tap(g_touch_swipe.last_x, g_touch_swipe.last_y);
        return;
    }

    if ((uint32_t)(now_ms - g_touch_swipe.last_swipe_ms) < TOUCH_SWIPE_COOLDOWN_MS)
    {
        return;
    }

    if (g_app.view == DRAWING_SCREEN_VIEW_FORECAST && g_app.forecast_hourly_open &&
        abs_delta_y >= TOUCH_SWIPE_MIN_Y_PX && abs_delta_x <= TOUCH_SWIPE_MAX_X_PX && abs_delta_y > abs_delta_x)
    {
        g_touch_swipe.last_swipe_ms = now_ms;
        // Swipe up shows later hours; swipe down shows earlier hours.
        app_scroll_forecast_hourly((delta_y < 0) ? 1 : -1);
        return;
    }

    if (abs_delta_x < TOUCH_SWIPE_MIN_X_PX || abs_delta_y > TOUCH_SWIPE_MAX_Y_PX || abs_delta_y >= abs_delta_x)
    {
        return;
    }

    g_touch_swipe.last_swipe_ms = now_ms;

    // Left swipe => next page, right swipe => previous page.
    int step = (delta_x < 0) ? 1 : -1;
    int view = (int)g_app.view + step;
    if (view < 0)
    {
        view = (int)DRAWING_SCREEN_VIEW_WIFI_SCAN;
    }
    else if (view > (int)DRAWING_SCREEN_VIEW_WIFI_SCAN)
    {
        view = (int)DRAWING_SCREEN_VIEW_NOW;
    }
    app_set_screen((drawing_screen_view_t)view);
}

static const char *weekday_name(int wday)
{
    if (wday < 0 || wday > 6)
    {
        return "?";
    }
    return WEEKDAY_SHORT[wday];
}

static void format_hour_label(int hour24, char *out, size_t out_size)
{
    int hour12 = hour24 % 12;
    if (hour12 == 0)
    {
        hour12 = 12;
    }
    const char *ampm = (hour24 >= 12) ? "PM" : "AM";
    snprintf(out, out_size, "%d%s", hour12, ampm);
}

static bool owm_icon_is_night(const char *icon_code)
{
    return (icon_code != NULL && strlen(icon_code) >= 3 && icon_code[2] == 'n');
}

static drawing_weather_icon_t map_owm_condition_to_icon(int weather_id, const char *icon_code)
{
    bool is_night = owm_icon_is_night(icon_code);

    if (weather_id >= 200 && weather_id < 300)
    {
        return DRAWING_WEATHER_ICON_THUNDERSTORM;
    }
    if (weather_id >= 300 && weather_id < 400)
    {
        return DRAWING_WEATHER_ICON_SHOWER_RAIN;
    }
    if (weather_id >= 500 && weather_id < 600)
    {
        if (weather_id == 511)
        {
            return DRAWING_WEATHER_ICON_SLEET;
        }
        if (weather_id >= 520)
        {
            return DRAWING_WEATHER_ICON_SHOWER_RAIN;
        }
        return DRAWING_WEATHER_ICON_RAIN;
    }
    if (weather_id >= 600 && weather_id < 700)
    {
        return DRAWING_WEATHER_ICON_SNOW;
    }
    if (weather_id >= 700 && weather_id < 800)
    {
        if (weather_id == 741)
        {
            return DRAWING_WEATHER_ICON_FOG;
        }
        return DRAWING_WEATHER_ICON_MIST;
    }
    if (weather_id == 800)
    {
        return is_night ? DRAWING_WEATHER_ICON_CLEAR_NIGHT : DRAWING_WEATHER_ICON_CLEAR_DAY;
    }
    if (weather_id == 801)
    {
        return is_night ? DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT : DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
    }
    if (weather_id == 802)
    {
        return DRAWING_WEATHER_ICON_CLOUDS;
    }
    if (weather_id >= 803 && weather_id <= 804)
    {
        return DRAWING_WEATHER_ICON_OVERCAST;
    }

    return is_night ? DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT : DRAWING_WEATHER_ICON_CLOUDS;
}

static void app_set_forecast_placeholders(void)
{
    static const char *default_titles[APP_FORECAST_ROWS] = {
        "Tue", "Wed", "Thu", "Fri"};
    static const char *default_details[APP_FORECAST_ROWS] = {
        "Low --° Wind --", "Low --° Wind --", "Low --° Wind --", "Low --° Wind --"};
    static const char *default_temps[APP_FORECAST_ROWS] = {
        "--°", "--°", "--°", "--°"};

    snprintf(g_app.forecast_title_text, sizeof(g_app.forecast_title_text), "Forecast");
    snprintf(g_app.forecast_body_text, sizeof(g_app.forecast_body_text), "Daily highs/lows");
    snprintf(g_app.forecast_preview_text, sizeof(g_app.forecast_preview_text),
             "Tue --°   Wed --°   Thu --°");
    g_app.forecast_row_count = APP_FORECAST_ROWS;
    g_app.forecast_hourly_open = false;
    g_app.forecast_hourly_day = 0;
    g_app.forecast_hourly_offset = 0;
    g_app.forecast_hourly_count = 0;
    g_app.forecast_hourly_day_title[0] = '\0';
    memset(&g_forecast_cache, 0, sizeof(g_forecast_cache));

    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        snprintf(g_app.forecast_row_title[i], sizeof(g_app.forecast_row_title[i]), "%s", default_titles[i]);
        snprintf(g_app.forecast_row_detail[i], sizeof(g_app.forecast_row_detail[i]), "%s", default_details[i]);
        snprintf(g_app.forecast_row_temp[i], sizeof(g_app.forecast_row_temp[i]), "%s", default_temps[i]);
        g_app.forecast_row_icon[i] = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        snprintf(g_app.forecast_hourly_time[i], sizeof(g_app.forecast_hourly_time[i]), "--");
        g_app.forecast_hourly_detail[i][0] = '\0';
        snprintf(g_app.forecast_hourly_temp[i], sizeof(g_app.forecast_hourly_temp[i]), "--°");
        g_app.forecast_hourly_icon[i] = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
    }
}

static void app_set_indoor_placeholders(void)
{
    snprintf(g_app.indoor_line_1, sizeof(g_app.indoor_line_1), "Indoor --\xC2\xB0" "F");
    snprintf(g_app.indoor_line_2, sizeof(g_app.indoor_line_2), "--%% RH");
    snprintf(g_app.indoor_line_3, sizeof(g_app.indoor_line_3), "-- hPa");
}

static void app_set_i2c_scan_placeholder(void)
{
    snprintf(g_app.i2c_scan_text, sizeof(g_app.i2c_scan_text),
             "I2C scan pending...\n"
             "Range: 0x03-0x77\n"
             "BME280 expected at 0x76 or 0x77");
}

static void app_set_wifi_scan_placeholder(void)
{
    snprintf(g_app.wifi_scan_text, sizeof(g_app.wifi_scan_text),
             "Wi-Fi scan pending...\n"
             "Swipe to this page after Wi-Fi connects.");
}

static void app_run_i2c_scan(i2c_master_bus_handle_t bus_handle)
{
    if (bus_handle == NULL)
    {
        snprintf(g_app.i2c_scan_text, sizeof(g_app.i2c_scan_text), "I2C bus not initialized");
        app_mark_dirty(false, true, false, false);
        return;
    }

    char text[640] = {0};
    size_t used = 0;
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
        used += snprintf(text + used, sizeof(text) - used,
                         "\n\nTotal: %d\nBME280 addr: %s\nDriver: %s",
                         found_count,
                         found_bme_addr ? "present" : "missing",
                         bsp_bme280_is_available() ? "initialized" : "not initialized");
    }

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

static void app_run_wifi_scan(void)
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
        used += snprintf(text + used, sizeof(text) - used, "No networks in range.");
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
            used += snprintf(text + used, sizeof(text) - used, "...and %u more",
                             (unsigned)(ap_count - shown));
        }
    }

    snprintf(g_app.wifi_scan_text, sizeof(g_app.wifi_scan_text), "%s", text);
    app_mark_dirty(false, true, false, false);
}

static void app_apply_indoor_data(const bsp_bme280_data_t *indoor)
{
    if (indoor == NULL)
    {
        return;
    }

    snprintf(g_app.indoor_line_1, sizeof(g_app.indoor_line_1), "Indoor %.1f\xC2\xB0" "F", indoor->temperature_f);
    snprintf(g_app.indoor_line_2, sizeof(g_app.indoor_line_2), "%.0f%% RH", indoor->humidity_pct);
    snprintf(g_app.indoor_line_3, sizeof(g_app.indoor_line_3), "%.0f hPa", indoor->pressure_hpa);
    app_mark_dirty(false, true, true, false);
}

static void app_apply_forecast_payload(const forecast_payload_t *fc)
{
    if (fc == NULL)
    {
        return;
    }

    g_forecast_cache = *fc;

    snprintf(g_app.forecast_title_text, sizeof(g_app.forecast_title_text), "Forecast");
    snprintf(g_app.forecast_body_text, sizeof(g_app.forecast_body_text), "Daily highs/lows");
    snprintf(g_app.forecast_preview_text, sizeof(g_app.forecast_preview_text), "%s", fc->preview_text);
    g_app.forecast_row_count = (fc->row_count > APP_FORECAST_ROWS) ? APP_FORECAST_ROWS : fc->row_count;

    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        snprintf(g_app.forecast_row_title[i], sizeof(g_app.forecast_row_title[i]), "%s", fc->rows[i].title);
        snprintf(g_app.forecast_row_detail[i], sizeof(g_app.forecast_row_detail[i]), "%s", fc->rows[i].detail);
        snprintf(g_app.forecast_row_temp[i], sizeof(g_app.forecast_row_temp[i]), "%s", fc->rows[i].temp_text);
        g_app.forecast_row_icon[i] = fc->rows[i].icon;
    }

    if (g_app.forecast_hourly_open)
    {
        if (g_app.forecast_hourly_day >= g_app.forecast_row_count ||
            g_forecast_cache.days[g_app.forecast_hourly_day].count == 0)
        {
            app_close_forecast_hourly();
        }
        else
        {
            app_build_forecast_hourly_visible();
            app_mark_dirty(true, false, false, true);
        }
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
    g_app.now_icon = wx->icon;

    g_app.has_weather = true;
    app_mark_dirty(false, true, true, false);
}

static void app_state_init_defaults(void)
{
    memset(&g_app, 0, sizeof(g_app));
    memset(&g_touch_swipe, 0, sizeof(g_touch_swipe));
    g_wifi_connected = false;
    g_wifi_connected_ms = 0;

    g_app.view = DRAWING_SCREEN_VIEW_NOW;
    g_app.forecast_page = 0;
    g_app.has_weather = false;

    snprintf(g_app.time_text, sizeof(g_app.time_text), "00:00:00");
    snprintf(g_app.now_time_text, sizeof(g_app.now_time_text), "--:--");
    snprintf(g_app.status_text, sizeof(g_app.status_text), "status: boot complete");
    snprintf(g_app.temp_text, sizeof(g_app.temp_text), "--\xC2\xB0" "F");
    snprintf(g_app.condition_text, sizeof(g_app.condition_text), "Waiting for weather");
    snprintf(g_app.weather_text, sizeof(g_app.weather_text), "Network fetch pending");
    snprintf(g_app.stats_line_1, sizeof(g_app.stats_line_1), "Feels --");
    snprintf(g_app.stats_line_2, sizeof(g_app.stats_line_2), "Humidity --");
    snprintf(g_app.stats_line_3, sizeof(g_app.stats_line_3), "Pressure --");
    app_set_indoor_placeholders();
    app_set_i2c_scan_placeholder();
    app_set_wifi_scan_placeholder();
    g_app.now_icon = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
    snprintf(g_app.bottom_text, sizeof(g_app.bottom_text), "Swipe left/right to switch views");

    app_set_forecast_placeholders();
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

static bool is_https_url(const char *url)
{
    return (url != NULL) && (strncmp(url, "https://", 8) == 0);
}

static esp_http_client_handle_t http_client_create(const char *url, bool use_cert_bundle)
{
    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = WEATHER_HTTP_TIMEOUT_MS;
    config.user_agent = "waveshare-s3-weather-test/1.0";
    config.keep_alive_enable = true;

    if (is_https_url(url))
    {
        const ptrdiff_t chain_len = openweather_trust_chain_pem_end - openweather_trust_chain_pem_start;
        config.tls_version = ESP_HTTP_CLIENT_TLS_VER_TLS_1_2;
        config.skip_cert_common_name_check = false;
        if (use_cert_bundle)
        {
            config.crt_bundle_attach = esp_crt_bundle_attach;
        }
        else
        {
            config.cert_pem = OPENWEATHER_CA_CERT_PEM;
            if (chain_len > 0)
            {
                config.cert_len = (size_t)chain_len;
            }
        }
    }

    return esp_http_client_init(&config);
}

static esp_err_t http_get_text_once(esp_http_client_handle_t client, const char *url, char *response_buf, size_t response_buf_size,
                                    int *status_code, int *bytes_read)
{
    if (client == NULL || url == NULL || response_buf_size == 0)
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

    esp_err_t err = esp_http_client_set_url(client, url);
    if (err != ESP_OK)
    {
        return err;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_timeout_ms(client, WEATHER_HTTP_TIMEOUT_MS);

    err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
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
    cJSON *weather_id = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "id") : NULL;
    cJSON *icon = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "icon") : NULL;

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
    out->icon = map_owm_condition_to_icon(cJSON_IsNumber(weather_id) ? weather_id->valueint : 0,
                                          cJSON_IsString(icon) ? icon->valuestring : NULL);

    snprintf(out->city, sizeof(out->city), "%s", cJSON_IsString(name) ? name->valuestring : "?");
    snprintf(out->country, sizeof(out->country), "%s", cJSON_IsString(country) ? country->valuestring : "");
    snprintf(out->condition, sizeof(out->condition), "%s", cJSON_IsString(desc) ? desc->valuestring : "(unknown)");

    cJSON_Delete(root);
    return true;
}

static void forecast_payload_set_defaults(forecast_payload_t *out)
{
    static const char *default_titles[APP_FORECAST_ROWS] = {
        "Tue", "Wed", "Thu", "Fri"};
    out->row_count = APP_FORECAST_ROWS;
    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        snprintf(out->rows[i].title, sizeof(out->rows[i].title), "%s", default_titles[i]);
        snprintf(out->rows[i].detail, sizeof(out->rows[i].detail), "Low --° Wind --");
        snprintf(out->rows[i].temp_text, sizeof(out->rows[i].temp_text), "--°");
        out->rows[i].temp_f = 0;
        out->rows[i].feels_f = 0;
        out->rows[i].wind_mph = 0;
        out->rows[i].icon = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        out->days[i].count = 0;
    }
    snprintf(out->preview_text, sizeof(out->preview_text), "Tue --°   Wed --°   Thu --°");
}

static bool parse_forecast_json(const char *json_text, forecast_payload_t *out)
{
    if (out == NULL)
    {
        return false;
    }

    forecast_payload_set_defaults(out);

    cJSON *root = cJSON_Parse(json_text);
    if (root == NULL)
    {
        return false;
    }

    cJSON *list = cJSON_GetObjectItemCaseSensitive(root, "list");
    if (!cJSON_IsArray(list))
    {
        cJSON_Delete(root);
        return false;
    }

    cJSON *city = cJSON_GetObjectItemCaseSensitive(root, "city");
    cJSON *timezone = (city != NULL) ? cJSON_GetObjectItemCaseSensitive(city, "timezone") : NULL;
    int tz_offset = cJSON_IsNumber(timezone) ? timezone->valueint : 0;

    typedef struct {
        bool set;
        int year;
        int yday;
        int wday;
        float high_f;
        float low_f;
        float wind_peak_mph;
        drawing_weather_icon_t icon;
        bool icon_set;
        int icon_score;
        uint8_t hourly_count;
        forecast_hourly_payload_t hourly[APP_FORECAST_HOURLY_MAX];
    } day_summary_t;

    static day_summary_t days[APP_FORECAST_MAX_DAYS];
    memset(days, 0, sizeof(days));
    int day_count = 0;
    int first_entry_hour = -1;

    int list_count = cJSON_GetArraySize(list);
    for (int i = 0; i < list_count; ++i)
    {
        cJSON *entry = cJSON_GetArrayItem(list, i);
        cJSON *dt = (entry != NULL) ? cJSON_GetObjectItemCaseSensitive(entry, "dt") : NULL;
        cJSON *main_obj = (entry != NULL) ? cJSON_GetObjectItemCaseSensitive(entry, "main") : NULL;
        if (!cJSON_IsNumber(dt) || !cJSON_IsObject(main_obj))
        {
            continue;
        }

        cJSON *temp = cJSON_GetObjectItemCaseSensitive(main_obj, "temp");
        cJSON *wind_obj = cJSON_GetObjectItemCaseSensitive(entry, "wind");
        cJSON *wind_speed = cJSON_IsObject(wind_obj) ? cJSON_GetObjectItemCaseSensitive(wind_obj, "speed") : NULL;
        if (!cJSON_IsNumber(temp))
        {
            continue;
        }

        int64_t dt_value = (int64_t)dt->valuedouble;
        time_t local_epoch = (time_t)(dt_value + (int64_t)tz_offset);
        struct tm tm_local = {};
        gmtime_r(&local_epoch, &tm_local);
        if (first_entry_hour < 0)
        {
            first_entry_hour = tm_local.tm_hour;
        }

        int idx = -1;
        for (int d = 0; d < day_count; ++d)
        {
            if (days[d].set && days[d].year == tm_local.tm_year && days[d].yday == tm_local.tm_yday)
            {
                idx = d;
                break;
            }
        }
        if (idx < 0)
        {
            if (day_count >= APP_FORECAST_MAX_DAYS)
            {
                continue;
            }
            idx = day_count++;
            days[idx].set = true;
            days[idx].year = tm_local.tm_year;
            days[idx].yday = tm_local.tm_yday;
            days[idx].wday = tm_local.tm_wday;
            days[idx].high_f = (float)temp->valuedouble;
            days[idx].low_f = (float)temp->valuedouble;
            days[idx].wind_peak_mph = cJSON_IsNumber(wind_speed) ? (float)wind_speed->valuedouble : 0.0f;
            days[idx].icon = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
            days[idx].icon_set = false;
            days[idx].icon_score = -1;
            days[idx].hourly_count = 0;
        }
        if ((float)temp->valuedouble > days[idx].high_f)
        {
            days[idx].high_f = (float)temp->valuedouble;
        }
        if ((float)temp->valuedouble < days[idx].low_f)
        {
            days[idx].low_f = (float)temp->valuedouble;
        }
        if (cJSON_IsNumber(wind_speed) && (float)wind_speed->valuedouble > days[idx].wind_peak_mph)
        {
            days[idx].wind_peak_mph = (float)wind_speed->valuedouble;
        }

        cJSON *weather_arr = cJSON_GetObjectItemCaseSensitive(entry, "weather");
        cJSON *weather0 = (weather_arr != NULL && cJSON_IsArray(weather_arr)) ? cJSON_GetArrayItem(weather_arr, 0) : NULL;
        cJSON *weather_id = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "id") : NULL;
        cJSON *weather_icon = (weather0 != NULL) ? cJSON_GetObjectItemCaseSensitive(weather0, "icon") : NULL;
        drawing_weather_icon_t mapped_icon = map_owm_condition_to_icon(
            cJSON_IsNumber(weather_id) ? weather_id->valueint : 0,
            cJSON_IsString(weather_icon) ? weather_icon->valuestring : NULL);

        int icon_score = 0;
        if (tm_local.tm_hour == 12)
        {
            icon_score = 3;
        }
        else if (tm_local.tm_hour == 9 || tm_local.tm_hour == 15)
        {
            icon_score = 2;
        }
        else
        {
            icon_score = 1;
        }
        if (!days[idx].icon_set || icon_score > days[idx].icon_score)
        {
            days[idx].icon = mapped_icon;
            days[idx].icon_set = true;
            days[idx].icon_score = icon_score;
        }

        if (days[idx].hourly_count < APP_FORECAST_HOURLY_MAX)
        {
            forecast_hourly_payload_t *slot = &days[idx].hourly[days[idx].hourly_count];
            int temp_i = (int)lroundf((float)temp->valuedouble);
            cJSON *feels_like = cJSON_GetObjectItemCaseSensitive(main_obj, "feels_like");
            int feels_i = cJSON_IsNumber(feels_like) ? (int)lroundf((float)feels_like->valuedouble) : temp_i;
            int wind_i = cJSON_IsNumber(wind_speed) ? (int)lroundf((float)wind_speed->valuedouble) : 0;

            slot->temp_f = temp_i;
            slot->feels_f = feels_i;
            slot->wind_mph = wind_i;
            slot->icon = mapped_icon;
            format_hour_label(tm_local.tm_hour, slot->time_text, sizeof(slot->time_text));
            snprintf(slot->detail, sizeof(slot->detail), "Feels %d° Wind %d", feels_i, wind_i);
            snprintf(slot->temp_text, sizeof(slot->temp_text), "%d°", temp_i);
            days[idx].hourly_count++;
        }
    }

    int start_day = 0;
    if (day_count > 1 && first_entry_hour > 0)
    {
        // OWM 5-day forecast starts from the next 3h slot; if it is not midnight,
        // the first grouped day is a partial "today" bucket. Skip it for day-ahead UI.
        start_day = 1;
    }

    int available_days = day_count - start_day;
    if (available_days < 0)
    {
        available_days = 0;
    }

    int row_count = (available_days < APP_FORECAST_ROWS) ? available_days : APP_FORECAST_ROWS;
    out->row_count = (uint8_t)row_count;
    for (int i = 0; i < row_count; ++i)
    {
        const day_summary_t *day = &days[start_day + i];
        forecast_row_payload_t *row = &out->rows[i];
        int high_i = (int)lroundf(day->high_f);
        int low_i = (int)lroundf(day->low_f);
        int wind_i = (int)lroundf(day->wind_peak_mph);

        row->temp_f = high_i;
        row->feels_f = low_i;
        row->wind_mph = wind_i;
        row->icon = day->icon_set ? day->icon : DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;

        snprintf(row->title, sizeof(row->title), "%s", weekday_name(day->wday));
        snprintf(row->detail, sizeof(row->detail), "Low %d° Wind %d", low_i, wind_i);
        snprintf(row->temp_text, sizeof(row->temp_text), "%d°", high_i);

        out->days[i].count = day->hourly_count;
        for (int h = 0; h < day->hourly_count && h < APP_FORECAST_HOURLY_MAX; ++h)
        {
            out->days[i].entries[h] = day->hourly[h];
        }
    }

    int preview_count = (available_days < APP_PREVIEW_DAYS) ? available_days : APP_PREVIEW_DAYS;
    if (preview_count > 0)
    {
        out->preview_text[0] = '\0';
        for (int i = 0; i < preview_count; ++i)
        {
            char day_chunk[32] = {0};
            const day_summary_t *day = &days[start_day + i];
            int high_i = (int)lroundf(day->high_f);
            snprintf(day_chunk, sizeof(day_chunk), "%s %d°", weekday_name(day->wday), high_i);

            if (i > 0)
            {
                strncat(out->preview_text, "   ", sizeof(out->preview_text) - strlen(out->preview_text) - 1);
            }
            strncat(out->preview_text, day_chunk, sizeof(out->preview_text) - strlen(out->preview_text) - 1);
        }
    }

    cJSON_Delete(root);
    return (row_count > 0);
}

static bool weather_fetch_once(void)
{
    char weather_url[512] = {0};
    int weather_url_len = snprintf(weather_url, sizeof(weather_url),
                                   "https://api.openweathermap.org/data/2.5/weather?%s&units=imperial&appid=%s",
                                   WEATHER_QUERY_LOCAL, WEATHER_API_KEY_LOCAL);
    if (weather_url_len <= 0 || weather_url_len >= (int)sizeof(weather_url))
    {
        app_set_status_fmt("https: url build failed");
        app_set_bottom_fmt("weather URL error");
        return false;
    }

    char forecast_url[512] = {0};
    int forecast_url_len = snprintf(forecast_url, sizeof(forecast_url),
                                    "https://api.openweathermap.org/data/2.5/forecast?%s&units=imperial&appid=%s",
                                    WEATHER_QUERY_LOCAL, WEATHER_API_KEY_LOCAL);
    if (forecast_url_len <= 0 || forecast_url_len >= (int)sizeof(forecast_url))
    {
        app_set_status_fmt("https: forecast url build failed");
        app_set_bottom_fmt("forecast URL error");
        return false;
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        bool use_cert_bundle = (attempt == 1);
        if (use_cert_bundle)
        {
            app_set_status_fmt("https: retry with cert bundle");
            app_set_bottom_fmt("retrying secure connect...");
            app_render_if_dirty();
        }

        esp_http_client_handle_t client = http_client_create(weather_url, use_cert_bundle);
        if (client == NULL)
        {
            if (!use_cert_bundle)
            {
                continue;
            }
            app_set_status_fmt("https: client init failed");
            app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            return false;
        }
        struct http_client_guard_t {
            esp_http_client_handle_t handle;
            ~http_client_guard_t()
            {
                if (handle != NULL)
                {
                    esp_http_client_cleanup(handle);
                }
            }
        } client_guard = {client};

        app_set_status_fmt("https: GET weather (%s)", WEATHER_QUERY_LOCAL);
        app_set_bottom_fmt("fetching current conditions...");
        app_render_if_dirty();

        static char weather_response[WEATHER_HTTP_BUFFER_SIZE] = {0};
        int http_status = 0;
        int http_bytes = 0;
        esp_err_t err = http_get_text_once(client, weather_url, weather_response, sizeof(weather_response), &http_status, &http_bytes);

        if (err != ESP_OK)
        {
            app_set_status_fmt("https: transport error %s", esp_err_to_name(err));
            app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            if (!use_cert_bundle && err == ESP_ERR_HTTP_CONNECT)
            {
                continue;
            }
            return false;
        }

        app_set_status_fmt("https: status %d bytes %d", http_status, http_bytes);

        if (http_status != 200)
        {
            snprintf(g_app.weather_text, sizeof(g_app.weather_text), "API returned status %d", http_status);
            app_mark_dirty(false, true, false, false);
            app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            return false;
        }

        weather_payload_t wx = {};
        if (!parse_weather_json(weather_response, &wx))
        {
            app_set_status_fmt("json: parse failed");
            snprintf(g_app.weather_text, sizeof(g_app.weather_text), "weather JSON parse failed");
            app_mark_dirty(false, true, false, false);
            app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            return false;
        }

        app_apply_weather(&wx);

        app_set_status_fmt("https: GET forecast");
        app_render_if_dirty();

        static char forecast_response[WEATHER_FORECAST_HTTP_BUFFER_SIZE] = {0};
        int fc_status = 0;
        int fc_bytes = 0;
        err = http_get_text_once(client, forecast_url, forecast_response, sizeof(forecast_response), &fc_status, &fc_bytes);
        if (err != ESP_OK)
        {
            app_set_status_fmt("https: forecast transport %s", esp_err_to_name(err));
            app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            if (!use_cert_bundle && err == ESP_ERR_HTTP_CONNECT)
            {
                continue;
            }
            return false;
        }

        if (fc_status != 200)
        {
            app_set_status_fmt("https: forecast status %d", fc_status);
            app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            return false;
        }

        memset(&g_forecast_cache, 0, sizeof(g_forecast_cache));
        if (!parse_forecast_json(forecast_response, &g_forecast_cache))
        {
            app_set_status_fmt("json: forecast parse failed");
            app_set_bottom_fmt("forecast retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
            return false;
        }

        app_apply_forecast_payload(&g_forecast_cache);
        app_set_status_fmt("sync: ok %s %s", wx.city, wx.country);
        app_set_bottom_fmt("next sync in %u min", (unsigned)(WEATHER_REFRESH_MS / 60000));
        return true;
    }

    app_set_status_fmt("https: secure connect failed");
    app_set_bottom_fmt("retry in %u s", (unsigned)(WEATHER_RETRY_MS / 1000));
    return false;
}

static void weather_task(void *arg)
{
    (void)arg;

    TickType_t loop_tick = xTaskGetTickCount();
    uint32_t last_time_sec = UINT32_MAX;
    uint32_t next_weather_sync_ms = 0;
    uint32_t next_indoor_sample_ms = 0;
    uint32_t next_i2c_scan_ms = 0;
    uint32_t next_wifi_scan_ms = 0;

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

        app_poll_touch_swipe(now_ms);

        if (bsp_bme280_is_available() && (int32_t)(now_ms - next_indoor_sample_ms) >= 0)
        {
            bsp_bme280_data_t indoor = {};
            if (bsp_bme280_read(&indoor) == ESP_OK)
            {
                app_apply_indoor_data(&indoor);
            }
            next_indoor_sample_ms = now_ms + BME280_REFRESH_MS;
        }

        if ((int32_t)(now_ms - next_i2c_scan_ms) >= 0)
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

    esp_err_t bme_err = bsp_bme280_init(i2c_bus_handle);
    if (bme_err == ESP_OK)
    {
        ESP_LOGI(TAG, "Indoor sensor ready (BME280)");
    }
    else
    {
        ESP_LOGW(TAG, "Indoor sensor not found: %s", esp_err_to_name(bme_err));
    }

    bsp_display_brightness_init();
    bsp_display_set_brightness(100);

    lv_port_init_local();

    app_state_init_defaults();
    app_run_i2c_scan(g_i2c_bus_handle);

    if (lvgl_lock_with_retry(pdMS_TO_TICKS(250), 8, "initializing drawing screen"))
    {
        drawing_screen_init();
        lvgl_port_unlock();
    }
    app_render_if_dirty();

    ESP_LOGI(TAG, "State-driven weather UI initialized");

    xTaskCreatePinnedToCore(weather_task, "weather_task", 1024 * 16, NULL, 3, NULL, 1);

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
