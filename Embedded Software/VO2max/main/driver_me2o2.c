#include "driver_me2o2.h"
#include "esp_log.h"

/* Oxygen register address */
#define OXYGEN_DATA_REGISTER 0x03 // Oxygen data register
#define USER_SET_REGISTER 0x08    // user set key value
#define ACTUAL_SET_REGISTER 0x09  // actual set key value
#define GET_KEY_REGISTER 0x0A     // get key value
#define VERSION_REGISTER 0x0F

static const char *TAG = "M2O2";

static i2c_master_dev_handle_t devHandle;

static void WriteRegister(uint8_t reg, uint8_t data);
static void ReadRegister(uint8_t reg, uint8_t *readBuffer, uint8_t readSize);
static float ReadKey();

void ME2O2_Initialize(i2c_master_bus_handle_t i2cBusHandle)
{
    uint8_t readBuffer;
     
    i2c_device_config_t devCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ME2O2_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2cBusHandle, &devCfg, &devHandle));

    ReadRegister(VERSION_REGISTER, &readBuffer, sizeof(readBuffer));

    ESP_LOGI(TAG, "Version = %d\n", readBuffer);
}


/* Set Key value */
void ME2O2_Calibrate(float vol)
{
    uint8_t keyValue = (uint8_t)(vol * 10.0f);

    WriteRegister(USER_SET_REGISTER, keyValue);
}

/* Reading oxygen concentration */
float ME2O2_ReadOxygen()
{
    uint8_t readBuffer[3];
    float key;

    key = ReadKey();

    ReadRegister(OXYGEN_DATA_REGISTER, readBuffer, sizeof(readBuffer));

    return key * ((float)readBuffer[0] + ((float)readBuffer[1] / 10.0) + ((float)readBuffer[2] / 100.0));
}

/* Write data to the i2c register  */
static void WriteRegister(uint8_t reg, uint8_t data)
{
    uint8_t writeBuffer[2];

    writeBuffer[0] = reg;
    writeBuffer[1] = data;

    i2c_master_transmit(devHandle, writeBuffer, sizeof(writeBuffer), -1);
}

static void ReadRegister(uint8_t reg, uint8_t *readBuffer, uint8_t readSize)
{
    i2c_master_transmit_receive(devHandle, &reg, sizeof(reg), readBuffer, readSize, -1);
}

/* Get the average data */
static float ComputeAverage(float *array, uint8_t length)
{
    float sum = 0.0f;

    for (uint8_t i = 0; i < length; i++)
    {
        sum += array[i];
    }

    return sum / (float)length;
}

static float ReadKey()
{
    uint8_t value = 0;
    float key;

    ReadRegister(GET_KEY_REGISTER, &value, 1);

    if (value == 0)
    {
        key = 20.9 / 120.0;
    }
    else
    {
        key = (float)value / 1000.0;
    }

    return key;
}