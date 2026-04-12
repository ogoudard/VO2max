#ifndef __SCD30_H__
#define __SCD30_H__

#include "driver/i2c_master.h"

void SCD30_Initialize(i2c_master_bus_handle_t i2cBusHandle);

bool SCD30_IsAvailable(void);

void SCD30_SetAutoSelfCalibration(bool enable);
void SCD30_SetMeasurementInterval(uint16_t interval);

void SCD30_StartPeriodicMeasurment(void);
void SCD30_StartPeriodicMeasurmentWithPressureCompensation(uint16_t pressure);
void SCD30_StopMeasurement(void);
void SCD30_SetTemperatureOffset(uint16_t offset);

bool SCD30_GetMeasures(float *co2, float *temperature, float *humidity);






#endif
