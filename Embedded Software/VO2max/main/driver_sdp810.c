#include "driver_SDP810.h"
#include "esp_log.h"

#include <stdlib.h>

#define SDP810_I2C_ADDRESS 0x25
#define SDP810_I2C_FREQUENCY 400000

#define SDP810_POLYNOMIAL 0x31 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

static const char *TAG = "SDP810";

static i2c_master_dev_handle_t devHandle;

static float ConvertTemperatureRawToCelsius(int16_t temperature_raw);
static void ReadData(uint8_t *readBuffer, uint8_t readSize);
static void WriteCommand(uint16_t command);
static void WriteReadCommand(uint16_t command, uint8_t *readBuffer, uint8_t readSize);
static uint8_t ComputeCrc8(uint8_t *data, uint8_t len);

void SDP810_Initialize(i2c_master_bus_handle_t i2cBusHandle)
{
    i2c_device_config_t devCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SDP810_I2C_ADDRESS,
        .scl_speed_hz = SDP810_I2C_FREQUENCY,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devCfg, &devHandle));
}

uint16_t SDP810_StartContinuousMeasurementWithMassFlowTCompAndAveraging()
{
    WriteCommand(0x3603);
    return 0;
}

uint16_t SDP810_StartContinuousMeasurementWithMassFlowTComp()
{
    WriteCommand(0x3608);
    return 0;
}

uint16_t SDP810_StartContinuousMeasurementWithDiffPressureTCompAndAveraging()
{
    WriteCommand(0x3615);
    return 0;
}

uint16_t SDP810_StartContinuousMeasurementWithDiffPressureTComp()
{
    WriteCommand(0x361E);
    return 0;
}

uint16_t SDP810_StopContinuousMeasurement()
{
    WriteCommand(0x3FF9);
    return 0;
}

bool SDP810_ReadMeasurementRaw(int16_t *differentialPressureRaw,
                               int16_t *temperatureRaw,
                               int16_t *scalingFactor)
{
    bool ret = true;
    uint8_t readBuffer[10];
    uint8_t crcRead;
    uint8_t crcComputed;

    ReadData(readBuffer, 9);

    crcRead = readBuffer[2];
    crcComputed = ComputeCrc8(&readBuffer[0], 2);

    if (crcRead == crcComputed)
    {
        *differentialPressureRaw = (readBuffer[0] << 8) + readBuffer[1];
    }
    else
    {
        ESP_LOGE(TAG, "Differential pressure CRCs don't match\n");
        ret = false;
    }

    crcRead = readBuffer[5];
    crcComputed = ComputeCrc8(&readBuffer[3], 2);

    if (crcRead == crcComputed)
    {
        *temperatureRaw = (readBuffer[3] << 8) + readBuffer[4];
    }
    else
    {
        ESP_LOGE(TAG, "Temperature CRCs don't match\n");
        ret = false;
    }

    crcRead = readBuffer[8];
    crcComputed = ComputeCrc8(&readBuffer[6], 2);

    if (crcRead == crcComputed)
    {
        *scalingFactor = (readBuffer[6] << 8) + readBuffer[7];
    }
    else
    {
        ESP_LOGE(TAG, "Scaling factor CRCs don't match\n");
        ret = false;
    }

    return ret;
}

bool SDP810_ReadMeasurement(float *differentialPressure,
                            float *temperature)
{
    bool ret;
    int16_t differentialPressureRaw;
    int16_t temperatureRaw;
    int16_t scalingFactor;

    ret = SDP810_ReadMeasurementRaw(&differentialPressureRaw, &temperatureRaw,
                                    &scalingFactor);

    if (true == ret)
    {
        *differentialPressure = (float)differentialPressureRaw / scalingFactor;
        *temperature = ConvertTemperatureRawToCelsius(temperatureRaw);
    }

    return ret;
}

uint16_t SDP810_ReadProductIdentifier(uint32_t *productNumber,
                                      uint8_t serialNumber[],
                                      uint8_t serialNumberSize)
{
    uint8_t buffer[18];

    WriteCommand(0x367C);
    WriteReadCommand(0xE102, buffer, sizeof(buffer));

    return 0;
}

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

static void ReadData(uint8_t *readBuffer, uint8_t readSize)
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

static uint8_t ComputeCrc8(uint8_t *data, uint8_t len)
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
                crc = (uint8_t)((crc << 1) ^ SDP810_POLYNOMIAL);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}