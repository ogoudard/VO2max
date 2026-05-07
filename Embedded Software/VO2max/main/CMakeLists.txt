idf_component_register(SRCS "driver_bmp388.c" "debug.c" "driver_lcd.c" "settings.c" "bluetooth.c" "driver_bmp280.c" "measure.c" "hmi.c" "menu.c" "driver_sdp8XX.c" "battery.c" "driver_me2o2.c" "driver_scd30.c" "app_main.c" "driver_scd30.c"
                       PRIV_REQUIRES esp_driver_spi esp_driver_gpio esp_driver_i2c esp_adc esp_timer bt nvs_flash esp_driver_uart
                       INCLUDE_DIRS ".")
component_compile_options("-Wfatal-errors")