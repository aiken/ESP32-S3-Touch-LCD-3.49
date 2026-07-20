#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Register frame buffer for USB screenshot
 * @param fb Frame buffer pointer (RGB565 format)
 * @param w Width
 * @param h Height
 */
void usb_screenshot_register_fb(uint16_t *fb, uint16_t w, uint16_t h);

/**
 * @brief Send screenshot via USB-Serial-JTAG (native USB)
 */
void usb_screenshot_send(void);

/**
 * @brief Start command listener: sends one frame on "screenshot" command
 */
void usb_screenshot_start_cmd(void);
