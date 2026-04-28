#ifndef __BATTERY_H__
#define __BATTERY_H__

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define BATTERY_SOC_USB_PLUGGED -2
#define BATTERY_SOC_CHARGING -1

extern QueueHandle_t g_batterySocQueue;

void BATTERY_Initialize();
bool BATTERY_MeasureSoc(int8_t *soc);

#endif