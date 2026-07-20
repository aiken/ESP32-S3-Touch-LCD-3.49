#pragma once

#include <stdint.h>
#include <stdbool.h>
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

/**
 * @brief Optional hooks so the frame copy is consistent: called around the
 * frame grab as lock -> refresh -> copy -> unlock. Register from the LVGL
 * porting layer (lock = take LVGL mutex, refresh = lv_refr_now wrapper).
 */
typedef bool (*usb_ss_lock_cb)(int timeout_ms);
typedef void (*usb_ss_unlock_cb)(void);
typedef void (*usb_ss_refresh_cb)(void);
void usb_screenshot_set_hooks(usb_ss_lock_cb lock, usb_ss_unlock_cb unlock,
                              usb_ss_refresh_cb refresh);
