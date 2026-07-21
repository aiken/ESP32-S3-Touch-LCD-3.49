#include "pcf85063_rtc.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"

static const char *TAG = "PCF85063_RTC";

#define PCF85063_I2C_ADDR 0x51

static int s_i2c_port = -1;

esp_err_t pcf85063_rtc_init(const pcf85063_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_LOGI(TAG, "Initializing PCF85063 RTC (SDA=%d, SCL=%d)", config->sda_gpio, config->scl_gpio);

    s_i2c_port = config->i2c_port;

    /* TODO: initialize I2C bus and verify PCF85063 chip ID */
    return ESP_OK;
}

esp_err_t pcf85063_rtc_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing RTC");
    s_i2c_port = -1;
    return ESP_OK;
}

esp_err_t pcf85063_rtc_get_time(struct tm *out_time)
{
    ESP_RETURN_ON_FALSE(out_time != NULL, ESP_ERR_INVALID_ARG, TAG, "out_time is NULL");
    ESP_RETURN_ON_FALSE(s_i2c_port >= 0, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");

    /* TODO: read BCD registers and convert to struct tm */
    return ESP_OK;
}

esp_err_t pcf85063_rtc_set_time(const struct tm *timeinfo)
{
    ESP_RETURN_ON_FALSE(timeinfo != NULL, ESP_ERR_INVALID_ARG, TAG, "timeinfo is NULL");
    ESP_RETURN_ON_FALSE(s_i2c_port >= 0, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");

    /* TODO: convert struct tm to BCD and write to RTC */
    return ESP_OK;
}

esp_err_t pcf85063_rtc_set_alarm(uint8_t minute, uint8_t hour, uint8_t day, uint8_t weekday)
{
    ESP_RETURN_ON_FALSE(s_i2c_port >= 0, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");

    ESP_LOGI(TAG, "Set alarm min=%02X hour=%02X day=%02X weekday=%02X",
             minute, hour, day, weekday);
    /* TODO: write alarm registers */
    return ESP_OK;
}

esp_err_t pcf85063_rtc_enable_alarm(bool enable)
{
    ESP_RETURN_ON_FALSE(s_i2c_port >= 0, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");

    ESP_LOGI(TAG, "%s alarm interrupt", enable ? "Enable" : "Disable");
    /* TODO: set control register alarm enable bit */
    return ESP_OK;
}

esp_err_t pcf85063_rtc_clear_alarm(void)
{
    ESP_RETURN_ON_FALSE(s_i2c_port >= 0, ESP_ERR_INVALID_STATE, TAG, "RTC not initialized");

    ESP_LOGI(TAG, "Clear alarm flag");
    /* TODO: clear alarm flag in control/status register */
    return ESP_OK;
}
