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
extern QueueHandle_t g_pressureQueue;
extern QueueHandle_t g_breathingFrequencyQueue;
extern QueueHandle_t g_vo2Queue;

extern SemaphoreHandle_t g_flowInitializationSemaphore;
extern SemaphoreHandle_t g_o2InitializationSemaphore;
extern SemaphoreHandle_t g_co2InitializationSemaphore;
extern SemaphoreHandle_t g_pressureInitializationSemaphore;
extern SemaphoreHandle_t g_resetExhaledVolumeSemaphore;

void MEASURE_Initialize(void);

#endif