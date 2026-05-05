#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "driver/gpio.h"
#include "hal/gpio_hal.h"

void DEBUG_Initialize(void);
void DEBUG_SetGpioDebugOut1(void);
void DEBUG_ResetGpioDebugOut1(void);
void DEBUG_ToggleGpioDebugOut1(void);
void DEBUG_SetGpioDebugOut2(void);
void DEBUG_ResetGpioDebugOut2(void);
void DEBUG_ToggleGpioDebugOut2(void);
bool DEBUG_GetGpioDebugInState(void);

#endif