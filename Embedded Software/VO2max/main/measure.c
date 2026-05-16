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
#include "plot.h"
#include "bluetooth.h"

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

#define BREATH_DURATION_THRESHOLD_US 400000

/* Air density at Standard Temperature and Pressure Desaturated */
#define RHO_STPD 1.292f

#define CONCAT(A, B) A##B
#define CONCAT_EXPAND(A, B) CONCAT(A, B)

#define PLOT_DATA(enable, id, value, timestamp) CONCAT_EXPAND(PLOT_DATA_, enable)(id, value, timestamp)

#if PLOT_ENABLE
#define PLOT_DATA_true(id, value, timestamp) PLOT_SendData(id, value, timestamp)
#else
#define PLOT_DATA_true(id, value, timestamp) \
    while (0)                                \
    {                                        \
    }
#endif
#define PLOT_DATA_false(id, value, timestamp) \
    while (0)                                 \
    {                                         \
    }

#define MAX_VALUE_FILTER_SIZE 30

typedef struct
{
    double values[MAX_VALUE_FILTER_SIZE];
    uint8_t index;
    uint8_t size;
} ValueFilter_t;

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
static float ComputeAirDensity(float temperature, float pressure, float humidity);
static double ComputeVO2(double rho, double o2percent, double exhaledVol, int64_t durationUs, double weight);
static double ComputeVCO2(double rho, double co2percent, double exhaledVol, int64_t durationUs, double weight);
static void FlowVolumeAndVo2Computation(double diffPressure);
static float ComputeAltitude(float altitudeReference, float pressureReference, float temperatureReference, float pressure);
static void AddValueToFilter(ValueFilter_t *filter, double value);
static double ComputeFilteredValue(ValueFilter_t *filter);

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
    g_flowQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_o2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_co2Queue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_cycleExhaledVolumeQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_totalExhaledVolumeQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_humidityQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_temperatureQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_pressureQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_altitudeQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_respiratoryRateQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_vO2Queue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_vO2MaxQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_vCo2Queue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_respiratoryQuotientQueue = xQueueCreate((UBaseType_t)1, sizeof(double));
    g_o2CalibrationQueue = xQueueCreate((UBaseType_t)1, sizeof(float));
    g_expiratoryFlowQueue = xQueueCreate((UBaseType_t)1, sizeof(double));

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
    double diffPressure;
    float temperature;

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

        xSemaphoreGive(g_flowInitializationSemaphore); // Signal that flow sensor is ready

        while (1)
        {
            if (SDP8XX_ReadMeasurements(&diffPressure, &temperature))
            {
                /* The sensor is calibrated for air and N2 at T = 25 °C and p = 966 mbar.
                If the properties of the system gas deviate from the properties of the calibration gas,
                a correction of the sensor reading may be applied. */
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
    int64_t timestamp;
    float o2CalValue;

    ESP_LOGI(o2TaskTag, "Task started");

    ESP_LOGI(o2TaskTag, "Initialize M2-02 dioxygen sensor");

    if (!ME2O2_Initialize(i2cHandle, ME2O2_I2C_ADDRESS))
    {
        ESP_LOGE(o2TaskTag, "ME2-O2 initialization failed, suspending task");

        // Default O2 value
        timestamp = esp_timer_get_time();
        o2 = 20.95f;
        xQueueOverwrite(g_o2Queue, (void *)&o2);
        PLOT_DATA(PLOT_O2, O2_PLOT_ID, o2, timestamp);

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

            timestamp = esp_timer_get_time();

            if (ME2O2_ReadOxygen(&o2)) // Start continuous measurement
            {
                xQueueOverwrite(g_o2Queue, (void *)&o2);
                PLOT_DATA(PLOT_O2, O2_PLOT_ID, o2, timestamp);
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
    int64_t timestamp;

    ESP_LOGI(co2TaskTag, "Task started");

    ESP_LOGI(co2TaskTag, "Initialize SCD30 Carbon Dioxide Sensor");

    if (!SCD30_Initialize(i2cHandle))
    {
        ESP_LOGE(co2TaskTag, "SCD30 initialization failed, suspending task");

        timestamp = esp_timer_get_time();

        // Default CO2 value
        co2 = 432.0f;
        xQueueOverwrite(g_co2Queue, (void *)&co2);
        PLOT_DATA(PLOT_CO2, CO2_PLOT_ID, co2, timestamp);

        // Default humidity value
        humidity = 50.0f;
        xQueueOverwrite(g_humidityQueue, (void *)&humidity);
        PLOT_DATA(PLOT_HUMIDITY, HUMIDITY_PLOT_ID, humidity, timestamp);

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
                    timestamp = esp_timer_get_time();

                    if (SCD30_GetMeasures(&co2, &temperature, &humidity))
                    {
                        xQueueOverwrite(g_co2Queue, (void *)&co2);
                        PLOT_DATA(PLOT_CO2, CO2_PLOT_ID, co2, timestamp);

                        xQueueOverwrite(g_humidityQueue, (void *)&humidity);
                        PLOT_DATA(PLOT_HUMIDITY, HUMIDITY_PLOT_ID, humidity, timestamp);
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
    int64_t timestamp;

    ESP_LOGI(pressureTaskTag, "Task started");

    ESP_LOGI(pressureTaskTag, "Initializing pressure and altitude sensor BMP280");

    if (!BMP388_Initialize(i2cHandle, BMP388_I2C_ADDRESS))
    {
        ESP_LOGE(pressureTaskTag, "BMP280 initialization failed, suspending task");

        timestamp = esp_timer_get_time();

        // Default temperature
        temperature = 25.0f;
        xQueueOverwrite(g_temperatureQueue, (void *)&temperature);
        PLOT_DATA(PLOT_TEMPERATURE, TEMPERATURE_PLOT_ID, temperature, timestamp);

        // Default pressure
        pressure = 101325.0f;
        xQueueOverwrite(g_pressureQueue, (void *)&pressure);
        PLOT_DATA(PLOT_PRESSURE, PRESSURE_PLOT_ID, pressure, timestamp);

        // Default altitude
        altitude = ComputeAltitude(g_settings.altitudeReference, g_settings.pressureReference, g_settings.temperatureReference, pressure);
        xQueueOverwrite(g_altitudeQueue, (void *)&altitude);
        PLOT_DATA(PLOT_ALTITUDE, ALTITUDE_PLOT_ID, altitude, timestamp);

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
                timestamp = esp_timer_get_time();

                BMP388_ReadTemperatureAndPressure(&temperature, &pressure);

                xQueueOverwrite(g_temperatureQueue, (void *)&temperature);
                PLOT_DATA(PLOT_TEMPERATURE, TEMPERATURE_PLOT_ID, temperature, timestamp);

                xQueueOverwrite(g_pressureQueue, (void *)&pressure);
                PLOT_DATA(PLOT_PRESSURE, PRESSURE_PLOT_ID, pressure, timestamp);

                altitude = ComputeAltitude(g_settings.altitudeReference, g_settings.pressureReference, g_settings.temperatureReference, pressure);
                xQueueOverwrite(g_altitudeQueue, (void *)&altitude);
                PLOT_DATA(PLOT_ALTITUDE, ALTITUDE_PLOT_ID, altitude, timestamp);
            }

            vTaskDelay(pdMS_TO_TICKS(PRESSURE_TASK_PERIOD_MS));
        }
    }
}

static float ComputeAirDensity(float temperature, float pressure, float humidity)
{
    float pSat;
    float pV;

    pSat = 611.21f * expf(17.502 * temperature / (240.97 + temperature));
    pV = pSat * humidity / 100.0f;

    return pressure / (287.058f * (temperature + 273.15f)) + pV / (461.495f * (temperature + 273.15f));
}

static double ComputeVO2(double rho, double o2percent, double exhaledVol, int64_t durationUs, double weight)
{
    double o2Consumed;

    o2Consumed = 20.95f - o2percent;

    return (exhaledVol * rho * o2Consumed * 600000000.0f) / ((double)durationUs * RHO_STPD * weight);
}

static double ComputeVCO2(double rho, double co2percent, double exhaledVol, int64_t durationUs, double weight)
{
    double co2Produced;

    co2Produced = co2percent - 0.0043f; // Substract CO2 concentration in the atmosphere

    return (exhaledVol * rho * co2Produced * 600000000.0f) / ((double)durationUs * RHO_STPD * weight);
}

static void FlowVolumeAndVo2Computation(double diffPressure)
{
    /* Timing variables */
    int64_t timestamp;
    int64_t endExhaleTimestamp;
    static int64_t previousTimestamp = 0;
    int64_t deltaT;
    static int64_t previousExhaleTimestamp = 0;
    int64_t breathDuration;
    double respiratoryRate; // breath/minute

    /* Variables for flow computation */
    double flow = 0.0f;
    static double cycleExhaledVolume = 0.0f;
    static double totalExhaledVolume = 0.0f;
    static double previousFlow = 0.0f;

    /* Variables for VO2 computation */
    static double vO2Max = 0.0f;
    double vO2 = 0.0f;
    static ValueFilter_t vO2Filter = {.index = 0, .size = 10, .values = {0.0f}};
    double vO2Filtered = 0.0f;
    double vCo2 = 0.0f;
    static ValueFilter_t vCo2Filter = {.index = 0, .size = 10, .values = {0.0f}};
    double vCo2Filtered = 0.0f;
    float o2;
    float co2;
    float temperature;
    float pressure;
    float humidity;
    double rho;
    bool vO2Compute;
    bool vCo2Compute;
    double respiratoryQuotient;
    double expiratoryFlow;
    double volume;
    static bool exhale = false; // Breath detection

    timestamp = esp_timer_get_time();
    deltaT = timestamp - previousTimestamp;
    previousTimestamp = timestamp;

    if (xSemaphoreTake(g_resetExhaledVolumeSemaphore, (TickType_t)0))
    {
        totalExhaledVolume = 0.0f;
        xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
        PLOT_DATA(PLOT_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_PLOT_ID, totalExhaledVolume, timestamp);

        cycleExhaledVolume = 0.0f;
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
        PLOT_DATA(PLOT_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_PLOT_ID, cycleExhaledVolume, timestamp);
    }

    if (xSemaphoreTake(g_resetVo2MaxSemaphore, (TickType_t)0))
    {
        vO2Max = 0.0f;
        xQueueOverwrite(g_vO2MaxQueue, (void *)&vO2Max);
        PLOT_DATA(PLOT_VO2MAX, VO2MAX_PLOT_ID, vO2Max, timestamp);
    }

    PLOT_DATA(PLOT_DIFFERENTIAL_PRESSURE, DIFFERENTIAL_PRESSURE_PLOT_ID, diffPressure, timestamp);

    // Bernoulli equation Q=k⋅sqrt(ΔP)
    if (diffPressure < 0.0f)
    {
        flow = -g_settings.flowCalibration * sqrt(-diffPressure);
    }
    else
    {
        flow = g_settings.flowCalibration * sqrt(diffPressure);
    }

    xQueueOverwrite(g_flowQueue, (void *)&flow);
    PLOT_DATA(PLOT_FLOW, FLOW_PLOT_ID, flow, timestamp);
    previousFlow = flow;

    volume = (double)deltaT * (flow + previousFlow) / 120000000.0f; // Integrate flow → volume (Trapezoidal rule)

    /* Detect exhalation using threshold + hysteresis */
    if ((DIFFERENTIAL_PRESSURE_THRESHOLD + DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) < diffPressure)
    {
        if (exhale == false) // Start of exhalation
        {
            exhale = true;

            xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
            PLOT_DATA(PLOT_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_PLOT_ID, totalExhaledVolume, timestamp);

            xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
            PLOT_DATA(PLOT_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_PLOT_ID, cycleExhaledVolume, timestamp);

            cycleExhaledVolume = 0.0f;
            xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
            PLOT_DATA(PLOT_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_PLOT_ID, cycleExhaledVolume, timestamp);
        }

        cycleExhaledVolume += volume;
        xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
        PLOT_DATA(PLOT_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_PLOT_ID, cycleExhaledVolume, timestamp);

        totalExhaledVolume += volume;
        xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
        PLOT_DATA(PLOT_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_PLOT_ID, totalExhaledVolume, timestamp);
    }
    else if ((DIFFERENTIAL_PRESSURE_THRESHOLD - DIFFERENTIAL_PRESSURE_THRESHOLD_HYSTERESIS) > diffPressure)
    {
        if (exhale == true) // End of exhalation
        {
            exhale = false;

            endExhaleTimestamp = timestamp;

            breathDuration = endExhaleTimestamp - previousExhaleTimestamp;
            previousExhaleTimestamp = timestamp;

            if (breathDuration > BREATH_DURATION_THRESHOLD_US)
            {
                cycleExhaledVolume += volume;
                xQueueOverwrite(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume);
                PLOT_DATA(PLOT_CYCLE_EXHALED_VOLUME, CYCLE_EXHALED_VOLUME_PLOT_ID, cycleExhaledVolume, timestamp);

                totalExhaledVolume += volume;
                xQueueOverwrite(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume);
                PLOT_DATA(PLOT_TOTAL_EXHALED_VOLUME, TOTAL_EXHALED_VOLUME_PLOT_ID, totalExhaledVolume, timestamp);

                respiratoryRate = 60000000.0f / (double)breathDuration; // Respiratory rate in breath/min
                xQueueOverwrite(g_respiratoryRateQueue, (void *)&respiratoryRate);
                PLOT_DATA(PLOT_RESPIRATORY_RATE, RESPIRATORY_RATE_PLOT_ID, respiratoryRate, timestamp);

                expiratoryFlow = respiratoryRate * cycleExhaledVolume;
                xQueueOverwrite(g_expiratoryFlowQueue, (void *)&expiratoryFlow);
                PLOT_DATA(PLOT_EXPIRATORY_FLOW, EXPIRATORY_FLOW_PLOT_ID, expiratoryFlow, timestamp);

                if (pdPASS == xQueuePeek(g_temperatureQueue, (void *)&temperature, (TickType_t)0))
                {
                    if (pdPASS == xQueuePeek(g_pressureQueue, (void *)&pressure, (TickType_t)0))
                    {
                        if (pdPASS == xQueuePeek(g_humidityQueue, (void *)&humidity, (TickType_t)0))
                        {
                            rho = ComputeAirDensity(temperature, pressure, humidity);
                            PLOT_DATA(PLOT_RHO, RHO_PLOT_ID, rho, timestamp);

                            vO2Compute = (pdPASS == xQueuePeek(g_o2Queue, (void *)&o2, (TickType_t)0));

                            if (true == vO2Compute)
                            {
                                vO2 = ComputeVO2(rho, o2, cycleExhaledVolume, breathDuration, g_settings.userWeight);
                                AddValueToFilter(&vO2Filter, vO2);
                                vO2Filtered = ComputeFilteredValue(&vO2Filter);

                                xQueueOverwrite(g_vO2Queue, (void *)&vO2Filtered);
                                PLOT_DATA(PLOT_VO2, VO2_PLOT_ID, vO2Filtered, timestamp);
                                BLUETOOTH_SendVO2(vO2Filtered);

                                if (vO2Filtered > vO2Max) // Compute VO2max
                                {
                                    vO2Max = vO2Filtered;
                                }

                                xQueueOverwrite(g_vO2MaxQueue, (void *)&vO2Max);
                                PLOT_DATA(PLOT_VO2MAX, VO2MAX_PLOT_ID, vO2Max, timestamp);
                                BLUETOOTH_SendVO2Max(vO2Max);
                            }

                            vCo2Compute = (pdPASS == xQueuePeek(g_co2Queue, (void *)&co2, (TickType_t)0));

                            if (true == vCo2Compute)
                            {
                                vCo2 = ComputeVCO2(rho, co2 / 10000.0f, cycleExhaledVolume, breathDuration, g_settings.userWeight);
                                AddValueToFilter(&vCo2Filter, vCo2);
                                vCo2Filtered = ComputeFilteredValue(&vCo2Filter);

                                xQueueOverwrite(g_vCo2Queue, (void *)&vCo2Filtered);
                                PLOT_DATA(PLOT_VCO2, VCO2_PLOT_ID, vCo2Filtered, timestamp);
                                BLUETOOTH_SendVCO2(vCo2Filtered);
                            }

                            if ((true == vCo2Compute) && (true == vO2Compute))
                            {
                                respiratoryQuotient = vCo2 / vO2;
                                xQueueOverwrite(g_respiratoryQuotientQueue, (void *)&respiratoryQuotient);
                                PLOT_DATA(PLOT_RESPIRATORY_QUOTIENT, RESPIRATORY_QUOTIENT_PLOT_ID, respiratoryQuotient, timestamp);
                                BLUETOOTH_SendRQ(respiratoryQuotient);
                            }
                        }
                    }
                }
            }
        }
    }
}

static float ComputeAltitude(float altitudeReference, float pressureReference, float temperatureReference, float pressure)
{
    // Hypsometric equation
    return altitudeReference + (temperatureReference + 273.15f) / 0.0065f * (1 - powf(pressure / pressureReference, 0.190263f));
}

static void AddValueToFilter(ValueFilter_t *filter, double value)
{
    filter->index = (filter->index + 1) % filter->size;
    filter->values[filter->index] = value;
}

static double ComputeFilteredValue(ValueFilter_t *filter)
{
    double sum = 0.0f;

    for (uint16_t i = 0; i < filter->size; i++)
    {
        sum += filter->values[i];
    }

    return sum / (double)filter->size;
}