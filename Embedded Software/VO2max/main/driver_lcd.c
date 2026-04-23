#include "driver_lcd.h"
#include "esp_log.h"

static const char *TAG = "[LCD]";

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
uint8_t LCD_Initialize(void)
{
    uint8_t res;
    uint8_t reg;
    uint8_t positiveVoltageControl[] = {0xf0, 0x05, 0x0a, 0x06, 0x06, 0x03, 0x2b, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2b, 0x32};
    uint8_t negativeVoltageControl[] = {0xf0, 0x08, 0x0c, 0x0b, 0x09, 0x24, 0x2b, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2f, 0x37};

    ESP_LOGI(TAG, "Initialize ST7789...");
    res = ST7789_Initialize(SPI2_HOST);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: init failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Software reset...");
    res = ST7789_SoftwareReset();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: software reset failed.");

        return 1;
    }

    /* sleep out */
    ESP_LOGI(TAG, "Setting sleep mode out");
    res = ST7789_SleepOut();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: sleep out failed.");

        return 1;
    }

    /* idle mode off */
    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_IdleModeOff();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: idle mode off failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting display mode on");
    res = ST7789_NormalDisplayModeOn();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: normal display mode on failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting display inversion on");
    res = ST7789_DisplayInversionOn();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: display inversion on failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting gamma");
    res = ST7789_SetGamma(ST7789_GAMMA_CURVE_1);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gamma failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting memory data access control");
    res = ST7789_SetMemoryDataAccessControl(ST7789_ORDER_PAGE_TOP_TO_BOTTOM |
                                            ST7789_ORDER_COLUMN_RIGHT_TO_LEFT |
                                            ST7789_ORDER_PAGE_COLUMN_REVERSE |
                                            ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                            ST7789_ORDER_COLOR_RGB |
                                            ST7789_ORDER_REFRESH_LEFT_TO_RIGHT);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set memory data access control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting interface pixel format");
    res = ST7789_SetInterfacePixelFormat(ST7789_RGB_INTERFACE_COLOR_FORMAT_65K,
                                         ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set interface pixel format failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting display brightness");
    res = ST7789_SetDisplayBrightness(0xFF);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set display brightness failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting display control");
    res = ST7789_SetDisplayControl(ST7789_BOOL_FALSE,
                                   ST7789_BOOL_FALSE,
                                   ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set display control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting brightness control and color enhancement");
    res = ST7789_SetBrightnessControlAndColorEnhancement(ST7789_BOOL_TRUE,
                                                         ST7789_COLOR_ENHANCEMENT_MODE_USER_INTERFACE,
                                                         ST7789_COLOR_ENHANCEMENT_LEVEL_HIGH);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set brightness control and color enhancement failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting cabc minimum bightness");
    res = ST7789_SetCabcMinimumBrightness(0x00);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set cabc minimum brightness failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting RAM control");
    res = ST7789_SetRamControl(ST7789_RAM_ACCESS_MCU,
                               ST7789_DISPLAY_MODE_MCU,
                               ST7789_FRAME_TYPE_0,
                               ST7789_DATA_MODE_MSB,
                               ST7789_RGB_BUS_WIDTH_18_BIT,
                               ST7789_PIXEL_TYPE_0);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set ram control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting RGB interface control");
    res = ST7789_SetRgbInterfaceControl(ST7789_DIRECT_RGB_MODE_MEM,
                                        ST7789_RGB_IF_ENABLE_MODE_MCU,
                                        ST7789_PIN_LEVEL_LOW,
                                        ST7789_PIN_LEVEL_LOW,
                                        ST7789_PIN_LEVEL_LOW,
                                        ST7789_PIN_LEVEL_LOW,
                                        0x02,
                                        0x14);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set rgb interface control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting porch");
    res = ST7789_SetPorch(0x0C,
                          0x0C,
                          ST7789_BOOL_FALSE,
                          0x03,
                          0x03,
                          0x03,
                          0x03);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set porch failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting frame rate control");
    res = ST7789_SetFrameRateControl(ST7789_BOOL_FALSE,
                                     ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_1,
                                     ST7789_INVERSION_IDLE_MODE_DOT,
                                     0x0F,
                                     ST7789_INVERSION_PARTIAL_MODE_DOT,
                                     0x0F);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set frame rate control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting partial mode control");
    res = ST7789_SetPartialModeControl(ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V63,
                                       ST7789_NON_DISPLAY_AREA_SCAN_MODE_NORMAL,
                                       ST7789_NON_DISPLAY_FRAME_FREQUENCY_EVERY);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set partial mode control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting gate control");
    res = ST7789_SetGateControl(ST7789_VGHS_14P97_V, ST7789_VGLS_NEGATIVE_8P23);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting gate on timing adjustment");
    res = ST7789_SetGateOnTimingAdjustment(0x22,
                                           0x07,
                                           0x05);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate on timing adjustment failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting digital gamma");
    res = ST7789_SetDigitalGamma(ST7789_BOOL_TRUE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set digital gamma failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vcom convert to register");
    res = ST7789_VcomConvertToRegister(1.625f, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vcom convert to register failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vcoms");
    res = ST7789_SetVcoms(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vcoms failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting lcm control");
    res = ST7789_SetLcmControl(ST7789_BOOL_FALSE,
                               ST7789_BOOL_TRUE,
                               ST7789_BOOL_FALSE,
                               ST7789_BOOL_TRUE,
                               ST7789_BOOL_TRUE,
                               ST7789_BOOL_FALSE,
                               ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set lcm control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vdv vrh from");
    res = ST7789_SetVdvVrhFrom(ST7789_VDV_VRH_FROM_CMD);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vdv vrh from failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vhrs convert to register");
    res = ST7789_VrhsConvertToRegister(5.1f, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vrhs convert to register failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vhrs");
    res = ST7789_SetVrhs(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vrhs failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vdv convert to register");
    res = ST7789_VdvConvertToRegister(0.0f, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vdv convert to register failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vdv");
    res = ST7789_SetVdv(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vdv failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vcoms offset convert to register");
    res = ST7789_VcomsOffsetConvertToRegister(0.2f, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vcoms offset convert to register failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting vcoms offset");
    res = ST7789_SetVcomsOffset(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vcoms offset failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting frame rate");
    res = ST7789_SetFrameRate(ST7789_INVERSION_SELECTION_DOT, ST7789_FRAME_RATE_53_HZ);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set frame rate failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting cabc control");
    res = ST7789_SetCabcControl(ST7789_BOOL_FALSE,
                                ST7789_BOOL_FALSE,
                                ST7789_BOOL_FALSE,
                                ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set cabc control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting pwm frequency");
    res = ST7789_SetPwmFrequency(ST7789_PWM_FREQUENCY_9P8_KHZ);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set pwm frequency failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting power control 1");
    res = ST7789_SetPowerControl1(ST7789_AVDD_6P8_V,
                                  ST7789_AVCL_NEGTIVE_4P8_V,
                                  ST7789_VDS_2P3_V);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set power control 1 failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting command 2 enable");
    res = ST7789_SetCommand2Enable(ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set command 2 enable failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting positive voltage gamma control");
    res = ST7789_SetPositiveVoltageGammaControl(positiveVoltageControl);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set positive voltage gamma control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting negative voltage gamma control");
    res = ST7789_SetNegativeVoltageGammaControl(negativeVoltageControl);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set negative voltage gamma control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting gate line convert to register");
    res = ST7789_GateLineConvertToRegister(320, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: gate line convert to register failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting gate");
    res = ST7789_SetGate(reg,
                         0x00,
                         ST7789_GATE_SCAN_MODE_INTERLACE,
                         ST7789_GATE_SCAN_DIRECTION_0_319);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting spi 2 enable");
    res = ST7789_SetSpi2Enable(ST7789_BOOL_FALSE, ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set spi2 enable failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting power control 2");
    res = ST7789_SetPowerControl2(ST7789_SBCLK_DIV_3, ST7789_STP14CK_DIV_6);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set power control 2 failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting equalize time control");
    res = ST7789_SetEqualizeTimeControl(0x11,
                                        0x11,
                                        0x08);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set equalize time control failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Setting program mode");
    res = ST7789_SetProgramModeEnable(ST7789_BOOL_FALSE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set program mode enable ailed.");

        return 1;
    }

    ESP_LOGI(TAG, "Display ON");
    res = ST7789_DisplayOn();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: display on failed.");

        return 1;
    }

    ESP_LOGI(TAG, "Clearing screen");
    res = ST7789_Clear();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: clear failed.");

        return 1;
    }

    return 0;
}

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t LCD_Deinitialize(void)
{
    /* st7789 deinit */
    if (ST7789_Deinitialize() != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief  basic example clear
 * @return status code
 *         - 0 success
 *         - 1 clear failed
 * @note   none
 */
uint8_t LCD_Clear(void)
{
    /* st7789 clear */
    if (ST7789_Clear() != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief  basic example display on
 * @return status code
 *         - 0 success
 *         - 1 display on failed
 * @note   none
 */
uint8_t LCD_DisplayOn(void)
{
    /* display on */
    if (ST7789_DisplayOn() != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief  basic example display off
 * @return status code
 *         - 0 success
 *         - 1 display off failed
 * @note   none
 */
uint8_t LCD_DisplayOff(void)
{
    /* display off */
    if (ST7789_DisplayOff() != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief     basic example draw a string
 * @param[in] x coordinate x
 * @param[in] y coordinate y
 * @param[in] *str pointer to a written string address
 * @param[in] len length of the string
 * @param[in] color display color
 * @param[in] font display font size
 * @return    status code
 *            - 0 success
 *            - 1 draw string failed
 * @note      none
 */
uint8_t LCD_String(uint16_t x, uint16_t y, char *str, uint16_t len, uint32_t color, ST7789_font_t font, bool negative)
{
    /* write string */
    if (ST7789_WriteString(x, y, str, len, color, font, negative) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief     basic example write a point
 * @param[in] x coordinate x
 * @param[in] y coordinate y
 * @param[in] color written color
 * @return    status code
 *            - 0 success
 *            - 1 write point failed
 * @note      none
 */
uint8_t LCD_WritePoint(uint16_t x, uint16_t y, uint32_t color)
{
    /* draw point */
    if (ST7789_DrawPoint(x, y, color) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief     basic example fill a rectangle
 * @param[in] left left coordinate x
 * @param[in] top top coordinate y
 * @param[in] right right coordinate x
 * @param[in] bottom bottom coordinate y
 * @param[in] color display color
 * @return    status code
 *            - 0 success
 *            - 1 fill rect failed
 * @note      none
 */
uint8_t LCD_Rectangle(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint32_t color)
{
    /* fill rect */
    if (ST7789_FillRectangle(left, top, right, bottom, color) != 0)
    {
        return 1;
    }

    return 0;
}

/**
 * @brief     basic example draw a 16 bits picture
 * @param[in] left left coordinate x
 * @param[in] top top coordinate y
 * @param[in] right right coordinate x
 * @param[in] bottom bottom coordinate y
 * @param[in] *img pointer to a image buffer
 * @return    status code
 *            - 0 success
 *            - 1 draw picture 16 bits failed
 * @note      none
 */
uint8_t LCD_DrawPicture16bits(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t *img)
{
    /* draw picture in 16 bits */
    if (ST7789_DrawPicture16bits(left, top, right, bottom, img) != 0)
    {
        return 1;
    }

    return 0;
}
