#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Wi-Fi STA + NTP time sync. Credentials come from Kconfig
   (idf.py menuconfig -> Calendar App Configuration). */
esp_err_t wifi_sync_init(void);

/* Start the background task: wait for IP -> SNTP sync -> write RTC,
   then re-sync every 6 hours. */
esp_err_t wifi_sync_start_time_task(void);

bool wifi_sync_is_connected(void);

#ifdef __cplusplus
}
#endif
