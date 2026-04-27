#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t g_batterySocQueue;

void BATTERY_Initialize();
bool BATTERY_MeasureSoc(float *soc);

#endif