idf_component_register(
    SRCS "pcf85063.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES
        driver
        lwip
)
