idf_component_register(
    SRCS "battery_bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc driver pcf85063_rtc
)
