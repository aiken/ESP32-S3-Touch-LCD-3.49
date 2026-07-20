#pragma once

#include <stdint.h>
#include "esp_err.h"

/**
 * @brief Register frame buffer for screenshot service
 * @param fb Frame buffer pointer (RGB565 format)
 * @param width Frame buffer width
 * @param height Frame buffer height
 */
void screenshot_server_register_fb(uint16_t *fb, uint16_t width, uint16_t height);

/**
 * @brief Start HTTP screenshot server
 * @param port Server port
 * @return ESP_OK on success
 */
esp_err_t screenshot_server_start(uint16_t port);
