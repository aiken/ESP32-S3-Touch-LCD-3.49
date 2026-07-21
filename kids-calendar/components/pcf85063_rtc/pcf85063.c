#include "pcf85063.h"

#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "esp_sntp.h"

static const char *TAG = "RTC";

#ifndef CONFIG_NTP_SERVER
#define CONFIG_NTP_SERVER "pool.ntp.org"
#endif
#ifndef CONFIG_TIMEZONE_OFFSET
#define CONFIG_TIMEZONE_OFFSET 8
#endif

#define RTC_PIN_SDA   47
#define RTC_PIN_SCL   48
#define RTC_ADDR      0x51
#define RTC_I2C_PORT  I2C_NUM_0
#define RTC_I2C_FREQ  300000

/* PCF85063 registers */
#define RTC_REG_CTRL1    0x00
#define RTC_REG_CTRL2    0x01
#define RTC_REG_OFFSET   0x02
#define RTC_REG_RAM      0x03
#define RTC_REG_SECONDS  0x04
#define RTC_REG_MINUTES  0x05
#define RTC_REG_HOURS    0x06
#define RTC_REG_DAYS     0x07
#define RTC_REG_WEEKDAYS 0x08
#define RTC_REG_MONTHS   0x09
#define RTC_REG_YEARS    0x0A

static i2c_master_bus_handle_t s_rtc_bus = NULL;
static i2c_master_dev_handle_t s_rtc_dev = NULL;
static bool s_ntp_done = false;

static uint8_t bcd_to_bin(uint8_t val)
{
    return (val >> 4) * 10 + (val & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

static esp_err_t rtc_i2c_read(uint8_t reg, uint8_t *buf, size_t len)
{
    ESP_RETURN_ON_FALSE(s_rtc_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "RTC device not ready");
    ESP_RETURN_ON_ERROR(i2c_master_bus_wait_all_done(s_rtc_bus, pdMS_TO_TICKS(100)), TAG, "I2C busy");
    return i2c_master_transmit_receive(s_rtc_dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

static esp_err_t rtc_i2c_write(uint8_t reg, const uint8_t *buf, size_t len)
{
    ESP_RETURN_ON_FALSE(s_rtc_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "RTC device not ready");
    uint8_t *tmp = malloc(len + 1);
    ESP_RETURN_ON_FALSE(tmp != NULL, ESP_ERR_NO_MEM, TAG, "malloc failed");
    tmp[0] = reg;
    memcpy(tmp + 1, buf, len);
    esp_err_t ret = i2c_master_transmit(s_rtc_dev, tmp, len + 1, pdMS_TO_TICKS(100));
    free(tmp);
    return ret;
}

i2c_master_bus_handle_t pcf85063_get_bus(void)
{
    return s_rtc_bus;
}

esp_err_t pcf85063_rtc_init(void)
{
    ESP_LOGI(TAG, "Initializing PCF85063 RTC (SDA=%d, SCL=%d, addr=0x%02X)",
             RTC_PIN_SDA, RTC_PIN_SCL, RTC_ADDR);

    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = RTC_I2C_PORT,
        .scl_io_num = RTC_PIN_SCL,
        .sda_io_num = RTC_PIN_SDA,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &s_rtc_bus), TAG, "I2C bus create failed");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = RTC_ADDR,
        .scl_speed_hz = RTC_I2C_FREQ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_rtc_bus, &dev_config, &s_rtc_dev),
                        TAG, "RTC device add failed");

    /* Clear stop bit and enable oscillator */
    uint8_t ctrl1 = 0x00;
    ESP_RETURN_ON_ERROR(rtc_i2c_write(RTC_REG_CTRL1, &ctrl1, 1), TAG, "set ctrl1 failed");

    ESP_LOGI(TAG, "RTC init complete");
    return ESP_OK;
}

esp_err_t pcf85063_rtc_get_time(struct tm *timeinfo)
{
    ESP_RETURN_ON_FALSE(timeinfo != NULL, ESP_ERR_INVALID_ARG, TAG, "timeinfo is NULL");

    uint8_t data[7];
    ESP_RETURN_ON_ERROR(rtc_i2c_read(RTC_REG_SECONDS, data, sizeof(data)), TAG, "read time failed");

    timeinfo->tm_sec  = bcd_to_bin(data[0] & 0x7F);
    timeinfo->tm_min  = bcd_to_bin(data[1] & 0x7F);
    timeinfo->tm_hour = bcd_to_bin(data[2] & 0x3F);
    timeinfo->tm_mday = bcd_to_bin(data[3] & 0x3F);
    timeinfo->tm_wday = bcd_to_bin(data[4] & 0x07);
    timeinfo->tm_mon  = bcd_to_bin(data[5] & 0x1F) - 1;
    timeinfo->tm_year = bcd_to_bin(data[6]) + 100;

    return ESP_OK;
}

esp_err_t pcf85063_rtc_set_time(struct tm *timeinfo)
{
    ESP_RETURN_ON_FALSE(timeinfo != NULL, ESP_ERR_INVALID_ARG, TAG, "timeinfo is NULL");

    uint8_t data[7];
    data[0] = bin_to_bcd(timeinfo->tm_sec) & 0x7F;
    data[1] = bin_to_bcd(timeinfo->tm_min) & 0x7F;
    data[2] = bin_to_bcd(timeinfo->tm_hour) & 0x3F;
    data[3] = bin_to_bcd(timeinfo->tm_mday) & 0x3F;
    data[4] = bin_to_bcd(timeinfo->tm_wday) & 0x07;
    data[5] = bin_to_bcd(timeinfo->tm_mon + 1) & 0x1F;
    data[6] = bin_to_bcd(timeinfo->tm_year - 100);

    ESP_RETURN_ON_ERROR(rtc_i2c_write(RTC_REG_SECONDS, data, sizeof(data)), TAG, "write time failed");

    ESP_LOGI(TAG, "RTC time set to %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
             timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    return ESP_OK;
}

int pcf85063_rtc_get_weekday(void)
{
    struct tm timeinfo;
    if (pcf85063_rtc_get_time(&timeinfo) == ESP_OK) {
        return timeinfo.tm_wday;
    }
    return -1;
}

static void sntp_sync_cb(struct timeval *tv)
{
    (void)tv;
    ESP_LOGI(TAG, "SNTP time received");

    /* Convert UTC to local time and write to RTC */
    time_t now = 0;
    struct tm timeinfo = {0};
    time(&now);

    /* Apply timezone offset (e.g. UTC+8 Beijing) */
    time_t local = now + CONFIG_TIMEZONE_OFFSET * 3600;
    timeinfo = *gmtime(&local);

    pcf85063_rtc_set_time(&timeinfo);
    s_ntp_done = true;
}

esp_err_t pcf85063_rtc_sync_ntp(void)
{
    if (s_ntp_done) {
        ESP_LOGI(TAG, "NTP already synced");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting SNTP sync, server=%s", CONFIG_NTP_SERVER);

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, CONFIG_NTP_SERVER);
    esp_sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();

    /* Wait up to 15 seconds for sync */
    int retry = 0;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retry < 150) {
        vTaskDelay(pdMS_TO_TICKS(100));
        retry++;
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        ESP_LOGI(TAG, "SNTP sync complete");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "SNTP sync timeout");
    return ESP_ERR_TIMEOUT;
}
