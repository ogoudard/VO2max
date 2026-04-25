#ifndef __DRIVER_SDP8XX_H__
#define __DRIVER_SDP8XX_H__

#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"

#define SDP8X0_I2C_ADDRESS 0x25
#define SDP8X1_I2C_ADDRESS 0x26

typedef enum
{
    SDP8XX_SDP800_500PA = 0x03020101,
    SDP8XX_SDP810_500PA = 0x03020A01,
    SDP8XX_SDP801_500PA = 0x03020401,
    SDP8XX_SDP811_500PA = 0x03020D01,
    SDP8XX_SDP800_125PA = 0x03020201,
    SDP8XX_SDP810_125PA = 0x03020B01
} SdpProductNumber_e;

bool SDP8XX_Initialize(i2c_master_bus_handle_t i2cBusHandle, SdpProductNumber_e productNumber);
bool SDP8XX_ReadProductIdentifier(uint32_t *productNumber,
                                  uint64_t *serialNumber);
void SDP8XX_SoftwareReset(void);
void SDP8XX_StartContinuousMeasurementWithMassFlowTCompAndAveraging(void);
void SDP8XX_StartContinuousMeasurementWithMassFlowTComp(void);
void SDP8XX_StartContinuousMeasurementWithDiffPressureTCompAndAveraging(void);
void SDP8XX_StartContinuousMeasurementWithDiffPressureTComp(void);
void SDP8XX_StopContinuousMeasurement(void);
bool SDP8XX_ReadScalingFactor(int16_t *scalingFactor);
bool SDP8XX_ReadDifferentialPressureRaw(int16_t *differentialPressureRaw);
bool SDP8XX_ReadMeasurementsRaw(int16_t *differentialPressureRaw,
                                int16_t *temperatureRaw,
                                int16_t *scalingFactor);
bool SDP8XX_ReadMeasurements(float *differentialPressure, float *temperature);

#endif /* __DRIVER_SDP8XX_H__ */
