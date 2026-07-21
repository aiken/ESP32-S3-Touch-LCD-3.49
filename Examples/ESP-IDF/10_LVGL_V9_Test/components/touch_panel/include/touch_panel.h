#ifndef TOUCH_PANEL_H
#define TOUCH_PANEL_H

#include "esp_err.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Touch panel I2C configuration
 */
typedef struct {
    int sda_gpio;
    int scl_gpio;
    int int_gpio;
    int rst_gpio;
    uint8_t i2c_addr;
    int i2c_port;
} touch_panel_config_t;

/**
 * @brief Initialize the touch panel
 *
 * @param config Pointer to touch panel configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t touch_panel_init(const touch_panel_config_t *config);

/**
 * @brief Deinitialize the touch panel
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t touch_panel_deinit(void);

/**
 * @brief Get the underlying LCD touch handle
 *
 * @param tp Output pointer to touch handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t touch_panel_read_data(esp_lcd_touch_handle_t *tp);

/**
 * @brief Read latest touch coordinates
 *
 * @param x Buffer for X coordinates
 * @param y Buffer for Y coordinates
 * @param strength Buffer for touch strength
 * @param point_num Output number of touch points
 * @param max_points Maximum number of points to read
 * @return esp_err_t ESP_OK on success
 */
esp_err_t touch_panel_get_coordinates(uint16_t *x, uint16_t *y, uint16_t *strength,
                                      uint8_t *point_num, uint8_t max_points);

/**
 * @brief Set touch coordinate mirroring
 *
 * @param mirror_x Mirror X axis
 * @param mirror_y Mirror Y axis
 * @return esp_err_t ESP_OK on success
 */
esp_err_t touch_panel_set_mirror_xy(bool mirror_x, bool mirror_y);

#ifdef __cplusplus
}
#endif

#endif
