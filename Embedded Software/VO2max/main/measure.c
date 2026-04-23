/************************************
 * INCLUDES
 ************************************/

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "measure.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "driver_sdp8XX.h"
#include "driver_scd30.h"
#include "driver_me2o2.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define I2C_BUS_NUM I2C_NUM_0

#define GPIO_PIN_NUM_I2C_SDA 21
#define GPIO_PIN_NUM_I2C_SCL 22

#define MEASURE_PERIOD_PRESSURE_MS 10
#define MEASURE_PERIOD_O2_MS 2000
#define MEASURE_PERIOD_CO2_MS 2010

#define PRESSURE_SENSOR_MODEL SDP8XX_SDP810_500PA

#define DIFFERENTIAL_PRESSURE_THRESHOLD 0.2f
#define DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS 0.1f

#define LOG_MASS_FLOW 0
#define LOG_O2 0
#define LOG_CO2 0
#define LOG_EXHALED_VOLUME_CYCLE 0
#define LOG_EXHALED_VOLUME_TOTAL 0

/************************************
 * PRIVATE VARIABLES
 ************************************/

static i2c_master_bus_handle_t i2cHandle;
static TaskHandle_t differentialPressureTaskHandle;
static TaskHandle_t o2TaskHandle;
static TaskHandle_t co2TaskHandle;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void MEASURE_Initialize()
{
    i2c_master_bus_config_t i2cMasterConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_NUM,
        .scl_io_num = GPIO_PIN_NUM_I2C_SCL,
        .sda_io_num = GPIO_PIN_NUM_I2C_SDA,
        .glitch_ignore_cnt = 7,
    };
    const char *measureTag = "[MEASURE]";

    ESP_LOGI(measureTag, "Initializing measure...");
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2cMasterConfig, &i2cHandle));

    xTaskCreate(MEASURE_MassFlowTask, "Differential Pressure Task", 4096, NULL, 0, &differentialPressureTaskHandle);
    xTaskCreate(MEASURE_O2Task, "O2 Task", 4096, NULL, 0, &o2TaskHandle);
    xTaskCreate(MEASURE_CO2Task, "CO2 Task", 4096, NULL, 0, &co2TaskHandle);
    ESP_LOGI(measureTag, "measure ready");
}
void MEASURE_MassFlowTask(void *pvParameters)
{
    const char *tagMassFlow = "[Mass Flow Task]";
    int64_t timestamp;
    float diffPressure;
    float massFlow = 0.0f;
    float exhaledVolumeCycle = 0.0f;
    float exhaledVolumeTotal = 0.0f;
    int64_t lastTimestamp;
    int64_t deltaT;
    float lastMassFlow = 0.0F;
    int16_t scalingFactor;
    int16_t diffPressureRaw;
    uint16_t i = 0;
    bool exhale = false;

    ESP_LOGI(tagMassFlow, "Task started");

    SDP8XX_Initialize(i2cHandle, PRESSURE_SENSOR_MODEL);

    SDP8XX_StartContinuousMeasurementWithMassFlowTCompAndAveraging();

    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for first measure

    SDP8XX_ReadScalingFactor(&scalingFactor);
    ESP_LOGI(tagMassFlow, "Scaling factor = %d", scalingFactor);

    lastTimestamp = esp_timer_get_time();

    while (1)
    {
        timestamp = esp_timer_get_time() / 1000;

        if (SDP8XX_ReadDifferentialPressureRaw(&diffPressureRaw))
        {
            diffPressure = (float)diffPressureRaw / (float)scalingFactor;
            deltaT = timestamp - lastTimestamp;
            lastTimestamp = timestamp;

            if (diffPressure < DIFFERENTIAL_PRESSURE_THRESHOLD - DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS)
            {
                if (exhale == true)
                {
                    exhaledVolumeTotal += exhaledVolumeCycle;
#if LOG_EXHALED_VOLUME_TOTAL
                    ESP_LOGI(tagMassFlow, "#5,%.3f,%d", exhaledVolumeTotal, timestamp);
#endif
                    exhale = false;
#if LOG_EXHALED_VOLUME_CYCLE
                    ESP_LOGI(tagMassFlow, "#4,%.3f,%d", 0.0f, timestamp);
#endif
                }
                diffPressure = 0.0f;
                exhaledVolumeCycle = 0.0f;
                massFlow = 0.0f;
            }
            else if (diffPressure > DIFFERENTIAL_PRESSURE_THRESHOLD + DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS)
            {
                if (exhale == false)
                {
#if LOG_EXHALED_VOLUME_TOTAL
                    ESP_LOGI(tagMassFlow, "#5,%.3f,%d", exhaledVolumeTotal, timestamp);
#endif
                    exhale = true;
                }

                massFlow = 4.9855f * sqrt(diffPressure);                                     // Bernoulli equation Q=k⋅sqrt(ΔP)
                exhaledVolumeCycle += (float)deltaT * (massFlow + lastMassFlow) / 120000.0f; // Trapezoidal rule
#if LOG_EXHALED_VOLUME_CYCLE
                ESP_LOGI(tagMassFlow, "#4,%.3f,%d", exhaledVolumeCycle, timestamp);
#endif
            }

            lastMassFlow = massFlow;

            if ((i % 5) == 0)
            {
#if LOG_MASS_FLOW
                ESP_LOGI(tagMassFlow, "#1,%.3f,%d", massFlow, timestamp);
#endif
            }

            if (i < 1000)
            {
                i++;
            }
            else
            {
                i = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD_PRESSURE_MS));
    }
}

void MEASURE_O2Task(void *pvParameters)
{
    const char *tagO2 = "[O2 Task]";
    int64_t timestamp;
    float o2;

    ESP_LOGI(tagO2, "Task started");

    ME2O2_Initialize(i2cHandle);
    ME2O2_Calibrate(20.9);

    while (1)
    {
        timestamp = esp_timer_get_time() / 1000;
        o2 = ME2O2_ReadOxygen();

#if LOG_O2
        ESP_LOGI(tagO2, "#2,%.1f,%d", o2, timestamp);
#endif
        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD_O2_MS));
    }
}

void MEASURE_CO2Task(void *pvParameters)
{
    const char *tagCO2 = "[CO2 Task]";
    int64_t timestamp;
    float co2;
    float temperatureScd30;
    float humidity;

    ESP_LOGI(tagCO2, "Task started");

    SCD30_Initialize(i2cHandle);
    SCD30_SetMeasurementInterval(2);
    SCD30_StartPeriodicMeasurment();

    while (1)
    {
        timestamp = esp_timer_get_time() / 1000;
        if (SCD30_GetMeasures(&co2, &temperatureScd30, &humidity))
        {
#if LOG_CO2
            ESP_LOGI(tagCO2, "#3,%.1f,%d", co2, timestamp);
#endif
        }

        vTaskDelay(pdMS_TO_TICKS(MEASURE_PERIOD_CO2_MS));
    }
}
