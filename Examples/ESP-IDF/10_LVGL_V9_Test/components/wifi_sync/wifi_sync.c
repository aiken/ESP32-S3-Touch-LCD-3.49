#include "wifi_sync.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_SYNC";

static bool s_inited = false;
static bool s_connected = false;
static wifi_sync_on_connected_cb_t s_on_connected = NULL;
static wifi_sync_on_disconnected_cb_t s_on_disconnected = NULL;

esp_err_t wifi_sync_init(void)
{
    ESP_LOGI(TAG, "Initializing Wi-Fi sync subsystem");

    /* TODO: init NVS, create default netif, init Wi-Fi */
    s_inited = true;
    return ESP_OK;
}

esp_err_t wifi_sync_deinit(void)
{
    ESP_LOGI(TAG, "Deinitializing Wi-Fi sync subsystem");

    /* TODO: stop Wi-Fi, deinit netif/NVS */
    s_inited = false;
    s_connected = false;
    return ESP_OK;
}

esp_err_t wifi_sync_start(const wifi_sync_config_t *config)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi sync not initialized");
    ESP_RETURN_ON_FALSE(config != NULL, ESP_ERR_INVALID_ARG, TAG, "config is NULL");

    ESP_LOGI(TAG, "Starting Wi-Fi connection to SSID: %s", config->ssid);
    /* TODO: configure Wi-Fi config and start station */
    return ESP_OK;
}

esp_err_t wifi_sync_stop(void)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi sync not initialized");

    ESP_LOGI(TAG, "Stopping Wi-Fi");
    s_connected = false;
    /* TODO: disconnect and stop Wi-Fi */
    return ESP_OK;
}

esp_err_t wifi_sync_ntp_sync(const char *ntp_server)
{
    ESP_RETURN_ON_FALSE(s_inited, ESP_ERR_INVALID_STATE, TAG, "Wi-Fi sync not initialized");
    ESP_RETURN_ON_FALSE(ntp_server != NULL, ESP_ERR_INVALID_ARG, TAG, "ntp_server is NULL");

    ESP_LOGI(TAG, "Syncing NTP from %s", ntp_server);
    /* TODO: start SNTP and wait for sync */
    return ESP_OK;
}

bool wifi_sync_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_sync_register_callbacks(wifi_sync_on_connected_cb_t on_connected,
                                       wifi_sync_on_disconnected_cb_t on_disconnected)
{
    s_on_connected = on_connected;
    s_on_disconnected = on_disconnected;
    ESP_LOGI(TAG, "Wi-Fi event callbacks registered");
    return ESP_OK;
}
