#ifndef __DRIVER_ME2O2_H__
#define __DRIVER_ME2O2_H__

#include "driver/i2c_master.h"

#define ME2O2_I2C_ADDRESS 0x73
//#define ME2O2_I2C_ADDRESS 0x71
//#define ME2O2_I2C_ADDRESS 0x72
//#define ME2O2_I2C_ADDRESS 0x73

void ME2O2_Initialize(i2c_master_bus_handle_t i2cBusHandle);
void ME2O2_Calibrate(float vol);
bool ME2O2_ReadOxygen(float *oxygen);

#endif /* __DRIVER_ME2O2_H__ */
