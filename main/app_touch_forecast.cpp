#include "app_priv.h"

static int app_forecast_row_from_y(int16_t y)
{
    const int row_top = 52;
    const int row_stride = 64;
    const int row_card_h = 56;
    if (y < row_top || y >= row_top + APP_FORECAST_ROWS * row_stride)
    {
        return -1;
    }
    int rel_y = y - row_top;
    int row = rel_y / row_stride;
    int row_y = rel_y % row_stride;
    if (row_y > row_card_h)
    {
        return -1;
    }
    return row;
}

static bool app_is_hourly_close_tap(int16_t x, int16_t y)
{
    if (y < 0 || y >= 42)
    {
        return false;
    }

    lv_disp_t *disp = lv_disp_get_default();
    int screen_w = (disp != NULL) ? (int)lv_disp_get_hor_res(disp) : EXAMPLE_LCD_V_RES;
    int close_x_min = screen_w - 144;
    if (close_x_min < 0)
    {
        close_x_min = 0;
    }
    return x >= close_x_min;
}

static bool app_handle_edge_nav_tap(int16_t x, int16_t y)
{
    lv_disp_t *disp = lv_disp_get_default();
    int screen_w = (disp != NULL) ? (int)lv_disp_get_hor_res(disp) : EXAMPLE_LCD_V_RES;
    int screen_h = (disp != NULL) ? (int)lv_disp_get_ver_res(disp) : EXAMPLE_LCD_H_RES;
    if (screen_w <= 0 || screen_h <= 0)
    {
        return false;
    }

    // Bottom edge tap fallback: left corner = previous page, right corner = next page.
    if (y < screen_h - 60)
    {
        return false;
    }

    int step = 0;
    if (x <= 60)
    {
        step = -1;
    }
    else if (x >= (screen_w - 60))
    {
        step = 1;
    }
    if (step == 0)
    {
        return false;
    }

    auto next_view = [](drawing_screen_view_t v) {
        switch (v)
        {
        case DRAWING_SCREEN_VIEW_NOW:
            return DRAWING_SCREEN_VIEW_FORECAST;
        case DRAWING_SCREEN_VIEW_INDOOR:
            return DRAWING_SCREEN_VIEW_NOW;
        case DRAWING_SCREEN_VIEW_FORECAST:
            return DRAWING_SCREEN_VIEW_I2C_SCAN;
        case DRAWING_SCREEN_VIEW_I2C_SCAN:
            return DRAWING_SCREEN_VIEW_WIFI_SCAN;
        case DRAWING_SCREEN_VIEW_WIFI_SCAN:
            return DRAWING_SCREEN_VIEW_ABOUT;
        case DRAWING_SCREEN_VIEW_ABOUT:
        default:
            return DRAWING_SCREEN_VIEW_INDOOR;
        }
    };
    auto prev_view = [](drawing_screen_view_t v) {
        switch (v)
        {
        case DRAWING_SCREEN_VIEW_NOW:
            return DRAWING_SCREEN_VIEW_INDOOR;
        case DRAWING_SCREEN_VIEW_INDOOR:
            return DRAWING_SCREEN_VIEW_ABOUT;
        case DRAWING_SCREEN_VIEW_FORECAST:
            return DRAWING_SCREEN_VIEW_NOW;
        case DRAWING_SCREEN_VIEW_I2C_SCAN:
            return DRAWING_SCREEN_VIEW_FORECAST;
        case DRAWING_SCREEN_VIEW_WIFI_SCAN:
            return DRAWING_SCREEN_VIEW_I2C_SCAN;
        case DRAWING_SCREEN_VIEW_ABOUT:
        default:
            return DRAWING_SCREEN_VIEW_WIFI_SCAN;
        }
    };
    app_set_screen((step > 0) ? next_view(g_app.view) : prev_view(g_app.view));
    return true;
}

void app_build_forecast_hourly_visible(void)
{
    if (!g_app.forecast_hourly_open || g_app.forecast_hourly_day >= g_app.forecast_row_count)
    {
        g_app.forecast_hourly_count = 0;
        return;
    }

    uint8_t day = g_app.forecast_hourly_day;
    uint8_t count = g_forecast_cache.days[day].count;
    g_app.forecast_hourly_count = count;

    uint8_t max_start = 0;
    if (count > APP_FORECAST_ROWS)
    {
        max_start = (uint8_t)(count - APP_FORECAST_ROWS);
    }
    if (g_app.forecast_hourly_offset > max_start)
    {
        g_app.forecast_hourly_offset = max_start;
    }

    snprintf(g_app.forecast_hourly_day_title, sizeof(g_app.forecast_hourly_day_title),
             "%.16s Hourly", g_app.forecast_row_title[day]);

    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        int src = (int)g_app.forecast_hourly_offset + i;
        if (src < count)
        {
            const forecast_hourly_payload_t *entry = &g_forecast_cache.days[day].entries[src];
            snprintf(g_app.forecast_hourly_time[i], sizeof(g_app.forecast_hourly_time[i]), "%s", entry->time_text);
            snprintf(g_app.forecast_hourly_detail[i], sizeof(g_app.forecast_hourly_detail[i]), "%s", entry->detail);
            snprintf(g_app.forecast_hourly_temp[i], sizeof(g_app.forecast_hourly_temp[i]), "%s", entry->temp_text);
            g_app.forecast_hourly_icon[i] = entry->icon;
        }
        else
        {
            snprintf(g_app.forecast_hourly_time[i], sizeof(g_app.forecast_hourly_time[i]), "--");
            g_app.forecast_hourly_detail[i][0] = '\0';
            snprintf(g_app.forecast_hourly_temp[i], sizeof(g_app.forecast_hourly_temp[i]), "--°");
            g_app.forecast_hourly_icon[i] = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        }
    }
}

void app_close_forecast_hourly(void)
{
    if (!g_app.forecast_hourly_open)
    {
        return;
    }
    g_app.forecast_hourly_open = false;
    g_app.forecast_hourly_offset = 0;
    g_app.forecast_hourly_count = 0;
    g_app.forecast_hourly_day_title[0] = '\0';
    app_mark_dirty(true, true, false, true);
}

void app_open_forecast_hourly(uint8_t day_row)
{
    if (day_row >= g_app.forecast_row_count)
    {
        return;
    }
    if (g_forecast_cache.days[day_row].count == 0)
    {
        return;
    }

    g_app.forecast_hourly_open = true;
    g_app.forecast_hourly_day = day_row;
    g_app.forecast_hourly_offset = 0;
    app_build_forecast_hourly_visible();
    app_mark_dirty(true, true, false, true);
}

void app_scroll_forecast_hourly(int dir)
{
    if (!g_app.forecast_hourly_open || g_app.forecast_hourly_day >= g_app.forecast_row_count)
    {
        return;
    }

    uint8_t day = g_app.forecast_hourly_day;
    uint8_t count = g_forecast_cache.days[day].count;
    if (count <= APP_FORECAST_ROWS)
    {
        return;
    }

    int max_start = (int)count - APP_FORECAST_ROWS;
    int next_offset = (int)g_app.forecast_hourly_offset + (dir * APP_FORECAST_ROWS);
    if (next_offset < 0)
    {
        next_offset = 0;
    }
    else if (next_offset > max_start)
    {
        next_offset = max_start;
    }

    if (next_offset == (int)g_app.forecast_hourly_offset)
    {
        return;
    }

    g_app.forecast_hourly_offset = (uint8_t)next_offset;
    app_build_forecast_hourly_visible();
    app_mark_dirty(false, true, false, true);
}

static void app_handle_touch_tap(int16_t x, int16_t y)
{
    if (app_handle_edge_nav_tap(x, y))
    {
        ESP_LOGI(APP_TAG, "touch: edge-nav tap x=%d y=%d -> view=%d", (int)x, (int)y, (int)g_app.view);
        return;
    }

    if (g_app.view != DRAWING_SCREEN_VIEW_FORECAST)
    {
        return;
    }

    if (g_app.forecast_hourly_open)
    {
        if (app_is_hourly_close_tap(x, y))
        {
            app_close_forecast_hourly();
            ESP_LOGI(APP_TAG, "touch: close hourly tap x=%d y=%d", (int)x, (int)y);
        }
        return;
    }

    int row = app_forecast_row_from_y(y);
    if (row < 0 || row >= g_app.forecast_row_count)
    {
        return;
    }
    app_open_forecast_hourly((uint8_t)row);
    ESP_LOGI(APP_TAG, "touch: open hourly row=%d x=%d y=%d", row, (int)x, (int)y);
}

uint16_t display_rotation_to_touch_rotation(lv_disp_rot_t display_rotation)
{
    switch (display_rotation)
    {
    case LV_DISP_ROT_90:
        return 1;
    case LV_DISP_ROT_180:
        return 2;
    case LV_DISP_ROT_270:
        return 3;
    case LV_DISP_ROT_NONE:
    default:
        return 0;
    }
}

void app_poll_touch_swipe(uint32_t now_ms)
{
    touch_data_t touch_data = {};
    bsp_touch_read();
    bool is_pressed = bsp_touch_get_coordinates(&touch_data);

    if (is_pressed)
    {
        int16_t x = (int16_t)touch_data.coords[0].x;
        int16_t y = (int16_t)touch_data.coords[0].y;

        if (!g_touch_swipe.pressed)
        {
            g_touch_swipe.pressed = true;
            g_touch_swipe.start_x = x;
            g_touch_swipe.start_y = y;
        }

        g_touch_swipe.last_x = x;
        g_touch_swipe.last_y = y;
        return;
    }

    if (!g_touch_swipe.pressed)
    {
        return;
    }

    int delta_x = (int)g_touch_swipe.last_x - (int)g_touch_swipe.start_x;
    int delta_y = (int)g_touch_swipe.last_y - (int)g_touch_swipe.start_y;
    int abs_delta_x = (delta_x >= 0) ? delta_x : -delta_x;
    int abs_delta_y = (delta_y >= 0) ? delta_y : -delta_y;
    g_touch_swipe.pressed = false;

    if (abs_delta_x <= TOUCH_TAP_MAX_MOVE_PX && abs_delta_y <= TOUCH_TAP_MAX_MOVE_PX)
    {
        ESP_LOGI(APP_TAG, "touch: tap x=%d y=%d view=%d", (int)g_touch_swipe.last_x, (int)g_touch_swipe.last_y, (int)g_app.view);
        app_handle_touch_tap(g_touch_swipe.last_x, g_touch_swipe.last_y);
        return;
    }

    if ((uint32_t)(now_ms - g_touch_swipe.last_swipe_ms) < TOUCH_SWIPE_COOLDOWN_MS)
    {
        return;
    }

    if (g_app.view == DRAWING_SCREEN_VIEW_FORECAST && g_app.forecast_hourly_open)
    {
        if (abs_delta_y >= TOUCH_SWIPE_MIN_Y_PX && abs_delta_y >= abs_delta_x)
        {
            g_touch_swipe.last_swipe_ms = now_ms;
            // Swipe up shows later hours; swipe down shows earlier hours.
            app_scroll_forecast_hourly((delta_y < 0) ? 1 : -1);
            ESP_LOGI(APP_TAG, "touch: hourly swipe dx=%d dy=%d", delta_x, delta_y);
            return;
        }
    }

    if (abs_delta_x < TOUCH_SWIPE_MIN_X_PX || abs_delta_y > TOUCH_SWIPE_MAX_Y_PX || abs_delta_y >= abs_delta_x)
    {
        return;
    }

    g_touch_swipe.last_swipe_ms = now_ms;

    auto next_view = [](drawing_screen_view_t v) {
        switch (v)
        {
        case DRAWING_SCREEN_VIEW_NOW:
            return DRAWING_SCREEN_VIEW_FORECAST;
        case DRAWING_SCREEN_VIEW_INDOOR:
            return DRAWING_SCREEN_VIEW_NOW;
        case DRAWING_SCREEN_VIEW_FORECAST:
            return DRAWING_SCREEN_VIEW_I2C_SCAN;
        case DRAWING_SCREEN_VIEW_I2C_SCAN:
            return DRAWING_SCREEN_VIEW_WIFI_SCAN;
        case DRAWING_SCREEN_VIEW_WIFI_SCAN:
            return DRAWING_SCREEN_VIEW_ABOUT;
        case DRAWING_SCREEN_VIEW_ABOUT:
        default:
            return DRAWING_SCREEN_VIEW_INDOOR;
        }
    };
    auto prev_view = [](drawing_screen_view_t v) {
        switch (v)
        {
        case DRAWING_SCREEN_VIEW_NOW:
            return DRAWING_SCREEN_VIEW_INDOOR;
        case DRAWING_SCREEN_VIEW_INDOOR:
            return DRAWING_SCREEN_VIEW_ABOUT;
        case DRAWING_SCREEN_VIEW_FORECAST:
            return DRAWING_SCREEN_VIEW_NOW;
        case DRAWING_SCREEN_VIEW_I2C_SCAN:
            return DRAWING_SCREEN_VIEW_FORECAST;
        case DRAWING_SCREEN_VIEW_WIFI_SCAN:
            return DRAWING_SCREEN_VIEW_I2C_SCAN;
        case DRAWING_SCREEN_VIEW_ABOUT:
        default:
            return DRAWING_SCREEN_VIEW_WIFI_SCAN;
        }
    };

    drawing_screen_view_t next = (delta_x < 0) ? next_view(g_app.view) : prev_view(g_app.view);
    app_set_screen(next);
    ESP_LOGI(APP_TAG, "touch: page swipe dx=%d dy=%d -> view=%d", delta_x, delta_y, (int)next);
}

void app_apply_forecast_payload(const forecast_payload_t *fc)
{
    if (fc == NULL)
    {
        return;
    }

    g_forecast_cache = *fc;

    snprintf(g_app.forecast_title_text, sizeof(g_app.forecast_title_text), "Forecast");
    snprintf(g_app.forecast_body_text, sizeof(g_app.forecast_body_text), "Daily highs/lows");
    snprintf(g_app.forecast_preview_text, sizeof(g_app.forecast_preview_text), "%s", fc->preview_text);
    g_app.forecast_row_count = (fc->row_count > APP_FORECAST_ROWS) ? APP_FORECAST_ROWS : fc->row_count;
    g_app.forecast_preview_count = (g_app.forecast_row_count > APP_PREVIEW_DAYS) ? APP_PREVIEW_DAYS : g_app.forecast_row_count;

    for (int i = 0; i < APP_FORECAST_ROWS; ++i)
    {
        snprintf(g_app.forecast_row_title[i], sizeof(g_app.forecast_row_title[i]), "%s", fc->rows[i].title);
        snprintf(g_app.forecast_row_detail[i], sizeof(g_app.forecast_row_detail[i]), "%s", fc->rows[i].detail);
        snprintf(g_app.forecast_row_temp[i], sizeof(g_app.forecast_row_temp[i]), "%s", fc->rows[i].temp_text);
        g_app.forecast_row_icon[i] = fc->rows[i].icon;
    }
    for (int i = 0; i < APP_PREVIEW_DAYS; ++i)
    {
        if (i < g_app.forecast_preview_count)
        {
            snprintf(g_app.forecast_preview_day[i], sizeof(g_app.forecast_preview_day[i]), "%.7s", fc->rows[i].title);
            snprintf(g_app.forecast_preview_hi[i], sizeof(g_app.forecast_preview_hi[i]), "%d°", fc->rows[i].temp_f);
            snprintf(g_app.forecast_preview_low[i], sizeof(g_app.forecast_preview_low[i]), "%d°", fc->rows[i].feels_f);
            g_app.forecast_preview_icon[i] = fc->rows[i].icon;
        }
        else
        {
            g_app.forecast_preview_day[i][0] = '\0';
            snprintf(g_app.forecast_preview_hi[i], sizeof(g_app.forecast_preview_hi[i]), "--°");
            snprintf(g_app.forecast_preview_low[i], sizeof(g_app.forecast_preview_low[i]), "--°");
            g_app.forecast_preview_icon[i] = DRAWING_WEATHER_ICON_FEW_CLOUDS_DAY;
        }
    }

    if (g_app.forecast_hourly_open)
    {
        if (g_app.forecast_hourly_day >= g_app.forecast_row_count ||
            g_forecast_cache.days[g_app.forecast_hourly_day].count == 0)
        {
            app_close_forecast_hourly();
        }
        else
        {
            app_build_forecast_hourly_visible();
            app_mark_dirty(true, false, false, true);
        }
    }

    app_mark_dirty(false, true, false, false);
}
