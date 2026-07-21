#ifndef ALARM_BUZZER_H
#define ALARM_BUZZER_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Buzzer PWM configuration
 */
typedef struct {
    int buzzer_gpio;
    uint32_t pwm_freq_hz;
    uint32_t pwm_resolution;
    uint32_t pwm_channel;
} alarm_buzzer_config_t;

/**
 * @brief Initialize the alarm buzzer
 *
 * @param config Pointer to buzzer configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_init(const alarm_buzzer_config_t *config);

/**
 * @brief Deinitialize the alarm buzzer
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_deinit(void);

/**
 * @brief Turn buzzer on
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_on(void);

/**
 * @brief Turn buzzer off
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_off(void);

/**
 * @brief Produce a beep sequence
 *
 * @param on_ms On duration in milliseconds
 * @param off_ms Off duration in milliseconds
 * @param repeat Number of beep repeats
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint32_t repeat);

/**
 * @brief Set buzzer volume
 *
 * @param volume Volume level (0-100)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t alarm_buzzer_set_volume(uint8_t volume);

#ifdef __cplusplus
}
#endif

#endif
