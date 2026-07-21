#include "touch_panel.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"

static const char *TAG = "TOUCH_PANEL";

static esp_lcd_touch_handle_t s_touch = NULL;

esp_err_t touch_panel_init(const touch_panel_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_LOGI(TAG, "Initializing touch panel (SDA=%d, SCL=%d, INT=%d)",
             config->sda_gpio, config->scl_gpio, config->int_gpio);

    /* TODO: create I2C bus and register touch driver */
    /* Coordinate buffers larger than 1 KB must be allocated from heap */

    return ESP_OK;
}

esp_err_t touch_panel_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing touch panel");

    if (s_touch != NULL) {
        esp_lcd_touch_del(s_touch);
        s_touch = NULL;
    }

    return ESP_OK;
}

esp_err_t touch_panel_read_data(esp_lcd_touch_handle_t *tp)
{
    ESP_RETURN_ON_FALSE(tp != NULL, ESP_ERR_INVALID_ARG, TAG, "tp pointer is NULL");
    ESP_RETURN_ON_FALSE(s_touch != NULL, ESP_ERR_INVALID_STATE, TAG, "touch not initialized");

    *tp = s_touch;
    return ESP_OK;
}

esp_err_t touch_panel_get_coordinates(uint16_t *x, uint16_t *y, uint16_t *strength,
                                      uint8_t *point_num, uint8_t max_points)
{
    ESP_RETURN_ON_FALSE(s_touch != NULL, ESP_ERR_INVALID_STATE, TAG, "touch not initialized");
    ESP_RETURN_ON_FALSE(x != NULL && y != NULL && strength != NULL && point_num != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "output buffers are NULL");

    ESP_RETURN_ON_ERROR(esp_lcd_touch_read_data(s_touch), TAG, "read touch data failed");

    return esp_lcd_touch_get_coordinates(s_touch, x, y, strength, point_num, max_points);
}

esp_err_t touch_panel_set_mirror_xy(bool mirror_x, bool mirror_y)
{
    ESP_RETURN_ON_FALSE(s_touch != NULL, ESP_ERR_INVALID_STATE, TAG, "touch not initialized");

    ESP_LOGI(TAG, "Set mirror x=%d, y=%d", mirror_x, mirror_y);
    /* TODO: apply mirror flags to touch driver */
    return ESP_OK;
}
