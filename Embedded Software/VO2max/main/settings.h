#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "freertos/FreeRTOS.h"

typedef struct
{
    float o2Calibration;
    float co2Calibration;
    float flowCalibration;
    float altitudeReference;
    float temperatureReference;
    float pressureReference;
    bool bleOn;
    float userWeight;
} Settings_t;

extern Settings_t g_settings;

void SETTINGS_Initialize(void);
void SETTINGS_LoadSettings();
void SETTINGS_SaveSettings();

#endif