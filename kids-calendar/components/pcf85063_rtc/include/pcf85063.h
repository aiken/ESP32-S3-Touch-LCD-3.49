#pragma once

#include <time.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note: "rtc_*" names conflict with ESP-IDF internal symbols in esp_hw_support.
   The real implementation uses pcf85063_rtc_*; the macros below keep the
   project-level API names from the specification working. */

esp_err_t pcf85063_rtc_init(void);
esp_err_t pcf85063_rtc_get_time(struct tm *timeinfo);
esp_err_t pcf85063_rtc_set_time(struct tm *timeinfo);
esp_err_t pcf85063_rtc_sync_ntp(void);
int pcf85063_rtc_get_weekday(void);

/* Bus handle shared with other devices on the RTC I2C bus (IMU, TCA9554) */
i2c_master_bus_handle_t pcf85063_get_bus(void);

#define rtc_init         pcf85063_rtc_init
#define rtc_get_time     pcf85063_rtc_get_time
#define rtc_set_time     pcf85063_rtc_set_time
#define rtc_sync_ntp     pcf85063_rtc_sync_ntp
#define rtc_get_weekday  pcf85063_rtc_get_weekday

#ifdef __cplusplus
}
#endif
