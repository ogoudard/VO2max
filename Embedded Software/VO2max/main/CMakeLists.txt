idf_component_register(SRCS "driver_sdp8XX.c" "driver_lcd.c" "battery.c" "driver_me2o2.c" "driver_scd30.c" "app_main.c" "driver_st7789.c" "driver_scd30.c" "driver_me2o2"
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_driver_i2c esp_adc esp_timer
                       INCLUDE_DIRS ".")
