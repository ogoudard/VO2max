#ifndef __HMI_H__
#define __HMI_H__

#include "freertos/FreeRTOS.h"

extern TaskHandle_t g_hmiTaskHandle;

void HMI_Initialize(void);

#endif