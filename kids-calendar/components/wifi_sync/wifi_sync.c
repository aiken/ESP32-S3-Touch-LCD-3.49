#include "wifi_sync.h"

#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "pcf85063.h"

static const char *TAG = "WIFI";

#define WIFI_CONNECTED_BIT  BIT0
#define NTP_RETRY_COUNT     20
#define RESYNC_INTERVAL_MS  (6 * 3600 * 1000)

static EventGroupHandle_t s_wifi_events = NULL;
static bool s_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "disconnected, reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
        s_connected = true;
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_sync_init(void)
{
    s_wifi_events = xEventGroupCreate();

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop failed");
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL),
                        TAG, "wifi event register failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL),
                        TAG, "ip event register failed");

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, CONFIG_WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    ESP_LOGI(TAG, "connecting to %s ...", CONFIG_WIFI_SSID);
    return ESP_OK;
}

bool wifi_sync_is_connected(void)
{
    return s_connected;
}

static void time_sync_task(void *arg)
{
    /* POSIX TZ sign is inverted: Beijing (UTC+8) -> "CST-8" */
    char tz[16];
    snprintf(tz, sizeof(tz), "CST-%d", CONFIG_TIMEZONE_OFFSET);
    setenv("TZ", tz, 1);
    tzset();

    for (;;) {
        xEventGroupWaitBits(s_wifi_events, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "starting NTP sync (server: %s)", CONFIG_NTP_SERVER);
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, CONFIG_NTP_SERVER);
        esp_sntp_init();

        time_t now = 0;
        struct tm ti = {0};
        bool synced = false;
        for (int i = 0; i < NTP_RETRY_COUNT; i++) {
            time(&now);
            localtime_r(&now, &ti);
            if (ti.tm_year >= 126) {  /* 2026 or later => time is sane */
                synced = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        if (synced) {
            if (rtc_set_time(&ti) == ESP_OK) {
                ESP_LOGI(TAG, "RTC synced: %04d-%02d-%02d %02d:%02d:%02d (wday=%d)",
                         ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                         ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_wday);
            } else {
                ESP_LOGE(TAG, "RTC write failed");
            }
        } else {
            ESP_LOGW(TAG, "NTP sync timeout, retry later");
        }

        esp_sntp_stop();
        vTaskDelay(pdMS_TO_TICKS(RESYNC_INTERVAL_MS));
    }
}

esp_err_t wifi_sync_start_time_task(void)
{
    return xTaskCreatePinnedToCore(time_sync_task, "ntp_sync", 4096, NULL, 3, NULL, 0) == pdPASS
           ? ESP_OK : ESP_FAIL;
}
