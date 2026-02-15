#include "drawing_screen_priv.h"

#include <stdio.h>
#include <string.h>

const char *text_or_fallback(const char *text, const char *fallback)
{
    return (text != NULL && text[0] != '\0') ? text : fallback;
}

void set_obj_hidden(lv_obj_t *obj, bool hidden)
{
    if (obj == NULL)
    {
        return;
    }
    if (hidden)
    {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void build_signal_text(const char *status_text, char *out, size_t out_size)
{
    const char *status = text_or_fallback(status_text, "");
    if (strstr(status, "sync: ok") != NULL || strstr(status, "connected ip") != NULL)
    {
        snprintf(out, out_size, "Wi-Fi");
        return;
    }
    if (strstr(status, "connecting") != NULL)
    {
        snprintf(out, out_size, "...");
        return;
    }
    if (strstr(status, "timeout") != NULL || strstr(status, "error") != NULL || strstr(status, "failed") != NULL)
    {
        snprintf(out, out_size, "offline");
        return;
    }
    snprintf(out, out_size, "--");
}

void copy_temp_compact(const char *temp_text, char *out, size_t out_size)
{
    if (out == NULL || out_size == 0)
    {
        return;
    }

    out[0] = '\0';
    if (temp_text == NULL || temp_text[0] == '\0')
    {
        snprintf(out, out_size, "--°");
        return;
    }

    size_t j = 0;
    for (size_t i = 0; temp_text[i] != '\0' && j < (out_size - 1); ++i)
    {
        char c = temp_text[i];
        if (c == 'F' || c == 'C' || c == ' ')
        {
            continue;
        }
        out[j++] = c;
    }
    out[j] = '\0';
    if (out[0] == '\0')
    {
        snprintf(out, out_size, "--°");
    }
}

void build_feels_text(const char *stats_line_1, char *out, size_t out_size)
{
    int feels = 0;
    if (stats_line_1 != NULL)
    {
        const char *marker = strstr(stats_line_1, "Feels ");
        if (marker != NULL && sscanf(marker + 6, "%d", &feels) == 1)
        {
            snprintf(out, out_size, "FEELS %d°", feels);
            return;
        }
    }
    snprintf(out, out_size, "FEELS --°");
}

void build_condition_text(const char *condition_text, char *out, size_t out_size)
{
    snprintf(out, out_size, "(%s)", text_or_fallback(condition_text, "Partly Cloudy"));
}

void apply_view_visibility(drawing_screen_view_t view)
{
    bool now_visible = (view == DRAWING_SCREEN_VIEW_NOW);
    bool indoor_visible = (view == DRAWING_SCREEN_VIEW_INDOOR);
    bool forecast_visible = (view == DRAWING_SCREEN_VIEW_FORECAST);
    bool i2c_visible = (view == DRAWING_SCREEN_VIEW_I2C_SCAN);
    bool wifi_visible = (view == DRAWING_SCREEN_VIEW_WIFI_SCAN);
    bool about_visible = (view == DRAWING_SCREEN_VIEW_ABOUT);

    set_obj_hidden(now_temp_label, !now_visible);
    set_obj_hidden(now_time_label, !now_visible);
    set_obj_hidden(now_condition_label, !now_visible);
    set_obj_hidden(now_weather_label, !now_visible);
    set_obj_hidden(now_stats_1_label, true);
    set_obj_hidden(now_stats_2_label, true);
    set_obj_hidden(now_stats_3_label, true);
    for (int i = 0; i < DRAWING_SCREEN_PREVIEW_DAYS; ++i)
    {
        set_obj_hidden(now_preview_labels[i], !now_visible);
    }
    set_obj_hidden(indoor_temp_label, !indoor_visible);
    set_obj_hidden(indoor_humidity_label, !indoor_visible);
    set_obj_hidden(indoor_pressure_label, !indoor_visible);

    for (int i = 0; i < FORECAST_ROWS; ++i)
    {
        set_obj_hidden(forecast_row_title_labels[i], !forecast_visible);
        set_obj_hidden(forecast_row_detail_labels[i], !forecast_visible);
        set_obj_hidden(forecast_row_temp_labels[i], !forecast_visible);
    }

    set_obj_hidden(i2c_scan_title_label, !(i2c_visible || about_visible));
    set_obj_hidden(i2c_scan_body_label, !(i2c_visible || about_visible));
    set_obj_hidden(wifi_scan_title_label, !wifi_visible);
    set_obj_hidden(wifi_scan_body_label, !wifi_visible);
}
