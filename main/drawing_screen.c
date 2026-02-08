#include "drawing_screen.h"

#include <stdint.h>

#include "esp_heap_caps.h"
#include "esp_log.h"

#define SCREEN_W 320
#define SCREEN_H 480

#define ICON_W 128
#define ICON_H 128

static const char *TAG = "drawing_screen";
static const char *TEMP_TEXT = "68\xC2\xB0""F";

extern const uint8_t _binary_clear_day_128_rgb565_start[] asm("_binary_clear_day_128_rgb565_start");
extern const uint8_t _binary_clear_day_128_rgb565_end[] asm("_binary_clear_day_128_rgb565_end");

lv_obj_t *canvas = NULL;
bool canvas_exit = false;
static lv_color_t *canvas_buf = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *temp_label = NULL;
static lv_obj_t *hint_label = NULL;

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
        if (py < 0 || py >= SCREEN_H)
        {
            continue;
        }
        for (int x = 0; x < ICON_W; ++x)
        {
            int px = dst_x + x;
            if (px < 0 || px >= SCREEN_W)
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

            canvas_buf[(size_t)py * SCREEN_W + px] = rgb565_to_lv_color(rgb565);
        }
    }
}

void drawing_screen_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_make(10, 26, 38), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    if (canvas_buf == NULL)
    {
        size_t canvas_bytes = LV_CANVAS_BUF_SIZE_TRUE_COLOR(SCREEN_W, SCREEN_H);
        canvas_buf = (lv_color_t *)heap_caps_malloc(canvas_bytes, MALLOC_CAP_SPIRAM);
        if (canvas_buf == NULL)
        {
            ESP_LOGE(TAG, "failed to allocate canvas buffer in PSRAM");
            return;
        }
    }

    if (canvas == NULL)
    {
        canvas = lv_canvas_create(screen);
        lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    }
    lv_canvas_set_buffer(canvas, canvas_buf, SCREEN_W, SCREEN_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_size(canvas, SCREEN_W, SCREEN_H);
    lv_obj_align(canvas, LV_ALIGN_CENTER, 0, 0);
    lv_canvas_fill_bg(canvas, lv_color_make(10, 26, 38), LV_OPA_COVER);

    draw_icon((SCREEN_W - ICON_W) / 2, 58);

    if (title_label == NULL)
    {
        title_label = lv_label_create(screen);
        lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
        lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    }
    lv_label_set_text(title_label, "Asset Test Screen");
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 18);

    if (temp_label == NULL)
    {
        temp_label = lv_label_create(screen);
        lv_obj_set_style_text_color(temp_label, lv_color_make(255, 186, 48), 0);
        lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_48, 0);
    }
    lv_label_set_text(temp_label, TEMP_TEXT);
    lv_obj_align(temp_label, LV_ALIGN_TOP_MID, 0, 204);

    if (hint_label == NULL)
    {
        hint_label = lv_label_create(screen);
        lv_obj_set_style_text_color(hint_label, lv_palette_lighten(LV_PALETTE_BLUE, 3), 0);
        lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_16, 0);
    }
    lv_label_set_text(hint_label, "clear_day 128 + LVGL temp text");
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -20);

    ESP_LOGI(TAG, "rendered icon + LVGL temp text");
}
