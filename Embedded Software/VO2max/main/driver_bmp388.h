#ifndef __BMP388_H__
#define __BMP388_H__

#include <stdint.h>

#include "driver/i2c_master.h"

#define BMP388_I2C_ADDRESS_0x76 0x76
#define BMP388_I2C_ADDRESS_0x77 0x77

#define BMP388_SEA_LEVEL_PRESSURE_PA 101325

typedef union
{
    struct
    {
        uint8_t fatal : 1;
        uint8_t cmd : 1;
        uint8_t conf : 1;
        uint8_t reserved3_7 : 1;
    };
    uint8_t reg;
} BMP388_Error_u;

typedef union
{
    struct
    {
        uint8_t reserved0_3 : 4;
        uint8_t cmdRdy : 1;
        uint8_t drdyPress : 1;
        uint8_t drdyTemp : 1;
        uint8_t reserved7 : 1;
    };
    uint8_t reg;
} BMP388_Status_u;

/**
 * @brief Oversampling rates for temperature and pressure
 */
typedef enum
{
    BMP388_OVERSAMPLING_1X = 0X00,
    BMP388_OVERSAMPLING_2X = 0X01,
    BMP388_OVERSAMPLING_4X = 0X02,
    BMP388_OVERSAMPLING_8X = 0X03,
    BMP388_OVERSAMPLING_16X = 0X04,
    BMP388_OVERSAMPLING_32X = 0X05
} BMP388_Oversampling_t;

/**
 * @brief Standby time period to be used in NORMAL mode
 *
 */
typedef enum
{
    BMP388_T_STANDBY_500US = 0X00,
    BMP388_T_STANDBY_62500US,
    BMP388_T_STANDBY_125MS,
    BMP388_T_STANDBY_250MS,
    BMP388_T_STANDBY_500MS,
    BMP388_T_STANDBY_1S,
    BMP388_T_STANDBY_2S,
    BMP388_T_STANDBY_4S
} BMP388_StandbyTime_t;

/**
 * @brief Possible operation modes of BMP388
 *
 */
typedef enum
{
    BMP388_MODE_SLEEP = 0x00,
    BMP388_MODE_FORCED = 0x01,
    BMP388_MODE_NORMAL = 0x03
} BMP388_OperationMode_t;

/**
 * @brief Possible values for filter coefficient
 *
 */
typedef enum
{
    BMP388_FILTER_OFF = 0X00,
    BMP388_FILTER_2X,
    BMP388_FILTER_4X,
    BMP388_FILTER_8X,
    BMP388_FILTER_16X
} BMP388_FilterTimeConstant_t;

/**
 * @brief Struct of calibration parameters unique to each BMP388 sensor
 *
 */
typedef struct
{
    uint16_t T1;
    uint16_t T2;
    int8_t T3;
    int16_t P1;
    int16_t P2;
    int8_t P3;
    int8_t P4;
    uint16_t P5;
    uint16_t P6;
    int8_t P7;
    int8_t P8;
    int16_t P9;
    int8_t P10;
    int8_t P11;
} BMP388_CalibrationParameters_t;

/**
 * @brief Structure needed in case of using BMP388_get_all() function
 *
 */
typedef struct
{
    float temperature;
    uint32_t pressure;
    float altitude;
} BMP388_SensorsData_t;

bool BMP388_Initialize(i2c_master_bus_handle_t i2cBusHandle, uint8_t address);
void BMP388_Reset(void);
BMP388_Error_u BMP388_GetError(void);
BMP388_Status_u BMP388_GetStatus(void);
void BMP388_EnableTemperatureMeasurement(bool en);
void BMP388_EnablePressureMeasurement(bool en);
void BMP388_SetOperationMode(BMP388_OperationMode_t operationMode);
void BMP388_SetPressureOversampling(BMP388_Oversampling_t osValue);
void BMP388_SetTemperatureOversampling(BMP388_Oversampling_t osValue);
void BMP388_SetStandbyTime(BMP388_StandbyTime_t standbyTime);
void BMP388_SetFilterTimeConstant(BMP388_FilterTimeConstant_t filterTimeConstant);
void BMP388_ReadTemperatureAndPressure(float *temperature, float *pressure);
void BMP388_CalculateAltitudeQuick(float *alt, uint32_t barometricPressure);
void BMP388_CalculateAltitudeHypsometric(float *alt, uint32_t barometricPressure, float ambientTemperatureInC);
void BMP388_GetOperationMode(BMP388_OperationMode_t *mode);
BMP388_Oversampling_t BMP388_GetTemperatureOversampling(void);
BMP388_Oversampling_t BMP388_GetPressureOversampling(void);
BMP388_StandbyTime_t BMP388_GetStandbyTime(void);
BMP388_FilterTimeConstant_t BMP388_GetFilterTimeConstant(void);

#endif
