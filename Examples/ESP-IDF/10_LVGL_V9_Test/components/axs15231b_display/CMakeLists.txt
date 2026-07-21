idf_component_register(
    SRCS "axs15231b_display.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd
    PRIV_REQUIRES driver esp_lcd_axs15231b
)
