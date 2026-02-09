#include "drawing_screen.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define ICON_W 128
#define ICON_H 128

static const char *TAG = "drawing_screen";

extern const uint8_t _binary_clear_day_128_rgb565_start[] asm("_binary_clear_day_128_rgb565_start");
extern const uint8_t _binary_clear_day_128_rgb565_end[] asm("_binary_clear_day_128_rgb565_end");

lv_obj_t *canvas = NULL;
bool canvas_exit = false;

static lv_color_t *canvas_buf = NULL;
static size_t canvas_buf_pixels = 0;
static int screen_w = 320;
static int screen_h = 480;

static drawing_screen_view_t current_view = DRAWING_SCREEN_VIEW_NOW;

static lv_obj_t *header_title_label = NULL;
static lv_obj_t *header_time_label = NULL;
static lv_obj_t *status_label = NULL;

static lv_obj_t *now_temp_label = NULL;
static lv_obj_t *now_condition_label = NULL;
static lv_obj_t *now_weather_label = NULL;
static lv_obj_t *now_stats_1_label = NULL;
static lv_obj_t *now_stats_2_label = NULL;
static lv_obj_t *now_stats_3_label = NULL;

static lv_obj_t *forecast_title_label = NULL;
static lv_obj_t *forecast_body_label = NULL;

static lv_obj_t *bottom_label = NULL;

static lv_color_t rgb565_to_lv_color(uint16_t rgb565)
{
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5) & 0x3F;
    uint8_t b5 = rgb565 & 0x1F;

    uint8_t r8 = (uint8_t)((r5 * 255) / 31);
    uint8_t g8 = (uint8_t)((g6 * 255) / 63);
    uint8_t b8 = (uint8_t)((b5 * 255) / 31);

    return lv_color_make(r8, g8, b8);
}

static const char *text_or_fallback(const char *text, const char *fallback)
{
    return (text != NULL && text[0] != '\0') ? text : fallback;
}

static void set_obj_hidden(lv_obj_t *obj, bool hidden)
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

static bool ensure_canvas_buffer(int w, int h)
{
    size_t needed_pixels = (size_t)w * (size_t)h;
    if (canvas_buf != NULL && canvas_buf_pixels == needed_pixels)
    {
        return true;
    }

    if (canvas_buf != NULL)
    {
        heap_caps_free(canvas_buf);
        canvas_buf = NULL;
        canvas_buf_pixels = 0;
    }

    size_t canvas_bytes = LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h);
    canvas_buf = (lv_color_t *)heap_caps_malloc(canvas_bytes, MALLOC_CAP_SPIRAM);
    if (canvas_buf == NULL)
    {
        ESP_LOGE(TAG, "failed to allocate canvas buffer (%u bytes)", (unsigned)canvas_bytes);
        return false;
    }

    canvas_buf_pixels = needed_pixels;
    return true;
}

static void fill_canvas(lv_color_t color)
{
    if (canvas_buf == NULL)
    {
        return;
    }
    for (size_t i = 0; i < canvas_buf_pixels; ++i)
    {
        canvas_buf[i] = color;
    }
}

static void fill_rect(int x, int y, int w, int h, lv_color_t color)
{
    if (canvas_buf == NULL || w <= 0 || h <= 0)
    {
        return;
    }

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;

    if (x1 > screen_w)
    {
        x1 = screen_w;
    }
    if (y1 > screen_h)
    {
        y1 = screen_h;
    }

    if (x0 >= x1 || y0 >= y1)
    {
        return;
    }

    for (int py = y0; py < y1; ++py)
    {
        size_t row = (size_t)py * (size_t)screen_w;
        for (int px = x0; px < x1; ++px)
        {
            canvas_buf[row + (size_t)px] = color;
        }
    }
}

static void stroke_rect(int x, int y, int w, int h, int stroke, lv_color_t color)
{
    if (stroke <= 0)
    {
        return;
    }

    fill_rect(x, y, w, stroke, color);
    fill_rect(x, y + h - stroke, w, stroke, color);
    fill_rect(x, y, stroke, h, color);
    fill_rect(x + w - stroke, y, stroke, h, color);
}

static void draw_icon(int dst_x, int dst_y)
{
    const uint8_t *icon = _binary_clear_day_128_rgb565_start;
    size_t icon_bytes = (size_t)(_binary_clear_day_128_rgb565_end - _binary_clear_day_128_rgb565_start);
    size_t expected = (size_t)ICON_W * ICON_H * 2;
    if (icon_bytes < expected)
    {
        ESP_LOGE(TAG, "icon asset too small: %u < %u", (unsigned)icon_bytes, (unsigned)expected);
        return;
    }

    for (int y = 0; y < ICON_H; ++y)
    {
        int py = dst_y + y;
        if (py < 0 || py >= screen_h)
        {
            continue;
        }

        for (int x = 0; x < ICON_W; ++x)
        {
            int px = dst_x + x;
            if (px < 0 || px >= screen_w)
            {
                continue;
            }

            size_t src = ((size_t)y * ICON_W + x) * 2;
            uint16_t rgb565 = (uint16_t)icon[src] | ((uint16_t)icon[src + 1] << 8);

            if (rgb565 == 0)
            {
                continue;
            }

            canvas_buf[(size_t)py * (size_t)screen_w + (size_t)px] = rgb565_to_lv_color(rgb565);
        }
    }
}

static void draw_now_background(void)
{
    lv_color_t bg = lv_color_make(10, 26, 38);
    lv_color_t panel_fill = lv_color_make(19, 42, 58);
    lv_color_t panel_stroke = lv_color_make(45, 78, 102);

    fill_canvas(bg);
    draw_icon(14, 70);

    // Right-side stats card stack area.
    fill_rect(screen_w - 170, 70, 156, 164, panel_fill);
    stroke_rect(screen_w - 170, 70, 156, 164, 2, panel_stroke);

    lv_obj_invalidate(canvas);
}

static void draw_forecast_background(void)
{
    lv_color_t bg = lv_color_make(8, 16, 28);
    lv_color_t card_fill = lv_color_make(16, 32, 50);
    lv_color_t card_stroke = lv_color_make(44, 72, 97);

    fill_canvas(bg);

    int left = 12;
    int gap = 10;
    int card_w = (screen_w - left * 2 - gap * 2);
    card_w /= 3;
    int card_h = 132;
    int top = 108;

    for (int i = 0; i < 3; ++i)
    {
        int x = left + i * (card_w + gap);
        fill_rect(x, top, card_w, card_h, card_fill);
        stroke_rect(x, top, card_w, card_h, 2, card_stroke);
    }

    lv_obj_invalidate(canvas);
}

static void apply_view_visibility(drawing_screen_view_t view)
{
    bool now_visible = (view == DRAWING_SCREEN_VIEW_NOW);

    set_obj_hidden(now_temp_label, !now_visible);
    set_obj_hidden(now_condition_label, !now_visible);
    set_obj_hidden(now_weather_label, !now_visible);
    set_obj_hidden(now_stats_1_label, !now_visible);
    set_obj_hidden(now_stats_2_label, !now_visible);
    set_obj_hidden(now_stats_3_label, !now_visible);

    set_obj_hidden(forecast_title_label, now_visible);
    set_obj_hidden(forecast_body_label, now_visible);
}

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

    lv_obj_set_style_bg_color(screen, lv_color_make(10, 26, 38), LV_PART_MAIN);
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

    if (header_title_label == NULL)
    {
        header_title_label = lv_label_create(screen);
        lv_obj_set_style_text_font(header_title_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(header_title_label, lv_color_white(), 0);
    }
    lv_obj_set_pos(header_title_label, 12, 8);

    if (header_time_label == NULL)
    {
        header_time_label = lv_label_create(screen);
        lv_obj_set_style_text_font(header_time_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(header_time_label, lv_palette_lighten(LV_PALETTE_BLUE, 4), 0);
    }
    lv_obj_align(header_time_label, LV_ALIGN_TOP_RIGHT, -12, 12);

    if (status_label == NULL)
    {
        status_label = lv_label_create(screen);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(status_label, lv_palette_lighten(LV_PALETTE_CYAN, 2), 0);
        lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(status_label, screen_w - 24);
    lv_obj_set_pos(status_label, 12, 36);

    if (now_temp_label == NULL)
    {
        now_temp_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_temp_label, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(now_temp_label, lv_color_make(255, 186, 48), 0);
    }
    lv_obj_set_pos(now_temp_label, 166, 86);

    if (now_condition_label == NULL)
    {
        now_condition_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_condition_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(now_condition_label, lv_color_white(), 0);
        lv_label_set_long_mode(now_condition_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(now_condition_label, screen_w - 180);
    lv_obj_set_pos(now_condition_label, 166, 146);

    if (now_stats_1_label == NULL)
    {
        now_stats_1_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_1_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_1_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    }
    lv_obj_set_pos(now_stats_1_label, 176, 184);

    if (now_stats_2_label == NULL)
    {
        now_stats_2_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_2_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_2_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    }
    lv_obj_set_pos(now_stats_2_label, 176, 206);

    if (now_stats_3_label == NULL)
    {
        now_stats_3_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_stats_3_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_stats_3_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    }
    lv_obj_set_pos(now_stats_3_label, 176, 228);

    if (now_weather_label == NULL)
    {
        now_weather_label = lv_label_create(screen);
        lv_obj_set_style_text_font(now_weather_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(now_weather_label, lv_color_white(), 0);
        lv_label_set_long_mode(now_weather_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(now_weather_label, screen_w - 24);
    lv_obj_set_pos(now_weather_label, 12, screen_h - 58);

    if (forecast_title_label == NULL)
    {
        forecast_title_label = lv_label_create(screen);
        lv_obj_set_style_text_font(forecast_title_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(forecast_title_label, lv_color_white(), 0);
    }
    lv_obj_set_pos(forecast_title_label, 12, 74);

    if (forecast_body_label == NULL)
    {
        forecast_body_label = lv_label_create(screen);
        lv_obj_set_style_text_font(forecast_body_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(forecast_body_label, lv_palette_lighten(LV_PALETTE_BLUE, 4), 0);
        lv_label_set_long_mode(forecast_body_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(forecast_body_label, screen_w - 24);
    lv_obj_set_pos(forecast_body_label, 12, 112);

    if (bottom_label == NULL)
    {
        bottom_label = lv_label_create(screen);
        lv_obj_set_style_text_font(bottom_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(bottom_label, lv_palette_lighten(LV_PALETTE_GREEN, 2), 0);
        lv_label_set_long_mode(bottom_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(bottom_label, screen_w - 24);
    lv_obj_set_pos(bottom_label, 12, screen_h - 24);

    apply_view_visibility(DRAWING_SCREEN_VIEW_NOW);
    draw_now_background();

    lv_label_set_text(header_title_label, "Weather Now");
    lv_label_set_text(header_time_label, "--:--:--");
    lv_label_set_text(status_label, "status: boot");
    lv_label_set_text(now_temp_label, "--\xC2\xB0""F");
    lv_label_set_text(now_condition_label, "waiting for weather");
    lv_label_set_text(now_stats_1_label, "Wind --");
    lv_label_set_text(now_stats_2_label, "Hum --");
    lv_label_set_text(now_stats_3_label, "Press --");
    lv_label_set_text(now_weather_label, "API weather pending...");
    lv_label_set_text(forecast_title_label, "Forecast (Skeleton)");
    lv_label_set_text(forecast_body_label, "No forecast data yet");
    lv_label_set_text(bottom_label, "Landscape build");

    ESP_LOGI(TAG, "rendered stateful weather screen (%dx%d)", screen_w, screen_h);
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
        char title[48] = {0};
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            snprintf(title, sizeof(title), "Weather Now");
        }
        else
        {
            snprintf(title, sizeof(title), "Forecast P%u", (unsigned)data->forecast_page + 1U);
        }

        lv_label_set_text(header_title_label, title);
        lv_label_set_text(header_time_label, text_or_fallback(data->time_text, "--:--:--"));
        lv_label_set_text(status_label, text_or_fallback(data->status_text, "status: idle"));
    }

    if (refresh_main)
    {
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            draw_now_background();
            lv_label_set_text(now_temp_label, text_or_fallback(data->temp_text, "--\xC2\xB0""F"));
            lv_label_set_text(now_condition_label, text_or_fallback(data->condition_text, "waiting for weather"));
            lv_label_set_text(now_weather_label, text_or_fallback(data->weather_text, "weather unavailable"));
        }
        else
        {
            draw_forecast_background();
            lv_label_set_text(forecast_title_label, text_or_fallback(data->forecast_title_text, "Forecast (Skeleton)"));
            lv_label_set_text(forecast_body_label, text_or_fallback(data->forecast_body_text, "Hourly endpoint pending"));
        }
    }

    if (refresh_stats)
    {
        if (current_view == DRAWING_SCREEN_VIEW_NOW)
        {
            lv_label_set_text(now_stats_1_label, text_or_fallback(data->stats_line_1, "Wind --"));
            lv_label_set_text(now_stats_2_label, text_or_fallback(data->stats_line_2, "Hum --"));
            lv_label_set_text(now_stats_3_label, text_or_fallback(data->stats_line_3, "Press --"));
        }
    }

    if (refresh_bottom)
    {
        lv_label_set_text(bottom_label, text_or_fallback(data->bottom_text, ""));
    }
}
