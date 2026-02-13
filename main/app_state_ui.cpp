#include "app_priv.h"

const char *OPENWEATHER_CA_CERT_PEM = reinterpret_cast<const char *>(openweather_trust_chain_pem_start);

esp_io_expander_handle_t expander_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
lv_disp_t *lvgl_disp = NULL;
i2c_master_bus_handle_t g_i2c_bus_handle = NULL;

app_state_t g_app = {};
forecast_payload_t g_forecast_cache = {};
touch_swipe_state_t g_touch_swipe = {};
app_wifi_config_t g_wifi_config = {};
bool g_wifi_connected = false;
uint32_t g_wifi_connected_ms = 0;

const char *WEEKDAY_SHORT[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

bool lvgl_lock_with_retry(TickType_t timeout_ticks, int max_attempts, const char *reason)
{
    for (int attempt = 1; attempt <= max_attempts; ++attempt)
    {
        if (lvgl_port_lock(timeout_ticks))
        {
            return true;
        }
        ESP_LOGW(APP_TAG, "LVGL lock timeout while %s (attempt %d/%d)", reason, attempt, max_attempts);
    }
    ESP_LOGE(APP_TAG, "Failed to acquire LVGL lock while %s", reason);
    return false;
}

void app_mark_dirty(bool header, bool main, bool stats, bool bottom)
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

void app_render_if_dirty(void)
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

void app_set_status_fmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_app.status_text, sizeof(g_app.status_text), fmt, args);
    va_end(args);

    ESP_LOGI(APP_TAG, "%s", g_app.status_text);
    app_mark_dirty(true, false, false, false);
}

void app_set_bottom_fmt(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_app.bottom_text, sizeof(g_app.bottom_text), fmt, args);
    va_end(args);

    app_mark_dirty(false, false, false, true);
}

bool app_format_local_time(char *out, size_t out_size)
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

void app_update_local_time(void)
{
    char next[16] = {0};
    (void)app_format_local_time(next, sizeof(next));

    if (strcmp(next, g_app.now_time_text) != 0)
    {
        snprintf(g_app.now_time_text, sizeof(g_app.now_time_text), "%s", next);
        app_mark_dirty(true, false, false, false);
    }
}

void app_update_connect_time(uint32_t now_ms)
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

bool app_sync_time_with_ntp(void)
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
            ESP_LOGI(APP_TAG, "time: synced via NTP (%s, %s)", local_time, LOCAL_TIMEZONE_TZ);
            return true;
        }

        vTaskDelay(pdMS_TO_TICKS(NTP_SYNC_POLL_MS));
        waited_ms += NTP_SYNC_POLL_MS;
    }

    ESP_LOGW(APP_TAG, "time: NTP sync pending after %d ms", NTP_SYNC_TIMEOUT_MS);
    return false;
}

void app_set_screen(drawing_screen_view_t view)
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

void app_set_forecast_placeholders(void)
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

void app_set_indoor_placeholders(void)
{
    snprintf(g_app.indoor_line_1, sizeof(g_app.indoor_line_1), "Indoor --\xC2\xB0" "F");
    snprintf(g_app.indoor_line_2, sizeof(g_app.indoor_line_2), "--%% RH");
    snprintf(g_app.indoor_line_3, sizeof(g_app.indoor_line_3), "-- hPa");
}

void app_set_i2c_scan_placeholder(void)
{
    snprintf(g_app.i2c_scan_text, sizeof(g_app.i2c_scan_text),
             "I2C scan pending...\n"
             "Range: 0x03-0x77\n"
             "BME280 expected at 0x76 or 0x77");
}

void app_set_wifi_scan_placeholder(void)
{
    snprintf(g_app.wifi_scan_text, sizeof(g_app.wifi_scan_text),
             "Wi-Fi scan pending...\n"
             "Swipe to this page after Wi-Fi connects.");
}

void app_apply_indoor_data(const bsp_bme280_data_t *indoor)
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

void app_apply_weather(const weather_payload_t *wx)
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

void app_state_init_defaults(void)
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
