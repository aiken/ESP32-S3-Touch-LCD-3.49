idf_component_register(
    SRCS "wifi_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_netif lwip pcf85063_rtc driver
)
