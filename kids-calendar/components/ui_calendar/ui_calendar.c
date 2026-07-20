#include "ui_calendar.h"
#include "ui_styles.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "UI";

#define SCREEN_W 172
#define SCREEN_H 640
#define STATUS_H 56
#define CONTENT_H (SCREEN_H - STATUS_H)

static lv_obj_t *s_root = NULL;
static lv_obj_t *s_status_bar = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_weekday_label = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_wifi_label = NULL;
static lv_obj_t *s_batt_icon_label = NULL;
static lv_obj_t *s_batt_pct_label = NULL;

static lv_obj_t *s_timeline = NULL;
static lv_obj_t *s_month_view = NULL;
static lv_obj_t *s_reminder_popup = NULL;
static lv_timer_t *s_reminder_timer = NULL;

static bool s_view_is_timeline = true;
static int s_current_year = 2026;
static int s_current_month = 7;
static int s_today_day = 20;

static const char *s_weekday_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
static const char *s_month_names[] = {"1月", "2月", "3月", "4月", "5月", "6月",
                                      "7月", "8月", "9月", "10月", "11月", "12月"};

/* Forward declarations */
static void ui_create_status_bar(void);
static void ui_create_timeline(void);
static void ui_create_month_view(void);
static void ui_create_reminder_popup(void);
static void ui_on_content_gesture(lv_event_t *e);
static void ui_on_status_bar_click(lv_event_t *e);
static void ui_on_course_card_click(lv_event_t *e);


esp_err_t ui_init(void)
{
    ESP_LOGI(TAG, "Initializing UI (%dx%d)", SCREEN_W, SCREEN_H);

    s_root = lv_screen_active();
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x101010), 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);

    ui_create_status_bar();
    ui_create_timeline();
    ui_create_month_view();
    ui_create_reminder_popup();

    /* Show timeline by default */
    lv_obj_clear_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
    s_view_is_timeline = true;

    ESP_LOGI(TAG, "UI init complete");

    return ESP_OK;
}

static void ui_create_status_bar(void)
{
    s_status_bar = lv_obj_create(s_root);
    lv_obj_set_size(s_status_bar, SCREEN_W, STATUS_H);
    lv_obj_set_pos(s_status_bar, 0, 0);
    ui_style_status_bar(s_status_bar);
    lv_obj_add_event_cb(s_status_bar, ui_on_status_bar_click, LV_EVENT_CLICKED, NULL);

    s_date_label = lv_label_create(s_status_bar);
    ui_style_label(s_date_label, font_normal, lv_color_white());
    lv_label_set_text(s_date_label, "--/--");
    lv_obj_align(s_date_label, LV_ALIGN_LEFT_MID, 4, -4);

    s_weekday_label = lv_label_create(s_status_bar);
    ui_style_label(s_weekday_label, font_normal, lv_color_hex(0x4ECDC4));
    lv_label_set_text(s_weekday_label, "--");
    lv_obj_align(s_weekday_label, LV_ALIGN_CENTER, 0, -4);

    s_time_label = lv_label_create(s_status_bar);
    ui_style_label(s_time_label, font_time, lv_color_white());
    lv_label_set_text(s_time_label, "--:--");
    lv_obj_align(s_time_label, LV_ALIGN_RIGHT_MID, -4, -4);

    s_wifi_label = lv_label_create(s_status_bar);
    ui_style_label(s_wifi_label, font_small, lv_color_hex(0x888888));
    lv_label_set_text(s_wifi_label, "WiFi");
    lv_obj_align(s_wifi_label, LV_ALIGN_BOTTOM_RIGHT, -4, 0);

    /* Battery: icon (Montserrat has the LV_SYMBOL_BATTERY_* glyphs) + percent */
    s_batt_icon_label = lv_label_create(s_status_bar);
    lv_obj_set_style_text_font(s_batt_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_batt_icon_label, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(s_batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);
    lv_obj_align(s_batt_icon_label, LV_ALIGN_BOTTOM_LEFT, 4, 0);

    s_batt_pct_label = lv_label_create(s_status_bar);
    ui_style_label(s_batt_pct_label, font_small, lv_color_hex(0xAAAAAA));
    lv_label_set_text(s_batt_pct_label, "--%");
    lv_obj_align_to(s_batt_pct_label, s_batt_icon_label, LV_ALIGN_OUT_RIGHT_MID, 2, 0);
}

static void ui_create_timeline(void)
{
    s_timeline = lv_obj_create(s_root);
    lv_obj_set_size(s_timeline, SCREEN_W, CONTENT_H);
    lv_obj_set_pos(s_timeline, 0, STATUS_H);
    ui_style_scroll_container(s_timeline);
    lv_obj_set_flex_flow(s_timeline, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_timeline, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(s_timeline, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_timeline, ui_on_content_gesture, LV_EVENT_GESTURE, NULL);

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
    lv_obj_set_size(s_month_view, SCREEN_W, CONTENT_H);
    lv_obj_set_pos(s_month_view, 0, STATUS_H);
    ui_style_scroll_container(s_month_view);
    lv_obj_set_flex_flow(s_month_view, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_month_view, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(s_month_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_month_view, ui_on_content_gesture, LV_EVENT_GESTURE, NULL);
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

void ui_update_statusbar(const char *date, const char *weekday,
                         const char *time_str, bool wifi_ok)
{
    if (date) {
        lv_label_set_text(s_date_label, date);
    }
    if (weekday) {
        lv_label_set_text(s_weekday_label, weekday);
    }
    if (time_str) {
        lv_label_set_text(s_time_label, time_str);
    }

    if (s_wifi_label) {
        lv_label_set_text(s_wifi_label, wifi_ok ? "WiFi" : "---");
        lv_obj_set_style_text_color(s_wifi_label,
                                    wifi_ok ? lv_color_hex(0x4ECDC4) : lv_color_hex(0x888888), 0);
    }
}

void ui_update_battery(int pct)
{
    if (!s_batt_icon_label || !s_batt_pct_label) {
        return;
    }

    if (pct < 0) {
        lv_label_set_text(s_batt_icon_label, LV_SYMBOL_BATTERY_EMPTY);
        lv_label_set_text(s_batt_pct_label, "--%");
        return;
    }

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

void ui_show_course_timeline(const course_t *courses, int count)
{
    if (s_timeline == NULL) {
        return;
    }

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

    for (int i = 0; i < count; i++) {
        const course_t *c = &courses[i];
        lv_color_t color = ui_color_from_hex(c->color);

        lv_obj_t *card = lv_obj_create(s_timeline);
        lv_obj_set_size(card, 164, 86);
        ui_style_card(card);
        lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
        lv_obj_add_event_cb(card, ui_on_course_card_click, LV_EVENT_CLICKED, (void *)c);

        lv_obj_t *bar = lv_obj_create(card);
        lv_obj_set_size(bar, 6, 74);
        ui_style_card_bar(bar, color);
        lv_obj_set_style_margin_right(bar, 6, 0);

        lv_obj_t *info = lv_obj_create(card);
        lv_obj_set_size(info, 140, 74);
        lv_obj_set_style_bg_opa(info, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(info, 0, 0);
        lv_obj_set_style_pad_all(info, 0, 0);
        lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);

        lv_obj_t *name = lv_label_create(info);
        ui_style_label(name, font_normal, lv_color_white());
        lv_label_set_text(name, c->name);

        lv_obj_t *time = lv_label_create(info);
        ui_style_label(time, font_small, lv_color_hex(0xCCCCCC));
        char time_buf[32];
        snprintf(time_buf, sizeof(time_buf), "%s - %s", c->start_time, c->end_time);
        lv_label_set_text(time, time_buf);

        lv_obj_t *teacher = lv_label_create(info);
        ui_style_label(teacher, font_small, lv_color_hex(0xAAAAAA));
        char teacher_buf[48];
        snprintf(teacher_buf, sizeof(teacher_buf), "%s · %s", c->teacher, c->location);
        lv_label_set_text(teacher, teacher_buf);
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

static void ui_on_content_gesture(lv_event_t *e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    if (dir == LV_DIR_LEFT || dir == LV_DIR_RIGHT) {
        ui_toggle_view();
    }
}

static void ui_on_status_bar_click(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Status bar tapped: trigger sync (TODO)");
}

static void ui_on_course_card_click(lv_event_t *e)
{
    lv_obj_t *card = lv_event_get_target(e);
    const course_t *c = (const course_t *)lv_event_get_user_data(e);
    (void)card;

    if (c != NULL) {
        ESP_LOGI(TAG, "Course card clicked: %s at %s", c->name, c->start_time);
        ui_show_reminder(c->name, c->start_time);
    }
}
