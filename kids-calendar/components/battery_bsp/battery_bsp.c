#include "battery_bsp.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pcf85063.h"

static const char *TAG = "BATT";

/* TCA9554 I/O expander on the RTC I2C bus */
#define TCA9554_ADDR        0x20
#define TCA9554_REG_OUTPUT  0x01
#define TCA9554_REG_CONFIG  0x03
#define TCA9554_EXIO1_BIT   0x02   /* EXIO1: battery measure enable (active low) */

/* Battery voltage: ADC1_CH3 (GPIO4), 3:1 divider */
#define BATT_ADC_UNIT       ADC_UNIT_1
#define BATT_ADC_CHAN       ADC_CHANNEL_3

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;

static esp_err_t tca9554_clear_bit(uint8_t reg, uint8_t mask)
{
    i2c_master_bus_handle_t bus = pcf85063_get_bus();
    ESP_RETURN_ON_FALSE(bus != NULL, ESP_ERR_INVALID_STATE, TAG, "RTC bus not ready");

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TCA9554_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_dev_handle_t dev;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &dev_config, &dev), TAG, "TCA9554 add failed");

    uint8_t val = 0;
    esp_err_t ret = i2c_master_transmit_receive(dev, &reg, 1, &val, 1, pdMS_TO_TICKS(100));
    if (ret == ESP_OK) {
        uint8_t buf[2] = {reg, (uint8_t)(val & ~mask)};
        ret = i2c_master_transmit(dev, buf, 2, pdMS_TO_TICKS(100));
    }
    i2c_master_bus_rm_device(dev);
    return ret;
}

esp_err_t battery_bsp_init(void)
{
    /* EXIO1: output, low (enable battery voltage divider) */
    ESP_RETURN_ON_ERROR(tca9554_clear_bit(TCA9554_REG_OUTPUT, TCA9554_EXIO1_BIT),
                        TAG, "TCA9554 output reg failed");
    ESP_RETURN_ON_ERROR(tca9554_clear_bit(TCA9554_REG_CONFIG, TCA9554_EXIO1_BIT),
                        TAG, "TCA9554 config reg failed");

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = BATT_ADC_UNIT,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &s_adc), TAG, "ADC unit failed");

    adc_oneshot_chan_cfg_t chan_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(s_adc, BATT_ADC_CHAN, &chan_config),
                        TAG, "ADC channel failed");

    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = BATT_ADC_UNIT,
        .chan = BATT_ADC_CHAN,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_config, &s_cali);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration unavailable (%s), using raw formula",
                 esp_err_to_name(err));
        s_cali = NULL;
    }

    ESP_LOGI(TAG, "Battery ADC init complete");
    return ESP_OK;
}

float battery_get_voltage(void)
{
    if (!s_adc) {
        return -1.0f;
    }

    int raw = 0;
    if (adc_oneshot_read(s_adc, BATT_ADC_CHAN, &raw) != ESP_OK) {
        return -1.0f;
    }

    float vadc;
    if (s_cali) {
        int mv = 0;
        adc_cali_raw_to_voltage(s_cali, raw, &mv);
        vadc = mv * 0.001f;
    } else {
        vadc = raw * 3.3f / 4096.0f;
    }
    return vadc * 3.0f; /* 3:1 divider */
}

int battery_get_percent(void)
{
    float vbat = battery_get_voltage();
    if (vbat < 0) {
        return -1;
    }

    int pct = (int)((vbat - 3.3f) / (4.2f - 3.3f) * 100.0f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}
