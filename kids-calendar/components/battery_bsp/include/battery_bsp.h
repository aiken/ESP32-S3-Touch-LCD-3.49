#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Init battery voltage measurement:
   - TCA9554 (on RTC I2C bus, addr 0x20): EXIO1 low = measure enable
   - ADC1_CH3 (GPIO4), divider 3:1, curve-fitting calibration */
esp_err_t battery_bsp_init(void);

/* Battery voltage in volts, -1.0 on read error */
float battery_get_voltage(void);

/* Battery level 0..100 (linear 3.3V..4.2V approximation) */
int battery_get_percent(void);

#ifdef __cplusplus
}
#endif
