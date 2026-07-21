#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "ui_calendar.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wi-Fi STA + NTP time sync. Credentials come from Kconfig
   (idf.py menuconfig -> Calendar App Configuration). */
esp_err_t wifi_sync_init(void);

/* Start background tasks: NTP time sync (6h) + course sync (5min,
   LAN URL first, Cloudflare URL as fallback). */
esp_err_t wifi_sync_start_time_task(void);

bool wifi_sync_is_connected(void);

/* Connected AP's SSID into buf (false when not connected) */
bool wifi_sync_get_ssid(char *buf, size_t len);

/* Active kid profile ("meixi" / "zhuangzhuang"), persisted in NVS */
const char *wifi_sync_get_kid(void);
void wifi_sync_set_kid(const char *kid);
void wifi_sync_toggle_kid(void);

/* Today's courses from the freshest successful server fetch.
   Returns course count (>=0), or -1 if nothing was ever fetched. */
int wifi_sync_get_courses(course_t *out, int max);

/* Monotonic version counter of the course data (UI watches for changes) */
uint32_t wifi_sync_courses_version(void);

/* true while a course refresh is in flight (for the 同步中 indicator) */
bool wifi_sync_is_syncing(void);

/* Force a refresh now (blocking, up to a few seconds) */
esp_err_t wifi_sync_refresh_courses(void);

#ifdef __cplusplus
}
#endif
