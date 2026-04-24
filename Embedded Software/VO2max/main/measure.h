#ifndef __MEASURE_H__
#define __MEASURE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t g_flowQueue;
extern QueueHandle_t g_o2Queue;
extern QueueHandle_t g_co2Queue;
extern QueueHandle_t g_temperatureQueue;
extern QueueHandle_t g_humidityQueue;
extern QueueHandle_t g_totalExhaledVolumeQueue;
extern QueueHandle_t g_cycleExhaledVolumeQueue;

void MEASURE_Initialize(void);
void MEASURE_FlowTask(void *pvParameters);
void MEASURE_O2Task(void *pvParameters);
void MEASURE_CO2Task(void *pvParameters);

#endif