idf_component_register(
    SRCS "ui_calendar.c" "ui_styles.c" "ui_screenshot.c" "lv_font_chinese_14.c" "lv_font_chinese_16.c" "lv_font_chinese_20.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl
    PRIV_REQUIRES esp_driver_uart
)
