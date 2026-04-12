#ifndef DRIVER_SDP810_H
#define DRIVER_SDP810_H

#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"

#define SDP810_I2C_ADDRESS_ 0x25

void SDP810_Initialize(i2c_master_bus_handle_t i2cBusHandle);
uint16_t SDP810_StartContinuousMeasurementWithMassFlowTCompAndAveraging(void);
uint16_t SDP810_StartContinuousMeasurementWithMassFlowTComp(void);
uint16_t SDP810_StartContinuousMeasurementWithDiffPressureTCompAndAveraging(void);
uint16_t SDP810_StartContinuousMeasurementWithDiffPressureTComp(void);
uint16_t SDP810_StopContinuousMeasurement(void);
bool SDP810_ReadMeasurementRaw(int16_t *differentialPressureRaw,
                                   int16_t *temperatureRaw,
                                   int16_t *scalingFactor);

bool SDP810_ReadMeasurement(float *differentialPressure, float *temperature);
uint16_t SDP810_ReadProductIdentifier(uint32_t *productNumber,
                                      uint8_t *serialNumber,
                                      uint8_t serialNumberSize);


#endif /* DRIVER_SDP810_H */
