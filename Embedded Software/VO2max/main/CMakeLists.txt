idf_component_register(SRCS "bmp280.c" "measure.c" "hmi.c" "menu.c" "driver_st7789.c" "driver_lcd.c" "driver_sdp8XX.c" "battery.c" "driver_me2o2.c" "driver_scd30.c" "app_main.c" "driver_scd30.c"
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_driver_i2c esp_adc esp_timer
                       INCLUDE_DIRS ".")
component_compile_options("-Wfatal-errors")