#ifndef AXS15231B_DISPLAY_H
#define AXS15231B_DISPLAY_H

#include "esp_err.h"
#include "esp_lcd_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AXS15231B QSPI display configuration
 */
typedef struct {
    int cs_gpio;
    int sck_gpio;
    int d0_gpio;
    int d1_gpio;
    int d2_gpio;
    int d3_gpio;
    int rst_gpio;
    int te_gpio;
    int bk_light_gpio;
    int h_res;
    int v_res;
} axs15231b_config_t;

/**
 * @brief Initialize the AXS15231B display panel
 *
 * @param config Pointer to display configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t axs15231b_display_init(const axs15231b_config_t *config);

/**
 * @brief Deinitialize the display panel
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t axs15231b_display_deinit(void);

/**
 * @brief Get the LCD panel handle
 *
 * @param panel Output pointer to panel handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t axs15231b_display_get_panel(esp_lcd_panel_handle_t *panel);

/**
 * @brief Set backlight brightness
 *
 * @param brightness Brightness level (0-255)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t axs15231b_display_set_backlight(uint8_t brightness);

/**
 * @brief Enable or disable tearing effect (TE) signal
 *
 * @param enable true to enable, false to disable
 * @return esp_err_t ESP_OK on success
 */
esp_err_t axs15231b_display_enable_te(bool enable);

#ifdef __cplusplus
}
#endif

#endif
