/************************************
 * INCLUDES
 ************************************/

#include "driver_scd30.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define SCD30_I2C_ADDRESS 0x61
#define SCD30_I2C_FREQUENCY 400000

#define SCD30_CMD_CONTINUOUS_MEASUREMENT 0x0010
#define SCD30_CMD_SET_MEASUREMENT_INTERVAL 0x4600
#define SCD30_CMD_GET_DATA_READY 0x0202
#define SCD30_CMD_READ_MEASUREMENT 0x0300
#define SCD30_CMD_STOP_MEASUREMENT 0x0104
#define SCD30_CMD_AUTOMATIC_SELF_CALIBRATION 0x5306
#define SCD30_CMD_SET_FORCED_RECALIBRATION_FACTOR 0x5204
#define SCD30_CMD_SET_TEMPERATURE_OFFSET 0x5403
#define SCD30_CMD_SET_ALTITUDE_COMPENSATION 0x5102
#define SCD30_CMD_READ_SERIALNBR 0xD033
#define SCD30_CMD_READ_FIRMWARE_VERSION 0xD100
#define SCD30_CMD_SET_TEMP_OFFSET 0x5403
#define SCD30_CMD_SOFTWARE_RESET 0xD304

#define SCD30_POLYNOMIAL 0x31 // P(x) = x^8 + x^5 + x^4 + 1 = 100110001

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[SCD30]";

static i2c_master_dev_handle_t devHandle;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static uint8_t ComputeCrc8(uint8_t *data, uint8_t len);
static void WriteCommand(uint16_t command);
static void WriteCommandWithArgument(uint16_t command, uint16_t argument);
static uint16_t WriteReadCommand(uint16_t command, uint8_t *readBuffer, uint8_t readSize);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void SCD30_Initialize(i2c_master_bus_handle_t i2cBusHandle)
{
    uint8_t versionMajor;
    uint8_t versionMinor;

    i2c_device_config_t devCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SCD30_I2C_ADDRESS,
        .scl_speed_hz = SCD30_I2C_FREQUENCY,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devCfg, &devHandle));

    ESP_LOGI(TAG, "Software reset...");
    SCD30_SoftwareReset();

    vTaskDelay(pdMS_TO_TICKS(100));

    if (SCD30_ReadFirmwareVersion(&versionMajor, &versionMinor))
    {
        ESP_LOGI(TAG, "Firmware version %d.%d", versionMajor, versionMinor);
    }
}

bool SCD30_ReadFirmwareVersion(uint8_t *major, uint8_t *minor)
{
    bool ret = false;
    uint8_t readBuffer[3];
    uint8_t computedCrc;
    uint8_t readCrc;

    WriteReadCommand(SCD30_CMD_READ_FIRMWARE_VERSION, readBuffer, sizeof(readBuffer));

    computedCrc = ComputeCrc8(&readBuffer[0], 2);
    readCrc = readBuffer[2];

    if (readCrc == computedCrc)
    {
        *major = readBuffer[0];
        *minor = readBuffer[1];
        ret = true;
    }
    else
    {
        ESP_LOGE(TAG, "Firmware version CRCs don't match");
    }

    return ret;
}

void SCD30_SoftwareReset(void)
{
    WriteCommand(SCD30_CMD_SOFTWARE_RESET);
}

void SCD30_SetTemperatureOffset(uint16_t offset)
{
    WriteCommandWithArgument(SCD30_CMD_SET_TEMP_OFFSET, offset);
}

bool SCD30_GetDataReadyStatus(bool *dataReady)
{
    bool ret = false;
    uint8_t readBuffer[3];
    uint8_t computedCrc;
    uint8_t readCrc;

    WriteReadCommand(SCD30_CMD_GET_DATA_READY, readBuffer, sizeof(readBuffer));

    computedCrc = ComputeCrc8(&readBuffer[0], 2);
    readCrc = readBuffer[2];

    if (readCrc == computedCrc)
    {
        *dataReady = (readBuffer[1] == 0x01);
        ret = true;
    }
    else
    {
        ESP_LOGE(TAG, "Data ready status CRCs don't match");
    }

    return ret;
}

void SCD30_SetAutoSelfCalibration(bool enable)
{
    if (enable)
    {
        WriteCommandWithArgument(SCD30_CMD_AUTOMATIC_SELF_CALIBRATION, 1); // Activate continuous ASC
    }
    else
    {
        WriteCommandWithArgument(SCD30_CMD_AUTOMATIC_SELF_CALIBRATION, 0); // Deactivate continuous ASC
    }
}

void SCD30_SetMeasurementInterval(uint16_t interval)
{
    WriteCommandWithArgument(SCD30_CMD_SET_MEASUREMENT_INTERVAL, interval);
}

void SCD30_StartPeriodicMeasurment(void)
{
    WriteCommandWithArgument(SCD30_CMD_CONTINUOUS_MEASUREMENT, 0x0000);
}

void SCD30_StartPeriodicMeasurmentWithPressureCompensation(uint16_t pressure)
{
    WriteCommandWithArgument(SCD30_CMD_CONTINUOUS_MEASUREMENT, pressure);
}

void SCD30_StopMeasurement(void)
{
    WriteCommand(SCD30_CMD_STOP_MEASUREMENT);
}

bool SCD30_GetMeasures(float *co2, float *temperature, float *humidity)
{
    uint8_t readBuffer[18] = {0};
    uint8_t crcRead;
    uint8_t crcComputed;
    bool ret = true;

    WriteReadCommand(SCD30_CMD_READ_MEASUREMENT, readBuffer, sizeof(readBuffer));

    crcRead = readBuffer[2];
    crcComputed = ComputeCrc8(&readBuffer[0], 2);

    if (crcRead == crcComputed)
    {
        crcRead = readBuffer[5];
        crcComputed = ComputeCrc8(&readBuffer[3], 2);

        if (crcRead == crcComputed)
        {
            *((uint32_t *)co2) = (uint32_t)((((uint32_t)readBuffer[0]) << 24) | (((uint32_t)readBuffer[1]) << 16) |
                                            (((uint32_t)readBuffer[3]) << 8) | ((uint32_t)readBuffer[4]));
        }
        else
        {
            ESP_LOGE(TAG, "CO2 concentration CRCs don't match\n");
            ret = false;
        }
    }
    else
    {
        ESP_LOGE(TAG, "CO2 concentration CRCs don't match\n");
        ret = false;
    }

    crcRead = readBuffer[8];
    crcComputed = ComputeCrc8(&readBuffer[6], 2);

    if (crcRead == crcComputed)
    {
        crcRead = readBuffer[11];
        crcComputed = ComputeCrc8(&readBuffer[9], 2);

        if (crcRead == crcComputed)
        {
            *((uint32_t *)temperature) = (uint32_t)((((uint32_t)readBuffer[6]) << 24) | (((uint32_t)readBuffer[7]) << 16) |
                                                    (((uint32_t)readBuffer[9]) << 8) | ((uint32_t)readBuffer[10]));
        }
        else
        {
            ESP_LOGE(TAG, "Temperature CRCs don't match\n");
            ret = false;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Temperature CRCs don't match\n");
        ret = false;
    }

    crcRead = readBuffer[14];
    crcComputed = ComputeCrc8(&readBuffer[12], 2);

    if (crcRead == crcComputed)
    {
        crcRead = readBuffer[17];
        crcComputed = ComputeCrc8(&readBuffer[15], 2);

        if (crcRead == crcComputed)
        {
            *((uint32_t *)humidity) = (uint32_t)((((uint32_t)readBuffer[12]) << 24) | (((uint32_t)readBuffer[13]) << 16) |
                                                 (((uint32_t)readBuffer[15]) << 8) | ((uint32_t)readBuffer[16]));
        }
        else
        {
            ESP_LOGE(TAG, "Humidity CRCs don't match\n");
            ret = false;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Humidity CRCs don't match\n");
        ret = false;
    }

    return ret;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static void WriteCommand(uint16_t command)
{
    uint8_t writeBuffer[2];

    writeBuffer[0] = command >> 8;
    writeBuffer[1] = command & 0xFF;

    i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
}

static void WriteCommandWithArgument(uint16_t command, uint16_t argument)
{
    uint8_t crc;
    uint8_t writeBuffer[5];

    writeBuffer[0] = command >> 8;
    writeBuffer[1] = command & 0xff;
    writeBuffer[2] = argument >> 8;
    writeBuffer[3] = argument & 0xff;

    crc = ComputeCrc8(&writeBuffer[2], 2); // Compute CRC only on argument

    writeBuffer[4] = crc;

    i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
}

static uint16_t WriteReadCommand(uint16_t command, uint8_t *readBuffer, uint8_t readSize)
{
    uint8_t writeBuffer[2];

    writeBuffer[0] = command >> 8;
    writeBuffer[1] = command & 0xFF;

    i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
    vTaskDelay(pdMS_TO_TICKS(10));
    i2c_master_receive(devHandle, readBuffer, readSize, -1);

    return 0;
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
                crc = (uint8_t)((crc << 1) ^ SCD30_POLYNOMIAL);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}
