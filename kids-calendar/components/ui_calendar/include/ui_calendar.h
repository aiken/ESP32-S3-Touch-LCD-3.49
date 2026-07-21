#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[32];
    char name[64];
    int day_of_week;
    char start_time[8];
    char end_time[8];
    char teacher[32];
    char location[64];
    char color[8];
    int remind_before;
} course_t;

esp_err_t ui_init(void);
void ui_update_statusbar(const char *date, const char *weekday,
                         const char *time_str, bool wifi_ok);
void ui_update_battery(int pct);
void ui_set_current_time(int hour, int minute);
void ui_set_orientation(bool landscape);
void ui_show_course_timeline(const course_t *courses, int count);
void ui_show_month_calendar(int year, int month, int today_day);
void ui_show_reminder(const char *course_name, const char *start_time);
void ui_hide_reminder(void);
void ui_toggle_view(void);
void ui_register_callbacks(void);

/* Screenshot functions */
void start_screenshot_task(void);
void take_screenshot(void);
void dump_framebuffer(void);

#ifdef __cplusplus
}
#endif
