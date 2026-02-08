#include "drawing_screen.h"

#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define ICON_W 128
#define ICON_H 128

static const char *TAG = "drawing_screen";
static const char *TEMP_TEXT_DEFAULT = "--\xC2\xB0""F";

extern const uint8_t _binary_clear_day_128_rgb565_start[] asm("_binary_clear_day_128_rgb565_start");
extern const uint8_t _binary_clear_day_128_rgb565_end[] asm("_binary_clear_day_128_rgb565_end");

lv_obj_t *canvas = NULL;
bool canvas_exit = false;

static lv_color_t *canvas_buf = NULL;
static size_t canvas_buf_pixels = 0;
static int screen_w = 320;
static int screen_h = 480;

static lv_obj_t *title_label = NULL;
static lv_obj_t *temp_label = NULL;
static lv_obj_t *hint_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *weather_label = NULL;

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

            // RGB565 icons were flattened over black; treat pure black as transparent.
            if (rgb565 == 0)
            {
                continue;
            }

            canvas_buf[(size_t)py * (size_t)screen_w + (size_t)px] = rgb565_to_lv_color(rgb565);
        }
    }
}

void drawing_screen_set_status(const char *status_text)
{
    if (status_label == NULL)
    {
        return;
    }
    lv_label_set_text(status_label, status_text ? status_text : "");
}

void drawing_screen_set_weather_text(const char *weather_text)
{
    if (weather_label == NULL)
    {
        return;
    }
    lv_label_set_text(weather_label, weather_text ? weather_text : "");
}

void drawing_screen_set_temp_text(const char *temp_text)
{
    if (temp_label == NULL)
    {
        return;
    }
    lv_label_set_text(temp_label, temp_text ? temp_text : TEMP_TEXT_DEFAULT);
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
    lv_canvas_fill_bg(canvas, lv_color_make(10, 26, 38), LV_OPA_COVER);

    bool is_landscape = (screen_w > screen_h);
    int icon_x = is_landscape ? 12 : (screen_w - ICON_W) / 2;
    int icon_y = is_landscape ? 62 : 58;
    draw_icon(icon_x, icon_y);

    if (title_label == NULL)
    {
        title_label = lv_label_create(screen);
        lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    }
    lv_label_set_text(title_label, "Network Weather Test");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 8);

    if (hint_label == NULL)
    {
        hint_label = lv_label_create(screen);
        lv_obj_set_style_text_color(hint_label, lv_palette_lighten(LV_PALETTE_BLUE, 3), 0);
        lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_16, 0);
    }
    lv_label_set_text(hint_label, is_landscape ? "Landscape mode: long lines" : "Portrait mode");
    lv_obj_align(hint_label, LV_ALIGN_TOP_MID, 0, 36);

    if (temp_label == NULL)
    {
        temp_label = lv_label_create(screen);
        lv_obj_set_style_text_color(temp_label, lv_color_make(255, 186, 48), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_48, 0);
    }
    lv_label_set_text(temp_label, TEMP_TEXT_DEFAULT);
    if (is_landscape)
    {
        int temp_x = icon_x + ICON_W + 16;
        if (temp_x > screen_w - 120)
        {
            temp_x = screen_w - 120;
        }
        lv_obj_set_pos(temp_label, temp_x, 92);
    }
    else
    {
        lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 204);
    }

    if (status_label == NULL)
    {
        status_label = lv_label_create(screen);
        lv_obj_set_style_text_color(status_label, lv_palette_lighten(LV_PALETTE_CYAN, 2), 0);
        lv_obj_set_style_text_font(status_label, &lv_font_montserrat_16, 0);
        lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(status_label, screen_w - 20);
    lv_label_set_text(status_label, "status: boot");
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 10, is_landscape ? -84 : -96);

    if (weather_label == NULL)
    {
        weather_label = lv_label_create(screen);
        lv_obj_set_style_text_color(weather_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(weather_label, &lv_font_montserrat_16, 0);
        lv_label_set_long_mode(weather_label, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_width(weather_label, screen_w - 20);
    lv_label_set_text(weather_label, "weather: waiting...");
    lv_obj_align(weather_label, LV_ALIGN_BOTTOM_LEFT, 10, -16);

    ESP_LOGI(TAG, "rendered weather test scene (%dx%d)", screen_w, screen_h);
}
