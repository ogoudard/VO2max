#include <stdio.h>

#include "version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver_st7789_basic.h"
#include "driver_sdp810.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver_scd30.h"
#include "driver_me2o2.h"
#include "battery.h"

#define SPI_HOST SPI2_HOST

#define I2C_BUS_NUM I2C_NUM_0

#define GPIO_PIN_NUM_I2C_SDA 21
#define GPIO_PIN_NUM_I2C_SCL 22

#define GPIO_PIN_NUM_SPI_MISO 25
#define GPIO_PIN_NUM_SPI_MOSI 19
#define GPIO_PIN_NUM_SPI_CLK 18

static const char *TAG = "main";

static void I2cBusInit(i2c_master_bus_handle_t *busHandle);
static void SpiBusInit();

void app_main(void)
{
    i2c_master_bus_handle_t i2cHandle;
    float batterySoc;

    ESP_LOGI(TAG, "VO2max embedded software version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    ESP_LOGI(TAG, "Initializing system...");

    BATTERY_Initialize();
    batterySoc = BATTERY_MeasureSoc();
    ESP_LOGI(TAG, "Battery SOC = %.0f %%", batterySoc);

    SpiBusInit();
    // st7789_basic_init();

    I2cBusInit(&i2cHandle);

    SDP810_Initialize(i2cHandle);
    SDP810_StartContinuousMeasurementWithMassFlowTCompAndAveraging();

    SCD30_Initialize(i2cHandle);
    SCD30_SetMeasurementInterval(2);
    SCD30_StartPeriodicMeasurment();

    ME2O2_Initialize(i2cHandle);
    ME2O2_Calibrate(20.9);

    while (1)
    {
        float diffPressure;
        float temperatureSdp810;
        float co2;
        float temperatureScd30;
        float humidity;
        float o2;

        if (SDP810_ReadMeasurement(&diffPressure, &temperatureSdp810))
        {
            ESP_LOGI(TAG, "Differential pressure = %.3f hPa, Temperature = %.1f C\n", diffPressure, temperatureSdp810);
        }

        if (SCD30_GetMeasures(&co2, &temperatureScd30, &humidity))
        {
            ESP_LOGI(TAG, "CO2 = %.1f ppm, Temperature = %.1f C, Humidity = %.1f %%\n", co2, temperatureScd30, humidity);
        }

        o2 = ME2O2_ReadOxygen();

        ESP_LOGI(TAG, "O2 = %.1f %\n", o2);

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void I2cBusInit(i2c_master_bus_handle_t *busHandle)
{
    i2c_master_bus_config_t i2cMasterConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_NUM,
        .scl_io_num = GPIO_PIN_NUM_I2C_SCL,
        .sda_io_num = GPIO_PIN_NUM_I2C_SDA,
        .glitch_ignore_cnt = 7,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2cMasterConfig, busHandle));
}

static void SpiBusInit()
{
    spi_bus_config_t buscfg = {
        .miso_io_num = GPIO_PIN_NUM_SPI_MISO,
        .mosi_io_num = GPIO_PIN_NUM_SPI_MOSI,
        .sclk_io_num = GPIO_PIN_NUM_SPI_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 1000 * 1000 * 2 + 8,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
}