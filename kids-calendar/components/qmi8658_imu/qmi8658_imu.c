#include "qmi8658_imu.h"

#include <string.h>
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcf85063.h"

static const char *TAG = "IMU";

#define QMI8658_ADDR        0x6B
#define QMI8658_REG_WHOAMI  0x00
#define QMI8658_REG_CTRL2   0x03
#define QMI8658_REG_CTRL7   0x08
#define QMI8658_REG_AX_L    0x35
#define QMI8658_WHOAMI_ID   0x05

/* CTRL2: aFS[6:4]=001 (±4g), aODR[3:0]=1101 (low-power 21Hz, accel-only) */
#define QMI8658_CTRL2_VAL   0x1D
/* CTRL7: bit0 = aEN (accelerometer enable) */
#define QMI8658_CTRL7_VAL   0x01

static i2c_master_dev_handle_t s_imu_dev = NULL;

static esp_err_t imu_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_imu_dev, buf, 2, pdMS_TO_TICKS(100));
}

static esp_err_t imu_read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_imu_dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

esp_err_t imu_init(void)
{
    i2c_master_bus_handle_t bus = pcf85063_get_bus();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_STATE, TAG, "RTC bus not ready");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = QMI8658_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_config, &s_imu_dev),
                        TAG, "IMU device add failed");

    uint8_t whoami = 0;
    ESP_RETURN_ON_ERROR(imu_read_regs(QMI8658_REG_WHOAMI, &whoami, 1), TAG, "WHOAMI read failed");
    if (whoami != QMI8658_WHOAMI_ID) {
        ESP_LOGE(TAG, "WHOAMI mismatch: 0x%02X (expect 0x%02X)", whoami, QMI8658_WHOAMI_ID);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(imu_write_reg(QMI8658_REG_CTRL2, QMI8658_CTRL2_VAL), TAG, "CTRL2 failed");
    ESP_RETURN_ON_ERROR(imu_write_reg(QMI8658_REG_CTRL7, QMI8658_CTRL7_VAL), TAG, "CTRL7 failed");

    ESP_LOGI(TAG, "QMI8658 init complete (WHOAMI=0x%02X)", whoami);
    return ESP_OK;
}

esp_err_t imu_read_accel(float *x, float *y, float *z)
{
    ESP_RETURN_ON_FALSE(s_imu_dev != NULL, ESP_ERR_INVALID_STATE, TAG, "IMU not initialized");

    uint8_t raw[6];
    ESP_RETURN_ON_ERROR(imu_read_regs(QMI8658_REG_AX_L, raw, sizeof(raw)), TAG, "accel read failed");

    int16_t ax = (int16_t)((uint16_t)raw[1] << 8 | raw[0]);
    int16_t ay = (int16_t)((uint16_t)raw[3] << 8 | raw[2]);
    int16_t az = (int16_t)((uint16_t)raw[5] << 8 | raw[4]);

    const float scale = 4.0f / 32768.0f; /* ±4g */
    *x = ax * scale;
    *y = ay * scale;
    *z = az * scale;
    return ESP_OK;
}
