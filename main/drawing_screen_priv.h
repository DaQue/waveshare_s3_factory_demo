#pragma once

#include <stddef.h>

#include "drawing_screen.h"

#define ICON_W 128
#define ICON_H 128
#define FORECAST_ROWS DRAWING_SCREEN_FORECAST_ROWS

extern const char *DRAWING_TAG;

extern lv_color_t *canvas_buf;
extern size_t canvas_buf_pixels;
extern int screen_w;
extern int screen_h;

extern drawing_screen_view_t current_view;

extern lv_obj_t *header_title_label;
extern lv_obj_t *header_time_label;
extern lv_obj_t *status_label;

extern lv_obj_t *now_temp_label;
extern lv_obj_t *now_time_label;
extern lv_obj_t *now_condition_label;
extern lv_obj_t *now_weather_label;
extern lv_obj_t *now_stats_1_label;
extern lv_obj_t *now_stats_2_label;
extern lv_obj_t *now_stats_3_label;
extern lv_obj_t *now_preview_labels[DRAWING_SCREEN_PREVIEW_DAYS];

extern lv_obj_t *forecast_row_title_labels[FORECAST_ROWS];
extern lv_obj_t *forecast_row_detail_labels[FORECAST_ROWS];
extern lv_obj_t *forecast_row_temp_labels[FORECAST_ROWS];
extern lv_obj_t *i2c_scan_title_label;
extern lv_obj_t *i2c_scan_body_label;
extern lv_obj_t *wifi_scan_title_label;
extern lv_obj_t *wifi_scan_body_label;

extern lv_obj_t *bottom_label;

const char *text_or_fallback(const char *text, const char *fallback);
void set_obj_hidden(lv_obj_t *obj, bool hidden);

bool ensure_canvas_buffer(int w, int h);
lv_color_t rgb565_to_lv_color(uint16_t rgb565);
void fill_rect(int x, int y, int w, int h, lv_color_t color);
void canvas_draw_card(int x, int y, int w, int h, int radius, lv_color_t fill, lv_color_t border, int border_w);
void draw_icon_scaled(drawing_weather_icon_t icon, int dst_x, int dst_y, int dst_w, int dst_h);

void build_signal_text(const char *status_text, char *out, size_t out_size);
void copy_temp_compact(const char *temp_text, char *out, size_t out_size);
void build_feels_text(const char *stats_line_1, char *out, size_t out_size);
void build_condition_text(const char *condition_text, char *out, size_t out_size);

void draw_now_background(drawing_weather_icon_t now_icon);
void draw_forecast_background(void);
void draw_i2c_background(void);
void draw_wifi_background(void);

void apply_view_visibility(drawing_screen_view_t view);
