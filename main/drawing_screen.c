#include "drawing_screen.h"

#define SQUARE_SIZE 220
#define X_MARGIN 24
#define X_WIDTH 14

lv_obj_t *canvas = NULL;
bool canvas_exit = false;

void drawing_screen_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *square = lv_obj_create(screen);
    lv_obj_clear_flag(square, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(square, SQUARE_SIZE, SQUARE_SIZE);
    lv_obj_align(square, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_radius(square, 0, 0);
    lv_obj_set_style_border_width(square, 0, 0);
    lv_obj_set_style_bg_color(square, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_opa(square, LV_OPA_COVER, 0);

    static lv_style_t x_style;
    lv_style_init(&x_style);
    lv_style_set_line_color(&x_style, lv_color_white());
    lv_style_set_line_width(&x_style, X_WIDTH);
    lv_style_set_line_rounded(&x_style, true);

    static lv_point_t x1_points[2] = {
        {X_MARGIN, X_MARGIN},
        {SQUARE_SIZE - X_MARGIN, SQUARE_SIZE - X_MARGIN},
    };
    static lv_point_t x2_points[2] = {
        {X_MARGIN, SQUARE_SIZE - X_MARGIN},
        {SQUARE_SIZE - X_MARGIN, X_MARGIN},
    };

    lv_obj_t *line1 = lv_line_create(square);
    lv_line_set_points(line1, x1_points, 2);
    lv_obj_add_style(line1, &x_style, LV_PART_MAIN);

    lv_obj_t *line2 = lv_line_create(square);
    lv_line_set_points(line2, x2_points, 2);
    lv_obj_add_style(line2, &x_style, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(screen);
    lv_label_set_text(label, "Graphics work so far!");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -20);
}
