#ifndef __DRAWING_SCREEN_H__
#define __DRAWING_SCREEN_H__

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

extern lv_obj_t *canvas;
extern bool canvas_exit;

#define DRAWING_SCREEN_FORECAST_ROWS 4

typedef enum {
    DRAWING_SCREEN_VIEW_NOW = 0,
    DRAWING_SCREEN_VIEW_FORECAST = 1,
    DRAWING_SCREEN_VIEW_I2C_SCAN = 2,
    DRAWING_SCREEN_VIEW_WIFI_SCAN = 3,
} drawing_screen_view_t;

typedef enum {
    DRAWING_WEATHER_ICON_CLEAR_DAY = 0,
    DRAWING_WEATHER_ICON_CLEAR_NIGHT,
    DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY,
    DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT,
    DRAWING_WEATHER_ICON_CLOUDS,
    DRAWING_WEATHER_ICON_OVERCAST,
    DRAWING_WEATHER_ICON_SHOWER_RAIN,
    DRAWING_WEATHER_ICON_RAIN,
    DRAWING_WEATHER_ICON_THUNDERSTORM,
    DRAWING_WEATHER_ICON_SNOW,
    DRAWING_WEATHER_ICON_SLEET,
    DRAWING_WEATHER_ICON_MIST,
    DRAWING_WEATHER_ICON_FOG,
    DRAWING_WEATHER_ICON_COUNT,
} drawing_weather_icon_t;

typedef struct {
    bool header;
    bool main;
    bool stats;
    bool bottom;
} drawing_screen_dirty_t;

typedef struct {
    drawing_screen_view_t view;
    uint8_t forecast_page;
    bool forecast_hourly_open;
    uint8_t forecast_hourly_offset;
    uint8_t forecast_hourly_count;
    bool has_weather;
    const char *time_text;
    const char *now_time_text;
    const char *status_text;
    const char *temp_text;
    const char *condition_text;
    const char *weather_text;
    const char *stats_line_1;
    const char *stats_line_2;
    const char *stats_line_3;
    const char *indoor_line_1;
    const char *indoor_line_2;
    const char *indoor_line_3;
    drawing_weather_icon_t now_icon;
    const char *forecast_title_text;
    const char *forecast_body_text;
    const char *forecast_preview_text;
    const char *forecast_row_title[DRAWING_SCREEN_FORECAST_ROWS];
    const char *forecast_row_detail[DRAWING_SCREEN_FORECAST_ROWS];
    const char *forecast_row_temp[DRAWING_SCREEN_FORECAST_ROWS];
    drawing_weather_icon_t forecast_row_icon[DRAWING_SCREEN_FORECAST_ROWS];
    const char *forecast_hourly_day_title;
    const char *forecast_hourly_time[DRAWING_SCREEN_FORECAST_ROWS];
    const char *forecast_hourly_detail[DRAWING_SCREEN_FORECAST_ROWS];
    const char *forecast_hourly_temp[DRAWING_SCREEN_FORECAST_ROWS];
    drawing_weather_icon_t forecast_hourly_icon[DRAWING_SCREEN_FORECAST_ROWS];
    const char *i2c_scan_text;
    const char *wifi_scan_text;
    const char *bottom_text;
} drawing_screen_data_t;

#ifdef __cplusplus
extern "C" {
#endif

void drawing_screen_init(void);
void drawing_screen_render(const drawing_screen_data_t *data, const drawing_screen_dirty_t *dirty);

#ifdef __cplusplus
}
#endif

#endif
