/************************************
 * INCLUDES
 ************************************/

#include <math.h>

#include "esp_log.h"
#include "esp_timer.h" // High resolution timer (µs precision)
#include "measure.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "driver_sdp8XX.h"
#include "driver_scd30.h"
#include "driver_me2o2.h"
#include "driver_bmp388.h"
#include "driver/gpio.h"
#include "hmi.h" // User interface (display)
#include "debug.h"
#include "settings.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

/* Each sensor runs in its own FreeRTOS task
   → stack size = memory allocated
   → priority = importance (higher = more CPU time)
   → period = sampling rate (O2 measured every 2s/CO2 measured every 3s/Flow measured every 10ms/Ambient pressure measure every 5s) */

#define O2_TASK_STACK_SIZE 8192
#define O2_TASK_PRIORITY 2
#define O2_TASK_PERIOD_MS 2000

#define CO2_TASK_STACK_SIZE 8192
#define CO2_TASK_PRIORITY 1
#define CO2_TASK_PERIOD_MS 3000
#define SCD30_MEASUREMENT_INTERVAL_S 2

#define FLOW_TASK_STACK_SIZE 8192

#define FLOW_TASK_PRIORITY 10
#define FLOW_TASK_PERIOD_MS 10

#define PRESSURE_TASK_STACK_SIZE 8192
#define PRESSURE_TASK_PRIORITY 5
#define PRESSURE_TASK_PERIOD_MS 5000

/* I2C configuration (shared bus for all sensors) */
#define I2C_BUS_NUM I2C_NUM_0

#define GPIO_PIN_NUM_I2C_SDA 21
#define GPIO_PIN_NUM_I2C_SCL 22

/* Sensor addresses */
#define ME2O2_I2C_ADDRESS ME2O2_I2C_ADDRESS_0x73

#define BMP388_I2C_ADDRESS BMP388_I2C_ADDRESS_0x77

#define PRESSURE_SENSOR_MODEL SDP8XX_SDP810_500PA

/* Threshold to detect exhalation */
#define DIFFERENTIAL_PRESSURE_THRESHOLD 0.3f
#define DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS 0.1f

/* Calibration constant for O2 sensor 20.9% */
#define INITIAL_O2_CONCENTRATION_PERCENT 20.9f

/* Air density at Standard Temperature and Pressure Desaturated */
#define RHO_STPD 1.292f

#define CONCAT(A, B) A##B
#define CONCAT_EXPAND(A, B) CONCAT(A, B)

#define LOG_DATA(enable, id, value, format, timestamp) CONCAT_EXPAND(LOG_DATA_, enable)(id, value, format, timestamp)

#define LOG_DATA_true(id, value, format, timestamp) printf("%d," format ",%ld\n", id, value, timestamp);
#define LOG_DATA_false(id, value, format, timestamp) \
    while (0)                                        \
    {                                                \
    }

/* Debug logging flags (enable/disable logs) */
#define LOG_TEMPERATURE true
#define LOG_HUMIDITY true
#define LOG_PRESSURE true
#define LOG_ALTITUDE true
#define LOG_FLOW true
#define LOG_O2 true
#define LOG_CO2 true
#define LOG_CYCLE_EXHALED_VOLUME true
#define LOG_TOTAL_EXHALED_VOLUME true
#define LOG_VO2 true
#define LOG_VO2MAX true
#define LOG_VCO2 true
#define LOG_RQ true
#define LOG_RR true
#define LOG_RHO true
#define LOG_EXPIRATORY_FLOW true

#define TEMPERATURE_LOG_ID 0
#define HUMIDITY_LOG_ID 1
#define PRESSURE_LOG_ID 2
#define ALTITUDE_LOG_ID 3
#define FLOW_LOG_ID 4
#define CYCLE_EXHALED_VOLUME_LOG_ID 5
#define TOTAL_EXHALED_VOLUME_LOG_ID 6
#define O2_LOG_ID 7
#define CO2_LOG_ID 8
#define VO2_LOG_ID 9
#define VO2MAX_LOG_ID 10
#define VCO2_LOG_ID 11
#define RQ_LOG_ID 12
#define RR_LOG_ID 13
#define RHO_LOG_ID 14
#define EXPIRATORY_FLOW_LOG_ID 15

/************************************
 * PRIVATE VARIABLES
 ************************************/

/* I2C bus handle shared by all sensors */
static i2c_master_bus_handle_t i2cHandle;

/* Handles for FreeRTOS tasks */
static TaskHandle_t flowTaskHandle;
static TaskHandle_t o2TaskHandle;
static TaskHandle_t co2TaskHandle;
static TaskHandle_t pressureTaskHandle;

static const char *flowTaskTag = "[Flow Task]";

/************************************
 * PUBLIC VARIABLES
 ************************************/

/* Semaphores → used to signal "sensor ready" */
SemaphoreHandle_t g_flowInitializationSemaphore;
SemaphoreHandle_t g_o2InitializationSemaphore;
SemaphoreHandle_t g_co2InitializationSemaphore;
SemaphoreHandle_t g_pressureInitializationSemaphore;
SemaphoreHandle_t g_resetExhaledVolumeSemaphore;
SemaphoreHandle_t g_resetVo2MaxSemaphore;

/* Queues for intertask communication */
QueueHandle_t g_flowQueue;
QueueHandle_t g_o2Queue;
QueueHandle_t g_co2Queue;
QueueHandle_t g_temperatureQueue;
QueueHandle_t g_humidityQueue;
QueueHandle_t g_pressureQueue;
QueueHandle_t g_altitudeQueue;
QueueHandle_t g_totalExhaledVolumeQueue;
QueueHandle_t g_cycleExhaledVolumeQueue;
QueueHandle_t g_respiratoryRateQueue;
QueueHandle_t g_vO2Queue;
QueueHandle_t g_vO2MaxQueue;
QueueHandle_t g_vCo2Queue;
QueueHandle_t g_respiratoryQuotientQueue;
QueueHandle_t g_o2CalibrationQueue;
QueueHandle_t g_expiratoryFlowQueue;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/
static void FlowTask(void *pvParameters);
static void O2Task(void *pvParameters);
static void CO2Task(void *pvParameters);
static void PressureTask(void *pvParameters);
static void ComputeAirDensity(float temperature, float pressure, float humidity, float *rho);
static float ComputeVO2(float rhoBtps, float o2percent, float exhaledVol, int64_t durationUs, float weight);
static float ComputeVCO2(float rhoBtps, float co2percent, float exhaledVol, int64_t durationUs, float weight);
static void FlowVolumeAndVo2Computation(float diffPressure);
static float CalculateAltitude(float altitudeReference, float pressureReference, float temperatureReference, float pressure);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void MEASURE_Initialize()
{
    /* Configure I2C bus (used by all sensors) */
    i2c_master_bus_config_t i2cMasterConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_BUS_NUM,
        .scl_io_num = GPIO_PIN_NUM_I2C_SCL,
        .sda_io_num = GPIO_PIN_NUM_I2C_SDA,
        .glitch_ignore_cnt = 7,
    };
    const char *measureTag = "[MEASURE]";

    gpio_config_t gpioConfig = {
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
        .pin_bit_mask = 1 << 27};

    gpio_config(&gpioConfig);

    ESP_LOGI(measureTag, "Initializing measure...");

    /* Initialize I2C */
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2cMasterConfig, &i2cHandle));

    /* Create queues (size = 1 → always keep latest value) */
    g_flowQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_o2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_co2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_cycleExhaledVolumeQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_totalExhaledVolumeQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_humidityQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_temperatureQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_pressureQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_altitudeQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_respiratoryRateQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_vO2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_vO2MaxQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_vCo2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_respiratoryQuotientQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_o2CalibrationQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_expiratoryFlowQueue = xQueueCreate((UBaseType_t)1, sizeof(float));

    /* Create semaphores (used to signal initialization complete) */
    g_flowInitializationSemaphore = xSemaphoreCreateBinary();
    g_o2InitializationSemaphore = xSemaphoreCreateBinary();
    g_co2InitializationSemaphore = xSemaphoreCreateBinary();
    g_pressureInitializationSemaphore = xSemaphoreCreateBinary();
    g_resetExhaledVolumeSemaphore = xSemaphoreCreateBinary();
    g_resetVo2MaxSemaphore = xSemaphoreCreateBinary();

    /* Create tasks (each sensor runs independently) */
    xTaskCreate(FlowTask, "Flow Task", FLOW_TASK_STACK_SIZE, NULL, FLOW_TASK_PRIORITY, &flowTaskHandle);
    xTaskCreate(O2Task, "O2 Task", O2_TASK_STACK_SIZE, NULL, O2_TASK_PRIORITY, &o2TaskHandle);
    xTaskCreate(CO2Task, "CO2 Task", CO2_TASK_STACK_SIZE, NULL, CO2_TASK_PRIORITY, &co2TaskHandle);
    xTaskCreate(PressureTask, "Pressure Task", PRESSURE_TASK_STACK_SIZE, NULL, PRESSURE_TASK_PRIORITY, &pressureTaskHandle);
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static void FlowTask(void *pvParameters)
{
    int16_t scalingFactor;
    int16_t diffPressureRaw;
    float diffPressure;

    ESP_LOGI(flowTaskTag, "Task started");

    ESP_LOGI(flowTaskTag, "Initialize SDP810 differential pressure sensor");

    if (!SDP8XX_Initialize(i2cHandle, PRESSURE_SENSOR_MODEL))
    {
        ESP_LOGE(flowTaskTag, "SDP810 initialization failed, suspending task");

        vTaskSuspend(flowTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(FLOW_TASK_PERIOD_MS));
        }
    }
    else
    {
        SDP8XX_StartContinuousMeasurementWithMassFlowTCompAndAveraging(); // Start continuous measurement

        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for first measure

        SDP8XX_ReadScalingFactor(&scalingFactor); // correctionfactors refer to the application note on Signal Compensation
        ESP_LOGI(flowTaskTag, "Scaling factor = %d", scalingFactor);

        xSemaphoreGive(g_flowInitializationSemaphore); // Signal that flow sensor is ready

        while (1)
        {
            if (SDP8XX_ReadDifferentialPressureRaw(&diffPressureRaw))
            {
                /* The sensor is calibrated for air and N2 at T = 25 °C and p = 966 mbar.
                If the properties of the system gas deviate from the properties of the calibration gas,
                a correction of the sensor reading may be applied. */
                diffPressure = (float)diffPressureRaw / (float)scalingFactor;
                FlowVolumeAndVo2Computation(diffPressure);
            }

            vTaskDelay(pdMS_TO_TICKS(FLOW_TASK_PERIOD_MS));
        }
    }
}

static void O2Task(void *pvParameters)
{
    const char *o2TaskTag = "[O2 Task]";
    float o2;
    int32_t timestamp;
    float o2CalValue;

    ESP_LOGI(o2TaskTag, "Task started");

    ESP_LOGI(o2TaskTag, "Initialize M2-02 dioxygen sensor");

    if (!ME2O2_Initialize(i2cHandle, ME2O2_I2C_ADDRESS))
    {
        ESP_LOGE(o2TaskTag, "ME2-O2 initialization failed, suspending task");

        vTaskSuspend(o2TaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(O2_TASK_PERIOD_MS));
        }
    }
    else
    {
        ME2O2_Calibrate(g_settings.o2Calibration);

        xSemaphoreGive(g_o2InitializationSemaphore);

        while (1)
        {
            if (pdPASS == xQueueReceive(g_o2CalibrationQueue, (void *)&o2CalValue, (TickType_t)0))
            {
                ME2O2_Calibrate(o2CalValue);
            }

            timestamp = esp_timer_get_time() / 1000;

            if (ME2O2_ReadOxygen(&o2)) // Start continuous measurement
            {
                xQueueOverwrite(g_o2Queue, (void *)&o2);
                LOG_DATA(LOG_O2, O2_LOG_ID, o2, "%.1f", timestamp);
            }

            vTaskDelay(pdMS_TO_TICKS(O2_TASK_PERIOD_MS));
        }
    }
}

static void CO2Task(void *pvParameters)
{
    const char *co2TaskTag = "[CO2 Task]";
    float co2;
    float temperature;
    float humidity;
    bool dataReady;
    int32_t timestamp;

    ESP_LOGI(co2TaskTag, "Task started");

    ESP_LOGI(co2TaskTag, "Initialize SCD30 Carbon Dioxide Sensor");

    if (!SCD30_Initialize(i2cHandle))
    {
        ESP_LOGE(co2TaskTag, "SCD30 initialization failed, suspending task");

        vTaskSuspend(co2TaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(CO2_TASK_PERIOD_MS));
        };
    }
    else
    {
        SCD30_SetAutoSelfCalibration(true);
        SCD30_SetMeasurementInterval(SCD30_MEASUREMENT_INTERVAL_S);
        SCD30_StartPeriodicMeasurment(); // Start periodic measurement

        vTaskDelay(pdMS_TO_TICKS(1500 * SCD30_MEASUREMENT_INTERVAL_S)); // Wait for first measure

        xSemaphoreGive(g_co2InitializationSemaphore);

        while (1)
        {
            if (false == SCD30_GetDataReadyStatus(&dataReady))
            {
                ESP_LOGE(co2TaskTag, "Could not retrieve data status");
            }
            else
            {
                if (false == dataReady)
                {
                    ESP_LOGE(co2TaskTag, "Data not ready");
                }
                else
                {
                    timestamp = esp_timer_get_time() / 1000;

                    if (SCD30_GetMeasures(&co2, &temperature, &humidity))
                    {
                        xQueueOverwrite(g_co2Queue, (void *)&co2);
                        LOG_DATA(LOG_CO2, CO2_LOG_ID, (uint32_t)co2, "%ld", timestamp);
                        xQueueOverwrite(g_humidityQueue, (void *)&humidity);
                        LOG_DATA(LOG_HUMIDITY, HUMIDITY_LOG_ID, (uint32_t)humidity, "%ld", timestamp);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(CO2_TASK_PERIOD_MS));
        }
    }
}

static void PressureTask(void *pvParameters)
{
    const char *pressureTaskTag = "[Pressure Task]";
    float pressure;
    float temperature;
    float altitude;
    BMP388_Status_u bmp388Status;
    int32_t timestamp;

    ESP_LOGI(pressureTaskTag, "Task started");

    ESP_LOGI(pressureTaskTag, "Initializing pressure and altitude sensor BMP280");

    if (!BMP388_Initialize(i2cHandle, BMP388_I2C_ADDRESS))
    {
        ESP_LOGE(pressureTaskTag, "BMP280 initialization failed, suspending task");

        vTaskSuspend(pressureTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS));
        };
    }
    else
    {
        BMP388_EnablePressureMeasurement(true);
        BMP388_EnableTemperatureMeasurement(true);
        BMP388_SetOperationMode(BMP388_MODE_NORMAL);

        xSemaphoreGive(g_pressureInitializationSemaphore); // Signal that ambient pressure sensor is ready

        while (1)
        {
            bmp388Status = BMP388_GetStatus();

            if ((0 != bmp388Status.drdyPress) && (0 != bmp388Status.drdyTemp))
            {
                timestamp = esp_timer_get_time() / 1000;

                BMP388_ReadTemperatureAndPressure(&temperature, &pressure);

                xQueueOverwrite(g_temperatureQueue, (void *)&temperature);
                LOG_DATA(LOG_TEMPERATURE, TEMPERATURE_LOG_ID, temperature, "%.1f", timestamp);

                xQueueOverwrite(g_pressureQueue, (void *)&pressure);
                LOG_DATA(LOG_PRESSURE, PRESSURE_LOG_ID, (uint32_t)pressure, "%ld", timestamp);

                altitude = CalculateAltitude(g_settings.altitudeReference, g_settings.pressureReference, g_settings.temperatureReference, pressure);
                xQueueOverwrite(g_altitudeQueue, (void *)&altitude);
                LOG_DATA(LOG_ALTITUDE, ALTITUDE_LOG_ID, (uint32_t)altitude, "%ld", timestamp);
            }

            vTaskDelay(pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS));
        }
    }
}

static void ComputeAirDensity(float temperature, float pressure, float humidity, float *rho)
{
    float pSat;
    float pV;

    pSat = 611.21f * expf(17.502 * temperature / (240.97 + temperature));
    pV = pSat * humidity / 100.0f;

    *rho = pressure / (287.058f * (temperature + 273.15f)) + pV / (461.495f * (temperature + 273.15f));
}

static float ComputeVO2(float rhoBtps, float o2percent, float exhaledVol, int64_t durationUs, float weight)
{
    float o2Consumed;

    o2Consumed = 20.95f - o2percent;

    return (exhaledVol * rhoBtps * o2Consumed * 600000000) / (durationUs * RHO_STPD * weight);
}

static float ComputeVCO2(float rhoBtps, float co2percent, float exhaledVol, int64_t durationUs, float weight)
{
    float co2Produced;

    co2Produced = co2percent - 0.0043f; // Substract CO2 concentration in the atmosphere

    return (exhaledVol * rhoBtps * co2Produced * 600000000) / (durationUs * RHO_STPD * weight);
}

static void FlowVolumeAndVo2Computation(float diffPressure)
{
    /* Timing variables */
    int32_t timestamp;
    static int64_t previousTimestamp = 0;
    int64_t deltaT;
    static int64_t previousExhaleTimestamp = 0;
    int64_t breathDuration;
    float respiratoryRate; // breath/minute

    /* Variables for flow computation */
    float flow = 0.0f;
    static float cycleExhaledVolume = 0.0f;
    static float totalExhaledVolume = 0.0f;
    static float previousFlow = 0.0f;

    /* Variables for VO2 computation */
    static float vO2Max = 0.0f;
    float vO2 = 0.0f;
    float vCo2 = 0.0f;
    float o2;
    float co2;
    float temperature;
    float pressure;
    float humidity;
    float rho;
    bool vO2Compute;
    bool vCo2Compute;
    float respiratoryQuotient;
    float expiratoryFlow;
    static bool exhale = false; // Breath detection

    timestamp = esp_timer_get_time() / 1000;

    if (xSemaphoreTake(g_resetExhaledVolumeSemaphore, (TickType_t)0))
    {
        totalExhaledVolume = 0.0f;
        xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
        LOG_DATA(LOG_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_LOG_ID, totalExhaledVolume, "%.2f", timestamp);

        cycleExhaledVolume = 0.0f;
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
        LOG_DATA(LOG_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_LOG_ID, cycleExhaledVolume, "%.2f", timestamp);
    }

    if (xSemaphoreTake(g_resetVo2MaxSemaphore, (TickType_t)0))
    {
        vO2Max = 0.0f;
        xQueueOverwrite(g_vO2MaxQueue, (void *)&vO2Max);
        LOG_DATA(LOG_VO2MAX, VO2MAX_LOG_ID, vO2Max, "%.2f", timestamp);
    }

    deltaT = timestamp - previousTimestamp;
    previousTimestamp = timestamp;

    /* Detect exhalation using threshold + hysteresis */
    if ((DIFFERENTIAL_PRESSURE_THRESHOLD + DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) < diffPressure)
    {
        if (exhale == false) // Start of exhalation
        {
            exhale = true;
            xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
            LOG_DATA(LOG_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_LOG_ID, totalExhaledVolume, "%.2f", timestamp);

            xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
            LOG_DATA(LOG_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_LOG_ID, cycleExhaledVolume, "%.2f", timestamp);

            cycleExhaledVolume = 0.0f;
            xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
            LOG_DATA(LOG_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_LOG_ID, cycleExhaledVolume, "%.2f", timestamp);
        }

        // Bernoulli equation Q=k⋅sqrt(ΔP)
        flow = g_settings.flowCalibration * sqrt(diffPressure);

        cycleExhaledVolume += (float)deltaT * (flow + previousFlow) / 120000.0f; // Integrate flow → volume (Trapezoidal rule)
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
        LOG_DATA(LOG_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_LOG_ID, cycleExhaledVolume, "%.2f", timestamp);

        totalExhaledVolume += cycleExhaledVolume;
        xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
        LOG_DATA(LOG_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_LOG_ID, totalExhaledVolume, "%.2f", timestamp);
    }
    else if ((DIFFERENTIAL_PRESSURE_THRESHOLD - DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) > diffPressure)
    {
        if (exhale == true) // End of exhalation
        {
            breathDuration = esp_timer_get_time() - previousExhaleTimestamp;
            previousExhaleTimestamp = esp_timer_get_time();

            respiratoryRate = 60000000 / (float)breathDuration; // Respiratory rate in breath/min
            xQueueOverwrite(g_respiratoryRateQueue, (void *)&respiratoryRate);
            LOG_DATA(LOG_RR, RR_LOG_ID, respiratoryRate, "%.2f", timestamp);

            expiratoryFlow = respiratoryRate * cycleExhaledVolume;
            xQueueOverwrite(g_expiratoryFlowQueue, (void *)&expiratoryFlow);
            LOG_DATA(LOG_EXPIRATORY_FLOW, EXPIRATORY_FLOW_LOG_ID, expiratoryFlow, "%.2f", timestamp);

            if (pdPASS == xQueuePeek(g_temperatureQueue, (void *)&temperature, (TickType_t)0))
            {
                if (pdPASS == xQueuePeek(g_pressureQueue, (void *)&pressure, (TickType_t)0))
                {
                    if (pdPASS == xQueuePeek(g_humidityQueue, (void *)&humidity, (TickType_t)0))
                    {
                        ComputeAirDensity(temperature, pressure, humidity, &rho);
                        LOG_DATA(LOG_RHO, RHO_LOG_ID, rho, "%.2f", timestamp);

                        vO2Compute = (pdPASS == xQueuePeek(g_o2Queue, (void *)&o2, (TickType_t)0));

                        if (true == vO2Compute)
                        {
                            vO2 = ComputeVO2(rho, o2, cycleExhaledVolume, breathDuration, g_settings.userWeight);

                            if (vO2 > vO2Max) // Compute VO2max
                            {
                                vO2Max = vO2;
                                xQueueOverwrite(g_vO2MaxQueue, (void *)&vO2Max);
                                LOG_DATA(LOG_VO2MAX, VO2MAX_LOG_ID, vO2Max, "%.2f", timestamp);
                            }

                            xQueueOverwrite(g_vO2Queue, (void *)&vO2);
                            LOG_DATA(LOG_VO2, VO2_LOG_ID, vO2, "%.2f", timestamp);
                        }

                        vCo2Compute = (pdPASS == xQueuePeek(g_co2Queue, (void *)&co2, (TickType_t)0));

                        if (true == vCo2Compute)
                        {
                            vCo2 = ComputeVCO2(rho, co2 / 10000.0f, cycleExhaledVolume, breathDuration, g_settings.userWeight);
                            xQueueOverwrite(g_vCo2Queue, (void *)&vCo2);
                            LOG_DATA(LOG_VCO2, VCO2_LOG_ID, vCo2, "%.2f", timestamp);
                        }

                        if ((true == vCo2Compute) && (true == vO2Compute))
                        {
                            respiratoryQuotient = vCo2 / vO2;
                            xQueueOverwrite(g_respiratoryQuotientQueue, (void *)&respiratoryQuotient);
                            LOG_DATA(LOG_RQ, RQ_LOG_ID, respiratoryQuotient, "%.2f", timestamp);
                        }
                    }
                }
            }

            exhale = false;
        }

        diffPressure = 0.0f;
        flow = 0.0f;

        xQueueOverwrite(g_flowQueue, (void *)&flow);
    }

    previousFlow = flow;

    xQueueOverwrite(g_flowQueue, (void *)&flow);
    LOG_DATA(LOG_FLOW, FLOW_LOG_ID, flow, "%.2f", timestamp);
}

static float CalculateAltitude(float altitudeReference, float pressureReference, float temperatureReference, float pressure)
{
    // Hypsometric equation
    return altitudeReference + (temperatureReference + 273.15f) / 0.0065f * (1 - powf(pressure / pressureReference, 0.190263));
}