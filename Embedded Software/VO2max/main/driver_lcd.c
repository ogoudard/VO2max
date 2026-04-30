#include "driver_lcd.h"
#include "driver_ST7789_font.h"
#include <driver/spi_master.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver_ST7789_settings.h"

/**
 * @brief chip information definition
 */
#define CHIP_NAME "Sitronix ST7789"  /**< chip name */
#define MANUFACTURER_NAME "Sitronix" /**< manufacturer name */
#define SUPPLY_VOLTAGE_MIN 2.4f      /**< chip min supply voltage */
#define SUPPLY_VOLTAGE_MAX 3.3f      /**< chip max supply voltage */
#define MAX_CURRENT 7.5f             /**< chip max current */
#define TEMPERATURE_MIN -30.0f       /**< chip min operating temperature */
#define TEMPERATURE_MAX 85.0f        /**< chip max operating temperature */
#define DRIVER_VERSION 1000          /**< driver version */

#define X_OFFSET 52
#define Y_OFFSET 40

/**
 * @brief command data type definition
 */
#define ST7789_CMD 0  /**< command type */
#define ST7789_DATA 1 /**< data type */

/**
 * @brief chip command definition
 */
#define ST7789_CMD_NOP 0x00       /**< no operation command */
#define ST7789_CMD_SWRESET 0x01   /**< software reset command */
#define ST7789_CMD_SLPIN 0x10     /**< sleep in command */
#define ST7789_CMD_SLPOUT 0x11    /**< sleep out command */
#define ST7789_CMD_PTLON 0x12     /**< partial mode on command */
#define ST7789_CMD_NORON 0x13     /**< normal display mode on command */
#define ST7789_CMD_INVOFF 0x20    /**< display inversion off command */
#define ST7789_CMD_INVON 0x21     /**< display inversion on command */
#define ST7789_CMD_GAMSET 0x26    /**< display inversion set command */
#define ST7789_CMD_DISPOFF 0x28   /**< display off command */
#define ST7789_CMD_DISPON 0x29    /**< display on command */
#define ST7789_CMD_CASET 0x2A     /**< column address set command */
#define ST7789_CMD_RASET 0x2B     /**< row address set command */
#define ST7789_CMD_RAMWR 0x2C     /**< memory write command */
#define ST7789_CMD_PTLAR 0x30     /**< partial start/end address set command */
#define ST7789_CMD_VSCRDEF 0x33   /**< vertical scrolling definition command */
#define ST7789_CMD_TEOFF 0x34     /**< tearing effect line off command */
#define ST7789_CMD_TEON 0x35      /**< tearing effect line on command */
#define ST7789_CMD_MADCTL 0x36    /**< memory data access control command */
#define ST7789_CMD_VSCRSADD 0x37  /**< vertical scrolling start address command */
#define ST7789_CMD_IDMOFF 0x38    /**< idle mode off command */
#define ST7789_CMD_IDMON 0x39     /**< idle mode on command */
#define ST7789_CMD_COLMOD 0x3A    /**< interface pixel format command */
#define ST7789_CMD_RAMWRC 0x3C    /**< memory write continue command */
#define ST7789_CMD_TESCAN 0x44    /**< set tear scanline command */
#define ST7789_CMD_WRDISBV 0x51   /**< write display brightness command */
#define ST7789_CMD_WRCTRLD 0x53   /**< write CTRL display command */
#define ST7789_CMD_WRCACE 0x55    /**< write content adaptive brightness control and color enhancement command */
#define ST7789_CMD_WRCABCMB 0x5E  /**< write CABC minimum brightness command */
#define ST7789_CMD_RAMCTRL 0xB0   /**< ram control command */
#define ST7789_CMD_RGBCTRL 0xB1   /**< rgb control command */
#define ST7789_CMD_PORCTRL 0xB2   /**< porch control command */
#define ST7789_CMD_FRCTRL1 0xB3   /**< frame rate control 1 command */
#define ST7789_CMD_PARCTRL 0xB5   /**< partial mode control command */
#define ST7789_CMD_GCTRL 0xB7     /**< gate control command */
#define ST7789_CMD_GTADJ 0xB8     /**< gate on timing adjustment command */
#define ST7789_CMD_DGMEN 0xBA     /**< digital gamma enable command */
#define ST7789_CMD_VCOMS 0xBB     /**< vcoms setting command */
#define ST7789_CMD_LCMCTRL 0xC0   /**< lcm control command */
#define ST7789_CMD_IDSET 0xC1     /**< id setting command */
#define ST7789_CMD_VDVVRHEN 0xC2  /**< vdv and vrh command enable command */
#define ST7789_CMD_VRHS 0xC3      /**< vrh set command */
#define ST7789_CMD_VDVSET 0xC4    /**< vdv setting command */
#define ST7789_CMD_VCMOFSET 0xC5  /**< vcoms offset set command */
#define ST7789_CMD_FRCTR2 0xC6    /**< fr control 2 command */
#define ST7789_CMD_CABCCTRL 0xC7  /**< cabc control command */
#define ST7789_CMD_REGSEL1 0xC8   /**< register value selection1 command */
#define ST7789_CMD_REGSEL2 0xCA   /**< register value selection2 command */
#define ST7789_CMD_PWMFRSEL 0xCC  /**< pwm frequency selection command */
#define ST7789_CMD_PWCTRL1 0xD0   /**< power control 1 command */
#define ST7789_CMD_VAPVANEN 0xD2  /**< enable vap/van signal output command */
#define ST7789_CMD_CMD2EN 0xDF    /**< command 2 enable command */
#define ST7789_CMD_PVGAMCTRL 0xE0 /**< positive voltage gamma control command */
#define ST7789_CMD_NVGAMCTRL 0xE1 /**< negative voltage gamma control command */
#define ST7789_CMD_DGMLUTR 0xE2   /**< digital gamma look-up table for red command */
#define ST7789_CMD_DGMLUTB 0xE3   /**< digital gamma look-up table for blue command */
#define ST7789_CMD_GATECTRL 0xE4  /**< gate control command */
#define ST7789_CMD_SPI2EN 0xE7    /**< spi2 command */
#define ST7789_CMD_PWCTRL2 0xE8   /**< power control 2 command */
#define ST7789_CMD_EQCTRL 0xE9    /**< equalize time control command */
#define ST7789_CMD_PROMCTRL 0xEC  /**< program control command */
#define ST7789_CMD_PROMEN 0xFA    /**< program mode enable command */
#define ST7789_CMD_NVMSET 0xFC    /**< nvm setting command */
#define ST7789_CMD_PROMACT 0xFE   /**< program action command */

static const char *TAG = "[ST7789]";

static spi_device_handle_t spiDeviceHandle;

static uint8_t buffer[ST7789_BUFFER_SIZE];

static uint8_t WriteByte(uint8_t data, uint32_t cmd);
static uint8_t WriteBytes(uint8_t *data, uint16_t len, uint32_t cmd);
static void SpiPreTransferCallback(spi_transaction_t *t);

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
void LCD_Initialize(void)
{
    uint8_t reg;
    uint8_t positiveVoltageControl[] = {0xf0, 0x05, 0x0a, 0x06, 0x06, 0x03, 0x2b, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2b, 0x32};
    uint8_t negativeVoltageControl[] = {0xf0, 0x08, 0x0c, 0x0b, 0x09, 0x24, 0x2b, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2f, 0x37};

    ESP_LOGI(TAG, "Initialize ST7789...");
    ST7789_Initialize(SPI2_HOST);

    ESP_LOGI(TAG, "Software reset...");
    ST7789_SoftwareReset();

    /* sleep out */
    ESP_LOGI(TAG, "Setting sleep mode out");
    ST7789_SleepOut();

    /* idle mode off */
    ESP_LOGI(TAG, "Setting idle mode off");
    ST7789_IdleModeOff();

    ESP_LOGI(TAG, "Setting display mode on");
    ST7789_NormalDisplayModeOn();

    ESP_LOGI(TAG, "Setting display inversion on");
    ST7789_DisplayInversionOn();

    ESP_LOGI(TAG, "Setting gamma");
    ST7789_SetGamma(ST7789_GAMMA_CURVE_1);

    ESP_LOGI(TAG, "Setting memory data access control");
    ST7789_SetMemoryDataAccessControl(ST7789_ORDER_PAGE_TOP_TO_BOTTOM |
                                      ST7789_ORDER_COLUMN_RIGHT_TO_LEFT |
                                      ST7789_ORDER_PAGE_COLUMN_REVERSE |
                                      ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                      ST7789_ORDER_COLOR_RGB |
                                      ST7789_ORDER_REFRESH_LEFT_TO_RIGHT);

    ESP_LOGI(TAG, "Setting interface pixel format");
    ST7789_SetInterfacePixelFormat(ST7789_RGB_INTERFACE_COLOR_FORMAT_65K,
                                   ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT);

    ESP_LOGI(TAG, "Setting display brightness");
    ST7789_SetDisplayBrightness(0xFF);

    ESP_LOGI(TAG, "Setting display control");
    ST7789_SetDisplayControl(ST7789_BOOL_FALSE,
                             ST7789_BOOL_FALSE,
                             ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting brightness control and color enhancement");
    ST7789_SetBrightnessControlAndColorEnhancement(ST7789_BOOL_TRUE,
                                                   ST7789_COLOR_ENHANCEMENT_MODE_USER_INTERFACE,
                                                   ST7789_COLOR_ENHANCEMENT_LEVEL_HIGH);

    ESP_LOGI(TAG, "Setting cabc minimum bightness");
    ST7789_SetCabcMinimumBrightness(0x00);

    ESP_LOGI(TAG, "Setting RAM control");
    ST7789_SetRamControl(ST7789_RAM_ACCESS_MCU,
                         ST7789_DISPLAY_MODE_MCU,
                         ST7789_FRAME_TYPE_0,
                         ST7789_DATA_MODE_MSB,
                         ST7789_RGB_BUS_WIDTH_18_BIT,
                         ST7789_PIXEL_TYPE_0);

    ESP_LOGI(TAG, "Setting porch");
    ST7789_SetPorch(0x0C,
                    0x0C,
                    ST7789_BOOL_FALSE,
                    0x03,
                    0x03,
                    0x03,
                    0x03);

    ESP_LOGI(TAG, "Setting frame rate control");
    ST7789_SetFrameRateControl(ST7789_BOOL_FALSE,
                               ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_1,
                               ST7789_INVERSION_IDLE_MODE_DOT,
                               0x0F,
                               ST7789_INVERSION_PARTIAL_MODE_DOT,
                               0x0F);

    ESP_LOGI(TAG, "Setting partial mode control");
    ST7789_SetPartialModeControl(ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V63,
                                 ST7789_NON_DISPLAY_AREA_SCAN_MODE_NORMAL,
                                 ST7789_NON_DISPLAY_FRAME_FREQUENCY_EVERY);

    ESP_LOGI(TAG, "Setting gate control");
    ST7789_SetGateControl(ST7789_VGHS_14P97_V, ST7789_VGLS_NEGATIVE_8P23);

    ESP_LOGI(TAG, "Setting digital gamma");
    ST7789_SetDigitalGamma(ST7789_BOOL_TRUE);


    ESP_LOGI(TAG, "Setting lcm control");
    ST7789_SetLcmControl(ST7789_BOOL_FALSE,
                         ST7789_BOOL_TRUE,
                         ST7789_BOOL_FALSE,
                         ST7789_BOOL_TRUE,
                         ST7789_BOOL_TRUE,
                         ST7789_BOOL_FALSE,
                         ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting vdv vrh from");
    ST7789_SetVdvVrhFrom(ST7789_VDV_VRH_FROM_CMD);

    ESP_LOGI(TAG, "Setting vhrs convert to register");
    ST7789_VrhsConvertToRegister(5.1f, &reg);

    ESP_LOGI(TAG, "Setting vhrs");
    ST7789_SetVrhs(reg);

    ESP_LOGI(TAG, "Setting vdv convert to register");
    ST7789_VdvConvertToRegister(0.0f, &reg);

    ESP_LOGI(TAG, "Setting vdv");
    ST7789_SetVdv(reg);

    ESP_LOGI(TAG, "Setting vcoms offset convert to register");
    ST7789_VcomsOffsetConvertToRegister(0.2f, &reg);

    ESP_LOGI(TAG, "Setting vcoms offset");
    ST7789_SetVcomsOffset(reg);

    ESP_LOGI(TAG, "Setting frame rate");
    ST7789_SetFrameRate(ST7789_INVERSION_SELECTION_DOT, ST7789_FRAME_RATE_53_HZ);

    ESP_LOGI(TAG, "Setting cabc control");
    ST7789_SetCabcControl(ST7789_BOOL_FALSE,
                          ST7789_BOOL_FALSE,
                          ST7789_BOOL_FALSE,
                          ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting pwm frequency");
    ST7789_SetPwmFrequency(ST7789_PWM_FREQUENCY_9P8_KHZ);

    ESP_LOGI(TAG, "Setting power control 1");
    ST7789_SetPowerControl1(ST7789_AVDD_6P8_V,
                            ST7789_AVCL_NEGTIVE_4P8_V,
                            ST7789_VDS_2P3_V);

    ESP_LOGI(TAG, "Setting positive voltage gamma control");
    ST7789_SetPositiveVoltageGammaControl(positiveVoltageControl);

    ESP_LOGI(TAG, "Setting negative voltage gamma control");
    ST7789_SetNegativeVoltageGammaControl(negativeVoltageControl);

    ESP_LOGI(TAG, "Setting gate line convert to register");
    ST7789_GateLineConvertToRegister(320, &reg);

    ESP_LOGI(TAG, "Setting gate");
    ST7789_SetGate(reg,
                   0x00,
                   ST7789_GATE_SCAN_MODE_INTERLACE,
                   ST7789_GATE_SCAN_DIRECTION_0_319);

    ESP_LOGI(TAG, "Setting power control 2");
    ST7789_SetPowerControl2(ST7789_SBCLK_DIV_3, ST7789_STP14CK_DIV_6);
}

/**
 * @brief     initialize the chip
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 spi initialization failed
 *            - 2 handle is NULL
 *            - 3 linked functions is NULL
 *            - 4 reset failed
 *            - 5 command && data init failed
 * @note      none
 */
void ST7789_Initialize(spi_host_device_t host)
{
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = SPI_CLOCK_FREQUENCY_HZ, // Clock out at 10 MHz
        .mode = 0,                                // SPI mode 0
        .spics_io_num = GPIO_PIN_NUM_CS,          // CS pin
        .queue_size = 7,                          // We want to be able to queue 7 transactions at a time
        .pre_cb = SpiPreTransferCallback          // Specify pre-transfer callback to handle D/C line
    };

    gpio_config_t gpiosConf = {.intr_type = GPIO_INTR_DISABLE,
                               .mode = GPIO_MODE_OUTPUT,
                               .pin_bit_mask = (1 << GPIO_PIN_NUM_DC) | (1 << GPIO_PIN_NUM_RST) | (1 << GPIO_PIN_NUM_BCKL),
                               .pull_down_en = GPIO_PULLDOWN_DISABLE,
                               .pull_up_en = GPIO_PULLUP_DISABLE};

    gpio_config(&gpiosConf);

    gpio_set_level(GPIO_PIN_NUM_BCKL, 1);

    gpio_set_level(GPIO_PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_PIN_NUM_RST, 1);

    vTaskDelay(pdMS_TO_TICKS(120)); /* over 120 ms */

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(spi_bus_add_device(host, &devcfg, &spiDeviceHandle));
}

/**
 * @brief     nop
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 nop failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_Nop()
{
    WriteByte(ST7789_CMD_NOP, ST7789_CMD); /* write nop command */
}

/**
 * @brief     software reset
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 software reset failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SoftwareReset()
{
    WriteByte(ST7789_CMD_SWRESET, ST7789_CMD); /* write software reset command */

    vTaskDelay(pdMS_TO_TICKS(200));
}

/**
 * @brief     sleep in
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 sleep in failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SleepIn()
{
    WriteByte(ST7789_CMD_SLPIN, ST7789_CMD); /* write sleep in command */
}

/**
 * @brief     sleep out
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 sleep out failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SleepOut()
{
    WriteByte(ST7789_CMD_SLPOUT, ST7789_CMD); /* write sleep out command */

    vTaskDelay(pdMS_TO_TICKS(200));
}

/**
 * @brief     partial display mode on
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 partial display mode on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_PartialDisplayModeOn()
{
    WriteByte(ST7789_CMD_PTLON, ST7789_CMD); /* write partial display mode on command */
}

/**
 * @brief     normal display mode on
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 normal display mode on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_NormalDisplayModeOn()
{
    WriteByte(ST7789_CMD_NORON, ST7789_CMD); /* write normal display mode on command */
}

/**
 * @brief     display inversion off
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 display inversion off failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_DisplayInversionOff()
{
    WriteByte(ST7789_CMD_INVOFF, ST7789_CMD); /* write display inversion off command */
}

/**
 * @brief     display inversion on
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 display inversion on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_DisplayInversionOn()
{
    WriteByte(ST7789_CMD_INVON, ST7789_CMD); /* write display inversion on command */
}

/**
 * @brief     set gamma
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] gamma set gamma
 * @return    status code
 *            - 0 success
 *            - 1 set gamma failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetGamma(uint8_t gamma)
{
    WriteByte(ST7789_CMD_GAMSET, ST7789_CMD); /* write set gamma command */
    WriteByte(gamma & 0x0F, ST7789_DATA);     /* write gamma data */
}

/**
 * @brief     display off
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 display off failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void LCD_DisplayOff()
{
    WriteByte(ST7789_CMD_DISPOFF, ST7789_CMD); /* write display off command */
}

/**
 * @brief     display on
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 display on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void LCD_DisplayOn()
{
    WriteByte(ST7789_CMD_DISPON, ST7789_CMD); /* write display on command */
}

/**
 * @brief     set the column address
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] start_address start address
 * @param[in] end_address end address
 * @return    status code
 *            - 0 success
 *            - 1 set column address failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 address is invalid
 *            - 5 start_address >= end_address
 * @note      start_address <= 319 && end_address <= 319 && start_address >= start_address
 */
void ST7789_SetColumnAddress(uint16_t start_address, uint16_t end_address)
{
    uint8_t buf[4];

    WriteByte(ST7789_CMD_CASET, ST7789_CMD); /* write set column address command */
    buf[0] = (start_address >> 8) & 0xFF;    /* start address msb */
    buf[1] = (start_address >> 0) & 0xFF;    /* start address lsb */
    buf[2] = (end_address >> 8) & 0xFF;      /* end address msb */
    buf[3] = (end_address >> 0) & 0xFF;      /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */
}

/**
 * @brief     set the row address
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] start_address start address
 * @param[in] end_address end address
 * @return    status code
 *            - 0 success
 *            - 1 set row address failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 address is invalid
 *            - 5 start_address >= end_address
 * @note      start_address <= 319 && end_address <= 319 && start_address >= start_address
 */
void ST7789_SetRowAddress(uint16_t start_address, uint16_t end_address)
{
    uint8_t buf[4];

    WriteByte(ST7789_CMD_RASET, ST7789_CMD); /* write set row address command */
    buf[0] = (start_address >> 8) & 0xFF;    /* start address msb */
    buf[1] = (start_address >> 0) & 0xFF;    /* start address lsb */
    buf[2] = (end_address >> 8) & 0xFF;      /* end address msb */
    buf[3] = (end_address >> 0) & 0xFF;      /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */
}

/**
 * @brief     memory write
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *data pointer to a data buffer
 * @param[in] len data length
 * @return    status code
 *            - 0 success
 *            - 1 memory write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_MemoryWrite(uint8_t *data, uint16_t len)
{
    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */
    WriteBytes(data, len, ST7789_DATA);      /* write data */
}

/**
 * @brief     set partial areas
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] start_row start row
 * @param[in] end_row end row
 * @return    status code
 *            - 0 success
 *            - 1 set partial areas failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPartialAreas(uint16_t start_row, uint16_t end_row)
{
    uint8_t buf[4];

    WriteByte(ST7789_CMD_PTLAR, ST7789_CMD); /* write set partial areas command */
    buf[0] = (start_row >> 8) & 0xFF;        /* start row msb */
    buf[1] = (start_row >> 0) & 0xFF;        /* start row lsb */
    buf[2] = (end_row >> 8) & 0xFF;          /* end row msb */
    buf[3] = (end_row >> 0) & 0xFF;          /* end row lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */
}

/**
 * @brief     set vertical scrolling
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] top_fixed_area top fixed area line
 * @param[in] scrolling_area scrolling area line
 * @param[in] bottom_fixed_area bottom fixed area line
 * @return    status code
 *            - 0 success
 *            - 1 set vertical scrolling failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetVerticalScrolling(uint16_t top_fixed_area,
                                 uint16_t scrolling_area, uint16_t bottom_fixed_area)
{
    uint8_t buf[6];

    WriteByte(ST7789_CMD_VSCRDEF, ST7789_CMD); /* write set vertical scrolling definition command */
    buf[0] = (top_fixed_area >> 8) & 0xFF;     /* top fixed area msb */
    buf[1] = (top_fixed_area >> 0) & 0xFF;     /* top fixed area lsb */
    buf[2] = (scrolling_area >> 8) & 0xFF;     /* scrolling area msb */
    buf[3] = (scrolling_area >> 0) & 0xFF;     /* scrolling area lsb */
    buf[4] = (bottom_fixed_area >> 8) & 0xFF;  /* bottom fixed area msb */
    buf[5] = (bottom_fixed_area >> 0) & 0xFF;  /* bottom fixed area lsb */
    WriteBytes(buf, 6, ST7789_DATA);           /* write data */
}

/**
 * @brief     tearing effect line off
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 tearing effect line off failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_TearingEffectLineOff()
{
    WriteByte(ST7789_CMD_TEOFF, ST7789_CMD); /* write tearing effect line off command */
}

/**
 * @brief     tearing effect line on
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] effect tearing effect
 * @return    status code
 *            - 0 success
 *            - 1 tearing effect line on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_TearingEffectLineOn(ST7789_tearing_effect_t effect)
{
    WriteByte(ST7789_CMD_TEON, ST7789_CMD); /* write tearing effect line off command */
    WriteByte(effect, ST7789_DATA);         /* write data */
}

/**
 * @brief     set memory data access control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] order memory data access control order
 * @return    status code
 *            - 0 success
 *            - 1 set memory data access control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetMemoryDataAccessControl(uint8_t order)
{
    WriteByte(ST7789_CMD_MADCTL, ST7789_CMD); /* write set memory data access control command */
    WriteByte(order, ST7789_DATA);            /* write data */
}

/**
 * @brief     set the vertical scroll start address
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] start_address start address
 * @return    status code
 *            - 0 success
 *            - 1 set vertical scroll start address failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 address is invalid
 * @note      none
 */
void ST7789_SetVerticalScrollStartAddress(uint16_t start_address)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_VSCRSADD, ST7789_CMD); /* write vertical scrolling start address command */
    buf[0] = (start_address >> 8) & 0xFF;       /* start address msb */
    buf[1] = (start_address >> 0) & 0xFF;       /* start address lsb */
    WriteBytes(buf, 2, ST7789_DATA);            /* write data */
}

/**
 * @brief     idle mode off
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 idle mode off failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_IdleModeOff()
{
    WriteByte(ST7789_CMD_IDMOFF, ST7789_CMD); /* write idle mode off command */
}

/**
 * @brief     idle mode on
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 idle mode on failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_IdleModeOn()
{
    WriteByte(ST7789_CMD_IDMON, ST7789_CMD); /* write idle mode on command */
}

/**
 * @brief     set interface pixel format
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] rgb rgb interface color format
 * @param[in] control control interface color format
 * @return    status code
 *            - 0 success
 *            - 1 set interface pixel format failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetInterfacePixelFormat(ST7789_rgb_interface_color_format_t rgb,
                                    ST7789_control_interface_color_format_t control)
{
    uint8_t data;

    WriteByte(ST7789_CMD_COLMOD, ST7789_CMD); /* write interface pixel format command */

    data = (rgb << 4) | (control << 0); /* set pixel format */
    WriteByte(data, ST7789_DATA);       /* write data */
}

/**
 * @brief     memory continue write
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *data pointer to a data buffer
 * @param[in] len data length
 * @return    status code
 *            - 0 success
 *            - 1 memory continue write failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_MemoryContinueWrite(uint8_t *data, uint16_t len)
{
    WriteByte(ST7789_CMD_RAMWRC, ST7789_CMD); /* write memory write continue command */
    WriteBytes(data, len, ST7789_DATA);       /* write data */
}

/**
 * @brief     set tear scanline
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] l tear line
 * @return    status code
 *            - 0 success
 *            - 1 set tear scanline failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetTearScanline(uint16_t l)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_TESCAN, ST7789_CMD); /* write set tear scanline command */
    buf[0] = (l >> 8) & 0xFF;                 /* start line msb */
    buf[1] = (l >> 0) & 0xFF;                 /* start line lsb */
    WriteBytes(buf, 2, ST7789_DATA);          /* write data */
}

/**
 * @brief     set display brightness
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] brightness display brightness
 * @return    status code
 *            - 0 success
 *            - 1 set display brightness failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetDisplayBrightness(uint8_t brightness)
{
    WriteByte(ST7789_CMD_WRDISBV, ST7789_CMD); /* write display brightness command */

    WriteByte(brightness, ST7789_DATA); /* write brightness */
}

/**
 * @brief     set display control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] brightness_control_block bool value
 * @param[in] display_dimming bool value
 * @param[in] backlight_control bool value
 * @return    status code
 *            - 0 success
 *            - 1 set display control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetDisplayControl(ST7789_bool_t brightness_control_block,
                              ST7789_bool_t display_dimming, ST7789_bool_t backlight_control)
{
    uint8_t data;

    WriteByte(ST7789_CMD_WRCTRLD, ST7789_CMD); /* write CTRL display command */

    data = (brightness_control_block << 5) | (display_dimming << 3) | (backlight_control << 2); /* set control data */
    WriteByte(data, ST7789_DATA);                                                               /* write control */
}

/**
 * @brief     set brightness control and color enhancement
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] color_enhancement bool value
 * @param[in] mode color enhancement mode
 * @param[in] level color enhancement level
 * @return    status code
 *            - 0 success
 *            - 1 set brightness control and color enhancement failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetBrightnessControlAndColorEnhancement(ST7789_bool_t color_enhancement,
                                                    ST7789_color_enhancement_mode_t mode, ST7789_color_enhancement_level_t level)
{
    uint8_t data;

    WriteByte(ST7789_CMD_WRCACE, ST7789_CMD); /* write content adaptive brightness control and color enhancement command */

    data = (color_enhancement << 7) | (level << 4) | (mode << 0); /* set control data */
    WriteByte(data, ST7789_DATA);                                 /* write control */
}

/**
 * @brief     set cabc minimum brightness
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] brightness display brightness
 * @return    status code
 *            - 0 success
 *            - 1 set cabc minimum brightness failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetCabcMinimumBrightness(uint8_t brightness)
{
    WriteByte(ST7789_CMD_WRCABCMB, ST7789_CMD); /* write CABC minimum brightness command */

    WriteByte(brightness, ST7789_DATA); /* write brightness */
}

/**
 * @brief     set ram control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] ram_mode ram mode
 * @param[in] display_mode display mode
 * @param[in] frame_type frame type
 * @param[in] data_mode data mode
 * @param[in] bus_width bus width
 * @param[in] pixel_type pixel type
 * @return    status code
 *            - 0 success
 *            - 1 set ram control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetRamControl(
    ST7789_ram_access_t ram_mode,
    ST7789_display_mode_t display_mode,
    ST7789_frame_type_t frame_type,
    ST7789_data_mode_t data_mode,
    ST7789_rgb_bus_width_t bus_width,
    ST7789_pixel_type_t pixel_type)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_RAMCTRL, ST7789_CMD);      /* write ram control command */
    buf[0] = (ram_mode << 4) | (display_mode << 0); /* set param1 */
    buf[1] = (frame_type << 4) | (data_mode << 3) |
             (bus_width << 2) | (pixel_type << 0); /* set param2 */
    WriteBytes(buf, 2, ST7789_DATA);               /* write data */
}

/**
 * @brief     set porch
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] back_porch_normal back porch setting in normal mode
 * @param[in] front_porch_normal front porch setting in normal mode
 * @param[in] separate_porch_enable bool value
 * @param[in] back_porch_idle back porch setting in idle mode
 * @param[in] front_porch_idle front porch setting in idle mode
 * @param[in] back_porch_partial back porch setting in partial mode
 * @param[in] front_porch_partial front porch setting in partial mode
 * @return    status code
 *            - 0 success
 *            - 1 set porch failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 back_porch_normal > 0x7F
 *            - 5 front_porch_normal > 0x7F
 *            - 6 back_porch_idle > 0xF
 *            - 7 front_porch_idle > 0xF
 *            - 8 back_porch_partial > 0xF
 *            - 9 front_porch_partial > 0xF
 * @note      0x01 <= back_porch_normal <= 0x7F
 *            0x01 <= front_porch_normal <= 0x7F
 *            0x01 <= back_porch_idle <= 0xF
 *            0x01 <= front_porch_idle <= 0xF
 *            0x01 <= back_porch_partial <= 0xF
 *            0x01 <= front_porch_partial <= 0xF
 */
void ST7789_SetPorch(uint8_t back_porch_normal,
                     uint8_t front_porch_normal,
                     ST7789_bool_t separate_porch_enable,
                     uint8_t back_porch_idle,
                     uint8_t front_porch_idle,
                     uint8_t back_porch_partial,
                     uint8_t front_porch_partial)
{
    uint8_t buf[5];

    WriteByte(ST7789_CMD_PORCTRL, ST7789_CMD);                              /* write porch control command */
    buf[0] = back_porch_normal;                                             /* set param1 */
    buf[1] = front_porch_normal;                                            /* set param2 */
    buf[2] = separate_porch_enable;                                         /* set param3 */
    buf[3] = (back_porch_idle & 0xF) << 4 | (front_porch_idle & 0xF);       /* set param4 */
    buf[4] = (back_porch_partial & 0xF) << 4 | (front_porch_partial & 0xF); /* set param5 */
    WriteBytes(buf, 5, ST7789_DATA);                                        /* write data */
}

/**
 * @brief     set frame rate control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] separate_fr_control bool value
 * @param[in] div_control frame rate divided control
 * @param[in] idle_mode inversion idle mode
 * @param[in] idle_frame_rate idle frame rate
 * @param[in] partial_mode inversion partial mode
 * @param[in] partial_frame_rate partial frame rate
 * @return    status code
 *            - 0 success
 *            - 1 set frame rate control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 idle_frame_rate > 0x1F
 *            - 5 partial_frame_rate > 0x1F
 * @note      0 <= idle_frame_rate <= 0x1F
 *            0 <= partial_frame_rate <= 0x1F
 */
void ST7789_SetFrameRateControl(ST7789_bool_t separate_fr_control,
                                ST7789_frame_rate_divided_control_t div_control,
                                ST7789_inversion_idle_mode_t idle_mode,
                                uint8_t idle_frame_rate,
                                ST7789_inversion_partial_mode_t partial_mode,
                                uint8_t partial_frame_rate)
{
    uint8_t buf[3];

    WriteByte(ST7789_CMD_FRCTRL1, ST7789_CMD);                         /* write frame rate control 1 command */
    buf[0] = (separate_fr_control << 4) | (div_control << 0);          /* set param1 */
    buf[1] = (idle_mode << 5) | ((idle_frame_rate & 0x1F) << 0);       /* set param2 */
    buf[2] = (partial_mode << 5) | ((partial_frame_rate & 0x1F) << 0); /* set param3 */
    WriteBytes(buf, 3, ST7789_DATA);                                   /* write data */
}

/**
 * @brief     set partial mode control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] level non-display source output level
 * @param[in] mode non-display area scan mode
 * @param[in] frequency non-display frame frequency
 * @return    status code
 *            - 0 success
 *            - 1 set partial mode control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPartialModeControl(
    ST7789_non_display_source_output_level_t level,
    ST7789_non_display_area_scan_mode_t mode,
    ST7789_non_display_frame_frequency_t frequency)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PARCTRL, ST7789_CMD);           /* write partial mode control command */
    reg = (level << 7) | (mode << 4) | (frequency << 0); /* set param */
    WriteByte(reg, ST7789_DATA);                         /* write data */
}

/**
 * @brief     set gate control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] vghs vghs level
 * @param[in] vgls vgls level
 * @return    status code
 *            - 0 success
 *            - 1 set gate control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetGateControl(ST7789_vghs_t vghs, ST7789_vgls_t vgls)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_GCTRL, ST7789_CMD); /* write gate control command */
    reg = (vghs << 4) | (vgls << 0);         /* set param */
    WriteByte(reg, ST7789_DATA);             /* write data */
}

/**
 * @brief     enable or disable digital gamma
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] enable bool value
 * @return    status code
 *            - 0 success
 *            - 1 set digital gamma failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetDigitalGamma(ST7789_bool_t enable)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_DGMEN, ST7789_CMD); /* write digital gamma enable command */
    reg = enable << 2;                       /* set param */
    WriteByte(reg, ST7789_DATA);             /* write data */
}

/**
 * @brief     set vcoms
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] vcoms set vcoms
 * @return    status code
 *            - 0 success
 *            - 1 set vcoms failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 vcoms > 0x3F
 * @note      0 <= vcoms <= 0x3F
 */
void ST7789_SetVcoms(uint8_t vcoms)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VCOMS, ST7789_CMD); /* write vcoms setting command */
    reg = vcoms & 0x3F;                      /* set param */
    WriteByte(reg, ST7789_DATA);             /* write data */
}

/**
 * @brief      convert the vcom to the register raw data
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  v vcom
 * @param[out] *reg pointer to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VcomConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v - 0.1f) / 0.025f); /* convert real data to register data */
}

/**
 * @brief      convert the register raw data to the vcom
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  reg register raw data
 * @param[out] *v pointer to a vcom buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VcomConvertToData(uint8_t reg, float *v)
{
    *v = (uint8_t)((float)(reg) * 0.025f + 0.1f); /* convert raw data to real data */
}

/**
 * @brief     set lcm control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] xmy bool value
 * @param[in] xbgr bool value
 * @param[in] xinv bool value
 * @param[in] xmx bool value
 * @param[in] xmh bool value
 * @param[in] xmv bool value
 * @param[in] xgs bool value
 * @return    status code
 *            - 0 success
 *            - 1 set lcm control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetLcmControl(
    ST7789_bool_t xmy,
    ST7789_bool_t xbgr,
    ST7789_bool_t xinv,
    ST7789_bool_t xmx,
    ST7789_bool_t xmh,
    ST7789_bool_t xmv,
    ST7789_bool_t xgs)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_LCMCTRL, ST7789_CMD); /* write lcm control command */
    reg = (xmy << 6) | (xbgr << 5) | (xinv << 4) | (xmx << 3) |
          (xmh << 2) | (xmv << 1) | (xgs << 0); /* set param */
    WriteByte(reg, ST7789_DATA);                /* write data */
}

/**
 * @brief     set id code setting
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *id pointer to an id buffer
 * @return    status code
 *            - 0 success
 *            - 1 set id code setting failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetIdCodeSetting(uint8_t id[3])
{
    WriteByte(ST7789_CMD_IDSET, ST7789_CMD); /* write id setting command */
    WriteBytes(id, 3, ST7789_DATA);          /* write data */
}

/**
 * @brief     set vdv vrh from
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] from vdv and vrh from
 * @return    status code
 *            - 0 success
 *            - 1 set vdv vrh from failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetVdvVrhFrom(ST7789_vdv_vrh_from_t from)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_VDVVRHEN, ST7789_CMD); /* write vdv and vrh command enable command */
    buf[0] = from;                              /* set param1 */
    buf[1] = 0xFF;                              /* set param2 */
    WriteBytes(buf, 2, ST7789_DATA);            /* write data */
}

/**
 * @brief     set vrhs
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] vrhs set vrhs
 * @return    status code
 *            - 0 success
 *            - 1 set vrhs failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 vrhs > 0x27
 * @note      0 <= vrhs <= 0x27
 */
void ST7789_SetVrhs(uint8_t vrhs)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VRHS, ST7789_CMD); /* write vrh set command */
    reg = vrhs & 0x3F;                      /* set param */
    WriteByte(reg, ST7789_DATA);            /* write data */
}

/**
 * @brief      convert the vrhs to the register raw data
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  v vcom
 * @param[out] *reg pointer to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VrhsConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v - 3.55f) / 0.05f); /* convert real data to register data */
}

/**
 * @brief      convert the register raw data to the vrhs
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  reg register raw data
 * @param[out] *v pointer to a vcom buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VrhsConvertToData(uint8_t reg, float *v)
{
    *v = (uint8_t)((float)(reg) * 0.05f + 3.55f); /* convert raw data to real data */
}

/**
 * @brief     set vdv
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] vdv set vdv
 * @return    status code
 *            - 0 success
 *            - 1 set vdv failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 vdv > 0x3F
 * @note      0 <= vdv <= 0x3F
 */
void ST7789_SetVdv(uint8_t vdv)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VDVSET, ST7789_CMD); /* write vdv setting command */
    reg = vdv & 0x3F;                         /* set param */
    WriteByte(reg, ST7789_DATA);              /* write data */
}

/**
 * @brief      convert the vdv to the register raw data
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  v vcom
 * @param[out] *reg pointer to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VdvConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v + 0.8f) / 0.025f); /* convert real data to register data */
}

/**
 * @brief      convert the register raw data to the vdv
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  reg register raw data
 * @param[out] *v pointer to a vcom buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VdvConvertToData(uint8_t reg, float *v)
{
    *v = (uint8_t)((float)(reg) * 0.025f - 0.8f); /* convert raw data to real data */
}

/**
 * @brief     set vcoms offset
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] offset vcoms offset
 * @return    status code
 *            - 0 success
 *            - 1 set vcoms offset failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 offset > 0x3F
 * @note      0 <= offset <= 0x3F
 */
void ST7789_SetVcomsOffset(uint8_t offset)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VCMOFSET, ST7789_CMD); /* write vcoms offset set command */
    reg = offset & 0x3F;                        /* set param */
    WriteByte(reg, ST7789_DATA);                /* write data */
}

/**
 * @brief      convert the vcoms offset to the register raw data
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  v vcoms offset
 * @param[out] *reg pointer to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VcomsOffsetConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v + 0.8f) / 0.025f); /* convert real data to register data */
}

/**
 * @brief      convert the register raw data to the vcoms offset
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  reg register raw data
 * @param[out] *v pointer to a vcoms offset buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_VcomsOffsetConvertToData(uint8_t reg, float *v)
{
    *v = (uint8_t)((float)(reg) * 0.025f - 0.8f); /* convert raw data to real data */
}

/**
 * @brief     set frame rate
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] selection inversion selection
 * @param[in] rate frame rate
 * @return    status code
 *            - 0 success
 *            - 1 set frame rate failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetFrameRate(ST7789_inversion_selection_t selection, ST7789_frame_rate_t rate)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_FRCTR2, ST7789_CMD); /* write fr control 2 command */
    reg = (selection << 5) | (rate << 0);     /* set param */
    WriteByte(reg, ST7789_DATA);              /* write data */
}

/**
 * @brief     set cabc control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] led_on bool value
 * @param[in] led_pwm_init bool value
 * @param[in] led_pwm_fix bool value
 * @param[in] led_pwm_polarity bool value
 * @return    status code
 *            - 0 success
 *            - 1 set cabc control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetCabcControl(ST7789_bool_t led_on,
                           ST7789_bool_t led_pwm_init,
                           ST7789_bool_t led_pwm_fix,
                           ST7789_bool_t led_pwm_polarity)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_CABCCTRL, ST7789_CMD); /* write cabc control command */
    reg = (led_on << 3) | (led_pwm_init << 2) |
          (led_pwm_fix << 1) | (led_pwm_polarity << 0); /* set param */
    WriteByte(reg, ST7789_DATA);                        /* write data */
}

/**
 * @brief     set pwm frequency
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] frequency pwm frequency
 * @return    status code
 *            - 0 success
 *            - 1 set pwm frequency failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPwmFrequency(ST7789_pwm_frequency_t frequency)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PWMFRSEL, ST7789_CMD); /* write pwm frequency selection command */
    reg = frequency;                            /* set param */
    WriteByte(reg, ST7789_DATA);                /* write data */
}

/**
 * @brief     set power control 1
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] avdd avdd param
 * @param[in] avcl avcl param
 * @param[in] vds vds param
 * @return    status code
 *            - 0 success
 *            - 1 set power control 1 failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPowerControl1(ST7789_avdd_t avdd, ST7789_avcl_t avcl, ST7789_vds_t vds)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_PWCTRL1, ST7789_CMD);       /* write power control 1 command */
    buf[0] = 0xA4;                                   /* set param 1 */
    buf[1] = (avdd << 6) | (avcl << 4) | (vds << 0); /* set param 2 */
    WriteBytes(buf, 2, ST7789_DATA);                 /* write data */
}

/**
 * @brief     set positive voltage gamma control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *param pointer to a param buffer
 * @return    status code
 *            - 0 success
 *            - 1 set positive voltage gamma control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPositiveVoltageGammaControl(uint8_t param[14])
{
    WriteByte(ST7789_CMD_PVGAMCTRL, ST7789_CMD); /* write positive voltage gamma control command */
    WriteBytes(param, 14, ST7789_DATA);          /* write data */
}

/**
 * @brief     set negative voltage gamma control
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *param pointer to a param buffer
 * @return    status code
 *            - 0 success
 *            - 1 set negative voltage gamma control failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetNegativeVoltageGammaControl(uint8_t param[14])
{
    WriteByte(ST7789_CMD_NVGAMCTRL, ST7789_CMD); /* write negative voltage gamma control command */
    WriteBytes(param, 14, ST7789_DATA);          /* write data */
}

/**
 * @brief     set blue digital gamma look up table
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *param pointer to a param buffer
 * @return    status code
 *            - 0 success
 *            - 1 set digital gamma look up table blue failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetDigitalGammaLookUpTableBlue(uint8_t param[64])
{
    WriteByte(ST7789_CMD_DGMLUTB, ST7789_CMD); /* write digital gamma look-up table for blue command */
    WriteBytes(param, 64, ST7789_DATA);        /* write data */
}

/**
 * @brief     set gate
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] gate_line_number gate line number
 * @param[in] first_scan_line_number first scan line number
 * @param[in] mode gate scan mode
 * @param[in] direction gate scan direction
 * @return    status code
 *            - 0 success
 *            - 1 set gate failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 gate_line_number > 0x3F
 *            - 5 first_scan_line_number > 0x3F
 * @note      0 <= gate_line_number <= 0x3F
 *            0 <= first_scan_line_number 0x3F
 */
void ST7789_SetGate(uint8_t gate_line_number,
                    uint8_t first_scan_line_number,
                    ST7789_gate_scan_mode_t mode,
                    ST7789_gate_scan_direction_t direction)
{
    uint8_t buf[3];

    WriteByte(ST7789_CMD_GATECTRL, ST7789_CMD);     /* write gate control command */
    buf[0] = gate_line_number;                      /* set param 1 */
    buf[1] = first_scan_line_number;                /* set param 2 */
    buf[2] = 0x10 | (mode << 2) | (direction << 0); /* set param 3 */
    WriteBytes(buf, 3, ST7789_DATA);                /* write data */
}

/**
 * @brief      convert the gate line to the register raw data
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  l gate line
 * @param[out] *reg pointer to a register raw buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_GateLineConvertToRegister(uint16_t l, uint8_t *reg)
{
    *reg = (uint8_t)((l / 8) - 1); /* convert real data to register data */
}

/**
 * @brief      convert the register raw data to the gate line
 * @param[in]  *handle pointer to an st7789 handle structure
 * @param[in]  reg register raw data
 * @param[out] *l pointer to a gate line buffer
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 *             - 3 handle is not initialized
 * @note       none
 */
void ST7789_GateLineConvertToData(uint8_t reg, uint16_t *l)
{
    *l = (uint8_t)(reg * 8 + 8); /* convert raw data to real data */
}

/**
 * @brief     set power control 2
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] sbclk sbclk div
 * @param[in] stp14ck stp14ck div
 * @return    status code
 *            - 0 success
 *            - 1 set power control 2 failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 * @note      none
 */
void ST7789_SetPowerControl2(ST7789_sbclk_div_t sbclk, ST7789_stp14ck_div_t stp14ck)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PWCTRL2, ST7789_CMD); /* write power control 2 command */
    reg = (sbclk << 4) | (stp14ck << 0);       /* set param */
    WriteByte(reg, ST7789_DATA);               /* write data */
}

/**
 * @brief     close the chip
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 spi deinit failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 power down failed
 *            - 5 reset gpio deinit failed
 *            - 6 command && data deinit failed
 * @note      none
 */
void ST7789_Deinitialize()
{
    spi_bus_remove_device(spiDeviceHandle);
}

/**
 * @brief     clear the display
 * @param[in] *handle pointer to an st7789 handle structure
 * @return    status code
 *            - 0 success
 *            - 1 clear failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 format is invalid
 * @note      none
 */
void LCD_Clear(void)
{
    uint8_t buf[4];
    uint32_t i;
    uint32_t m;
    uint32_t n;

    WriteByte(ST7789_CMD_CASET, ST7789_CMD);               /* write set column address command */
    buf[0] = (Y_OFFSET >> 8) & 0xFF;                       /* start address msb */
    buf[1] = (Y_OFFSET >> 0) & 0xFF;                       /* start address lsb */
    buf[2] = ((Y_OFFSET + SCREEN_HEIGHT - 1) >> 8) & 0xFF; /* end address msb */
    buf[3] = ((Y_OFFSET + SCREEN_HEIGHT - 1) >> 0) & 0xFF; /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);                       /* write data */

    WriteByte(ST7789_CMD_RASET, ST7789_CMD);              /* write set row address command */
    buf[0] = (X_OFFSET >> 8) & 0xFF;                      /* start address msb */
    buf[1] = (X_OFFSET >> 0) & 0xFF;                      /* start address lsb */
    buf[2] = ((X_OFFSET + SCREEN_WIDTH - 1) >> 8) & 0xFF; /* end address msb */
    buf[3] = ((X_OFFSET + SCREEN_WIDTH - 1) >> 0) & 0xFF; /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);                      /* write data */

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */

    memset(buffer, 0xFF, sizeof(uint8_t) * ST7789_BUFFER_SIZE);                    /* clear buffer */
    m = (uint32_t)SCREEN_HEIGHT * (uint32_t)SCREEN_WIDTH * 2 / ST7789_BUFFER_SIZE; /* total times */
    n = (uint32_t)SCREEN_HEIGHT * (uint32_t)SCREEN_WIDTH * 2 % ST7789_BUFFER_SIZE; /* the last */

    for (i = 0; i < m; i++)
    {
        WriteBytes(buffer, ST7789_BUFFER_SIZE, ST7789_DATA); /* write data */
    }
    
    if (n != 0) /* not end */
    {
        WriteBytes(buffer, n, ST7789_DATA); /* write data */
    }
}

/**
 * @brief     fill the rect
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] left left coordinate x
 * @param[in] top top coordinate y
 * @param[in] right right coordinate x
 * @param[in] bottom bottom coordinate y
 * @param[in] color display color
 * @return    status code
 *            - 0 success
 *            - 1 fill rect failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 left is over column
 *            - 5 right is over column
 *            - 6 left >= right
 *            - 7 top is over row
 *            - 8 bottom is over row
 *            - 9 top >= bottom
 * @note      left <= column && right <= column && left < right && top <= row && bottom <= row && top < bottom
 */
void LCD_FillRectangle(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint32_t color)
{
    uint8_t buf[4];
    uint32_t i;
    uint32_t m;
    uint32_t n;

    WriteByte(ST7789_CMD_CASET, ST7789_CMD);   /* write set column address command */
    buf[0] = ((Y_OFFSET + left) >> 8) & 0xFF;  /* start address msb */
    buf[1] = ((Y_OFFSET + left) >> 0) & 0xFF;  /* start address lsb */
    buf[2] = ((Y_OFFSET + right) >> 8) & 0xFF; /* end address msb */
    buf[3] = ((Y_OFFSET + right) >> 0) & 0xFF; /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);           /* write data */

    WriteByte(ST7789_CMD_RASET, ST7789_CMD);    /* write set row address command */
    buf[0] = ((X_OFFSET + top) >> 8) & 0xFF;    /* start address msb */
    buf[1] = ((X_OFFSET + top) >> 0) & 0xFF;    /* start address lsb */
    buf[2] = ((X_OFFSET + bottom) >> 8) & 0xFF; /* end address msb */
    buf[3] = ((X_OFFSET + bottom) >> 0) & 0xFF; /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);            /* write data */

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */

    for (i = 0; i < ST7789_BUFFER_SIZE; i += 2) /* fill the buffer */
    {
        buffer[i] = (color >> 8) & 0xFF;     /* set the color */
        buffer[i + 1] = (color >> 0) & 0xFF; /* set the color */
    }

    m = (uint32_t)(right - left + 1) * (bottom - top + 1) * 2 /
        ST7789_BUFFER_SIZE; /* total times */
    n = ((uint32_t)(right - left + 1) * (bottom - top + 1) * 2) %
        ST7789_BUFFER_SIZE; /* the last */

    for (i = 0; i < m; i++)
    {
        WriteBytes(buffer, ST7789_BUFFER_SIZE, ST7789_DATA);
    }

    if (n != 0) /* not end */
    {
        WriteBytes(buffer, n, ST7789_DATA);
    }
}

/**
 * @brief     draw a picture
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] left left coordinate x
 * @param[in] top top coordinate y
 * @param[in] right right coordinate x
 * @param[in] bottom bottom coordinate y
 * @param[in] *image pointer to an image buffer
 * @return    status code
 *            - 0 success
 *            - 1 draw picture 16bits failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 left is over column
 *            - 5 right is over column
 *            - 6 left >= right
 *            - 7 top is over row
 *            - 8 bottom is over row
 *            - 9 top >= bottom
 * @note      left <= column && right <= column && left < right && top <= row && bottom <= row && top < bottom
 */
void LCD_DrawPicture16bits(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t *image)
{
    uint8_t buf[4];
    uint32_t i;
    uint32_t j;
    uint32_t m;
    uint32_t n;
    uint32_t point;
    uint16_t r;
    uint16_t c;
    uint16_t color;

    WriteByte(ST7789_CMD_CASET, ST7789_CMD); /* write set column address command */
    buf[0] = (left >> 8) & 0xFF;             /* start address msb */
    buf[1] = (left >> 0) & 0xFF;             /* start address lsb */
    buf[2] = ((right) >> 8) & 0xFF;          /* end address msb */
    buf[3] = ((right) >> 0) & 0xFF;          /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */

    WriteByte(ST7789_CMD_RASET, ST7789_CMD); /* write set row address command */
    buf[0] = (top >> 8) & 0xFF;              /* start address msb */
    buf[1] = (top >> 0) & 0xFF;              /* start address lsb */
    buf[2] = ((bottom) >> 8) & 0xFF;         /* end address msb */
    buf[3] = ((bottom) >> 0) & 0xFF;         /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */

    c = right - left + 1; /* column */
    r = bottom - top + 1; /* row */
    point = 0;            /* image point init 0 */

    m = ((uint32_t)(right - left + 1) * (bottom - top + 1) * 2) /
        ST7789_BUFFER_SIZE; /* total times */
    n = ((uint32_t)(right - left + 1) * (bottom - top + 1) * 2) %
        ST7789_BUFFER_SIZE; /* the last */

    for (i = 0; i < m; i++)
    {
        for (j = 0; j < ST7789_BUFFER_SIZE; j += 2) /* fill the buffer */
        {
            color = image[(point % c) * r + (point / c)]; /* set color */
            buffer[j] = (color >> 8) & 0xFF;              /* set the color */
            buffer[j + 1] = (color >> 0) & 0xFF;          /* set the color */
            point++;                                      /* point++ */
        }

        WriteBytes(buf, ST7789_BUFFER_SIZE, ST7789_DATA); /* write data */
    }

    if (n != 0) /* not end */
    {
        for (j = 0; j < n; j += 2) /* fill the buffer */
        {
            color = image[(point % c) * r + (point / c)]; /* set color */
            buffer[j] = (color >> 8) & 0xFF;              /* set the color */
            buffer[j + 1] = (color >> 0) & 0xFF;          /* set the color */
            point++;                                      /* point++ */
        }

        WriteBytes(buf, n, ST7789_DATA); /* write data */
    }
}

/**
 * @brief     write a string in the display
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] x coordinate x
 * @param[in] y coordinate y
 * @param[in] *str pointer to a write string address
 * @param[in] len length of the string
 * @param[in] color display color
 * @param[in] font string font
 * @return    status code
 *            - 0 success
 *            - 1 draw point failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 x or y is invalid
 * @note      x < column && y < row
 */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t len, uint32_t color, LCD_Font_t font)
{
    while ((len != 0) && (*str <= '~') && (*str >= ' ')) /* write all string */
    {
        ST7789_DrawChar(x, y, *str, font, color);

        x += (uint8_t)(font / 2); /* x + font/2 */
        str++;                    /* str address++ */
        len--;                    /* str length-- */
    }
}

void LCD_ClearString(uint16_t x, uint16_t y, uint8_t length, uint32_t color, LCD_Font_t font)
{
    switch (font)
    {
    case LCD_FONT_12:
        LCD_FillRectangle(x + 6, y - 6, x + 6 * (length + 1), y + 6, color);
        break;
    case LCD_FONT_16:
        LCD_FillRectangle(x + 8, y - 9, x + 8 * (length + 1), y + 9, color);
        break;
    case LCD_FONT_24:
        LCD_FillRectangle(x + 12, y - 12, x + 12 * (length + 1), y + 12, color);
        break;
    }
}

/**
 * @brief     draw a point in the display
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] x coordinate x
 * @param[in] y coordinate y
 * @param[in] color point color
 * @return    status code
 *            - 0 success
 *            - 1 draw point failed
 *            - 2 handle is NULL
 *            - 3 handle is not initialized
 *            - 4 x is over column
 *            - 5 y is over row
 * @note      x < column && y < row
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    uint8_t buf[4];

    WriteByte(ST7789_CMD_CASET, ST7789_CMD); /* write set column address command */
    buf[0] = ((X_OFFSET + x) >> 8) & 0xFF;   /* start address msb */
    buf[1] = ((X_OFFSET + x) >> 0) & 0xFF;   /* start address lsb */
    buf[2] = ((X_OFFSET + x) >> 8) & 0xFF;   /* end address msb */
    buf[3] = ((X_OFFSET + x) >> 0) & 0xFF;   /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */

    WriteByte(ST7789_CMD_RASET, ST7789_CMD); /* write set row address command */
    buf[0] = ((Y_OFFSET + y) >> 8) & 0xFF;   /* start address msb */
    buf[1] = ((Y_OFFSET + y) >> 0) & 0xFF;   /* start address lsb */
    buf[2] = ((Y_OFFSET + y) >> 8) & 0xFF;   /* end address msb */
    buf[3] = ((Y_OFFSET + y) >> 0) & 0xFF;   /* end address lsb */
    WriteBytes(buf, 4, ST7789_DATA);         /* write data */

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */

    buf[0] = (color >> 8) & 0xFF;    /* set the color */
    buf[1] = (color >> 0) & 0xFF;    /* set the color */
    WriteBytes(buf, 2, ST7789_DATA); /* write data */
}

/**
 * @brief      get chip's information
 * @param[out] *info pointer to an st7789 info structure
 * @return     status code
 *             - 0 success
 *             - 2 handle is NULL
 * @note       none
 */
void ST7789_Info(ST7789_info_t *info)
{
    memset(info, 0, sizeof(ST7789_info_t));                  /* initialize st7789 info structure */
    strncpy(info->chip_name, CHIP_NAME, 32);                 /* copy chip name */
    strncpy(info->manufacturer_name, MANUFACTURER_NAME, 32); /* copy manufacturer name */
    strncpy(info->interface, "SPI", 8);                      /* copy interface name */
    info->supply_voltage_min_v = SUPPLY_VOLTAGE_MIN;         /* set minimal supply voltage */
    info->supply_voltage_max_v = SUPPLY_VOLTAGE_MAX;         /* set maximum supply voltage */
    info->max_current_ma = MAX_CURRENT;                      /* set maximum current */
    info->temperature_max = TEMPERATURE_MAX;                 /* set minimal temperature */
    info->temperature_min = TEMPERATURE_MIN;                 /* set maximum temperature */
    info->driver_version = DRIVER_VERSION;                   /* set driver version */
}

/**
 * @brief     draw a char in the display
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] x coordinate x
 * @param[in] y coordinate y
 * @param[in] chr display char
 * @param[in] size display size
 * @param[in] color display color
 * @return    status code
 *            - 0 success
 *            - 1 show char failed
 * @note      none
 */
void ST7789_DrawChar(uint16_t x, uint16_t y, uint8_t chr, uint8_t size, uint32_t color)
{
    uint8_t temp, t, t1;
    uint16_t y0 = y;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2); /* get size */

    chr = chr - ' ';            /* get index */
    for (t = 0; t < csize; t++) /* write size */
    {
        if (size == 12) /* if size 12 */
        {
            temp = gsc_st7789_ascii_1206[chr][t]; /* get ascii 1206 */
        }
        else if (size == 16) /* if size 16 */
        {
            temp = gsc_st7789_ascii_1608[chr][t]; /* get ascii 1608 */
        }
        else if (size == 24) /* if size 24 */
        {
            temp = gsc_st7789_ascii_2412[chr][t]; /* get ascii 2412 */
        }

        for (t1 = 0; t1 < 8; t1++) /* write one line */
        {
            if ((temp & 0x80) != 0) /* if 1 */
            {
                LCD_DrawPoint(x, y, color);
            }

            temp <<= 1; /* left shift 1 */
            y++;
            if ((y - y0) == size) /* reset size */
            {
                y = y0; /* set y */
                x++;    /* x++ */

                break; /* break */
            }
        }
    }
}

/**
 * @brief     write one byte
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] data written data
 * @param[in] cmd data type
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
static uint8_t WriteByte(uint8_t data, uint32_t cmd)
{
    spi_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = 8;                       // Command is 8 bits
    transaction.tx_buffer = &data;                // The data is the cmd itself
    transaction.user = (void *)cmd;

    return spi_device_polling_transmit(spiDeviceHandle, &transaction);
}

/**
 * @brief     write bytes
 * @param[in] *handle pointer to an st7789 handle structure
 * @param[in] *data pointer to a data buffer
 * @param[in] len data length
 * @param[in] cmd data type
 * @return    status code
 *            - 0 success
 *            - 1 write failed
 * @note      none
 */
static uint8_t WriteBytes(uint8_t *data, uint16_t len, uint32_t cmd)
{
    spi_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = len * 8;                 // Command is 8 bits
    transaction.tx_buffer = data;                 // The data is the cmd itself
    transaction.user = (void *)cmd;

    return spi_device_polling_transmit(spiDeviceHandle, &transaction);
}

static void SpiPreTransferCallback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(GPIO_PIN_NUM_DC, dc);
}