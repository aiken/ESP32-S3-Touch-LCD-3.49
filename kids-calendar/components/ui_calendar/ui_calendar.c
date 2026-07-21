#include "ui_calendar.h"
#include "ui_styles.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI";

#define SCREEN_W_PORTRAIT   172
#define SCREEN_H_PORTRAIT   640
#define SCREEN_W_LANDSCAPE  640
#define SCREEN_H_LANDSCAPE  172
#define STATUS_H_PORTRAIT   56
#define STATUS_H_LANDSCAPE  36

static bool s_landscape = false;
static int s_screen_w = SCREEN_W_PORTRAIT;
static int s_screen_h = SCREEN_H_PORTRAIT;
static int s_status_h = STATUS_H_PORTRAIT;
#define CONTENT_H (s_screen_h - s_status_h)

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_status_bar = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_weekday_label = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_wifi_label = NULL;
static lv_obj_t *s_batt_icon_label = NULL;
static lv_obj_t *s_batt_pct_label = NULL;
static lv_obj_t *s_batt_charge_label = NULL;

static lv_obj_t *s_timeline = NULL;
static lv_obj_t *s_month_view = NULL;
static lv_obj_t *s_reminder_popup = NULL;
static lv_timer_t *s_reminder_timer = NULL;

static bool s_view_is_timeline = true;
static int s_current_year = 2026;
static int s_current_month = 7;
static int s_today_day = 20;

/* Cached content so ui_set_orientation() can restore after a rebuild */
static const course_t *s_last_courses = NULL;
static int s_last_course_count = 0;
static char s_last_date[16] = "--/--";
static char s_last_weekday[8] = "--";
static char s_last_time[16] = "--:--";
static bool s_last_wifi = false;
static char s_last_wifi_text[24] = "WiFi";
static int s_last_batt_pct = -1;
static bool s_last_charging = false;
static int s_now_minutes = -1;   /* current time in minutes, for "进行中" highlight */
static lv_obj_t *s_kid_label = NULL;
static lv_obj_t *s_sync_label = NULL;
static char s_last_kid_name[16] = "--";
static void (*s_kid_switch_cb)(void) = NULL;

static const char *s_weekday_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
static const char *s_month_names[] = {"1月", "2月", "3月", "4月", "5月", "6月",
                                      "7月", "8月", "9月", "10月", "11月", "12月"};

/* ---------------- course detail popup (fullscreen dismiss layer) ---------------- */

static lv_obj_t *s_detail_layer = NULL;
static lv_obj_t *s_detail_popup = NULL;
static lv_obj_t *s_detail_name = NULL;
static lv_obj_t *s_detail_time = NULL;
static lv_obj_t *s_detail_meta = NULL;
static lv_obj_t *s_detail_remind = NULL;

static void ui_on_detail_layer_click(lv_event_t *e)
{
    (void)e;
    if (s_detail_layer) {
        lv_obj_add_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_create_detail_popup(void)
{
    /* Fullscreen dim layer: tap anywhere outside the card to dismiss */
    s_detail_layer = lv_obj_create(s_root);
    lv_obj_set_size(s_detail_layer, s_screen_w, s_screen_h);
    lv_obj_set_pos(s_detail_layer, 0, 0);
    lv_obj_set_style_bg_color(s_detail_layer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_detail_layer, LV_OPA_40, 0);
    lv_obj_set_style_border_width(s_detail_layer, 0, 0);
    lv_obj_set_style_pad_all(s_detail_layer, 0, 0);
    lv_obj_add_event_cb(s_detail_layer, ui_on_detail_layer_click, LV_EVENT_CLICKED, NULL);

    s_detail_popup = lv_obj_create(s_detail_layer);
    lv_obj_set_size(s_detail_popup, s_landscape ? 320 : (s_screen_w - 16),
                    s_landscape ? 140 : 220);
    lv_obj_center(s_detail_popup);
    lv_obj_set_style_bg_color(s_detail_popup, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_detail_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_detail_popup, 12, 0);
    lv_obj_set_style_border_width(s_detail_popup, 2, 0);
    lv_obj_set_style_pad_all(s_detail_popup, 12, 0);
    lv_obj_set_flex_flow(s_detail_popup, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(s_detail_popup, 6, 0);

    s_detail_name = lv_label_create(s_detail_popup);
    ui_style_label(s_detail_name, font_large, lv_color_white());

    s_detail_time = lv_label_create(s_detail_popup);
    ui_style_label(s_detail_time, font_normal, lv_color_hex(0x4ECDC4));

    s_detail_meta = lv_label_create(s_detail_popup);
    ui_style_label(s_detail_meta, font_small, lv_color_hex(0x9AA0B4));

    s_detail_remind = lv_label_create(s_detail_popup);
    ui_style_label(s_detail_remind, font_small, lv_color_hex(0xFFE66D));

    lv_obj_t *hint = lv_label_create(s_detail_popup);
    ui_style_label(hint, font_small, lv_color_hex(0x555566));
    lv_label_set_text(hint, "点击空白处关闭");

    lv_obj_add_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
}

static void ui_show_course_detail(const course_t *c)
{
    if (!c || !s_detail_layer) {
        return;
    }
    lv_label_set_text(s_detail_name, c->name);
    lv_label_set_text_fmt(s_detail_time, "%s - %s  周%s",
                          c->start_time, c->end_time, s_weekday_names[c->day_of_week % 7] + 1);
    lv_label_set_text_fmt(s_detail_meta, "老师:%s  地点:%s",
                          c->teacher[0] ? c->teacher : "--", c->location[0] ? c->location : "--");
    lv_label_set_text_fmt(s_detail_remind, "提前 %d 分钟提醒", c->remind_before);
    lv_obj_set_style_border_color(s_detail_popup, ui_color_from_hex(c->color), 0);
    lv_obj_clear_flag(s_detail_layer, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_detail_layer);
}
static void ui_create_status_bar(void);
static void ui_create_timeline(void);
static void ui_create_month_view(void);
static void ui_create_reminder_popup(void);
static void ui_create_detail_popup(void);
static void ui_on_status_bar_click(lv_event_t *e);
static void ui_on_course_card_click(lv_event_t *e);
static void ui_on_kid_click(lv_event_t *e);


esp_err_t ui_init(void)
{
    ESP_LOGI(TAG, "Initializing UI (%dx%d)", s_screen_w, s_screen_h);

    ui_styles_init_fonts();

    s_root = lv_screen_active();
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101010), 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    ui_create_status_bar();
    ui_create_timeline();
    ui_create_month_view();
    ui_create_reminder_popup();
    ui_create_detail_popup();

    /* Show timeline by default */
    lv_obj_clear_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
    s_view_is_timeline = true;

    ESP_LOGI(TAG, "UI init complete");

    return ESP_OK;
}

static void ui_layout_status_bar(void)
{
    if (!s_landscape) {
        /* Portrait (172x640): top row date+weekday+clock, bottom row batt+wifi */
        lv_obj_align(s_date_label, LV_ALIGN_LEFT_MID, 4, -8);
        lv_obj_align_to(s_weekday_label, s_date_label, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        lv_obj_align_to(s_kid_label, s_weekday_label, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        lv_obj_align(s_time_label, LV_ALIGN_RIGHT_MID, -4, -8);
        lv_obj_align(s_wifi_label, LV_ALIGN_BOTTOM_RIGHT, -4, 0);
        lv_obj_align(s_batt_icon_label, LV_ALIGN_BOTTOM_LEFT, 4, 0);
        lv_obj_align_to(s_batt_charge_label, s_batt_icon_label, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
        lv_obj_align_to(s_batt_pct_label, s_batt_charge_label, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
    } else {
        /* Landscape (640x172): single row, battery + wifi left of the clock */
        lv_obj_align(s_date_label, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_align_to(s_weekday_label, s_date_label, LV_ALIGN_OUT_RIGHT_MID, 16, 0);
        lv_obj_align_to(s_kid_label, s_weekday_label, LV_ALIGN_OUT_RIGHT_MID, 12, 0);
        lv_obj_align(s_time_label, LV_ALIGN_RIGHT_MID, -4, 0);
        lv_obj_align_to(s_wifi_label, s_time_label, LV_ALIGN_OUT_LEFT_MID, -14, 0);
        lv_obj_align_to(s_batt_pct_label, s_wifi_label, LV_ALIGN_OUT_LEFT_MID, -12, 0);
        lv_obj_align_to(s_batt_charge_label, s_batt_pct_label, LV_ALIGN_OUT_LEFT_MID, -3, 0);
        lv_obj_align_to(s_batt_icon_label, s_batt_charge_label, LV_ALIGN_OUT_LEFT_MID, -3, 0);
    }
}

static void ui_create_status_bar(void)
{
    s_status_bar = lv_obj_create(s_root);
    lv_obj_set_size(s_status_bar, s_screen_w, s_status_h);
    lv_obj_set_pos(s_status_bar, 0, 0);
    ui_style_status_bar(s_status_bar);
    lv_obj_add_event_cb(s_status_bar, ui_on_status_bar_click, LV_EVENT_CLICKED, NULL);

    s_date_label = lv_label_create(s_status_bar);
    ui_style_label(s_date_label, font_normal, lv_color_white());
    lv_label_set_text(s_date_label, "--/--");

    s_weekday_label = lv_label_create(s_status_bar);
    ui_style_label(s_weekday_label, font_normal, lv_color_hex(0x4ECDC4));
    lv_label_set_text(s_weekday_label, "--");

    s_time_label = lv_label_create(s_status_bar);
    ui_style_label(s_time_label, &lv_font_montserrat_24, lv_color_white());
    lv_label_set_text(s_time_label, "--:--");

    s_wifi_label = lv_label_create(s_status_bar);
    ui_style_label(s_wifi_label, font_small, lv_color_hex(0x888888));
    lv_label_set_text(s_wifi_label, "WiFi");

    /* Battery: icon (Montserrat has the LV_SYMBOL_BATTERY_* glyphs) + percent */
    s_batt_icon_label = lv_label_create(s_status_bar);
    lv_obj_set_style_text_font(s_batt_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_batt_icon_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);

    s_batt_pct_label = lv_label_create(s_status_bar);
    ui_style_label(s_batt_pct_label, font_small, lv_color_hex(0xAAAAAA));
    lv_label_set_text(s_batt_pct_label, "--%");

    /* Charging bolt (shown only when on USB power) */
    s_batt_charge_label = lv_label_create(s_status_bar);
    lv_obj_set_style_text_font(s_batt_charge_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_batt_charge_label, lv_color_hex(0x4ADE80), 0);
    lv_label_set_text(s_batt_charge_label, "");

    /* Kid profile label (tap to switch 美熹/壮壮) — padded for a bigger hit area */
    s_kid_label = lv_label_create(s_status_bar);
    ui_style_label(s_kid_label, font_small, lv_color_hex(0xFFE66D));
    lv_label_set_text(s_kid_label, s_last_kid_name);
    lv_obj_set_style_pad_all(s_kid_label, 8, 0);
    lv_obj_add_flag(s_kid_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_kid_label, ui_on_kid_click, LV_EVENT_CLICKED, NULL);

    /* Sync-in-progress hint (hidden by default) */
    s_sync_label = lv_label_create(s_status_bar);
    ui_style_label(s_sync_label, font_small, lv_color_hex(0x4ECDC4));
    lv_label_set_text(s_sync_label, "同步中");
    lv_obj_add_flag(s_sync_label, LV_OBJ_FLAG_HIDDEN);
    if (s_landscape) {
        lv_obj_align(s_sync_label, LV_ALIGN_CENTER, 0, 0);
    } else {
        lv_obj_align(s_sync_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    ui_layout_status_bar();
}

static void ui_create_timeline(void)
{
    s_timeline = lv_obj_create(s_root);
    lv_obj_set_size(s_timeline, s_screen_w, CONTENT_H);
    lv_obj_set_pos(s_timeline, 0, s_status_h);
    ui_style_scroll_container(s_timeline);
    lv_obj_set_flex_flow(s_timeline, s_landscape ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_timeline, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);

    /* Placeholder label shown when no courses */
    lv_obj_t *placeholder = lv_label_create(s_timeline);
    ui_style_label(placeholder, font_normal, lv_color_hex(0x888888));
    lv_label_set_text(placeholder, "暂无课程");
    lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(placeholder);
}

static void ui_create_month_view(void)
{
    s_month_view = lv_obj_create(s_root);
    lv_obj_set_size(s_month_view, s_screen_w, CONTENT_H);
    lv_obj_set_pos(s_month_view, 0, s_status_h);
    ui_style_scroll_container(s_month_view);
    lv_obj_set_flex_flow(s_month_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_month_view, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
}

static void reminder_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_hide_reminder();
}

static void ui_create_reminder_popup(void)
{
    s_reminder_popup = lv_obj_create(s_root);
    lv_obj_set_size(s_reminder_popup, 152, 100);
    lv_obj_center(s_reminder_popup);
    lv_obj_set_style_bg_color(s_reminder_popup, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_radius(s_reminder_popup, 12, 0);
    lv_obj_set_style_border_width(s_reminder_popup, 2, 0);
    lv_obj_set_style_border_color(s_reminder_popup, lv_color_hex(0x4ECDC4), 0);
    lv_obj_add_flag(s_reminder_popup, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *title = lv_label_create(s_reminder_popup);
    ui_style_label(title, font_normal, lv_color_hex(0x4ECDC4));
    lv_label_set_text(title, "即将上课");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *name = lv_label_create(s_reminder_popup);
    ui_style_label(name, font_normal, lv_color_white());
    lv_label_set_text(name, "--");
    lv_obj_set_user_data(s_reminder_popup, name);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *time = lv_label_create(s_reminder_popup);
    ui_style_label(time, font_small, lv_color_hex(0xAAAAAA));
    lv_label_set_text(time, "--:--");
    lv_obj_align(time, LV_ALIGN_BOTTOM_MID, 0, -8);

    s_reminder_timer = lv_timer_create(reminder_timer_cb, 5000, NULL);
    lv_timer_pause(s_reminder_timer);
}

void ui_set_orientation(bool landscape)
{
    if (landscape == s_landscape || s_root == NULL) {
        return;
    }
    s_landscape = landscape;
    s_screen_w = landscape ? SCREEN_W_LANDSCAPE : SCREEN_W_PORTRAIT;
    s_screen_h = landscape ? SCREEN_H_LANDSCAPE : SCREEN_H_PORTRAIT;
    s_status_h = landscape ? STATUS_H_LANDSCAPE : STATUS_H_PORTRAIT;
    s_view_is_timeline = true; /* month view is portrait-only */

    /* Delete top-level containers (children go with them) and rebuild */
    if (s_reminder_timer) { lv_timer_del(s_reminder_timer); s_reminder_timer = NULL; }
    if (s_detail_layer)   { lv_obj_del(s_detail_layer);   s_detail_layer = NULL; s_detail_popup = NULL;
                            s_detail_name = s_detail_time = s_detail_meta = s_detail_remind = NULL; }
    if (s_status_bar)     { lv_obj_del(s_status_bar);     s_status_bar = NULL; }
    if (s_timeline)       { lv_obj_del(s_timeline);       s_timeline = NULL; }
    if (s_month_view)     { lv_obj_del(s_month_view);     s_month_view = NULL; }
    if (s_reminder_popup) { lv_obj_del(s_reminder_popup); s_reminder_popup = NULL; }
    s_date_label = s_weekday_label = s_time_label = s_wifi_label = NULL;
    s_batt_icon_label = s_batt_pct_label = s_batt_charge_label = NULL;
    s_kid_label = NULL;
    s_sync_label = NULL;

    ui_create_status_bar();
    ui_create_timeline();
    ui_create_month_view();
    ui_create_reminder_popup();
    ui_create_detail_popup();

    lv_obj_clear_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);

    /* Restore cached content */
    ui_update_statusbar(s_last_date, s_last_weekday, s_last_time, s_last_wifi, s_last_wifi_text);
    ui_update_battery(s_last_batt_pct, s_last_charging);
    ui_show_course_timeline(s_last_courses, s_last_course_count);
    ui_show_month_calendar(s_current_year, s_current_month, s_today_day);

    ESP_LOGI(TAG, "UI rebuilt: %s (%dx%d)", landscape ? "landscape" : "portrait",
             s_screen_w, s_screen_h);
}

void ui_update_statusbar(const char *date, const char *weekday,
                         const char *time_str, bool wifi_ok,
                         const char *wifi_text)
{
    if (date) {
        lv_label_set_text(s_date_label, date);
        strncpy(s_last_date, date, sizeof(s_last_date) - 1);
    }
    if (weekday) {
        lv_label_set_text(s_weekday_label, weekday);
        strncpy(s_last_weekday, weekday, sizeof(s_last_weekday) - 1);
    }
    if (time_str) {
        lv_label_set_text(s_time_label, time_str);
        strncpy(s_last_time, time_str, sizeof(s_last_time) - 1);
    }
    s_last_wifi = wifi_ok;
    if (wifi_text) {
        strncpy(s_last_wifi_text, wifi_text, sizeof(s_last_wifi_text) - 1);
    }

    if (s_wifi_label) {
        lv_label_set_text(s_wifi_label, wifi_text ? wifi_text : "WiFi");
        lv_obj_set_style_text_color(s_wifi_label,
                                    wifi_ok ? lv_color_hex(0x4ECDC4) : lv_color_hex(0x555566), 0);
    }

    /* Label widths changed with content — redo the right-side alignment */
    ui_layout_status_bar();
}

void ui_set_current_time(int hour, int minute)
{
    s_now_minutes = (hour >= 0 && minute >= 0) ? hour * 60 + minute : -1;
}

void ui_update_battery(int pct, bool charging)
{
    s_last_batt_pct = pct;
    s_last_charging = charging;
    if (!s_batt_icon_label || !s_batt_pct_label) {
        return;
    }

    if (pct < 0) {
        lv_label_set_text(s_batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);
        lv_label_set_text(s_batt_pct_label, "--%");
    } else {
        const char *icon;
        if (pct >= 80)      icon = LV_SYMBOL_BATTERY_FULL;
        else if (pct >= 50) icon = LV_SYMBOL_BATTERY_3;
        else if (pct >= 25) icon = LV_SYMBOL_BATTERY_2;
        else if (pct >= 10) icon = LV_SYMBOL_BATTERY_1;
        else                icon = LV_SYMBOL_BATTERY_EMPTY;
        lv_label_set_text(s_batt_icon_label, icon);

        lv_label_set_text_fmt(s_batt_pct_label, "%d%%", pct);

        /* Warn in red when low */
        lv_color_t color = (pct < 20) ? lv_color_hex(0xFF6B6B) : lv_color_hex(0xAAAAAA);
        lv_obj_set_style_text_color(s_batt_icon_label, color, 0);
        lv_obj_set_style_text_color(s_batt_pct_label, color, 0);
    }

    /* Charging bolt next to the battery icon (green when on USB) */
    if (s_batt_charge_label) {
        lv_label_set_text(s_batt_charge_label, charging ? LV_SYMBOL_CHARGE : "");
    }
}

void ui_show_course_timeline(const course_t *courses, int count)
{
    if (s_timeline == NULL) {
        return;
    }

    /* Cache for orientation rebuilds */
    s_last_courses = courses;
    s_last_course_count = count;

    /* Remove existing cards */
    uint32_t child_cnt = lv_obj_get_child_cnt(s_timeline);
    for (int i = child_cnt - 1; i >= 0; i--) {
        lv_obj_t *child = lv_obj_get_child(s_timeline, i);
        lv_obj_del(child);
    }

    if (count <= 0 || courses == NULL) {
        lv_obj_t *placeholder = lv_label_create(s_timeline);
        ui_style_label(placeholder, font_normal, lv_color_hex(0x888888));
        lv_label_set_text(placeholder, "No courses");
        lv_obj_set_style_text_align(placeholder, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(placeholder);
        return;
    }

    const int card_w = s_landscape ? 148 : 156;
    const int card_h = s_landscape ? (CONTENT_H - 12) : 96;

    for (int i = 0; i < count; i++) {
        const course_t *c = &courses[i];
        lv_color_t color = ui_color_from_hex(c->color);

        /* "进行中" detection: start <= now < end */
        bool is_now = false;
        if (s_now_minutes >= 0) {
            int sh = 0, sm = 0, eh = 0, em = 0;
            if (sscanf(c->start_time, "%d:%d", &sh, &sm) == 2 &&
                sscanf(c->end_time, "%d:%d", &eh, &em) == 2) {
                int start = sh * 60 + sm, end = eh * 60 + em;
                is_now = (s_now_minutes >= start && s_now_minutes < end);
            }
        }

        lv_obj_t *card = lv_obj_create(s_timeline);
        lv_obj_set_size(card, card_w, card_h);
        lv_obj_set_style_bg_color(card, ui_color_blend(color, lv_color_hex(0x16213E), 40), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 12, 0);
        lv_obj_set_style_border_width(card, is_now ? 2 : 1, 0);
        lv_obj_set_style_border_color(card, is_now ? color : ui_color_blend(color, lv_color_hex(0x16213E), 100), 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_add_event_cb(card, ui_on_course_card_click, LV_EVENT_PRESSED, (void *)c);

        lv_obj_t *bar = lv_obj_create(card);
        lv_obj_set_size(bar, 8, card_h - 16);
        ui_style_card_bar(bar, color);
        lv_obj_set_style_margin_right(bar, 8, 0);

        lv_obj_t *info = lv_obj_create(card);
        lv_obj_set_size(info, card_w - 40, card_h - 16);
        lv_obj_set_style_bg_opa(info, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(info, 0, 0);
        lv_obj_set_style_pad_all(info, 0, 0);
        lv_obj_set_style_pad_row(info, 2, 0);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *name = lv_label_create(info);
        ui_style_label(name, font_large, lv_color_white());
        lv_label_set_text(name, c->name);

        lv_obj_t *time = lv_label_create(info);
        ui_style_label(time, font_normal, ui_color_brighten(color, 110));
        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%s - %s", c->start_time, c->end_time);
        lv_label_set_text(time, time_buf);

        lv_obj_t *teacher = lv_label_create(info);
        ui_style_label(teacher, font_small, lv_color_hex(0x9AA0B4));
        char teacher_buf[48];
        snprintf(teacher_buf, sizeof(teacher_buf), "%s · %s", c->teacher, c->location);
        lv_label_set_text(teacher, teacher_buf);

        if (is_now) {
            lv_obj_t *badge = lv_label_create(card);
            ui_style_label(badge, font_small, lv_color_white());
            lv_label_set_text(badge, "进行中");
            lv_obj_set_style_bg_color(badge, color, 0);
            lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(badge, 8, 0);
            lv_obj_set_style_pad_all(badge, 3, 0);
            lv_obj_add_flag(badge, LV_OBJ_FLAG_IGNORE_LAYOUT); /* not a flex item */
            lv_obj_align(badge, LV_ALIGN_TOP_RIGHT, -2, -2);
        }
    }
}

void ui_show_month_calendar(int year, int month, int today_day)
{
    if (s_month_view == NULL) {
        return;
    }

    s_current_year = year;
    s_current_month = month;
    s_today_day = today_day;

    /* Clear month view */
    uint32_t child_cnt = lv_obj_get_child_cnt(s_month_view);
    for (int i = child_cnt - 1; i >= 0; i--) {
        lv_obj_del(lv_obj_get_child(s_month_view, i));
    }

    /* Header */
    lv_obj_t *header = lv_label_create(s_month_view);
    ui_style_label(header, font_normal, lv_color_white());
    char header_buf[32];
    snprintf(header_buf, sizeof(header_buf), "%d年 %s", year, s_month_names[month - 1]);
    lv_label_set_text(header, header_buf);
    lv_obj_set_style_margin_bottom(header, 8, 0);

    /* Weekday header */
    lv_obj_t *day_header = lv_obj_create(s_month_view);
    lv_obj_set_size(day_header, 164, 20);
    lv_obj_set_style_bg_opa(day_header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(day_header, 0, 0);
    lv_obj_set_style_pad_all(day_header, 0, 0);
    lv_obj_set_flex_flow(day_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(day_header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 7; i++) {
        lv_obj_t *d = lv_label_create(day_header);
        ui_style_label(d, font_small, lv_color_hex(0x888888));
        lv_label_set_text(d, s_weekday_names[i]);
    }

    /* Simple day grid: assume month starts on Sunday for demo */
    int days_in_month = 31;
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        days_in_month = 30;
    } else if (month == 2) {
        days_in_month = 28;
    }

    int day = 1;
    for (int row = 0; row < 6 && day <= days_in_month; row++) {
        lv_obj_t *row_obj = lv_obj_create(s_month_view);
        lv_obj_set_size(row_obj, 164, 24);
        lv_obj_set_style_bg_opa(row_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_obj, 0, 0);
        lv_obj_set_style_pad_all(row_obj, 0, 0);
        lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        for (int col = 0; col < 7 && day <= days_in_month; col++) {
            lv_obj_t *day_label = lv_label_create(row_obj);
            ui_style_label(day_label, font_small,
                           (day == today_day) ? lv_color_hex(0x4ECDC4) : lv_color_white());
            char buf[4];
            snprintf(buf, sizeof(buf), "%d", day);
            lv_label_set_text(day_label, buf);
            day++;
        }
    }
}

void ui_show_reminder(const char *course_name, const char *start_time)
{
    if (s_reminder_popup == NULL) {
        return;
    }

    lv_obj_t *name_label = (lv_obj_t *)lv_obj_get_user_data(s_reminder_popup);
    lv_obj_t *time_label = lv_obj_get_child(s_reminder_popup, 2);

    if (name_label) {
        lv_label_set_text(name_label, course_name ? course_name : "--");
    }
    if (time_label) {
        lv_label_set_text(time_label, start_time ? start_time : "--:--");
    }

    lv_obj_clear_flag(s_reminder_popup, LV_OBJ_FLAG_HIDDEN);
    if (s_reminder_timer) {
        lv_timer_reset(s_reminder_timer);
        lv_timer_resume(s_reminder_timer);
    }
}

void ui_hide_reminder(void)
{
    if (s_reminder_popup) {
        lv_obj_add_flag(s_reminder_popup, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_reminder_timer) {
        lv_timer_pause(s_reminder_timer);
    }
}

void ui_toggle_view(void)
{
    if (s_landscape) {
        return; /* month view is portrait-only */
    }
    if (s_view_is_timeline) {
        lv_obj_add_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
    }
    s_view_is_timeline = !s_view_is_timeline;

    ESP_LOGI(TAG, "Switched to %s view", s_view_is_timeline ? "timeline" : "month");
}

void ui_register_callbacks(void)
{
    /* Callbacks registered during object creation */
}

static void (*s_sync_request_cb)(void) = NULL;

void ui_set_sync_callback(void (*cb)(void))
{
    s_sync_request_cb = cb;
}

void ui_set_kid_switch_callback(void (*cb)(void))
{
    s_kid_switch_cb = cb;
}

void ui_set_kid_label(const char *name)
{
    if (name) {
        strncpy(s_last_kid_name, name, sizeof(s_last_kid_name) - 1);
        s_last_kid_name[sizeof(s_last_kid_name) - 1] = '\0';
    }
    if (s_kid_label) {
        lv_label_set_text(s_kid_label, s_last_kid_name);
    }
}

void ui_show_syncing(bool on)
{
    if (!s_sync_label) {
        return;
    }
    if (on) {
        lv_obj_clear_flag(s_sync_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_sync_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_on_kid_click(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Kid label tapped: switch kid");
    if (s_kid_switch_cb) {
        s_kid_switch_cb();
    }
}

static void ui_on_status_bar_click(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Status bar tapped: trigger sync");
    if (s_sync_request_cb) {
        s_sync_request_cb();
    }
}

static void ui_on_course_card_click(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    const course_t *c = (const course_t *)lv_event_get_user_data(e);
    (void)card;

    if (c != NULL) {
        ESP_LOGI(TAG, "Course card clicked: %s at %s", c->name, c->start_time);
        ui_show_course_detail(c);
    }
}
