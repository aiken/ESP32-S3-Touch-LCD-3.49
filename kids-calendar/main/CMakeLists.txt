idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        ui_calendar
        i2c_bsp
        lcd_bl_pwm_bsp
        pcf85063_rtc
        usb_screenshot
        battery_bsp
        qmi8658_imu
        driver
        esp_lcd
        espressif__esp_lcd_axs15231b
        esp_timer
)

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)
