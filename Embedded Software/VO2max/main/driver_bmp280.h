#ifndef __BMP280_H__
#define __BMP280_H__

#include <stdint.h>

#include "driver/i2c_master.h"

#define BMP280_I2C_ADDRESS_0x76 0x76
#define BMP280_I2C_ADDRESS_0x77 0x77

#define BMP280_SEA_LEVEL_PRESSURE_PA 101325

/**
 * @brief Oversampling rates for temperature and pressure
 */
typedef enum
{
    BMP280_OVERSAMPLING_0X = 0X00,
    BMP280_OVERSAMPLING_1X = 0X01,
    BMP280_OVERSAMPLING_2X = 0X02,
    BMP280_OVERSAMPLING_4X = 0X03,
    BMP280_OVERSAMPLING_8X = 0X04,
    BMP280_OVERSAMPLING_16X = 0X05
} BMP280_Oversampling_t;

/**
 * @brief Standby time period to be used in NORMAL mode
 *
 */
typedef enum
{
    BMP280_T_STANDBY_500US = 0X00,
    BMP280_T_STANDBY_62500US,
    BMP280_T_STANDBY_125MS,
    BMP280_T_STANDBY_250MS,
    BMP280_T_STANDBY_500MS,
    BMP280_T_STANDBY_1S,
    BMP280_T_STANDBY_2S,
    BMP280_T_STANDBY_4S
} BMP280_StandbyTime_t;

/**
 * @brief Possible operation modes of BMP280
 *
 */
typedef enum
{
    BMP280_MODE_SLEEP = 0x00,
    BMP280_MODE_FORCED = 0x01,
    BMP280_MODE_NORMAL = 0x03
} BMP280_OperationMode_t;

/**
 * @brief Possible values for filter coefficient
 *
 */
typedef enum
{
    BMP280_FILTER_OFF = 0X00,
    BMP280_FILTER_2X,
    BMP280_FILTER_4X,
    BMP280_FILTER_8X,
    BMP280_FILTER_16X
} BMP280_FilterTimeConstant_t;

/**
 * @brief Struct of calibration parameters unique to each BMP280 sensor
 *
 */
typedef struct
{
    uint16_t T1;
    int16_t T2;
    int16_t T3;
    uint16_t P1;
    int16_t P2;
    int16_t P3;
    int16_t P4;
    int16_t P5;
    int16_t P6;
    int16_t P7;
    int16_t P8;
    int16_t P9;
} BMP280_CalibrationParameters_t;

/**
 * @brief Structure needed in case of using bmp280_get_all() function
 *
 */
typedef struct
{
    float temperature;
    uint32_t pressure;
    float altitude;
} BMP280_SensorsData_t;

/**
 * @brief BMP280 initializer
 *
 * Checks for NULL handle, checks for NULL interface and address (for I2C),
 * resets and initializes sensor, gets the calibration data for further
 * calculations. also sets the default values.
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param hw_interface: Defines which hardware interface is used: I2C, SPI or a software mock.
 * @param i2c_address: I2C address in case of I2C interface.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
bool BMP280_Initialize(i2c_master_bus_handle_t i2cBusHandle, uint8_t address);

/**
 * @brief BMP280 soft reset
 *
 * Soft resets BMP280 using special reset register
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_Reset(void);

/**
 * @brief BMP280 set mode
 *
 * Setting bmp280 mode: MODE_SLEEP, MODE_FORCED, MODE_NORMAL
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param operationMode: The mode of operation: NORMAL_MODE, SLEEP_MODE, FORCED_MODE
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_SetOperationMode(BMP280_OperationMode_t operationMode);

/**
 * @brief BMP280 set pressure oversampling
 *
 * Setting pressure oversampling from 0 (skip) to 16x
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param osValue: Pressure oversampling value
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_SetPressureOversampling(BMP280_Oversampling_t osValue);

/**
 * @brief BMP280 set temperature oversampling
 *
 * Setting temperature oversampling from 0 (skip) to 16x
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param osValue: Temperature oversampling value
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_SetTemperatureOversampling(BMP280_Oversampling_t osValue);

/**
 * @brief BMP280 set standby time
 *
 * Setting standby time period in normal mode
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param standbyTime: standby time value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_SetStandbyTime(BMP280_StandbyTime_t standbyTime);

/**
 * @brief BMP280 set filter coefficient
 *
 * Sets low pass internal filter coefficient
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param filterCoefficient: Filter coefficient value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_SetFilterTimeConstant(BMP280_FilterTimeConstant_t filterTimeConstant);

/**
 * @brief BMP280 get temperature
 *
 * Gets temperature in Centigrade
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param temperature: Pointer to temperature value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
float BMP280_GetTemperature(void);

/**
 * @brief BMP280 get pressure
 *
 * Gets pressure in Pascal
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param pressure: Pointer to pressure value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
uint32_t BMP280_GetPressure(void);

/**
 * @brief BMP280 calculate altitude
 *
 * Calculates altitude from barometric pressure without temperature as an argument
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param alt: Pointer to altitude value.
 * @param barometricPressure: Barometric pressure read from sensor
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_CalculateAltitudeQuick(float *alt, uint32_t barometricPressure);

/**
 * @brief BMP280 calculate altitude
 *
 * Calculates altitude from barometric pressure and temperature as arguments
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param alt: Pointer to altitude value.
 * @param barometricPressure: Barometric pressure read from sensor
 * @param ambientTemperatureInC: Temperature in Centigrade
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_CalculateAltitudeHypsometric(float *alt, uint32_t barometricPressure, float ambientTemperatureInC);

/**
 * @brief BMP280 get mode of operation
 *
 * Returns mode of operation: MODE_NORMAL, MODE_SLEEP or MODE_FORCED
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param mode: Pointer to mode of operation value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
void BMP280_GetOperationMode(BMP280_OperationMode_t *mode);

/**
 * @brief BMP280 get temperature oversampling
 *
 * Returns the current temperature oversampling
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param tempOS: Pointer to temperature oversampling value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
BMP280_Oversampling_t BMP280_GetTemperatureOversampling(void);

/**
 * @brief BMP280 get pressure oversampling
 *
 * Returns the current pressure oversampling
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param pressureOS: Pointer to pressure oversampling value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
BMP280_Oversampling_t BMP280_GetPressureOversampling(void);

/**
 * @brief BMP280 get standby time
 *
 * Returns the current standby time (for normal mode)
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param standby_time: Pointer to standby time value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
BMP280_StandbyTime_t BMP280_GetStandbyTime(void);

/**
 * @brief BMP280 get filter coefficient
 *
 * Returns the current IIR filter coefficient
 *
 * @param handle: Pointer to the BMP280 instance handle structure.
 * @param filter_coeff: Pointer to filter coefficient value.
 * @return 0 or ERROR_OK on success, other values on errors.
 */
BMP280_FilterTimeConstant_t BMP280_GetFilterTimeConstant(void);

#endif
