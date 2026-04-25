#ifndef __SCD30_H__
#define __SCD30_H__

#include "driver/i2c_master.h"

void SCD30_Initialize(i2c_master_bus_handle_t i2cBusHandle);
void SCD30_SoftwareReset(void);
bool SCD30_ReadFirmwareVersion(uint8_t *major, uint8_t *minor);
bool SCD30_GetDataReadyStatus(bool *dataReady);
void SCD30_SetAutoSelfCalibration(bool enable);
void SCD30_SetMeasurementInterval(uint16_t interval);
void SCD30_StartPeriodicMeasurment(void);
void SCD30_StartPeriodicMeasurmentWithPressureCompensation(uint16_t pressure);
void SCD30_StopMeasurement(void);
void SCD30_SetTemperatureOffset(uint16_t offset);
bool SCD30_GetMeasures(float *co2, float *temperature, float *humidity);

#endif
