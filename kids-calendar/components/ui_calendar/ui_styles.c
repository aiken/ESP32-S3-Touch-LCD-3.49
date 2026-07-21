#include "ui_styles.h"

#include <stdlib.h>
#include "lvgl.h"
#include "esp_log.h"

/* Course colors */
const lv_color_t course_color_red    = LV_COLOR_MAKE(0xFF, 0x6B, 0x6B);
const lv_color_t course_color_cyan   = LV_COLOR_MAKE(0x4E, 0xCD, 0xC4);
const lv_color_t course_color_yellow = LV_COLOR_MAKE(0xFF, 0xE6, 0x6D);
const lv_color_t course_color_blue   = LV_COLOR_MAKE(0x4A, 0x90, 0xD9);
const lv_color_t course_color_purple = LV_COLOR_MAKE(0xAA, 0x96, 0xDA);

/* Use custom NotoSansSC fonts (ASCII + the CJK chars used by the UI).
   The built-in Source Han Sans CJK fonts lack several glyphs used by the
   demo data (语/术/王/李/张), so they showed as boxes. */
LV_FONT_DECLARE(lv_font_chinese_14);
LV_FONT_DECLARE(lv_font_chinese_16);
LV_FONT_DECLARE(lv_font_chinese_20);
const lv_font_t *font_small  = &lv_font_chinese_14;
const lv_font_t *font_normal = &lv_font_chinese_16;
const lv_font_t *font_large  = &lv_font_chinese_20;
const lv_font_t *font_time   = &lv_font_montserrat_20;

lv_color_t ui_color_from_hex(const char *hex)
{
    if (hex == NULL || hex[0] != '#') {
        return lv_color_white();
    }

    uint32_t rgb = (uint32_t)strtol(hex + 1, NULL, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;

    return lv_color_make(r, g, b);
}

lv_color_t ui_color_blend(lv_color_t fg, lv_color_t bg, uint8_t ratio)
{
    uint16_t inv = 255 - ratio;
    return lv_color_make(
        (uint8_t)((fg.red * ratio + bg.red * inv) / 255),
        (uint8_t)((fg.green * ratio + bg.green * inv) / 255),
        (uint8_t)((fg.blue * ratio + bg.blue * inv) / 255));
}

lv_color_t ui_color_brighten(lv_color_t c, uint8_t ratio)
{
    lv_color_t white = lv_color_white();
    return ui_color_blend(white, c, ratio);
}

void ui_style_status_bar(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 4, 0);
}

void ui_style_card(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x16213E), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 6, 0);
}

void ui_style_card_bar(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 4, 0);
}

void ui_style_label(lv_obj_t *obj, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_color(obj, color, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, 0);
}

void ui_style_scroll_container(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x101010), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 4, 0);
    lv_obj_set_style_pad_row(obj, 8, 0);
}
