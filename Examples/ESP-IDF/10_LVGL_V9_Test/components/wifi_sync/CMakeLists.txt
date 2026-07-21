idf_component_register(
    SRCS "wifi_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi nvs_flash esp_netif
    PRIV_REQUIRES esp_event
)
