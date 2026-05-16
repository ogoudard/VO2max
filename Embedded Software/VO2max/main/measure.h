#ifndef __MEASURE_H__
#define __MEASURE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Plot flags (enable/disable plots) */

#define PLOT_ENABLE true

#define PLOT_TEMPERATURE true
#define PLOT_HUMIDITY true
#define PLOT_PRESSURE true
#define PLOT_ALTITUDE true
#define PLOT_RHO true
#define PLOT_O2 true
#define PLOT_CO2 true
#define PLOT_DIFFERENTIAL_PRESSURE true
#define PLOT_FLOW true
#define PLOT_EXPIRATORY_FLOW true
#define PLOT_CYCLE_EXHALED_VOLUME true
#define PLOT_TOTAL_EXHALED_VOLUME true
#define PLOT_RESPIRATORY_RATE true
#define PLOT_VO2 true
#define PLOT_VO2MAX true
#define PLOT_VCO2 true
#define PLOT_RESPIRATORY_QUOTIENT true

#define TEMPERATURE_PLOT_ID 0
#define HUMIDITY_PLOT_ID 1
#define PRESSURE_PLOT_ID 2
#define ALTITUDE_PLOT_ID 3
#define RHO_PLOT_ID 4
#define O2_PLOT_ID 5
#define CO2_PLOT_ID 6
#define DIFFERENTIAL_PRESSURE_PLOT_ID 7
#define FLOW_PLOT_ID 8
#define EXPIRATORY_FLOW_PLOT_ID 9
#define CYCLE_EXHALED_VOLUME_PLOT_ID 10
#define TOTAL_EXHALED_VOLUME_PLOT_ID 11
#define RESPIRATORY_RATE_PLOT_ID 12
#define VO2_PLOT_ID 13
#define VO2MAX_PLOT_ID 14
#define VCO2_PLOT_ID 15
#define RESPIRATORY_QUOTIENT_PLOT_ID 16

extern QueueHandle_t g_flowQueue;
extern QueueHandle_t g_o2Queue;
extern QueueHandle_t g_co2Queue;
extern QueueHandle_t g_temperatureQueue;
extern QueueHandle_t g_humidityQueue;
extern QueueHandle_t g_totalExhaledVolumeQueue;
extern QueueHandle_t g_cycleExhaledVolumeQueue;
extern QueueHandle_t g_pressureQueue;
extern QueueHandle_t g_altitudeQueue;
extern QueueHandle_t g_respiratoryRateQueue;
extern QueueHandle_t g_vO2Queue;
extern QueueHandle_t g_vO2MaxQueue;
extern QueueHandle_t g_vCo2Queue;
extern QueueHandle_t g_respiratoryQuotientQueue;
extern QueueHandle_t g_o2CalibrationQueue;
extern QueueHandle_t g_expiratoryFlowQueue;

extern SemaphoreHandle_t g_flowInitializationSemaphore;
extern SemaphoreHandle_t g_o2InitializationSemaphore;
extern SemaphoreHandle_t g_co2InitializationSemaphore;
extern SemaphoreHandle_t g_pressureInitializationSemaphore;
extern SemaphoreHandle_t g_resetExhaledVolumeSemaphore;
extern SemaphoreHandle_t g_resetVo2MaxSemaphore;

void MEASURE_Initialize(void);

#endif