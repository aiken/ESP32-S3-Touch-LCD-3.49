#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Course colors */
extern const lv_color_t course_color_red;
extern const lv_color_t course_color_cyan;
extern const lv_color_t course_color_yellow;
extern const lv_color_t course_color_blue;
extern const lv_color_t course_color_purple;

/* Fonts */
extern const lv_font_t *font_small;
extern const lv_font_t *font_normal;
extern const lv_font_t *font_time;

/* Helpers */
lv_color_t ui_color_from_hex(const char *hex);

/* Style apply helpers */
void ui_style_status_bar(lv_obj_t *obj);
void ui_style_card(lv_obj_t *obj);
void ui_style_card_bar(lv_obj_t *obj, lv_color_t color);
void ui_style_label(lv_obj_t *obj, const lv_font_t *font, lv_color_t color);
void ui_style_scroll_container(lv_obj_t *obj);

/* Debug */
void verify_font(void);

#ifdef __cplusplus
}
#endif
