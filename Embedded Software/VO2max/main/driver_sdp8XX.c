/************************************
 * INCLUDES
 ************************************/

#include <stdlib.h>

#include "driver_SDP8XX.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define SDP8XX_I2C_FREQUENCY 400000

#define SDP8XX_POLYNOMIAL 0x31 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

#define DELAY_SOFTWARE_RESET_MS 10

#define CMD_SOFTWARE_RESET 0x06
#define CMD_CONT_MASS_FLOW_T_COMP_AND_AVERAGING 0x3603
#define CMD_CONT_MASS_FLOW_T_COMP 0x3608
#define CMD_CONT_DIFF_PRESSURE_T_COMP_AND_AVERAGING 0x3615
#define CMD_CONT_DIFF_PRESSURE_T_COMP 0x361E
#define CMD_STOP_CONT_MEASUREMENT 0x3FF9
#define CMD_READ_PRODUCT_ID_FIRST 0x367C
#define CMD_READ_PRODUCT_ID_SECOND 0xE102

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[SDP8XX]";

static i2c_master_dev_handle_t devHandle;
static i2c_master_dev_handle_t generalCallHandle;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static float ConvertTemperatureRawToCelsius(int16_t temperature_raw);
static void ReadCommand(uint8_t *readBuffer, uint8_t readSize);
static void WriteCommand(uint16_t command);
static void WriteReadCommand(uint16_t command, uint8_t *readBuffer, uint8_t readSize);
static uint8_t ComputeCrc8(const uint8_t *data, uint8_t len);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

bool SDP8XX_Initialize(i2c_master_bus_handle_t i2cBusHandle, SdpProductNumber_e productNumber)
{
    uint64_t serialNumber;
    uint32_t readProductNumber;
    bool ret = true;

    i2c_device_config_t devConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = SDP8XX_I2C_FREQUENCY,
    };

    switch (productNumber)
    {
    case SDP8XX_SDP800_500PA:
    case SDP8XX_SDP810_500PA:
    case SDP8XX_SDP800_125PA:
    case SDP8XX_SDP810_125PA:
        devConfig.device_address = SDP8X0_I2C_ADDRESS;
        break;

    case SDP8XX_SDP801_500PA:
    case SDP8XX_SDP811_500PA:
        devConfig.device_address = SDP8X1_I2C_ADDRESS;
        break;
    default:
        ESP_LOGE(TAG, "Invalid product number selected");
        ret = false;
    }

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devConfig, &devHandle));

    devConfig.device_address = 0x00;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devConfig, &generalCallHandle));

    SDP8XX_SoftwareReset();

    if (SDP8XX_ReadProductIdentifier(&readProductNumber, &serialNumber))
    {
        ESP_LOGI(TAG, "Product number = 0x%X", readProductNumber);
        ESP_LOGI(TAG, "Serial number = 0x%" PRIX64 "", serialNumber);

        if (readProductNumber != productNumber)
        {
            ESP_LOGE(TAG, "Product number set (0x%X) does not match product number read from device (0x%X)", productNumber, readProductNumber);
            ESP_LOGE(TAG, "Initialization failed");
            ret = false;
        }
        else
        {
            ESP_LOGI(TAG, "Initialization successfull");
        }
    }
    else
    {
        ESP_LOGE(TAG, "Could not read product identifiers");
        ESP_LOGE(TAG, "Initialization failed");
        ret = false;
    }

    return ret;
}

void SDP8XX_SoftwareReset()
{
    uint8_t writeBuffer[1];

    ESP_LOGI(TAG, "Resetting device...");
    writeBuffer[0] = CMD_SOFTWARE_RESET;

    i2c_master_transmit(generalCallHandle, writeBuffer, sizeof(writeBuffer), -1);

    vTaskDelay(pdMS_TO_TICKS(DELAY_SOFTWARE_RESET_MS));
}

bool SDP8XX_ReadProductIdentifier(uint32_t *productNumber,
                                  uint64_t *serialNumber)
{
    uint8_t readBuffer[18]; // 6 (4 + 2) for product number (+ CRC)
                            // 12 (8 + 4) for serial number (+ CRC)
    uint8_t crcRead;
    uint8_t crcComputed;
    bool ret = true;

    WriteCommand(CMD_READ_PRODUCT_ID_FIRST);
    WriteReadCommand(CMD_READ_PRODUCT_ID_SECOND, readBuffer, sizeof(readBuffer));

    crcRead = readBuffer[2];
    crcComputed = ComputeCrc8(&readBuffer[0], 2);

    if (crcRead != crcComputed)
    {
        ESP_LOGE(TAG, "Product number CRCs don't match");
        ret = false;
    }
    else
    {
        crcRead = readBuffer[5];
        crcComputed = ComputeCrc8(&readBuffer[3], 2);

        if (crcRead != crcComputed)
        {
            ESP_LOGE(TAG, "Product number CRCs don't match");
            ret = false;
        }
        else
        {
            *productNumber = ((uint32_t)readBuffer[0] << 24) +
                             ((uint32_t)readBuffer[1] << 16) +
                             ((uint32_t)readBuffer[3] << 8) +
                             (uint32_t)readBuffer[4];
        }
    }

    crcRead = readBuffer[8];
    crcComputed = ComputeCrc8(&readBuffer[6], 2);

    if (crcRead != crcComputed)
    {
        ESP_LOGE(TAG, "Serial number CRCs don't match");
        ret = false;
    }
    else
    {
        crcRead = readBuffer[11];
        crcComputed = ComputeCrc8(&readBuffer[9], 2);

        if (crcRead != crcComputed)
        {
            ESP_LOGE(TAG, "Serial number CRCs don't match");
            ret = false;
        }
        else
        {
            crcRead = readBuffer[14];
            crcComputed = ComputeCrc8(&readBuffer[12], 2);

            if (crcRead != crcComputed)
            {
                ESP_LOGE(TAG, "Serial number CRCs don't match");
                ret = false;
            }
            else
            {
                crcRead = readBuffer[17];
                crcComputed = ComputeCrc8(&readBuffer[15], 2);

                if (crcRead != crcComputed)
                {
                    ESP_LOGE(TAG, "Serial number CRCs don't match");
                    ret = false;
                }
                else
                {
                    *serialNumber = ((uint64_t)readBuffer[6] << 56) +
                                    ((uint64_t)readBuffer[7] << 48) +
                                    ((uint64_t)readBuffer[9] << 40) +
                                    ((uint64_t)readBuffer[10] << 32) +
                                    ((uint64_t)readBuffer[12] << 24) +
                                    ((uint64_t)readBuffer[13] << 16) +
                                    ((uint64_t)readBuffer[15] << 8) +
                                    (uint64_t)readBuffer[16];
                }
            }
        }
    }

    return ret;
}

void SDP8XX_StartContinuousMeasurementWithMassFlowTCompAndAveraging()
{
    WriteCommand(CMD_CONT_MASS_FLOW_T_COMP_AND_AVERAGING);
}

void SDP8XX_StartContinuousMeasurementWithMassFlowTComp()
{
    WriteCommand(CMD_CONT_MASS_FLOW_T_COMP);
}

void SDP8XX_StartContinuousMeasurementWithDiffPressureTCompAndAveraging()
{
    WriteCommand(CMD_CONT_DIFF_PRESSURE_T_COMP_AND_AVERAGING);
}

void SDP8XX_StartContinuousMeasurementWithDiffPressureTComp()
{
    WriteCommand(CMD_CONT_DIFF_PRESSURE_T_COMP);
}

void SDP8XX_StopContinuousMeasurement()
{
    WriteCommand(CMD_STOP_CONT_MEASUREMENT);
}

bool SDP8XX_ReadScalingFactor(int16_t *scalingFactor)
{
    int16_t differentialPressureRaw;
    int16_t tempRaw;

    return SDP8XX_ReadMeasurementsRaw(&differentialPressureRaw, &tempRaw, scalingFactor);
}

bool SDP8XX_ReadDifferentialPressureRaw(int16_t *differentialPressureRaw)
{
    bool ret = false;
    uint8_t readBuffer[3];
    uint8_t crcRead;
    uint8_t crcComputed;

    ReadCommand(readBuffer, sizeof(readBuffer));

    crcRead = readBuffer[2];
    crcComputed = ComputeCrc8(&readBuffer[0], 2);

    if (crcRead != crcComputed)
    {
        ESP_LOGE(TAG, "Differential pressure CRCs don't match");
    }
    else
    {
        *differentialPressureRaw = (readBuffer[0] << 8) + readBuffer[1];
        ret = true;
    }

    return ret;
}

bool SDP8XX_ReadMeasurementsRaw(int16_t *differentialPressureRaw,
                                int16_t *temperatureRaw,
                                int16_t *scalingFactor)
{
    bool ret = false;
    uint8_t readBuffer[9];
    uint8_t crcRead;
    uint8_t crcComputed;

    ReadCommand(readBuffer, sizeof(readBuffer));

    crcRead = readBuffer[2];
    crcComputed = ComputeCrc8(&readBuffer[0], 2);

    if (crcRead != crcComputed)
    {
        ESP_LOGE(TAG, "Differential pressure CRCs don't match");
    }
    else
    {
        *differentialPressureRaw = (int16_t)(((uint16_t)readBuffer[0] << 8) + (uint16_t)readBuffer[1]);

        crcRead = readBuffer[5];
        crcComputed = ComputeCrc8(&readBuffer[3], 2);

        if (crcRead != crcComputed)
        {
            ESP_LOGE(TAG, "Temperature CRCs don't match\n");
        }
        else
        {
            *temperatureRaw = (int16_t)(((uint16_t)readBuffer[3] << 8) + (uint16_t)readBuffer[4]);
            crcRead = readBuffer[8];
            crcComputed = ComputeCrc8(&readBuffer[6], 2);

            if (crcRead != crcComputed)
            {
                ESP_LOGE(TAG, "Scaling factor CRCs don't match\n");
            }
            else
            {
                *scalingFactor = (int16_t)(((uint16_t)readBuffer[6] << 8) + (uint16_t)readBuffer[7]);
                ret = true;
            }
        }
    }

    return ret;
}

bool SDP8XX_ReadMeasurements(float *differentialPressure,
                             float *temperature)
{
    bool ret = false;
    int16_t differentialPressureRaw;
    int16_t temperatureRaw;
    int16_t scalingFactor;

    if (SDP8XX_ReadMeasurementsRaw(&differentialPressureRaw,
                                   &temperatureRaw,
                                   &scalingFactor))
    {
        *differentialPressure = (float)differentialPressureRaw / scalingFactor;
        *temperature = ConvertTemperatureRawToCelsius(temperatureRaw);
        ret = true;
    }

    return ret;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static float ConvertTemperatureRawToCelsius(int16_t temperature_raw)
{
    return (float)temperature_raw / 200.0;
}

static void WriteCommand(uint16_t command)
{
    uint8_t writeBuffer[2];

    writeBuffer[0] = command >> 8;
    writeBuffer[1] = command & 0xFF;

    i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
}

static void ReadCommand(uint8_t *readBuffer, uint8_t readSize)
{
    i2c_master_receive(devHandle, readBuffer, readSize, -1);
}

static void WriteReadCommand(uint16_t command, uint8_t *readBuffer, uint8_t readSize)
{
    uint8_t writeBuffer[2];

    writeBuffer[0] = command >> 8;
    writeBuffer[1] = command & 0xFF;

    i2c_master_transmit_receive(devHandle, writeBuffer, sizeof(writeBuffer), readBuffer, readSize, -1);
}

static uint8_t ComputeCrc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    size_t i;
    size_t j;

    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (uint8_t)((crc << 1) ^ SDP8XX_POLYNOMIAL);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}