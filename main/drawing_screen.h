#ifndef __DRAWING_SCREEN_H__
#define __DRAWING_SCREEN_H__

#include "lvgl.h"

extern lv_obj_t *canvas;
extern bool canvas_exit;

#ifdef __cplusplus
extern "C" {
#endif

void drawing_screen_init(void);
void drawing_screen_set_status(const char *status_text);
void drawing_screen_set_weather_text(const char *weather_text);
void drawing_screen_set_temp_text(const char *temp_text);

#ifdef __cplusplus
}
#endif

#endif
