#ifndef __SETTINGS_H__
#define __SETTINGS_H__

typedef struct
{
    float o2Calibration;
    float co2Calibration;
    float flowCalibration;
    bool bleOn;
    float userWeight;
} Settings_t;

void SETTINGS_Initialize(void);
void SETTINGS_LoadSettings(Settings_t *settings);
void SETTINGS_SaveSettings(const Settings_t *settings);

#endif