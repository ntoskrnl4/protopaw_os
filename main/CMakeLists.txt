idf_component_register(
    SRCS
        entrypoint.c
        init.c
    INCLUDE_DIRS "."
    REQUIRES
        driver
        display
        imu
        input
        sdcard
        esp_psram
        esp_app_format
)
