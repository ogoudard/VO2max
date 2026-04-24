/************************************
 * INCLUDES
 ************************************/

#include "driver_me2o2.h"
#include "esp_log.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define ME202_I2C_FREQUENCY 400000

/* ME2-O2 register address */
#define OXYGEN_DATA_REGISTER 0x03 // Oxygen data register
#define USER_SET_REGISTER 0x08    // user set key value
#define ACTUAL_SET_REGISTER 0x09  // actual set key value
#define GET_KEY_REGISTER 0x0A     // get key value
#define VERSION_REGISTER 0x0F

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[M2-O2]";

static i2c_master_dev_handle_t devHandle;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void WriteRegister(uint8_t reg, uint8_t data);
static void ReadRegister(uint8_t reg, uint8_t *readBuffer, uint8_t readSize);
static float ReadKey(void);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void ME2O2_Initialize(i2c_master_bus_handle_t i2cBusHandle)
{
    uint8_t readBuffer;
     
    i2c_device_config_t devCfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ME2O2_I2C_ADDRESS,
        .scl_speed_hz = ME202_I2C_FREQUENCY,
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
bool ME2O2_ReadOxygen(float *oxygen)
{
    uint8_t readBuffer[3];
    float key;
    bool ret = false;
    float value;

    key = ReadKey();

    ReadRegister(OXYGEN_DATA_REGISTER, readBuffer, sizeof(readBuffer));

    value = key * ((float)readBuffer[0] + ((float)readBuffer[1] / 10.0) + ((float)readBuffer[2] / 100.0));

    if(value != NAN)
    {
        *oxygen = value;
        ret = true;
    }

    return ret;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

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

static float ReadKey(void)
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