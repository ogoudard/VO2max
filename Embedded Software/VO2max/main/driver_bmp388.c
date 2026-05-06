/************************************
 * INCLUDES
 ************************************/

#include <math.h>

#include "driver_BMP388.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define BMP388_I2C_FREQUENCY 400000

#define BMP388_RESET_DELAY_MS 100

#define BMP388_REGISTER_CHIP_ID 0x00
#define BMP388_REGISTER_ERR_REG 0X02
#define BMP388_REGISTER_STATUS 0X03
#define BMP388_REGISTER_PRESSURE_XLSB 0x04
#define BMP388_REGISTER_PRESSURE_LSB 0x05
#define BMP388_REGISTER_PRESSURE_MSB 0x06
#define BMP388_REGISTER_TEMPERATURE_XLSB 0x07
#define BMP388_REGISTER_TEMPERATURE_LSB 0x08
#define BMP388_REGISTER_TEMPERATURE_MSB 0x09
#define BMP388_REGISTER_SENSOR_TIME_0 0x0C
#define BMP388_REGISTER_SENSOR_TIME_1 0x0D
#define BMP388_REGISTER_SENSOR_TIME_2 0x0E
#define BMP388_REGISTER_EVENT 0x10
#define BMP388_REGISTER_INT_STATUS 0x11
#define BMP388_REGISTER_FIFO_LENGTH_LSB 0x12
#define BMP388_REGISTER_FIFO_LENGTH_MSB 0x13
#define BMP388_REGISTER_FIFO_DATA 0x14
#define BMP388_REGISTER_FIFO_WATERMARK_LSB 0x15
#define BMP388_REGISTER_FIFO_WATERMARK_MSB 0x16
#define BMP388_REGISTER_FIFO_CONFIG_1 0x17
#define BMP388_REGISTER_FIFO_CONFIG_2 0x18
#define BMP388_REGISTER_INT_CTRL 0x19
#define BMP388_REGISTER_IF_CONF 0x1A
#define BMP388_REGISTER_PWR_CTRL 0x1B
#define BMP388_REGISTER_OSR 0x1C
#define BMP388_REGISTER_ODR 0x1D
#define BMP388_REGISTER_CONFIG 0x1F
#define BMP388_REGISTER_CAL_T1 0X31
#define BMP388_REGISTER_CAL_T2 0X33
#define BMP388_REGISTER_CAL_T3 0X35
#define BMP388_REGISTER_CAL_P1 0X36
#define BMP388_REGISTER_CAL_P2 0X38
#define BMP388_REGISTER_CAL_P3 0X3A
#define BMP388_REGISTER_CAL_P4 0X3B
#define BMP388_REGISTER_CAL_P5 0X3C
#define BMP388_REGISTER_CAL_P6 0X3E
#define BMP388_REGISTER_CAL_P7 0X40
#define BMP388_REGISTER_CAL_P8 0X41
#define BMP388_REGISTER_CAL_P9 0X42
#define BMP388_REGISTER_CAL_P10 0X44
#define BMP388_REGISTER_CAL_P11 0X45
#define BMP388_REGISTER_CMD 0x7E

#define BMP388_RESET_VALUE 0xB6
#define BMP388_CHIP_ID 0x50

#define BMP388_REGISTER_BIT_OSR_P 0x07
#define BMP388_REGISTER_SHIFT_OSR_P 0
#define BMP388_REGISTER_BIT_OSR_T 0x07
#define BMP388_REGISTER_SHIFT_OSR_T 3

#define BMP388_REGISTER_SHIFT_PRESSURE_EN 0
#define BMP388_REGISTER_SHIFT_TEMP_EN 1
#define BMP388_REGISTER_BIT_OPERATION_MODE 0x03
#define BMP388_REGISTER_SHIFT_OPERATION_MODE 4

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[BMP388]";
static i2c_master_dev_handle_t devHandle;
static BMP388_CalibrationParameters_t calibration;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void WriteRegister(uint8_t reg, uint8_t value);
static uint8_t Read8BitRegister(uint8_t reg);
static uint16_t Read16BitRegister(uint8_t reg);
static void ReadRawPressureAndTemperature(uint32_t *pressure, uint32_t *temperature);
static int64_t CompensateTemperature(uint32_t rawTemperature, int64_t *tFine);
static int64_t CompensatePressure(int64_t tFine, uint32_t rawPressure);
static bool CheckChipId(void);
static void ReadCalibration(void);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

bool BMP388_Initialize(i2c_master_bus_handle_t i2cBusHandle, uint8_t address)
{
	bool ret = false;

	i2c_device_config_t devCfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = address,
		.scl_speed_hz = BMP388_I2C_FREQUENCY,
	};

	ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devCfg, &devHandle));

	BMP388_Reset();

	if (CheckChipId())
	{
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

void BMP388_Reset(void)
{
	WriteRegister(BMP388_REGISTER_CMD, BMP388_RESET_VALUE);
	vTaskDelay(pdMS_TO_TICKS(BMP388_RESET_DELAY_MS));
}

BMP388_Error_u BMP388_GetError(void)
{
	return (BMP388_Error_u)Read8BitRegister(BMP388_REGISTER_ERR_REG);
}

BMP388_Status_u BMP388_GetStatus(void)
{
	return (BMP388_Status_u)Read8BitRegister(BMP388_REGISTER_STATUS);
}

void BMP388_SetOperationMode(BMP388_OperationMode_t operationMode)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP388_REGISTER_PWR_CTRL);

	regValue &= ~(BMP388_REGISTER_BIT_OPERATION_MODE << BMP388_REGISTER_SHIFT_OPERATION_MODE);
	regValue |= (operationMode << BMP388_REGISTER_SHIFT_OPERATION_MODE);

	WriteRegister(BMP388_REGISTER_PWR_CTRL, regValue);
}

void BMP388_EnableTemperatureMeasurement(bool en)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP388_REGISTER_PWR_CTRL);

	if (true == en)
	{
		regValue |= (1 << BMP388_REGISTER_SHIFT_TEMP_EN);
	}
	else
	{
		regValue &= ~(1 << BMP388_REGISTER_SHIFT_TEMP_EN);
	}

	WriteRegister(BMP388_REGISTER_PWR_CTRL, regValue);
}

void BMP388_EnablePressureMeasurement(bool en)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP388_REGISTER_PWR_CTRL);

	if (true == en)
	{
		regValue |= (1 << BMP388_REGISTER_SHIFT_PRESSURE_EN);
	}
	else
	{
		regValue &= ~(1 << BMP388_REGISTER_SHIFT_PRESSURE_EN);
	}

	WriteRegister(BMP388_REGISTER_PWR_CTRL, regValue);
}

void BMP388_SetPressureOversampling(BMP388_Oversampling_t osValue)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP388_REGISTER_OSR);

	regValue &= ~(BMP388_REGISTER_BIT_OSR_P << BMP388_REGISTER_SHIFT_OSR_P);
	regValue |= (osValue << BMP388_REGISTER_SHIFT_OSR_P);

	WriteRegister(BMP388_REGISTER_OSR, regValue);
}

void BMP388_SetTemperatureOversampling(BMP388_Oversampling_t osValue)
{
	uint8_t regValue;

	regValue = Read8BitRegister(BMP388_REGISTER_OSR);

	regValue &= ~(BMP388_REGISTER_BIT_OSR_T << BMP388_REGISTER_SHIFT_OSR_T);
	regValue |= (osValue << BMP388_REGISTER_SHIFT_OSR_T);

	WriteRegister(BMP388_REGISTER_OSR, regValue);
}

void BMP388_SetStandbyTime(BMP388_StandbyTime_t standbyTime)
{
	// uint8_t regValue;

	// regValue = Read8BitRegister(BMP388_REGISTER_CONFIG);

	// regValue &= ~(0x7 << BMP388_REGISTER_BIT_STANDBY_TIME);
	// regValue |= (standbyTime << BMP388_REGISTER_BIT_STANDBY_TIME);

	// WriteRegister(BMP388_REGISTER_CONFIG, regValue);
}

/*sets low pass internal filter coefficient for BMP388. used in noisy environments*/
void BMP388_SetFilterTimeConstant(BMP388_FilterTimeConstant_t filterTimeConstant)
{
	// uint8_t regValue;

	// regValue = Read8BitRegister(BMP388_REGISTER_CONFIG);

	// regValue &= ~(0x7 << BMP388_REGISTER_BIT_FILTER);
	// regValue |= (filterTimeConstant << BMP388_REGISTER_BIT_FILTER);

	// WriteRegister(BMP388_REGISTER_CONFIG, regValue);
}

void BMP388_ReadTemperatureAndPressure(float *temperature, float *pressure)
{
	uint32_t rawPressure;
	uint32_t rawTemperature;
	int64_t tFine;

	ReadRawPressureAndTemperature(&rawPressure, &rawTemperature);

	*temperature = (float)((double)CompensateTemperature(rawTemperature, &tFine) / 100.0f);
	*pressure = (float)((double)CompensatePressure(tFine, rawPressure) / 10000.0f);
}

void BMP388_CalculateAltitudeQuick(float *alt, uint32_t barometricPressure)
{
	float temp_result;

	temp_result = powf(barometricPressure, 0.190284);

	*alt = 44307.69396 * (1 - 0.111555816 * temp_result); /*calculating altitude from barometric formula*/
}

void BMP388_CalculateAltitudeHypsometric(float *alt, uint32_t barometricPressure, float ambientTemperatureInC)
{
	float tempResult;

	tempResult = powf((float)BMP388_SEA_LEVEL_PRESSURE_PA / (float)barometricPressure, 0.00019022256f);

	*alt = ((ambientTemperatureInC + 273.15) * (tempResult - 1)) / 0.0065;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

/*extracting calibration data in chip's "non volatile memory". we need to do this only once for each chip*/
static void ReadCalibration(void)
{
	calibration.T1 = Read16BitRegister(BMP388_REGISTER_CAL_T1);
	calibration.T2 = Read16BitRegister(BMP388_REGISTER_CAL_T2);
	calibration.T3 = (int8_t)Read8BitRegister(BMP388_REGISTER_CAL_T3);
	calibration.P1 = (int16_t)Read16BitRegister(BMP388_REGISTER_CAL_P1);
	calibration.P2 = (int16_t)Read16BitRegister(BMP388_REGISTER_CAL_P2);
	calibration.P3 = (int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P3);
	calibration.P4 = (int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P4);
	calibration.P5 = Read16BitRegister(BMP388_REGISTER_CAL_P5);
	calibration.P6 = Read16BitRegister(BMP388_REGISTER_CAL_P6);
	calibration.P7 = ((int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P7));
	calibration.P8 = ((int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P8));
	calibration.P9 = ((int16_t)Read16BitRegister(BMP388_REGISTER_CAL_P9));
	calibration.P10 = ((int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P10));
	calibration.P11 = ((int8_t)Read8BitRegister(BMP388_REGISTER_CAL_P11));
}

static uint8_t Read8BitRegister(uint8_t reg)
{
	uint8_t regValue;

	i2c_master_transmit_receive(devHandle, &reg, sizeof(reg), &regValue, sizeof(regValue), -1);

	return regValue;
}

static uint16_t Read16BitRegister(uint8_t reg)
{
	uint8_t readBuffer[2];

	i2c_master_transmit_receive(devHandle, &reg, sizeof(reg), readBuffer, sizeof(readBuffer), -1);

	return ((uint16_t)readBuffer[1] << 8) | (uint16_t)readBuffer[0];
}

static void ReadRawPressureAndTemperature(uint32_t *pressure, uint32_t *temperature)
{
	uint8_t readBuffer[6];
	uint8_t reg = BMP388_REGISTER_PRESSURE_XLSB;

	// Single write/read for both pressure and temperature registers
	i2c_master_transmit_receive(devHandle, &reg, sizeof(reg), readBuffer, sizeof(readBuffer), -1);

	*pressure = ((uint32_t)readBuffer[2] << 16) | ((uint32_t)readBuffer[1] << 8) | (uint32_t)readBuffer[0];
	*temperature = ((uint32_t)readBuffer[5] << 16) | ((uint32_t)readBuffer[4] << 8) | (uint32_t)readBuffer[3];
}

static int64_t CompensateTemperature(uint32_t rawTemperature, int64_t *tFine)
{
	uint64_t partialData1;
	uint64_t partialData2;
	uint64_t partialData3;
	int64_t partialData4;
	int64_t partialData5;
	int64_t partialData6;

	/* calculate compensate temperature */
	partialData1 = (uint64_t)(rawTemperature - (256 * (uint64_t)(calibration.T1)));
	partialData2 = (uint64_t)(calibration.T2 * partialData1);
	partialData3 = (uint64_t)(partialData1 * partialData1);
	partialData4 = (int64_t)(((int64_t)partialData3) * ((int64_t)calibration.T3));
	partialData5 = ((int64_t)(((int64_t)partialData2) * 262144) + (int64_t)partialData4);
	partialData6 = (int64_t)(((int64_t)partialData5) / 4294967296U);
	*tFine = partialData6;

	return (int64_t)((partialData6 * 25) / 16384);
}

static int64_t CompensatePressure(int64_t tFine, uint32_t rawPressure)
{
	int64_t partialData1;
	int64_t partialData2;
	int64_t partialData3;
	int64_t partialData4;
	int64_t partialData5;
	int64_t partialData6;
	int64_t offset;
	int64_t sensitivity;

	/* calculate compensate pressure */
	partialData1 = tFine * tFine;
	partialData2 = partialData1 / 64;
	partialData3 = (partialData2 * tFine) / 256;
	partialData4 = (calibration.P8 * partialData3) / 32;
	partialData5 = (calibration.P7 * partialData1) * 16;
	partialData6 = (calibration.P6 * tFine) * 4194304;
	offset = (int64_t)((int64_t)(calibration.P5) * (int64_t)140737488355328U) + partialData4 + partialData5 + partialData6;
	partialData2 = (((int64_t)calibration.P4) * partialData3) / 32;
	partialData4 = (calibration.P3 * partialData1) * 4;
	partialData5 = ((int64_t)(calibration.P2) - 16384) * ((int64_t)tFine) * 2097152;
	sensitivity = (((int64_t)(calibration.P1) - 16384) * (int64_t)70368744177664U) + partialData2 + partialData4 + partialData5;
	partialData1 = (sensitivity / 16777216) * rawPressure;
	partialData2 = (int64_t)(calibration.P10) * (int64_t)(tFine);
	partialData3 = partialData2 + (65536 * (int64_t)(calibration.P9));
	partialData4 = (partialData3 * rawPressure) / 8192;
	partialData5 = (partialData4 * rawPressure) / 512;
	partialData6 = (int64_t)((uint64_t)rawPressure * (uint64_t)rawPressure);
	partialData2 = ((int64_t)(calibration.P11) * (int64_t)(partialData6)) / 65536;
	partialData3 = (partialData2 * rawPressure) / 128;
	partialData4 = (offset / 4) + partialData1 + partialData5 + partialData3;

	return (((uint64_t)partialData4 * 25) / (uint64_t)1099511627776U);
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

	registerValue = Read8BitRegister(BMP388_REGISTER_CHIP_ID);

	if (BMP388_CHIP_ID == registerValue)
	{
		ret = true;
	}
	else
	{
		ESP_LOGE(TAG, "Invalid Chip ID: expected %X, got %X", BMP388_CHIP_ID, registerValue);
	}

	return ret;
}