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
#include "driver_bmp280.h"
#include "driver/gpio.h"
#include "hmi.h" // User interface (display)

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

#define BMP280_I2C_ADDRESS BMP280_I2C_ADDRESS_0x77

#define PRESSURE_SENSOR_MODEL SDP8XX_SDP810_500PA

/* Threshold to detect exhalation */
#define DIFFERENTIAL_PRESSURE_THRESHOLD 0.2f
#define DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS 0.1f

/* Calibration constant for O2 sensor 20.9% */
#define INITIAL_O2_CONCENTRATION_PERCENT 20.9f

/* Air density at Standard Temperature and Pressure Desaturated */
#define RHO_STPD 1.292f

#define USER_WHEIGHT 74.0f

/* Debug logging flags (enable/disable logs) */
#define LOG_FLOW 0
#define LOG_O2 0
#define LOG_CO2 0
#define LOG_CYCLE_EXHALED_VOLUME 0
#define LOG_TOTAL_EXHALED_VOLUME 0
#define LOG_VO2 0

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
QueueHandle_t g_totalExhaledVolumeQueue;
QueueHandle_t g_cycleExhaledVolumeQueue;
QueueHandle_t g_respiratoryRateQueue;
QueueHandle_t g_vo2Queue;
QueueHandle_t g_vo2MaxQueue;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/
static void FlowTask(void *pvParameters);
static void O2Task(void *pvParameters);
static void CO2Task(void *pvParameters);
static void PressureTask(void *pvParameters);
static void ComputeAirDensity(float temp, float press, float *rho, float *rhoBtps);
static float ComputeVO2(float rhoBtps, float o2percent, float exhaledVol, int64_t durationUs, float weight);
static void FlowVolumeAndVo2Computation(float diffPressure);

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
    g_respiratoryRateQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_vo2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_vo2MaxQueue = xQueueCreate((UBaseType_t)1, sizeof(float));

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
    // xTaskCreate(PressureTask, "Pressure Task", PRESSURE_TASK_STACK_SIZE, NULL, PRESSURE_TASK_PRIORITY, &pressureTaskHandle);
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
    const char *tagO2 = "[O2 Task]";
    float o2;

    ESP_LOGI(tagO2, "Task started");

    ESP_LOGI(tagO2, "Initialize M2-02 dioxygen sensor");

    if (!ME2O2_Initialize(i2cHandle, ME2O2_I2C_ADDRESS))
    {
        ESP_LOGE(tagO2, "ME2-O2 initialization failed, suspending task");

        vTaskSuspend(o2TaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(O2_TASK_PERIOD_MS));
        }
    }
    else
    {
        ME2O2_Calibrate(INITIAL_O2_CONCENTRATION_PERCENT);

        xSemaphoreGive(g_o2InitializationSemaphore);

        while (1)
        {
            if (ME2O2_ReadOxygen(&o2)) // Start continuous measurement
            {
                xQueueOverwrite(g_o2Queue, (void *)&o2);
#if LOG_O2
                int64_t timestamp = esp_timer_get_time() / 1000;
                ESP_LOGI(tagO2, "#2,%.1f,%d", o2, timestamp);
#endif
            }

            vTaskDelay(pdMS_TO_TICKS(O2_TASK_PERIOD_MS));
        }
    }
}

static void CO2Task(void *pvParameters)
{
    const char *tagCO2 = "[CO2 Task]";
    float co2;
    float temperature;
    float humidity;

    ESP_LOGI(tagCO2, "Task started");

    ESP_LOGI(tagCO2, "Initialize SCD30 carbon dioxide sensor");

    if (!SCD30_Initialize(i2cHandle))
    {
        ESP_LOGE(tagCO2, "SCD30 initialization failed, suspending task");

        vTaskSuspend(co2TaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(CO2_TASK_PERIOD_MS));
        };
    }
    else
    {
        SCD30_SetMeasurementInterval(SCD30_MEASUREMENT_INTERVAL_S);
        SCD30_StartPeriodicMeasurment(); // Start periodic measurement

        vTaskDelay(pdMS_TO_TICKS(1500 * SCD30_MEASUREMENT_INTERVAL_S)); // Wait for first measure

        xSemaphoreGive(g_co2InitializationSemaphore);

        while (1)
        {
            bool dataReady;

            if (false == SCD30_GetDataReadyStatus(&dataReady))
            {
                ESP_LOGE(tagCO2, "Could not retrieve data status");
            }
            else
            {
                if (false == dataReady)
                {
                    ESP_LOGE(tagCO2, "Data not ready");
                }
                else
                {
                    if (SCD30_GetMeasures(&co2, &temperature, &humidity))
                    {
                        xQueueOverwrite(g_co2Queue, (void *)&co2);
                        xQueueOverwrite(g_temperatureQueue, (void *)&temperature);
                        xQueueOverwrite(g_humidityQueue, (void *)&humidity);
#if LOG_CO2
                        int64_t timestamp = esp_timer_get_time() / 1000;
                        ESP_LOGI(tagCO2, "#3,%.1f,%d", co2, timestamp);
#endif
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(CO2_TASK_PERIOD_MS));
        }
    }
}

static void PressureTask(void *pvParameters)
{
    const char *pressureTag = "[Pressure Task]";

    ESP_LOGI(pressureTag, "Task started");

    ESP_LOGI(pressureTag, "Initializing pressure and altitude sensor BMP280");

    if (!BMP280_Initialize(i2cHandle, BMP280_I2C_ADDRESS))
    {
        ESP_LOGE(pressureTag, "BMP280 initialization failed, suspending task");

        vTaskSuspend(pressureTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS));
        };
    }
    else
    {
        BMP280_SetFilterTimeConstant(BMP280_FILTER_16X);
        BMP280_SetPressureOversampling(BMP280_OVERSAMPLING_16X);
        BMP280_SetTemperatureOversampling(BMP280_OVERSAMPLING_16X);
        BMP280_SetStandbyTime(BMP280_T_STANDBY_4S);
        BMP280_SetOperationMode(BMP280_MODE_NORMAL);

        xSemaphoreGive(g_pressureInitializationSemaphore); // Signal that ambient pressure sensor is ready

        while (1)
        {
            uint32_t pressure;

            pressure = BMP280_GetPressure();
            ESP_LOGI(pressureTag, "Pressure = %.2f hPa", (float)pressure / 100.0f);
            xQueueOverwrite(g_pressureQueue, (void *)&pressure);
            vTaskDelay(pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS));
        }
    }
}

static void ComputeAirDensity(float temp, float press, float *rho, float *rhoBtps)
{
    *rho = press / ((temp + 273.15f) * 287.058f); // calculation of air density
    *rhoBtps = press / 90257.135f;                // density at BTPS: 35°C, 95% humidity
}

static float ComputeVO2(float rhoBtps, float o2percent, float exhaledVol, int64_t durationUs, float weight)
{
    float o2Consumed;

    o2Consumed = 20.95f - o2percent;

    return (exhaledVol * rhoBtps * o2Consumed * 600000000) / (durationUs * RHO_STPD * weight);
}

static void FlowVolumeAndVo2Computation(float diffPressure)
{
    /* Timing variables */
    int64_t timestamp;
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
    static float vo2Max = 0.0f;
    float vo2 = 0.0f;

    /* Variables for VO2 computation */
    float o2;
    float temp;
    float rho;
    float rhoBtps;

    /* Breath detection */
    static bool exhale = false;

#if LOG_FLOW
    uint16_t i = 0;
#endif

    if (xSemaphoreTake(g_resetExhaledVolumeSemaphore, (TickType_t)0))
    {
        totalExhaledVolume = 0.0f;
        cycleExhaledVolume = 0.0f;
        xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
    }

    if (xSemaphoreTake(g_resetVo2MaxSemaphore, (TickType_t)0))
    {
        vo2Max = 0.0f;
        xQueueOverwrite(g_vo2MaxQueue, (void *)&vo2Max);
    }

    timestamp = esp_timer_get_time() / 1000; // µs to ms

    deltaT = timestamp - previousTimestamp;
    previousTimestamp = timestamp;

    /* Detect exhalation using threshold + hysteresis */
    if ((DIFFERENTIAL_PRESSURE_THRESHOLD + DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) < diffPressure)
    {
        if (exhale == false) // Start of exhalation
        {
            exhale = true;
#if LOG_TOTAL_EXHALED_VOLUME
            ESP_LOGI(flowTaskTag, "#5,%.3f,%d", totalExhaledVolume, timestamp);
#endif
            cycleExhaledVolume = 0.0f;

#if LOG_CYCLE_EXHALED_VOLUME
            ESP_LOGI(flowTaskTag, "#4,%.3f,%d", cycleExhaledVolume, timestamp);
#endif
            xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
        }

        // Bernoulli equation Q=k⋅sqrt(ΔP)
        // TO DO: use flow calibration value
        flow = 12.618f * sqrt(diffPressure);
        cycleExhaledVolume += (float)deltaT * (flow + previousFlow) / 120000.0f; // Integrate flow → volume (Trapezoidal rule)
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
#if LOG_CYCLE_EXHALED_VOLUME
        ESP_LOGI(flowTaskTag, "#4,%.3f,%d", cycleExhaledVolume, timestamp);
#endif
    }
    else if ((DIFFERENTIAL_PRESSURE_THRESHOLD - DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) > diffPressure)
    {
        if (exhale == true) // End of exhalation
        {
            breathDuration = esp_timer_get_time() - previousExhaleTimestamp;
            previousExhaleTimestamp = esp_timer_get_time();
            respiratoryRate = 60000000 / (float)breathDuration; // Respiratory rate in breath/min
            xQueueOverwrite(g_respiratoryRateQueue, (void *)&respiratoryRate);
            totalExhaledVolume += cycleExhaledVolume;
            xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);

            if (pdPASS == xQueuePeek(g_o2Queue, (void *)&o2, (TickType_t)0))
            {
                if (pdPASS == xQueuePeek(g_temperatureQueue, (void *)&temp, (TickType_t)0))
                {
                    // TO DO: replace sea level pressure by measured pressure value when BMP280 driver will work
                    ComputeAirDensity(temp, BMP280_SEA_LEVEL_PRESSURE_PA, &rho, &rhoBtps);
                    // TO DO: replace user weight by setted value in settings
                    vo2 = ComputeVO2(rhoBtps, o2, cycleExhaledVolume, breathDuration, USER_WHEIGHT);

                    if (vo2 > vo2Max) // Compute VO2max
                    {
                        vo2Max = vo2;
                        xQueueOverwrite(g_vo2MaxQueue, (void *)&vo2Max);
                    }

                    xQueueOverwrite(g_vo2Queue, (void *)&vo2);
#if LOG_VO2
                    ESP_LOGI(flowTaskTag, "#6,%.2f,%d", vo2, timestamp);
#endif
                }
            }

#if LOG_TOTAL_EXHALED_VOLUME
            ESP_LOGI(flowTaskTag, "#5,%.3f,%d", totalExhaledVolume, timestamp);
#endif
            exhale = false;
        }

        diffPressure = 0.0f;
        flow = 0.0f;

        xQueueOverwrite(g_flowQueue, (void *)&flow);
    }

    previousFlow = flow;

    xQueueOverwrite(g_flowQueue, (void *)&flow);
#if LOG_FLOW
    if ((i % 5) == 0)
    {
        ESP_LOGI(flowTaskTag, "#1,%.3f,%d", flow, timestamp);
    }

    if (i < 1000)
    {
        i++;
    }
    else
    {
        i = 0;
    }
#endif
}