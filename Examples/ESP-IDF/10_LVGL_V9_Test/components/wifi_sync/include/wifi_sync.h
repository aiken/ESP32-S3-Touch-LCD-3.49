#ifndef WIFI_SYNC_H
#define WIFI_SYNC_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Wi-Fi station configuration
 */
typedef struct {
    char ssid[32];
    char password[64];
    uint32_t retry_num;
} wifi_sync_config_t;

/**
 * @brief Connected event callback
 */
typedef void (*wifi_sync_on_connected_cb_t)(void);

/**
 * @brief Disconnected event callback
 */
typedef void (*wifi_sync_on_disconnected_cb_t)(void);

/**
 * @brief Initialize Wi-Fi sync subsystem (NVS + netif + Wi-Fi)
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_init(void);

/**
 * @brief Deinitialize Wi-Fi sync subsystem
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_deinit(void);

/**
 * @brief Start Wi-Fi station with given credentials
 *
 * @param config Pointer to Wi-Fi configuration
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_start(const wifi_sync_config_t *config);

/**
 * @brief Stop Wi-Fi station
 *
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_stop(void);

/**
 * @brief Synchronize time via NTP
 *
 * @param ntp_server NTP server hostname or IP
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_ntp_sync(const char *ntp_server);

/**
 * @brief Check whether Wi-Fi is currently connected
 *
 * @return true Connected
 * @return false Not connected
 */
bool wifi_sync_is_connected(void);

/**
 * @brief Register Wi-Fi event callbacks
 *
 * @param on_connected Connected callback (may be NULL)
 * @param on_disconnected Disconnected callback (may be NULL)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t wifi_sync_register_callbacks(wifi_sync_on_connected_cb_t on_connected,
                                       wifi_sync_on_disconnected_cb_t on_disconnected);

#ifdef __cplusplus
}
#endif

#endif
