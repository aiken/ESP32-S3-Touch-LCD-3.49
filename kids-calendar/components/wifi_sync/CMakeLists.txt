idf_component_register(
    SRCS "wifi_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif lwip pcf85063_rtc ui_calendar driver esp-tls json nvs_flash esp_http_client
)
