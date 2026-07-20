idf_component_register(
    SRCS "qmi8658_imu.c"
    INCLUDE_DIRS "include"
    REQUIRES driver pcf85063_rtc
)
