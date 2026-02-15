#include "drawing_screen_priv.h"

#include <stdio.h>
#include <string.h>

#include "esp_app_desc.h"
#include "esp_log.h"

#ifndef PROJECT_VER
#define PROJECT_VER "dev"
#endif

static const char *app_version_string(void)
{
    const esp_app_desc_t *desc = esp_app_get_description();
    if (desc != NULL && desc->version[0] != '\0')
    {
        return desc->version;
    }
    return PROJECT_VER;
}

const char *DRAWING_TAG = "drawing_screen";

lv_obj_t *canvas = NULL;
bool canvas_exit = false;

lv_color_t *canvas_buf = NULL;
size_t canvas_buf_pixels = 0;
int screen_w = 320;
int screen_h = 480;

drawing_screen_view_t current_view = DRAWING_SCREEN_VIEW_NOW;

lv_obj_t *header_title_label = NULL;
lv_obj_t *header_time_label = NULL;
lv_obj_t *status_label = NULL;

lv_obj_t *now_temp_label = NULL;
lv_obj_t *now_time_label = NULL;
lv_obj_t *now_condition_label = NULL;
lv_obj_t *now_weather_label = NULL;
lv_obj_t *now_stats_1_label = NULL;
lv_obj_t *now_stats_2_label = NULL;
lv_obj_t *now_stats_3_label = NULL;
lv_obj_t *now_preview_labels[DRAWING_SCREEN_PREVIEW_DAYS] = {0};
lv_obj_t *indoor_temp_label = NULL;
lv_obj_t *indoor_humidity_label = NULL;
lv_obj_t *indoor_pressure_label = NULL;

lv_obj_t *forecast_row_title_labels[FORECAST_ROWS] = {0};
lv_obj_t *forecast_row_detail_labels[FORECAST_ROWS] = {0};
lv_obj_t *forecast_row_temp_labels[FORECAST_ROWS] = {0};
lv_obj_t *i2c_scan_title_label = NULL;
lv_obj_t *i2c_scan_body_label = NULL;
lv_obj_t *wifi_scan_title_label = NULL;
lv_obj_t *wifi_scan_body_label = NULL;

lv_obj_t *bottom_label = NULL;

static const char *MOCK_FORECAST_TITLES[FORECAST_ROWS] = {
    "Tue",
    "Wed",
    "Thu",
    "Fri",
};

static const char *FALLBACK_FORECAST_DETAILS[FORECAST_ROWS] = {
    "Low --° Wind --",
    "Low --° Wind --",
    "Low --° Wind --",
    "Low --° Wind --",
};

static const char *ABOUT_APP_NAME = "Waveshare S3 Weather Demo";
static const char *ABOUT_AUTHOR = "David Queen";
static const char *ABOUT_GITHUB = "github.com/DaQue/Waveshare-S3-Weather-Demo";
static const char *ABOUT_GITHUB_HANDLE = "@DaQue";

void drawing_screen_init(void)
{
    lv_obj_t *screen = lv_scr_act();

    int w = lv_obj_get_width(screen);
    int h = lv_obj_get_height(screen);
    if (w > 0 && h > 0)
    {
        screen_w = w;
        screen_h = h;
    }

    lv_obj_set_style_bg_color(screen, lv_color_make(27, 31, 39), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    if (!ensure_canvas_buffer(screen_w, screen_h))
    {
        return;
    }

    if (canvas == NULL)
    {
        canvas = lv_canvas_create(screen);
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_canvas_set_buffer(canvas, canvas_buf, screen_w, screen_h, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_size(canvas, screen_w, screen_h);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);

    if (header_time_label == NULL)
    {
        header_time_label = lv_label_create(screen);
        lv_obj_set_style_text_font(header_time_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(header_time_label, lv_color_make(222, 225, 230), 0);
    }
    lv_obj_set_pos(header_time_label, 14, 4);

    if (header_title_label == NULL)
    {
        header_title_label = lv_label_create(screen);
        lv_obj_set_style_text_font(header_title_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(header_title_label, lv_color_make(222, 225, 230), 0);
    }
    lv_obj_align(header_title_label, LV_ALIGN_TOP_MID, 0, 4);

    if (status_label == NULL)
    {
        status_label = lv_label_create(screen);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(status_label, lv_color_make(182, 187, 196), 0);
    }
    lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -10, 8);

    if (now_temp_label == NULL)
    {
        now_temp_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_temp_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(now_temp_label, lv_color_make(232, 235, 240), 0);
    }
    lv_obj_set_pos(now_temp_label, 174, 80);

    if (now_time_label == NULL)
    {
        now_time_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_time_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(now_time_label, lv_color_make(188, 196, 208), 0);
    }
    lv_obj_set_pos(now_time_label, 338, 90);

    if (now_condition_label == NULL)
    {
        now_condition_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_condition_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(now_condition_label, lv_color_make(166, 208, 255), 0);
    }
    lv_obj_set_pos(now_condition_label, 182, 145);

    if (now_weather_label == NULL)
    {
        now_weather_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_weather_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(now_weather_label, lv_color_make(214, 218, 226), 0);
        lv_label_set_long_mode(now_weather_label, LV_LABEL_LONG_CLIP);
    }
    lv_obj_set_width(now_weather_label, 280);
    lv_obj_set_pos(now_weather_label, 172, 178);

    if (now_stats_1_label == NULL)
    {
        now_stats_1_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_1_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_1_label, lv_color_make(225, 228, 233), 0);
    }
    lv_obj_set_pos(now_stats_1_label, 16, 246);

    if (now_stats_2_label == NULL)
    {
        now_stats_2_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_2_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_2_label, lv_color_make(184, 189, 198), 0);
    }
    lv_obj_set_pos(now_stats_2_label, 16, 278);

    if (now_stats_3_label == NULL)
    {
        now_stats_3_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_3_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_3_label, lv_color_make(184, 189, 198), 0);
    }
    lv_obj_set_pos(now_stats_3_label, 16, 292);

    for (int i = 0; i < DRAWING_SCREEN_PREVIEW_DAYS; ++i)
    {
        if (now_preview_labels[i] == NULL)
        {
            now_preview_labels[i] = lv_label_create(screen);
            lv_obj_set_style_text_font(now_preview_labels[i], &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(now_preview_labels[i], lv_color_make(214, 218, 226), 0);
            lv_label_set_long_mode(now_preview_labels[i], LV_LABEL_LONG_CLIP);
            lv_obj_set_width(now_preview_labels[i], 82);
        }
        lv_obj_set_pos(now_preview_labels[i], 66 + i * 160, 244);
    }

    if (indoor_temp_label == NULL)
    {
        indoor_temp_label = lv_label_create(screen);
        lv_obj_set_style_text_font(indoor_temp_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(indoor_temp_label, lv_color_make(232, 235, 240), 0);
    }
    lv_obj_set_pos(indoor_temp_label, 24, 76);

    if (indoor_humidity_label == NULL)
    {
        indoor_humidity_label = lv_label_create(screen);
        lv_obj_set_style_text_font(indoor_humidity_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(indoor_humidity_label, lv_color_make(188, 196, 208), 0);
    }
    lv_obj_set_pos(indoor_humidity_label, 24, 154);

    if (indoor_pressure_label == NULL)
    {
        indoor_pressure_label = lv_label_create(screen);
        lv_obj_set_style_text_font(indoor_pressure_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(indoor_pressure_label, lv_color_make(166, 208, 255), 0);
    }
    lv_obj_set_pos(indoor_pressure_label, 24, 232);

    for (int i = 0; i < FORECAST_ROWS; ++i)
    {
        if (forecast_row_title_labels[i] == NULL)
        {
            forecast_row_title_labels[i] = lv_label_create(screen);
            lv_obj_set_style_text_font(forecast_row_title_labels[i], &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(forecast_row_title_labels[i], lv_color_make(225, 228, 233), 0);
        }
        lv_obj_set_pos(forecast_row_title_labels[i], 80, 56 + i * 64);

        if (forecast_row_detail_labels[i] == NULL)
        {
            forecast_row_detail_labels[i] = lv_label_create(screen);
            lv_obj_set_style_text_font(forecast_row_detail_labels[i], &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(forecast_row_detail_labels[i], lv_color_make(175, 181, 191), 0);
        }
        lv_obj_set_pos(forecast_row_detail_labels[i], 80, 86 + i * 64);

        if (forecast_row_temp_labels[i] == NULL)
        {
            forecast_row_temp_labels[i] = lv_label_create(screen);
            lv_obj_set_style_text_font(forecast_row_temp_labels[i], &lv_font_montserrat_48, 0);
            lv_obj_set_style_text_color(forecast_row_temp_labels[i], lv_color_make(225, 228, 233), 0);
        }
        lv_obj_set_pos(forecast_row_temp_labels[i], screen_w - 94, 50 + i * 64);
    }

    if (i2c_scan_title_label == NULL)
    {
        i2c_scan_title_label = lv_label_create(screen);
        lv_obj_set_style_text_font(i2c_scan_title_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(i2c_scan_title_label, lv_color_make(225, 228, 233), 0);
    }
    lv_obj_set_pos(i2c_scan_title_label, 22, 64);

    if (i2c_scan_body_label == NULL)
    {
        i2c_scan_body_label = lv_label_create(screen);
        lv_obj_set_style_text_font(i2c_scan_body_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(i2c_scan_body_label, lv_color_make(184, 189, 198), 0);
        lv_label_set_long_mode(i2c_scan_body_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(i2c_scan_body_label, screen_w - 44);
    lv_obj_set_pos(i2c_scan_body_label, 22, 96);

    if (wifi_scan_title_label == NULL)
    {
        wifi_scan_title_label = lv_label_create(screen);
        lv_obj_set_style_text_font(wifi_scan_title_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(wifi_scan_title_label, lv_color_make(225, 228, 233), 0);
    }
    lv_obj_set_pos(wifi_scan_title_label, 22, 64);

    if (wifi_scan_body_label == NULL)
    {
        wifi_scan_body_label = lv_label_create(screen);
        lv_obj_set_style_text_font(wifi_scan_body_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(wifi_scan_body_label, lv_color_make(184, 189, 198), 0);
        lv_label_set_long_mode(wifi_scan_body_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(wifi_scan_body_label, screen_w - 44);
    lv_obj_set_pos(wifi_scan_body_label, 22, 96);

    if (bottom_label == NULL)
    {
        bottom_label = lv_label_create(screen);
        lv_obj_set_style_text_font(bottom_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(bottom_label, lv_color_make(182, 187, 196), 0);
        lv_label_set_long_mode(bottom_label, LV_LABEL_LONG_CLIP);
    }
    lv_obj_set_width(bottom_label, 210);
    lv_obj_set_pos(bottom_label, 252, 224);

    apply_view_visibility(DRAWING_SCREEN_VIEW_NOW);
    draw_now_background(DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY);

    lv_label_set_text(header_time_label, "10:42 AM");
    lv_label_set_text(header_title_label, "St Charles, MO");
    lv_label_set_text(status_label, "Wi-Fi");
    lv_label_set_text(now_temp_label, "72°");
    lv_label_set_text(now_time_label, "10:42 AM");
    lv_label_set_text(now_condition_label, "FEELS 69°");
    lv_label_set_text(now_weather_label, "(Partly Cloudy)");
    lv_label_set_text(now_stats_1_label, "Indoor --°F");
    lv_label_set_text(now_stats_2_label, "--% RH");
    lv_label_set_text(now_stats_3_label, "-- hPa");
    lv_label_set_text(indoor_temp_label, "Indoor --.-°F");
    lv_label_set_text(indoor_humidity_label, "--% RH");
    lv_label_set_text(indoor_pressure_label, "-- hPa");
    lv_label_set_text(bottom_label, "(swipe right for indoor, left for forecast)");
    for (int i = 0; i < DRAWING_SCREEN_PREVIEW_DAYS; ++i)
    {
        lv_label_set_text(now_preview_labels[i], "Tue\n--°/--°");
    }

    for (int i = 0; i < FORECAST_ROWS; ++i)
    {
        lv_label_set_text(forecast_row_title_labels[i], MOCK_FORECAST_TITLES[i]);
        lv_label_set_text(forecast_row_detail_labels[i], FALLBACK_FORECAST_DETAILS[i]);
        lv_label_set_text(forecast_row_temp_labels[i], "--°");
    }
    lv_label_set_text(i2c_scan_title_label, "I2C Bus Scan");
    lv_label_set_text(i2c_scan_body_label, "Scan pending...");
    lv_label_set_text(wifi_scan_title_label, "Wi-Fi Networks");
    lv_label_set_text(wifi_scan_body_label, "Scan pending...");

    ESP_LOGI(DRAWING_TAG, "rendered mock-matched weather screen (%dx%d)", screen_w, screen_h);
}

void drawing_screen_render(const drawing_screen_data_t *data, const drawing_screen_dirty_t *dirty)
{
    if (data == NULL)
    {
        return;
    }

    bool refresh_header = (dirty == NULL) ? true : dirty->header;
    bool refresh_main = (dirty == NULL) ? true : dirty->main;
    bool refresh_stats = (dirty == NULL) ? true : dirty->stats;
    bool refresh_bottom = (dirty == NULL) ? true : dirty->bottom;

    bool view_changed = (data->view != current_view);
    if (view_changed)
    {
        current_view = data->view;
        apply_view_visibility(current_view);
        refresh_header = true;
        refresh_main = true;
        refresh_stats = true;
        refresh_bottom = true;
    }

    if (refresh_header)
    {
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            char signal[24] = {0};
            build_signal_text(data->status_text, signal, sizeof(signal));

            lv_label_set_text(header_time_label, text_or_fallback(data->time_text, "--:-- --"));
            lv_label_set_text(header_title_label, text_or_fallback(data->weather_text, "St Charles, MO"));
            lv_label_set_text(status_label, signal);
            lv_label_set_text(now_time_label, text_or_fallback(data->now_time_text, "--:--"));

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(header_title_label, LV_ALIGN_TOP_MID, 0, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -10, 8);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_INDOOR)
        {
            lv_label_set_text(header_time_label, "Indoor Sensor");
            lv_label_set_text(header_title_label, "");
            lv_label_set_text(status_label, "< Main  > Forecast");

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -12, 8);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_FORECAST)
        {
            if (data->forecast_hourly_open)
            {
                lv_label_set_text(header_time_label, text_or_fallback(data->forecast_hourly_day_title, "Hourly"));
                lv_label_set_text(status_label, "◀ Main");
            }
            else
            {
                lv_label_set_text(header_time_label, "Forecast");
                lv_label_set_text(status_label, "> I2C");
            }
            lv_label_set_text(header_title_label, "");

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -12, 8);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_I2C_SCAN)
        {
            lv_label_set_text(header_time_label, "I2C Scan");
            lv_label_set_text(header_title_label, "");
            lv_label_set_text(status_label, "> WiFi");

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -12, 8);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_WIFI_SCAN)
        {
            lv_label_set_text(header_time_label, "Wi-Fi Scan");
            lv_label_set_text(header_title_label, "");
            lv_label_set_text(status_label, "> About");

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -12, 8);
        }
        else
        {
            lv_label_set_text(header_time_label, "About");
            lv_label_set_text(header_title_label, "");
            lv_label_set_text(status_label, "> Main");

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -12, 8);
        }
    }

    if (refresh_main)
    {
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            char temp_compact[24] = {0};
            char feels_line[32] = {0};
            char condition_line[96] = {0};
            copy_temp_compact(data->temp_text, temp_compact, sizeof(temp_compact));
            build_feels_text(data->stats_line_1, feels_line, sizeof(feels_line));
            build_condition_text(data->condition_text, condition_line, sizeof(condition_line));

            draw_now_background(data->now_icon);

            lv_obj_set_pos(now_temp_label, 168, 72);
            lv_obj_set_pos(now_time_label, 336, 86);
            lv_obj_set_pos(now_condition_label, 168, 132);
            lv_obj_set_pos(now_weather_label, 168, 168);

            lv_label_set_text(now_temp_label, temp_compact);
            lv_label_set_text(now_time_label, text_or_fallback(data->now_time_text, "--:--"));
            lv_label_set_text(now_condition_label, feels_line);
            lv_label_set_text(now_weather_label, condition_line);

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(swipe: right Indoor | left Forecast)");
            for (int i = 0; i < DRAWING_SCREEN_PREVIEW_DAYS; ++i)
            {
                char row_line[40] = {0};
                const char *day = (data->forecast_preview_day[i] != NULL) ? data->forecast_preview_day[i] : "";
                const char *hi = (data->forecast_preview_hi[i] != NULL) ? data->forecast_preview_hi[i] : "--°";
                const char *low = (data->forecast_preview_low[i] != NULL) ? data->forecast_preview_low[i] : "--°";
                int card_x = 10 + i * (((screen_w - 40) / 3) + 10);
                if (i < data->forecast_preview_count && day[0] != '\0')
                {
                    snprintf(row_line, sizeof(row_line), "%s\n%s/%s", day, hi, low);
                    draw_icon_scaled(data->forecast_preview_icon[i], card_x + 10, 246, 44, 44);
                }
                else
                {
                    snprintf(row_line, sizeof(row_line), "--\n--°/--°");
                }
                lv_obj_set_pos(now_preview_labels[i], card_x + 58, 244);
                lv_label_set_text(now_preview_labels[i], row_line);
            }
        }
        else if (current_view == DRAWING_SCREEN_VIEW_INDOOR)
        {
            char indoor_temp[48] = {0};
            const char *line1 = text_or_fallback(data->indoor_line_1, "Indoor --.-°F");
            if (sscanf(line1, "Indoor %47[^\n]", indoor_temp) != 1)
            {
                snprintf(indoor_temp, sizeof(indoor_temp), "%s", line1);
            }

            draw_indoor_background();
            lv_obj_set_pos(indoor_temp_label, 24, 76);
            lv_obj_set_pos(indoor_humidity_label, 24, 154);
            lv_obj_set_pos(indoor_pressure_label, 24, 232);
            lv_label_set_text(indoor_temp_label, indoor_temp);
            lv_label_set_text(indoor_humidity_label, text_or_fallback(data->indoor_line_2, "--% RH"));
            lv_label_set_text(indoor_pressure_label, text_or_fallback(data->indoor_line_3, "-- hPa"));

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(BME280 live data)");
        }
        else if (current_view == DRAWING_SCREEN_VIEW_FORECAST)
        {
            draw_forecast_background();

            for (int i = 0; i < FORECAST_ROWS; ++i)
            {
                if (data->forecast_hourly_open)
                {
                    draw_icon_scaled(data->forecast_hourly_icon[i], 19, 62 + i * 64, 36, 34);
                    lv_label_set_text(forecast_row_title_labels[i], text_or_fallback(data->forecast_hourly_time[i], "--"));
                    lv_label_set_text(forecast_row_detail_labels[i], text_or_fallback(data->forecast_hourly_detail[i], ""));
                    lv_label_set_text(forecast_row_temp_labels[i], text_or_fallback(data->forecast_hourly_temp[i], "--°"));
                }
                else
                {
                    draw_icon_scaled(data->forecast_row_icon[i], 19, 62 + i * 64, 36, 34);
                    lv_label_set_text(forecast_row_title_labels[i], text_or_fallback(data->forecast_row_title[i], MOCK_FORECAST_TITLES[i]));
                    lv_label_set_text(forecast_row_detail_labels[i], text_or_fallback(data->forecast_row_detail[i], FALLBACK_FORECAST_DETAILS[i]));
                    lv_label_set_text(forecast_row_temp_labels[i], text_or_fallback(data->forecast_row_temp[i], "--°"));
                }
            }
            lv_obj_invalidate(canvas);

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            if (data->forecast_hourly_open)
            {
                lv_label_set_text(bottom_label, "(tap ◀ Main, swipe up/down hours, left/right pages)");
            }
            else
            {
                lv_label_set_text(bottom_label, "(tap a day for hourly, swipe left/right pages)");
            }
        }
        else if (current_view == DRAWING_SCREEN_VIEW_I2C_SCAN)
        {
            draw_i2c_background();
            lv_label_set_text(i2c_scan_title_label, "Detected Devices");
            lv_label_set_text(i2c_scan_body_label, text_or_fallback(data->i2c_scan_text, "I2C scan pending..."));

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(swipe left/right to switch pages)");
        }
        else if (current_view == DRAWING_SCREEN_VIEW_WIFI_SCAN)
        {
            draw_wifi_background();
            lv_label_set_text(wifi_scan_title_label, "Nearby Networks");
            lv_label_set_text(wifi_scan_body_label, text_or_fallback(data->wifi_scan_text, "Wi-Fi scan pending..."));

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(swipe left/right to switch pages)");
        }
        else
        {
            char about_body[320] = {0};
            draw_i2c_background();
            lv_label_set_text(i2c_scan_title_label, ABOUT_APP_NAME);
            snprintf(about_body, sizeof(about_body),
                     "Author: %s\n"
                     "GitHub: %s\n"
                     "Handle: %s\n"
                     "Version: %s",
                     ABOUT_AUTHOR,
                     ABOUT_GITHUB,
                     ABOUT_GITHUB_HANDLE,
                     app_version_string());
            lv_label_set_text(i2c_scan_body_label, about_body);

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(swipe left/right to switch pages)");
        }
    }

    if (refresh_stats)
    {
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            char feels_line[32] = {0};
            build_feels_text(data->stats_line_1, feels_line, sizeof(feels_line));
            lv_label_set_text(now_condition_label, feels_line);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_INDOOR)
        {
            lv_label_set_text(indoor_humidity_label, text_or_fallback(data->indoor_line_2, "--% RH"));
            lv_label_set_text(indoor_pressure_label, text_or_fallback(data->indoor_line_3, "-- hPa"));
        }
    }

    if (refresh_bottom)
    {
        if (current_view == DRAWING_SCREEN_VIEW_FORECAST && data->bottom_text != NULL && data->bottom_text[0] != '\0')
        {
            lv_label_set_text(bottom_label, data->bottom_text);
        }
    }
}
