#include "drawing_screen_priv.h"

#include <stdint.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

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

bool ensure_canvas_buffer(int w, int h)
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
        ESP_LOGE(DRAWING_TAG, "failed to allocate canvas buffer (%u bytes)", (unsigned)canvas_bytes);
        return false;
    }

    canvas_buf_pixels = needed_pixels;
    return true;
}

lv_color_t rgb565_to_lv_color(uint16_t rgb565)
{
    uint8_t r5 = (rgb565 >> 11) & 0x1F;
    uint8_t g6 = (rgb565 >> 5) & 0x3F;
    uint8_t b5 = rgb565 & 0x1F;

    uint8_t r8 = (uint8_t)((r5 * 255) / 31);
    uint8_t g8 = (uint8_t)((g6 * 255) / 63);
    uint8_t b8 = (uint8_t)((b5 * 255) / 31);

    return lv_color_make(r8, g8, b8);
}

void fill_rect(int x, int y, int w, int h, lv_color_t color)
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

void canvas_draw_card(int x, int y, int w, int h, int radius, lv_color_t fill, lv_color_t border, int border_w)
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

void draw_icon_scaled(drawing_weather_icon_t icon, int dst_x, int dst_y, int dst_w, int dst_h)
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
        ESP_LOGE(DRAWING_TAG, "icon asset invalid: %u < %u", (unsigned)icon_bytes, (unsigned)expected);
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

void draw_now_background(drawing_weather_icon_t now_icon)
{
    lv_color_t bg = lv_color_make(27, 31, 39);
    lv_color_t line = lv_color_make(56, 63, 76);
    lv_color_t card_fill = lv_color_make(20, 25, 35);
    lv_color_t card_border = lv_color_make(63, 75, 95);
    lv_color_t forecast_fill = lv_color_make(23, 29, 40);
    lv_color_t forecast_border = lv_color_make(66, 86, 108);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);

    fill_rect(0, 34, screen_w, 1, line);
    fill_rect(0, 44, screen_w, 1, lv_color_make(45, 52, 64));

    canvas_draw_card(10, 52, screen_w - 20, 164, 14, card_fill, card_border, 2);
    draw_icon_scaled(now_icon, 30, 72, 118, 118);

    fill_rect(0, 224, screen_w, 1, line);

    const int card_w = (screen_w - 40) / 3;
    const int gap = 10;
    for (int i = 0; i < 3; ++i)
    {
        int x = 10 + i * (card_w + gap);
        canvas_draw_card(x, 232, card_w, 80, 12, forecast_fill, forecast_border, 2);
    }

    lv_obj_invalidate(canvas);
}

void draw_indoor_background(void)
{
    lv_color_t bg = lv_color_make(22, 28, 38);
    lv_color_t line = lv_color_make(58, 70, 84);
    lv_color_t card_fill = lv_color_make(20, 29, 40);
    lv_color_t card_border = lv_color_make(66, 86, 108);

    lv_canvas_fill_bg(canvas, bg, LV_OPA_COVER);
    fill_rect(0, 34, screen_w, 1, line);
    canvas_draw_card(10, 52, screen_w - 20, screen_h - 64, 16, card_fill, card_border, 2);
    lv_obj_invalidate(canvas);
}

void draw_forecast_background(void)
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

void draw_i2c_background(void)
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

void draw_wifi_background(void)
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
