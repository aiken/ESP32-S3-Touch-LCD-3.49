#include "alarm_buzzer.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/ledc.h"
#include <inttypes.h>

static const char *TAG = "ALARM_BUZZER";

static bool s_inited = false;

esp_err_t alarm_buzzer_init(const alarm_buzzer_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_LOGI(TAG, "Initializing buzzer on GPIO %d", config->buzzer_gpio);

    /* TODO: configure LEDC timer/channel for PWM buzzer */
    s_inited = true;
    return ESP_OK;
}

esp_err_t alarm_buzzer_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing buzzer");

    /* TODO: deinit LEDC */
    s_inited = false;
    return ESP_OK;
}

esp_err_t alarm_buzzer_on(void)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "buzzer not initialized");

    ESP_LOGI(TAG, "Buzzer ON");
    /* TODO: start PWM output */
    return ESP_OK;
}

esp_err_t alarm_buzzer_off(void)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "buzzer not initialized");

    ESP_LOGI(TAG, "Buzzer OFF");
    /* TODO: stop PWM output */
    return ESP_OK;
}

esp_err_t alarm_buzzer_beep(uint32_t on_ms, uint32_t off_ms, uint32_t repeat)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "buzzer not initialized");

    ESP_LOGI(TAG, "Beep on=%" PRIu32 "ms off=%" PRIu32 "ms repeat=%" PRIu32,
             on_ms, off_ms, repeat);
    /* TODO: implement non-blocking beep sequence */
    return ESP_OK;
}

esp_err_t alarm_buzzer_set_volume(uint8_t volume)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "buzzer not initialized");

    if (volume > 100) {
        volume = 100;
    }
    ESP_LOGI(TAG, "Set volume %u", volume);
    /* TODO: adjust PWM duty cycle */
    return ESP_OK;
}
