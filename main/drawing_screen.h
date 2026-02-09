#ifndef __DRAWING_SCREEN_H__
#define __DRAWING_SCREEN_H__

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

extern lv_obj_t *canvas;
extern bool canvas_exit;

typedef enum {
    DRAWING_SCREEN_VIEW_NOW = 0,
    DRAWING_SCREEN_VIEW_FORECAST = 1,
} drawing_screen_view_t;

typedef struct {
    bool header;
    bool main;
    bool stats;
    bool bottom;
} drawing_screen_dirty_t;

typedef struct {
    drawing_screen_view_t view;
    uint8_t forecast_page;
    bool has_weather;
    const char *time_text;
    const char *status_text;
    const char *temp_text;
    const char *condition_text;
    const char *weather_text;
    const char *stats_line_1;
    const char *stats_line_2;
    const char *stats_line_3;
    const char *forecast_title_text;
    const char *forecast_body_text;
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
