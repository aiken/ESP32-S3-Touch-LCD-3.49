#include "axs15231b_display.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_axs15231b.h"

static const char *TAG = "AXS15231B_DISPLAY";

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static int s_bk_light_gpio = -1;

esp_err_t axs15231b_display_init(const axs15231b_config_t *config)
{
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");
    ESP_LOGI(TAG, "Initializing AXS15231B QSPI display (%dx%d)", config->h_res, config->v_res);

    /* TODO: implement QSPI bus + panel IO + AXS15231B driver init */
    /* Buffers larger than 1 KB must be allocated from heap (heap_caps_malloc) */

    s_bk_light_gpio = config->bk_light_gpio;
    return ESP_OK;
}

esp_err_t axs15231b_display_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing display");

    if (s_panel != NULL) {
        esp_lcd_panel_del(s_panel);
        s_panel = NULL;
    }

    if (s_io_handle != NULL) {
        esp_lcd_panel_io_del(s_io_handle);
        s_io_handle = NULL;
    }

    s_bk_light_gpio = -1;
    return ESP_OK;
}

esp_err_t axs15231b_display_get_panel(esp_lcd_panel_handle_t *panel)
{
    ESP_RETURN_ON_FALSE(panel != NULL, ESP_ERR_INVALID_ARG, TAG, "panel pointer is NULL");
    ESP_RETURN_ON_FALSE(s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel not initialized");

    *panel = s_panel;
    return ESP_OK;
}

esp_err_t axs15231b_display_set_backlight(uint8_t brightness)
{
    ESP_RETURN_ON_FALSE(s_bk_light_gpio >= 0, ESP_ERR_INVALID_STATE, TAG, "backlight GPIO not configured");

    ESP_LOGI(TAG, "Set backlight brightness: %u", brightness);
    /* TODO: implement PWM or GPIO backlight control */
    return ESP_OK;
}

esp_err_t axs15231b_display_enable_te(bool enable)
{
    ESP_RETURN_ON_FALSE(s_panel != NULL, ESP_ERR_INVALID_STATE, TAG, "panel not initialized");

    ESP_LOGI(TAG, "%s tearing effect", enable ? "Enable" : "Disable");
    /* TODO: configure TE GPIO / panel command */
    return ESP_OK;
}
