#ifndef PCF85063_RTC_H
#define PCF85063_RTC_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PCF85063 RTC I2C configuration
 */
typedef struct {
    int sda_gpio;
    int scl_gpio;
    int i2c_port;
    uint32_t i2c_freq_hz;
} pcf85063_config_t;

/**
 * @brief Initialize the PCF85063 RTC
 *
 * @param config Pointer to RTC configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_init(const pcf85063_config_t *config);

/**
 * @brief Deinitialize the RTC
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_deinit(void);

/**
 * @brief Read current time from RTC
 *
 * @param out_time Output time structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_get_time(struct tm *out_time);

/**
 * @brief Write current time to RTC
 *
 * @param timeinfo Time structure to write
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_set_time(const struct tm *timeinfo);

/**
 * @brief Configure RTC alarm
 *
 * @param minute Alarm minute (0-59), 0xFF to ignore
 * @param hour Alarm hour (0-23), 0xFF to ignore
 * @param day Alarm day (1-31), 0xFF to ignore
 * @param weekday Alarm weekday (0-6), 0xFF to ignore
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_set_alarm(uint8_t minute, uint8_t hour, uint8_t day, uint8_t weekday);

/**
 * @brief Enable or disable RTC alarm interrupt
 *
 * @param enable true to enable, false to disable
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_enable_alarm(bool enable);

/**
 * @brief Clear pending RTC alarm flag
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t pcf85063_rtc_clear_alarm(void);

#ifdef __cplusplus
}
#endif

#endif
