#include "drawing_screen.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define ICON_W 128
#define ICON_H 128
#define FORECAST_ROWS DRAWING_SCREEN_FORECAST_ROWS

static const char *TAG = "drawing_screen";

extern const uint8_t _binary_clear_day_128_rgb565_start[] asm("_binary_clear_day_128_rgb565_start");
extern const uint8_t _binary_clear_day_128_rgb565_end[] asm("_binary_clear_day_128_rgb565_end");
extern const uint8_t _binary_clear_night_128_rgb565_start[] asm("_binary_clear_night_128_rgb565_start");
extern const uint8_t _binary_clear_night_128_rgb565_end[] asm("_binary_clear_night_128_rgb565_end");
extern const uint8_t _binary_few_clouds_day_128_rgb565_start[] asm("_binary_few_clouds_day_128_rgb565_start");
extern const uint8_t _binary_few_clouds_day_128_rgb565_end[] asm("_binary_few_clouds_day_128_rgb565_end");
extern const uint8_t _binary_few_clouds_night_128_rgb565_start[] asm("_binary_few_clouds_night_128_rgb565_start");
extern const uint8_t _binary_few_clouds_night_128_rgb565_end[] asm("_binary_few_clouds_night_128_rgb565_end");
extern const uint8_t _binary_clouds_128_rgb565_start[] asm("_binary_clouds_128_rgb565_start");
extern const uint8_t _binary_clouds_128_rgb565_end[] asm("_binary_clouds_128_rgb565_end");
extern const uint8_t _binary_overcast_128_rgb565_start[] asm("_binary_overcast_128_rgb565_start");
extern const uint8_t _binary_overcast_128_rgb565_end[] asm("_binary_overcast_128_rgb565_end");
extern const uint8_t _binary_shower_rain_128_rgb565_start[] asm("_binary_shower_rain_128_rgb565_start");
extern const uint8_t _binary_shower_rain_128_rgb565_end[] asm("_binary_shower_rain_128_rgb565_end");
extern const uint8_t _binary_rain_128_rgb565_start[] asm("_binary_rain_128_rgb565_start");
extern const uint8_t _binary_rain_128_rgb565_end[] asm("_binary_rain_128_rgb565_end");
extern const uint8_t _binary_thunderstorm_128_rgb565_start[] asm("_binary_thunderstorm_128_rgb565_start");
extern const uint8_t _binary_thunderstorm_128_rgb565_end[] asm("_binary_thunderstorm_128_rgb565_end");
extern const uint8_t _binary_snow_128_rgb565_start[] asm("_binary_snow_128_rgb565_start");
extern const uint8_t _binary_snow_128_rgb565_end[] asm("_binary_snow_128_rgb565_end");
extern const uint8_t _binary_sleet_128_rgb565_start[] asm("_binary_sleet_128_rgb565_start");
extern const uint8_t _binary_sleet_128_rgb565_end[] asm("_binary_sleet_128_rgb565_end");
extern const uint8_t _binary_mist_128_rgb565_start[] asm("_binary_mist_128_rgb565_start");
extern const uint8_t _binary_mist_128_rgb565_end[] asm("_binary_mist_128_rgb565_end");
extern const uint8_t _binary_fog_128_rgb565_start[] asm("_binary_fog_128_rgb565_start");
extern const uint8_t _binary_fog_128_rgb565_end[] asm("_binary_fog_128_rgb565_end");

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

static lv_obj_t *forecast_row_title_labels[FORECAST_ROWS] = {0};
static lv_obj_t *forecast_row_detail_labels[FORECAST_ROWS] = {0};
static lv_obj_t *forecast_row_temp_labels[FORECAST_ROWS] = {0};
static lv_obj_t *i2c_scan_title_label = NULL;
static lv_obj_t *i2c_scan_body_label = NULL;
static lv_obj_t *wifi_scan_title_label = NULL;
static lv_obj_t *wifi_scan_body_label = NULL;

static lv_obj_t *bottom_label = NULL;

typedef struct {
    const uint8_t *start;
    const uint8_t *end;
} icon_asset_t;

static const icon_asset_t ICON_ASSETS[DRAWING_WEATHER_ICON_COUNT] = {
    [DRAWING_WEATHER_ICON_CLEAR_DAY] = {_binary_clear_day_128_rgb565_start, _binary_clear_day_128_rgb565_end},
    [DRAWING_WEATHER_ICON_CLEAR_NIGHT] = {_binary_clear_night_128_rgb565_start, _binary_clear_night_128_rgb565_end},
    [DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY] = {_binary_few_clouds_day_128_rgb565_start, _binary_few_clouds_day_128_rgb565_end},
    [DRAWING_WEATHER_ICON_FEW_CLOUDS_NIGHT] = {_binary_few_clouds_night_128_rgb565_start, _binary_few_clouds_night_128_rgb565_end},
    [DRAWING_WEATHER_ICON_CLOUDS] = {_binary_clouds_128_rgb565_start, _binary_clouds_128_rgb565_end},
    [DRAWING_WEATHER_ICON_OVERCAST] = {_binary_overcast_128_rgb565_start, _binary_overcast_128_rgb565_end},
    [DRAWING_WEATHER_ICON_SHOWER_RAIN] = {_binary_shower_rain_128_rgb565_start, _binary_shower_rain_128_rgb565_end},
    [DRAWING_WEATHER_ICON_RAIN] = {_binary_rain_128_rgb565_start, _binary_rain_128_rgb565_end},
    [DRAWING_WEATHER_ICON_THUNDERSTORM] = {_binary_thunderstorm_128_rgb565_start, _binary_thunderstorm_128_rgb565_end},
    [DRAWING_WEATHER_ICON_SNOW] = {_binary_snow_128_rgb565_start, _binary_snow_128_rgb565_end},
    [DRAWING_WEATHER_ICON_SLEET] = {_binary_sleet_128_rgb565_start, _binary_sleet_128_rgb565_end},
    [DRAWING_WEATHER_ICON_MIST] = {_binary_mist_128_rgb565_start, _binary_mist_128_rgb565_end},
    [DRAWING_WEATHER_ICON_FOG] = {_binary_fog_128_rgb565_start, _binary_fog_128_rgb565_end},
};

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

static void fill_rect(int x, int y, int w, int h, lv_color_t color)
{
    if (canvas_buf == NULL || w <= 0 || h <= 0)
    {
        return;
    }

    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
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

static void canvas_draw_card(int x, int y, int w, int h, int radius, lv_color_t fill, lv_color_t border, int border_w)
{
    if (canvas == NULL)
    {
        return;
    }

    lv_draw_rect_dsc_t rect = {};
    lv_draw_rect_dsc_init(&rect);
    rect.radius = radius;
    rect.bg_opa = LV_OPA_COVER;
    rect.bg_color = fill;
    rect.border_opa = (border_w > 0) ? LV_OPA_COVER : LV_OPA_TRANSP;
    rect.border_width = border_w;
    rect.border_color = border;
    lv_canvas_draw_rect(canvas, x, y, w, h, &rect);
}

static const icon_asset_t *get_icon_asset(drawing_weather_icon_t icon)
{
    if (icon < 0 || icon >= DRAWING_WEATHER_ICON_COUNT)
    {
        return &ICON_ASSETS[DRAWING_WEATHER_ICON_CLEAR_DAY];
    }
    return &ICON_ASSETS[icon];
}

static void draw_icon_scaled(drawing_weather_icon_t icon, int dst_x, int dst_y, int dst_w, int dst_h)
{
    if (canvas_buf == NULL || dst_w <= 0 || dst_h <= 0)
    {
        return;
    }

    const icon_asset_t *asset = get_icon_asset(icon);
    const uint8_t *icon_data = asset->start;
    size_t icon_bytes = (size_t)(asset->end - asset->start);
    size_t expected = (size_t)ICON_W * ICON_H * 2;
    if (icon_data == NULL || icon_bytes < expected)
    {
        ESP_LOGE(TAG, "icon asset invalid: %u < %u", (unsigned)icon_bytes, (unsigned)expected);
        return;
    }

    for (int y = 0; y < dst_h; ++y)
    {
        int py = dst_y + y;
        if (py < 0 || py >= screen_h)
        {
            continue;
        }

        int src_y = (y * ICON_H) / dst_h;
        for (int x = 0; x < dst_w; ++x)
        {
            int px = dst_x + x;
            if (px < 0 || px >= screen_w)
            {
                continue;
            }

            int src_x = (x * ICON_W) / dst_w;
            size_t src = ((size_t)src_y * ICON_W + (size_t)src_x) * 2;
            uint16_t rgb565 = (uint16_t)icon_data[src] | ((uint16_t)icon_data[src + 1] << 8);
            if (rgb565 == 0)
            {
                continue;
            }

            canvas_buf[(size_t)py * (size_t)screen_w + (size_t)px] = rgb565_to_lv_color(rgb565);
        }
    }
}

static void build_signal_text(const char *status_text, char *out, size_t out_size)
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

static void copy_temp_compact(const char *temp_text, char *out, size_t out_size)
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

static void build_feels_text(const char *stats_line_1, char *out, size_t out_size)
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

static void build_condition_text(const char *condition_text, char *out, size_t out_size)
{
    snprintf(out, out_size, "(%s)", text_or_fallback(condition_text, "Partly Cloudy"));
}

static void draw_now_background(drawing_weather_icon_t now_icon)
{
    lv_color_t bg = lv_color_make(27, 31, 39);
    lv_color_t line = lv_color_make(56, 63, 76);
    lv_color_t card_fill = lv_color_make(20, 25, 35);
    lv_color_t card_border = lv_color_make(63, 75, 95);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);

    fill_rect(0, 34, screen_w, 1, line);
    fill_rect(0, 44, screen_w, 1, lv_color_make(45, 52, 64));

    canvas_draw_card(10, 52, screen_w - 20, 178, 14, card_fill, card_border, 2);
    draw_icon_scaled(now_icon, 34, 74, 110, 110);

    fill_rect(0, 236, screen_w, 1, line);
    fill_rect(screen_w / 2, 236, 1, screen_h - 236, line);

    lv_obj_invalidate(canvas);
}

static void draw_forecast_background(void)
{
    lv_color_t bg = lv_color_make(27, 31, 39);
    lv_color_t line = lv_color_make(56, 63, 76);
    lv_color_t card_fill = lv_color_make(24, 29, 39);
    lv_color_t card_border = lv_color_make(63, 75, 95);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    fill_rect(0, 34, screen_w, 1, line);

    for (int i = 0; i < FORECAST_ROWS; ++i)
    {
        int y = 52 + i * 64;
        canvas_draw_card(10, y, screen_w - 20, 56, 14, card_fill, card_border, 2);
    }

    lv_obj_invalidate(canvas);
}

static void draw_i2c_background(void)
{
    lv_color_t bg = lv_color_make(27, 31, 39);
    lv_color_t line = lv_color_make(56, 63, 76);
    lv_color_t card_fill = lv_color_make(22, 27, 37);
    lv_color_t card_border = lv_color_make(63, 75, 95);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    fill_rect(0, 34, screen_w, 1, line);
    canvas_draw_card(10, 52, screen_w - 20, screen_h - 86, 14, card_fill, card_border, 2);
    lv_obj_invalidate(canvas);
}

static void draw_wifi_background(void)
{
    lv_color_t bg = lv_color_make(24, 30, 39);
    lv_color_t line = lv_color_make(58, 70, 84);
    lv_color_t card_fill = lv_color_make(20, 29, 40);
    lv_color_t card_border = lv_color_make(66, 86, 108);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    fill_rect(0, 34, screen_w, 1, line);
    canvas_draw_card(10, 52, screen_w - 20, screen_h - 86, 14, card_fill, card_border, 2);
    lv_obj_invalidate(canvas);
}

static void apply_view_visibility(drawing_screen_view_t view)
{
    bool now_visible = (view == DRAWING_SCREEN_VIEW_NOW);
    bool forecast_visible = (view == DRAWING_SCREEN_VIEW_FORECAST);
    bool i2c_visible = (view == DRAWING_SCREEN_VIEW_I2C_SCAN);
    bool wifi_visible = (view == DRAWING_SCREEN_VIEW_WIFI_SCAN);

    set_obj_hidden(now_temp_label, !now_visible);
    set_obj_hidden(now_condition_label, !now_visible);
    set_obj_hidden(now_weather_label, !now_visible);
    set_obj_hidden(now_stats_1_label, !now_visible);
    set_obj_hidden(now_stats_2_label, !now_visible);
    set_obj_hidden(now_stats_3_label, !now_visible);

    for (int i = 0; i < FORECAST_ROWS; ++i)
    {
        set_obj_hidden(forecast_row_title_labels[i], !forecast_visible);
        set_obj_hidden(forecast_row_detail_labels[i], !forecast_visible);
        set_obj_hidden(forecast_row_temp_labels[i], !forecast_visible);
    }

    set_obj_hidden(i2c_scan_title_label, !i2c_visible);
    set_obj_hidden(i2c_scan_body_label, !i2c_visible);
    set_obj_hidden(wifi_scan_title_label, !wifi_visible);
    set_obj_hidden(wifi_scan_body_label, !wifi_visible);
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
        lv_obj_set_style_text_font(now_stats_1_label, &lv_font_montserrat_20, 0);
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
    lv_obj_set_pos(now_stats_3_label, 16, 300);

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
    lv_obj_set_width(bottom_label, 220);
    lv_obj_set_pos(bottom_label, 250, 246);

    apply_view_visibility(DRAWING_SCREEN_VIEW_NOW);
    draw_now_background(DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY);

    lv_label_set_text(header_time_label, "10:42 AM");
    lv_label_set_text(header_title_label, "St Charles, MO");
    lv_label_set_text(status_label, "Wi-Fi");
    lv_label_set_text(now_temp_label, "72°");
    lv_label_set_text(now_condition_label, "FEELS 69°");
    lv_label_set_text(now_weather_label, "(Partly Cloudy)");
    lv_label_set_text(now_stats_1_label, "Indoor --°F");
    lv_label_set_text(now_stats_2_label, "--% RH");
    lv_label_set_text(now_stats_3_label, "-- hPa");
    lv_label_set_text(bottom_label, "Forecast >\nTue --°   Wed --°   Thu --°");

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

    ESP_LOGI(TAG, "rendered mock-matched weather screen (%dx%d)", screen_w, screen_h);
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

            lv_obj_set_pos(header_time_label, 14, 4);
            lv_obj_align(header_title_label, LV_ALIGN_TOP_MID, 0, 4);
            lv_obj_align(status_label, LV_ALIGN_TOP_RIGHT, -10, 8);
        }
        else if (current_view == DRAWING_SCREEN_VIEW_FORECAST)
        {
            lv_label_set_text(header_time_label, "Forecast");
            lv_label_set_text(header_title_label, "");
            lv_label_set_text(status_label, "> I2C");

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
        else
        {
            lv_label_set_text(header_time_label, "Wi-Fi Scan");
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

            lv_obj_set_pos(now_temp_label, 174, 80);
            lv_obj_set_pos(now_condition_label, 182, 145);
            lv_obj_set_pos(now_weather_label, 172, 178);

            lv_label_set_text(now_temp_label, temp_compact);
            lv_label_set_text(now_condition_label, feels_line);
            lv_label_set_text(now_weather_label, condition_line);
            lv_label_set_text(now_stats_1_label, text_or_fallback(data->indoor_line_1, "Indoor --°F"));
            lv_label_set_text(now_stats_2_label, text_or_fallback(data->indoor_line_2, "--% RH"));
            lv_label_set_text(now_stats_3_label, text_or_fallback(data->indoor_line_3, "-- hPa"));

            lv_obj_set_width(bottom_label, 220);
            lv_obj_set_pos(bottom_label, 250, 246);
            if (data->forecast_preview_text != NULL && data->forecast_preview_text[0] != '\0')
            {
                char preview_line[160] = {0};
                snprintf(preview_line, sizeof(preview_line), "Forecast >\n%s", data->forecast_preview_text);
                lv_label_set_text(bottom_label, preview_line);
            }
            else
            {
                lv_label_set_text(bottom_label, "Forecast >\nTue --°   Wed --°   Thu --°");
            }
        }
        else if (current_view == DRAWING_SCREEN_VIEW_FORECAST)
        {
            draw_forecast_background();

            for (int i = 0; i < FORECAST_ROWS; ++i)
            {
                draw_icon_scaled(data->forecast_row_icon[i], 19, 62 + i * 64, 36, 34);
                lv_label_set_text(forecast_row_title_labels[i], text_or_fallback(data->forecast_row_title[i], MOCK_FORECAST_TITLES[i]));
                lv_label_set_text(forecast_row_detail_labels[i], text_or_fallback(data->forecast_row_detail[i], FALLBACK_FORECAST_DETAILS[i]));
                lv_label_set_text(forecast_row_temp_labels[i], text_or_fallback(data->forecast_row_temp[i], "--°"));
            }
            lv_obj_invalidate(canvas);

            lv_obj_set_width(bottom_label, screen_w - 24);
            lv_obj_set_pos(bottom_label, 12, screen_h - 22);
            lv_label_set_text(bottom_label, "(swipe left/right or tap to switch pages)");
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
        else
        {
            draw_wifi_background();
            lv_label_set_text(wifi_scan_title_label, "Nearby Networks");
            lv_label_set_text(wifi_scan_body_label, text_or_fallback(data->wifi_scan_text, "Wi-Fi scan pending..."));

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
            lv_label_set_text(now_stats_1_label, text_or_fallback(data->indoor_line_1, "Indoor --°F"));
            lv_label_set_text(now_stats_2_label, text_or_fallback(data->indoor_line_2, "--% RH"));
            lv_label_set_text(now_stats_3_label, text_or_fallback(data->indoor_line_3, "-- hPa"));
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
