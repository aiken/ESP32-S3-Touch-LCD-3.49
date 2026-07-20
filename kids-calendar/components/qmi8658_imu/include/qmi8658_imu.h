#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* QMI8658 IMU on the RTC I2C bus (addr 0x6B). Accelerometer only —
   gyroscope stays disabled (orientation detection does not need it). */
esp_err_t imu_init(void);

/* Read acceleration in g. Returns ESP_ERR_INVALID_STATE if not initialized. */
esp_err_t imu_read_accel(float *x, float *y, float *z);

#ifdef __cplusplus
}
#endif
