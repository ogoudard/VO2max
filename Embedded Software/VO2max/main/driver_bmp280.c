/************************************
 * INCLUDES
 ************************************/

#include <math.h>

#include "driver_bmp280.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define BMP280_I2C_FREQUENCY 400000

#define BMP280_RESET_DELAY_MS 100

#define BMP280_REGISTER_T1 0X88
#define BMP280_REGISTER_T2 0X8A
#define BMP280_REGISTER_T3 0X8C
#define BMP280_REGISTER_P1 0X8E
#define BMP280_REGISTER_P2 0X90
#define BMP280_REGISTER_P3 0X92
#define BMP280_REGISTER_P4 0X94
#define BMP280_REGISTER_P5 0X96
#define BMP280_REGISTER_P6 0X98
#define BMP280_REGISTER_P7 0X9A
#define BMP280_REGISTER_P8 0X9C
#define BMP280_REGISTER_P9 0X9E
#define BMP280_REGISTER_ID 0XD0
#define BMP280_REGISTER_RESET 0XE0
#define BMP280_REGISTER_STATUS 0XF3
#define BMP280_REGISTER_CONTROL_MEAS 0XF4
#define BMP280_REGISTER_CONFIG 0XF5
#define BMP280_REGISTER_PRESSURE_MSB 0xF7
#define BMP280_REGISTER_PRESSURE_LSB 0xF8
#define BMP280_REGISTER_PRESSURE_XLSB 0xF9
#define BMP280_REGISTER_TEMPERATURE_MSB 0xFA
#define BMP280_REGISTER_TEMPERATURE_LSB 0xFB
#define BMP280_REGISTER_TEMPERATURE_XLSB 0xFC

#define BMP280_REGISTER_BIT_IM_UPDATE 0
#define BMP280_REGISTER_BIT_MEASURING 3
#define BMP280_REGISTER_BIT_MODE 0
#define BMP280_REGISTER_BIT_OSRS_P 2
#define BMP280_REGISTER_BIT_OSRS_T 5
#define BMP280_REGISTER_BIT_SPI3W_EN 0
#define BMP280_REGISTER_BIT_FILTER 2
#define BMP280_REGISTER_BIT_STANDBY_TIME 5

#define BMP280_RESET_VALUE 0xB6
#define BMP280_CHIP_ID 0x58

#define SEA_LEVEL_PRESSURE 101325

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[BMP280]";
static i2c_master_dev_handle_t devHandle;
static BMP280_CalibrationParameters_t calibration;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void WriteRegister(uint8_t reg, uint8_t value);
static uint8_t Read8BitRegister(uint8_t reg);
static uint16_t Read16BitRegister(uint8_t reg);
static uint32_t Read24BitRegister(uint8_t reg);
static bool CheckChipId(void);
static void GetRawTemperature(int32_t *raw_data);
static void GetRawPressure(int32_t *raw_data);
static void ReadCalibration(void);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

bool BMP280_Initialize(i2c_master_bus_handle_t i2cBusHandle, uint8_t address)
{
	bool ret = false;

	i2c_device_config_t devCfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = address,
		.scl_speed_hz = BMP280_I2C_FREQUENCY,
	};

	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devCfg, &devHandle));

	if(CheckChipId())
	{
		BMP280_Reset();

		ReadCalibration();
		ret = true;
		ESP_LOGI(TAG, "Initialization successfull");
	}
	else
	{
		ESP_LOGE(TAG, "Initialization failed");
	}

	return ret;
}

void BMP280_Reset(void)
{
	WriteRegister(BMP280_REGISTER_RESET, BMP280_RESET_VALUE);
	vTaskDelay(pdMS_TO_TICKS(BMP280_RESET_DELAY_MS));
}

void BMP280_SetOperationMode(BMP280_OperationMode_t operationMode)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	regValue &= ~(0x3 << BMP280_REGISTER_BIT_MODE);
	regValue |= (operationMode << BMP280_REGISTER_BIT_MODE);

	WriteRegister(BMP280_REGISTER_CONTROL_MEAS, regValue);
}

void BMP280_SetPressureOversampling(BMP280_Oversampling_t osValue)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	regValue &= ~(0x7 << BMP280_REGISTER_BIT_OSRS_P);
	regValue |= (osValue << BMP280_REGISTER_BIT_OSRS_P);

	WriteRegister(BMP280_REGISTER_CONTROL_MEAS, regValue);
}

void BMP280_SetTemperatureOversampling(BMP280_Oversampling_t osValue)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	regValue &= ~(0x7 << BMP280_REGISTER_BIT_OSRS_T);
	regValue |= (osValue << BMP280_REGISTER_BIT_OSRS_T);

	WriteRegister(BMP280_REGISTER_CONTROL_MEAS, regValue);
}

void BMP280_SetStandbyTime(BMP280_StandbyTime_t standbyTime)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP280_REGISTER_CONFIG);

	regValue &= ~(0x7 << BMP280_REGISTER_BIT_STANDBY_TIME);
	regValue |= (standbyTime << BMP280_REGISTER_BIT_STANDBY_TIME);

	WriteRegister(BMP280_REGISTER_CONFIG, regValue);
}

/*sets low pass internal filter coefficient for bmp280. used in noisy environments*/
void BMP280_SetFilterTimeConstant(BMP280_FilterTimeConstant_t filterTimeConstant)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP280_REGISTER_CONFIG);

	regValue &= ~(0x7 << BMP280_REGISTER_BIT_FILTER);
	regValue |= (filterTimeConstant << BMP280_REGISTER_BIT_FILTER);

	WriteRegister(BMP280_REGISTER_CONFIG, regValue);
}

float BMP280_GetTemperature(void)
{
	float temperature;
	int32_t adcT;
	int32_t tFine;
	int32_t var1;
	int32_t var2;
	int32_t temp;

	GetRawTemperature(&adcT);

	var1 = ((((adcT >> 3) - ((int32_t)calibration.T1 << 1))) * ((int32_t)calibration.T2)) >> 11;
	var2 = (((((adcT >> 4) - ((int32_t)calibration.T1)) * ((adcT >> 4) - ((int32_t)calibration.T1))) >> 12) * ((int32_t)calibration.T3)) >> 14;

	tFine = var1 + var2;
	temp = (tFine * 5 + 128) >> 8;
	temperature = ((float)temp) / 100.0f;

	return temperature;
}

uint32_t BMP280_GetPressure(void)
{
	int32_t adcP;
	int32_t var1;
	int32_t var2;
	int32_t tFine = 1;
	uint32_t pressure = 0.0f;

	GetRawPressure(&adcP);

	var1 = (((int32_t)tFine) / 2) - (int32_t)64000;
	var2 = (((var1 / 4) * (var1 / 4)) / 2048) * ((int32_t)calibration.P6);
	var2 = var2 + ((var1 * ((int32_t)calibration.P5)) * 2);
	var2 = (var2 / 4) + (((int32_t)calibration.P4) * 65536);
	var1 = (((calibration.P3 * (((var1 / 4) * (var1 / 4)) / 8192)) / 8) + ((((int32_t)calibration.P2) * var1) / 2)) / 262144;
	var1 = ((((32768 + var1)) * ((int32_t)calibration.P1)) / 32768);

	pressure = (uint32_t)(((int32_t)(1048576 - adcP) - (var2 / 4096)) * 3125);

	/* Avoid exception caused by division with zero */
	if (var1 != 0)
	{
		/* Check for overflows against UINT32_MAX/2; if pres is left-shifted by 1 */
		if (pressure < 0x80000000)
		{
			pressure = (pressure << 1) / ((uint32_t)var1);
		}
		else
		{
			pressure = (pressure / (uint32_t)var1) * 2;
		}
		var1 = (((int32_t)calibration.P9) * ((int32_t)(((pressure / 8) * (pressure / 8)) / 8192))) / 4096;
		var2 = (((int32_t)(pressure / 4)) * ((int32_t)calibration.P8)) / 8192;
		pressure = (uint32_t)((int32_t)pressure + ((var1 + var2 + calibration.P7) / 16));
	}

	return pressure;
}

void BMP280_CalculateAltitudeQuick(float *alt, uint32_t barometricPressure)
{
	float temp_result;

	temp_result = powf(barometricPressure, 0.190284);

	*alt = 44307.69396 * (1 - 0.111555816 * temp_result); /*calculating altitude from barometric formula*/
}

void BMP280_CalculateAltitudeHypsometric(float *alt, uint32_t barometricPressure, float ambientTemperatureInC)
{
	float tempResult;

	tempResult = powf((float)SEA_LEVEL_PRESSURE / (float)barometricPressure, 0.00019022256f);

	*alt = ((ambientTemperatureInC + 273.15) * (tempResult - 1)) / 0.0065;
}

void BMP280_GetStatus(bool *updating, bool *measuring)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_STATUS);

	*updating = ((registerValue & 1) == 0x1);
	*measuring = ((registerValue >> 1) == 0x1);
}

BMP280_OperationMode_t BMP280_GetMode(void)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	return (registerValue >> BMP280_REGISTER_BIT_MODE) & 0x3;
}

BMP280_Oversampling_t BMP280_GetTemperatureOversampling(void)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	return (registerValue >> BMP280_REGISTER_BIT_OSRS_T) & 0x7;
}

BMP280_Oversampling_t BMP280_GetPressureOversampling(void)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_CONTROL_MEAS);

	return (registerValue >> BMP280_REGISTER_BIT_OSRS_T) & 0x7;
}

BMP280_StandbyTime_t BMP280_GetStandbyTime(void)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_CONFIG);

	return (registerValue >> BMP280_REGISTER_BIT_STANDBY_TIME) & 0x7;
}

BMP280_FilterTimeConstant_t BMP280_GetFilterTimeConstant(void)
{
	uint8_t registerValue;

	registerValue = Read8BitRegister(BMP280_REGISTER_CONFIG);

	return (registerValue >> BMP280_REGISTER_BIT_FILTER) & 0x7;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static void GetRawTemperature(int32_t *raw)
{
	*raw = (int32_t)(Read24BitRegister(BMP280_REGISTER_TEMPERATURE_MSB) >> 4);
}

static void GetRawPressure(int32_t *raw)
{
	*raw = (int32_t)(Read24BitRegister(BMP280_REGISTER_TEMPERATURE_MSB) >> 4);
}

/*extracting calibration data in chip's "non volatile memory". we need to do this only once for each chip*/
static void ReadCalibration(void)
{
	calibration.P1 = Read16BitRegister(BMP280_REGISTER_P1);
	calibration.P2 = (int16_t)Read16BitRegister(BMP280_REGISTER_P2);
	calibration.P3 = (int16_t)Read16BitRegister(BMP280_REGISTER_P3);
	calibration.P4 = (int16_t)Read16BitRegister(BMP280_REGISTER_P4);
	calibration.P5 = (int16_t)Read16BitRegister(BMP280_REGISTER_P5);
	calibration.P6 = (int16_t)Read16BitRegister(BMP280_REGISTER_P6);
	calibration.P7 = (int16_t)Read16BitRegister(BMP280_REGISTER_P7);
	calibration.P8 = (int16_t)Read16BitRegister(BMP280_REGISTER_P8);
	calibration.P9 = (int16_t)Read16BitRegister(BMP280_REGISTER_P9);
	calibration.T1 = Read16BitRegister(BMP280_REGISTER_T1);
	calibration.T2 = (int16_t)Read16BitRegister(BMP280_REGISTER_T2);
	calibration.T3 = (int16_t)Read16BitRegister(BMP280_REGISTER_T3);
}

static uint8_t Read8BitRegister(uint8_t reg)
{
	uint8_t regValue;

	i2c_master_transmit(devHandle, &reg, sizeof(reg), -1);
	i2c_master_receive(devHandle, &regValue, sizeof(regValue), -1);

	return regValue;
}

static uint16_t Read16BitRegister(uint8_t reg)
{
	uint8_t readBuffer[2];

	i2c_master_transmit(devHandle, &reg, sizeof(reg), -1);
	i2c_master_receive(devHandle, readBuffer, sizeof(readBuffer), -1);

	return ((uint16_t)readBuffer[0] << 8) | (uint16_t)readBuffer[1];
}

static uint32_t Read24BitRegister(uint8_t reg)
{
	uint8_t readBuffer[3];

	i2c_master_transmit(devHandle, &reg, sizeof(reg), -1);
	i2c_master_receive(devHandle, readBuffer, sizeof(readBuffer), -1);

	return ((uint32_t)readBuffer[0] << 16) | ((uint32_t)readBuffer[1] << 8) | readBuffer[2];
}

static void WriteRegister(uint8_t reg, uint8_t value)
{
	uint8_t writeBuffer[2];

	writeBuffer[0] = reg;
	writeBuffer[1] = value;

	i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
}

static bool CheckChipId(void)
{
	uint8_t registerValue;
	bool ret = false;

	registerValue = Read8BitRegister(BMP280_REGISTER_ID);

	if (BMP280_CHIP_ID == registerValue)
	{
		ret = true;
	}
	else
	{
		ESP_LOGE(TAG, "Invalid Chip ID: expected %X, got %X", BMP280_CHIP_ID, registerValue);
	}

	return ret;
}