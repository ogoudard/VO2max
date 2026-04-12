/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver_st7789_basic.h"
#include "driver_sdp810.h"
#include "driver/i2c_master.h"
#include "driver_scd30.h"
#include "driver_me2o2.h"

#define I2C_MASTER_SDA_IO 21
#define I2C_MASTER_SCL_IO 22

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing system...\n");

    i2c_master_bus_config_t i2cMasterConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
    };
    i2c_master_bus_handle_t busHandle;

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2cMasterConfig, &busHandle));

    SDP810_Initialize(busHandle);
    SDP810_StartContinuousMeasurementWithMassFlowTCompAndAveraging();

    SCD30_Initialize(busHandle);
    SCD30_SetMeasurementInterval(2);
    SCD30_StartPeriodicMeasurment();

    ME2O2_Initialize(busHandle);
    ME2O2_Calibrate(20.9);

    while (1) {
        float diffPressure;
        float temperatureSdp810;
        float co2;
        float temperatureScd30;
        float humidity;
        float o2;

        if(SDP810_ReadMeasurement(&diffPressure, &temperatureSdp810))
        {
            ESP_LOGI(TAG, "Differential pressure = %.3f hPa, Temperature = %.1f C", diffPressure, temperatureSdp810);
        }

        if(SCD30_GetMeasures(&co2, &temperatureScd30, &humidity))
        {
            ESP_LOGI(TAG, "CO2 = %.1f ppm, Temperature = %.1f C, Humidity = %.1f %%", co2, temperatureScd30, humidity);
        }

        o2 = ME2O2_ReadOxygen();

        ESP_LOGI(TAG, "O2 = %.1f %", o2);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
