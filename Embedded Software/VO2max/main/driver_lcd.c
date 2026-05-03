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

#define COL_COUNT 240
#define ROW_COUNT 320

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

static LCD_Orientation_e currentOrientation;
static uint8_t buffer[ST7789_BUFFER_SIZE];

static uint8_t WriteByte(uint8_t data, uint32_t cmd);
static uint8_t WriteBytes(uint8_t *data, uint16_t len, uint32_t cmd);
static void SpiPreTransferCallback(spi_transaction_t *t);
static void TransformPortrait(uint16_t x, uint16_t y, uint16_t *col, uint16_t *row);
static void TransformLandscape(uint16_t x, uint16_t y, uint16_t *col, uint16_t *row);
static void InitializeController(spi_host_device_t host);
static void SoftwareReset();
static void SleepOut();
static void NormalDisplayModeOn();
static void SetGamma(uint8_t gamma);
static void SetMemoryDataAccessControl(uint8_t order);
static void IdleModeOff();
static void SetInterfacePixelFormat(ST7789_rgb_interface_color_format_t rgb,
                                    ST7789_control_interface_color_format_t control);
static void SetDisplayBrightness(uint8_t brightness);
static void SetDisplayControl(ST7789_bool_t brightness_control_block,
                              ST7789_bool_t display_dimming,
                              ST7789_bool_t backlight_control);
static void SetBrightnessControlAndColorEnhancement(ST7789_bool_t color_enhancement,
                                                    ST7789_color_enhancement_mode_t mode,
                                                    ST7789_color_enhancement_level_t level);
static void SetCabcMinimumBrightness(uint8_t brightness);
static void SetRamControl(ST7789_ram_access_t ram_mode,
                          ST7789_display_mode_t display_mode,
                          ST7789_frame_type_t frame_type,
                          ST7789_data_mode_t data_mode,
                          ST7789_rgb_bus_width_t bus_width,
                          ST7789_pixel_type_t pixel_type);
static void SetPorch(uint8_t back_porch_normal,
                     uint8_t front_porch_normal,
                     ST7789_bool_t separate_porch_enable,
                     uint8_t back_porch_idle,
                     uint8_t front_porch_idle,
                     uint8_t back_porch_partial,
                     uint8_t front_porch_partial);
static void SetFrameRateControl(ST7789_bool_t separate_fr_control,
                                ST7789_frame_rate_divided_control_t div_control,
                                ST7789_inversion_idle_mode_t idle_mode,
                                uint8_t idle_frame_rate,
                                ST7789_inversion_partial_mode_t partial_mode,
                                uint8_t partial_frame_rate);
static void SetPartialModeControl(ST7789_non_display_source_output_level_t level,
                                  ST7789_non_display_area_scan_mode_t mode,
                                  ST7789_non_display_frame_frequency_t frequency);
static void SetGateControl(ST7789_vghs_t vghs, ST7789_vgls_t vgls);
static void SetDigitalGamma(ST7789_bool_t enable);
static void SetLcmControl(ST7789_bool_t xmy,
                          ST7789_bool_t xbgr,
                          ST7789_bool_t xinv,
                          ST7789_bool_t xmx,
                          ST7789_bool_t xmh,
                          ST7789_bool_t xmv,
                          ST7789_bool_t xgs);
static void SetVdvVrhFrom(ST7789_vdv_vrh_from_t from);
static void SetVrhs(uint8_t vrhs);
static void VrhsConvertToRegister(float v, uint8_t *reg);
static void SetVdv(uint8_t vdv);
static void VdvConvertToRegister(float v, uint8_t *reg);
static void SetVcomsOffset(uint8_t offset);
static void VcomsOffsetConvertToRegister(float v, uint8_t *reg);
static void SetFrameRate(ST7789_inversion_selection_t selection, ST7789_frame_rate_t rate);
static void SetCabcControl(ST7789_bool_t led_on,
                           ST7789_bool_t led_pwm_init,
                           ST7789_bool_t led_pwm_fix,
                           ST7789_bool_t led_pwm_polarity);
static void SetPwmFrequency(ST7789_pwm_frequency_t frequency);
static void SetPowerControl1(ST7789_avdd_t avdd, ST7789_avcl_t avcl, ST7789_vds_t vds);
static void SetPositiveVoltageGammaControl(uint8_t param[14]);
static void SetNegativeVoltageGammaControl(uint8_t param[14]);
static void SetGate(uint8_t gate_line_number,
                    uint8_t first_scan_line_number,
                    ST7789_gate_scan_mode_t mode,
                    ST7789_gate_scan_direction_t direction);
static void GateLineConvertToRegister(uint16_t l, uint8_t *reg);
static void SetPowerControl2(ST7789_sbclk_div_t sbclk, ST7789_stp14ck_div_t stp14ck);
static void SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

static void (*transform)(uint16_t, uint16_t, uint16_t *, uint16_t *) = TransformPortrait;

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
    InitializeController(SPI2_HOST);

    ESP_LOGI(TAG, "Software reset...");
    SoftwareReset();

    /* sleep out */
    ESP_LOGI(TAG, "Setting sleep mode out");
    SleepOut();

    /* idle mode off */
    ESP_LOGI(TAG, "Setting idle mode off");
    IdleModeOff();

    ESP_LOGI(TAG, "Setting display mode on");
    NormalDisplayModeOn();

    ESP_LOGI(TAG, "Setting display inversion on");
    LCD_DisplayInversionOn();

    ESP_LOGI(TAG, "Setting gamma");
    SetGamma(ST7789_GAMMA_CURVE_1);

    ESP_LOGI(TAG, "Setting interface pixel format");
    SetInterfacePixelFormat(ST7789_RGB_INTERFACE_COLOR_FORMAT_65K,
                            ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT);

    ESP_LOGI(TAG, "Setting display brightness");
    SetDisplayBrightness(0xFF);

    ESP_LOGI(TAG, "Setting display control");
    SetDisplayControl(ST7789_BOOL_FALSE,
                      ST7789_BOOL_FALSE,
                      ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting brightness control and color enhancement");
    SetBrightnessControlAndColorEnhancement(ST7789_BOOL_TRUE,
                                            ST7789_COLOR_ENHANCEMENT_MODE_USER_INTERFACE,
                                            ST7789_COLOR_ENHANCEMENT_LEVEL_HIGH);

    ESP_LOGI(TAG, "Setting cabc minimum bightness");
    SetCabcMinimumBrightness(0x00);

    ESP_LOGI(TAG, "Setting RAM control");
    SetRamControl(ST7789_RAM_ACCESS_MCU,
                  ST7789_DISPLAY_MODE_MCU,
                  ST7789_FRAME_TYPE_0,
                  ST7789_DATA_MODE_MSB,
                  ST7789_RGB_BUS_WIDTH_18_BIT,
                  ST7789_PIXEL_TYPE_0);

    ESP_LOGI(TAG, "Setting porch");
    SetPorch(0x0C,
             0x0C,
             ST7789_BOOL_FALSE,
             0x03,
             0x03,
             0x03,
             0x03);

    ESP_LOGI(TAG, "Setting frame rate control");
    SetFrameRateControl(ST7789_BOOL_FALSE,
                        ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_1,
                        ST7789_INVERSION_IDLE_MODE_DOT,
                        0x0F,
                        ST7789_INVERSION_PARTIAL_MODE_DOT,
                        0x0F);

    ESP_LOGI(TAG, "Setting partial mode control");
    SetPartialModeControl(ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V63,
                          ST7789_NON_DISPLAY_AREA_SCAN_MODE_NORMAL,
                          ST7789_NON_DISPLAY_FRAME_FREQUENCY_EVERY);

    ESP_LOGI(TAG, "Setting gate control");
    SetGateControl(ST7789_VGHS_14P97_V, ST7789_VGLS_NEGATIVE_8P23);

    ESP_LOGI(TAG, "Setting digital gamma");
    SetDigitalGamma(ST7789_BOOL_TRUE);

    ESP_LOGI(TAG, "Setting lcm control");
    SetLcmControl(ST7789_BOOL_FALSE,
                  ST7789_BOOL_TRUE,
                  ST7789_BOOL_FALSE,
                  ST7789_BOOL_TRUE,
                  ST7789_BOOL_TRUE,
                  ST7789_BOOL_FALSE,
                  ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting vdv vrh from");
    SetVdvVrhFrom(ST7789_VDV_VRH_FROM_CMD);

    ESP_LOGI(TAG, "Setting vhrs convert to register");
    VrhsConvertToRegister(5.1f, &reg);

    ESP_LOGI(TAG, "Setting vhrs");
    SetVrhs(reg);

    ESP_LOGI(TAG, "Setting vdv convert to register");
    VdvConvertToRegister(0.0f, &reg);

    ESP_LOGI(TAG, "Setting vdv");
    SetVdv(reg);

    ESP_LOGI(TAG, "Setting vcoms offset convert to register");
    VcomsOffsetConvertToRegister(0.2f, &reg);

    ESP_LOGI(TAG, "Setting vcoms offset");
    SetVcomsOffset(reg);

    ESP_LOGI(TAG, "Setting frame rate");
    SetFrameRate(ST7789_INVERSION_SELECTION_DOT, ST7789_FRAME_RATE_53_HZ);

    ESP_LOGI(TAG, "Setting cabc control");
    SetCabcControl(ST7789_BOOL_FALSE,
                   ST7789_BOOL_FALSE,
                   ST7789_BOOL_FALSE,
                   ST7789_BOOL_FALSE);

    ESP_LOGI(TAG, "Setting pwm frequency");
    SetPwmFrequency(ST7789_PWM_FREQUENCY_9P8_KHZ);

    ESP_LOGI(TAG, "Setting power control 1");
    SetPowerControl1(ST7789_AVDD_6P8_V,
                     ST7789_AVCL_NEGTIVE_4P8_V,
                     ST7789_VDS_2P3_V);

    ESP_LOGI(TAG, "Setting positive voltage gamma control");
    SetPositiveVoltageGammaControl(positiveVoltageControl);

    ESP_LOGI(TAG, "Setting negative voltage gamma control");
    SetNegativeVoltageGammaControl(negativeVoltageControl);

    ESP_LOGI(TAG, "Setting gate line convert to register");
    GateLineConvertToRegister(320, &reg);

    ESP_LOGI(TAG, "Setting gate");
    SetGate(reg,
            0x00,
            ST7789_GATE_SCAN_MODE_INTERLACE,
            ST7789_GATE_SCAN_DIRECTION_0_319);

    ESP_LOGI(TAG, "Setting power control 2");
    SetPowerControl2(ST7789_SBCLK_DIV_3, ST7789_STP14CK_DIV_6);
}

void LCD_SetBacklight(bool on)
{
    gpio_set_level(GPIO_PIN_NUM_BCKL, (int)on);
}

void LCD_SetOrientation(LCD_Orientation_e orientation)
{
    switch (orientation)
    {
    case LCD_ORIENTATION_PORTRAIT_1:
        currentOrientation = orientation;
        transform = TransformPortrait;
        SetMemoryDataAccessControl(ST7789_ORDER_PAGE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLUMN_LEFT_TO_RIGHT |
                                   ST7789_ORDER_PAGE_COLUMN_NORMAL |
                                   ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLOR_RGB |
                                   ST7789_ORDER_REFRESH_LEFT_TO_RIGHT);
        break;
    case LCD_ORIENTATION_PORTRAIT_2:
        currentOrientation = orientation;
        transform = TransformPortrait;
        SetMemoryDataAccessControl(ST7789_ORDER_PAGE_BOTTOM_TO_TOP |
                                   ST7789_ORDER_COLUMN_RIGHT_TO_LEFT |
                                   ST7789_ORDER_PAGE_COLUMN_NORMAL |
                                   ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLOR_RGB |
                                   ST7789_ORDER_REFRESH_LEFT_TO_RIGHT);
        break;
    case LCD_ORIENTATION_LANDSCAPE_1:
        currentOrientation = orientation;
        transform = TransformLandscape;
        SetMemoryDataAccessControl(ST7789_ORDER_PAGE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLUMN_RIGHT_TO_LEFT |
                                   ST7789_ORDER_PAGE_COLUMN_REVERSE |
                                   ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLOR_RGB |
                                   ST7789_ORDER_REFRESH_LEFT_TO_RIGHT);
        break;
    case LCD_ORIENTATION_LANDSCAPE_2:
        currentOrientation = orientation;
        transform = TransformLandscape;
        SetMemoryDataAccessControl(ST7789_ORDER_PAGE_BOTTOM_TO_TOP |
                                   ST7789_ORDER_COLUMN_LEFT_TO_RIGHT |
                                   ST7789_ORDER_PAGE_COLUMN_REVERSE |
                                   ST7789_ORDER_LINE_TOP_TO_BOTTOM |
                                   ST7789_ORDER_COLOR_RGB |
                                   ST7789_ORDER_REFRESH_RIGHT_TO_LEFT);
        break;
    default:
        break;
    }
}

void LCD_DisplayOff()
{
    WriteByte(ST7789_CMD_DISPOFF, ST7789_CMD); /* write display off command */
}

void LCD_DisplayOn()
{
    WriteByte(ST7789_CMD_DISPON, ST7789_CMD); /* write display on command */
}

void LCD_DisplayInversionOff()
{
    WriteByte(ST7789_CMD_INVOFF, ST7789_CMD); /* write display inversion off command */
}

void LCD_DisplayInversionOn()
{
    WriteByte(ST7789_CMD_INVON, ST7789_CMD); /* write display inversion on command */
}

static void InitializeController(spi_host_device_t host)
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

    gpio_set_level(GPIO_PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GPIO_PIN_NUM_RST, 1);

    vTaskDelay(pdMS_TO_TICKS(120)); /* over 120 ms */

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(spi_bus_add_device(host, &devcfg, &spiDeviceHandle));
}

static void SoftwareReset()
{
    WriteByte(ST7789_CMD_SWRESET, ST7789_CMD); /* write software reset command */

    vTaskDelay(pdMS_TO_TICKS(200));
}

static void SleepOut()
{
    WriteByte(ST7789_CMD_SLPOUT, ST7789_CMD); /* write sleep out command */

    vTaskDelay(pdMS_TO_TICKS(200));
}

static void NormalDisplayModeOn()
{
    WriteByte(ST7789_CMD_NORON, ST7789_CMD); /* write normal display mode on command */
}

static void SetGamma(uint8_t gamma)
{
    WriteByte(ST7789_CMD_GAMSET, ST7789_CMD); /* write set gamma command */
    WriteByte(gamma & 0x0F, ST7789_DATA);     /* write gamma data */
}

static void SetMemoryDataAccessControl(uint8_t order)
{
    WriteByte(ST7789_CMD_MADCTL, ST7789_CMD); /* write set memory data access control command */
    WriteByte(order, ST7789_DATA);            /* write data */
}

static void IdleModeOff()
{
    WriteByte(ST7789_CMD_IDMOFF, ST7789_CMD); /* write idle mode off command */
}

static void SetInterfacePixelFormat(ST7789_rgb_interface_color_format_t rgb,
                                    ST7789_control_interface_color_format_t control)
{
    uint8_t data;

    WriteByte(ST7789_CMD_COLMOD, ST7789_CMD); /* write interface pixel format command */

    data = (rgb << 4) | (control << 0); /* set pixel format */
    WriteByte(data, ST7789_DATA);       /* write data */
}

static void SetDisplayBrightness(uint8_t brightness)
{
    WriteByte(ST7789_CMD_WRDISBV, ST7789_CMD); /* write display brightness command */

    WriteByte(brightness, ST7789_DATA); /* write brightness */
}

static void SetDisplayControl(ST7789_bool_t brightness_control_block,
                              ST7789_bool_t display_dimming,
                              ST7789_bool_t backlight_control)
{
    uint8_t data;

    WriteByte(ST7789_CMD_WRCTRLD, ST7789_CMD); /* write CTRL display command */

    data = (brightness_control_block << 5) | (display_dimming << 3) | (backlight_control << 2); /* set control data */
    WriteByte(data, ST7789_DATA);                                                               /* write control */
}

static void SetBrightnessControlAndColorEnhancement(ST7789_bool_t color_enhancement,
                                                    ST7789_color_enhancement_mode_t mode, ST7789_color_enhancement_level_t level)
{
    uint8_t data;

    WriteByte(ST7789_CMD_WRCACE, ST7789_CMD); /* write content adaptive brightness control and color enhancement command */

    data = (color_enhancement << 7) | (level << 4) | (mode << 0); /* set control data */
    WriteByte(data, ST7789_DATA);                                 /* write control */
}

static void SetCabcMinimumBrightness(uint8_t brightness)
{
    WriteByte(ST7789_CMD_WRCABCMB, ST7789_CMD); /* write CABC minimum brightness command */

    WriteByte(brightness, ST7789_DATA); /* write brightness */
}

static void SetRamControl(ST7789_ram_access_t ram_mode,
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

static void SetPorch(uint8_t back_porch_normal,
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

static void SetFrameRateControl(ST7789_bool_t separate_fr_control,
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

static void SetPartialModeControl(ST7789_non_display_source_output_level_t level,
                                  ST7789_non_display_area_scan_mode_t mode,
                                  ST7789_non_display_frame_frequency_t frequency)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PARCTRL, ST7789_CMD);           /* write partial mode control command */
    reg = (level << 7) | (mode << 4) | (frequency << 0); /* set param */
    WriteByte(reg, ST7789_DATA);                         /* write data */
}

static void SetGateControl(ST7789_vghs_t vghs, ST7789_vgls_t vgls)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_GCTRL, ST7789_CMD); /* write gate control command */
    reg = (vghs << 4) | (vgls << 0);         /* set param */
    WriteByte(reg, ST7789_DATA);             /* write data */
}

static void SetDigitalGamma(ST7789_bool_t enable)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_DGMEN, ST7789_CMD); /* write digital gamma enable command */
    reg = enable << 2;                       /* set param */
    WriteByte(reg, ST7789_DATA);             /* write data */
}

static void SetLcmControl(ST7789_bool_t xmy,
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

static void SetVdvVrhFrom(ST7789_vdv_vrh_from_t from)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_VDVVRHEN, ST7789_CMD); /* write vdv and vrh command enable command */
    buf[0] = from;                              /* set param1 */
    buf[1] = 0xFF;                              /* set param2 */
    WriteBytes(buf, 2, ST7789_DATA);            /* write data */
}

static void SetVrhs(uint8_t vrhs)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VRHS, ST7789_CMD); /* write vrh set command */
    reg = vrhs & 0x3F;                      /* set param */
    WriteByte(reg, ST7789_DATA);            /* write data */
}

static void VrhsConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v - 3.55f) / 0.05f); /* convert real data to register data */
}

static void SetVdv(uint8_t vdv)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VDVSET, ST7789_CMD); /* write vdv setting command */
    reg = vdv & 0x3F;                         /* set param */
    WriteByte(reg, ST7789_DATA);              /* write data */
}

static void VdvConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v + 0.8f) / 0.025f); /* convert real data to register data */
}

static void SetVcomsOffset(uint8_t offset)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_VCMOFSET, ST7789_CMD); /* write vcoms offset set command */
    reg = offset & 0x3F;                        /* set param */
    WriteByte(reg, ST7789_DATA);                /* write data */
}

static void VcomsOffsetConvertToRegister(float v, uint8_t *reg)
{
    *reg = (uint8_t)((v + 0.8f) / 0.025f); /* convert real data to register data */
}

static void SetFrameRate(ST7789_inversion_selection_t selection, ST7789_frame_rate_t rate)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_FRCTR2, ST7789_CMD); /* write fr control 2 command */
    reg = (selection << 5) | (rate << 0);     /* set param */
    WriteByte(reg, ST7789_DATA);              /* write data */
}

static void SetCabcControl(ST7789_bool_t led_on,
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

static void SetPwmFrequency(ST7789_pwm_frequency_t frequency)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PWMFRSEL, ST7789_CMD); /* write pwm frequency selection command */
    reg = frequency;                            /* set param */
    WriteByte(reg, ST7789_DATA);                /* write data */
}

static void SetPowerControl1(ST7789_avdd_t avdd, ST7789_avcl_t avcl, ST7789_vds_t vds)
{
    uint8_t buf[2];

    WriteByte(ST7789_CMD_PWCTRL1, ST7789_CMD);       /* write power control 1 command */
    buf[0] = 0xA4;                                   /* set param 1 */
    buf[1] = (avdd << 6) | (avcl << 4) | (vds << 0); /* set param 2 */
    WriteBytes(buf, 2, ST7789_DATA);                 /* write data */
}

static void SetPositiveVoltageGammaControl(uint8_t param[14])
{
    WriteByte(ST7789_CMD_PVGAMCTRL, ST7789_CMD); /* write positive voltage gamma control command */
    WriteBytes(param, 14, ST7789_DATA);          /* write data */
}

static void SetNegativeVoltageGammaControl(uint8_t param[14])
{
    WriteByte(ST7789_CMD_NVGAMCTRL, ST7789_CMD); /* write negative voltage gamma control command */
    WriteBytes(param, 14, ST7789_DATA);          /* write data */
}

static void SetGate(uint8_t gate_line_number,
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

static void GateLineConvertToRegister(uint16_t l, uint8_t *reg)
{
    *reg = (uint8_t)((l / 8) - 1); /* convert real data to register data */
}

static void SetPowerControl2(ST7789_sbclk_div_t sbclk, ST7789_stp14ck_div_t stp14ck)
{
    uint8_t reg;

    WriteByte(ST7789_CMD_PWCTRL2, ST7789_CMD); /* write power control 2 command */
    reg = (sbclk << 4) | (stp14ck << 0);       /* set param */
    WriteByte(reg, ST7789_DATA);               /* write data */
}

static void SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // Column address set
    WriteByte(ST7789_CMD_CASET, ST7789_CMD);
    WriteByte(x0 >> 8, ST7789_DATA);
    WriteByte(x0 & 0xFF, ST7789_DATA);
    WriteByte(x1 >> 8, ST7789_DATA);
    WriteByte(x1 & 0xFF, ST7789_DATA);

    // Row address set
    WriteByte(ST7789_CMD_RASET, ST7789_CMD);
    WriteByte(y0 >> 8, ST7789_DATA);
    WriteByte(y0 & 0xFF, ST7789_DATA);
    WriteByte(y1 >> 8, ST7789_DATA);
    WriteByte(y1 & 0xFF, ST7789_DATA);
}

void LCD_Clear(void)
{
    uint32_t i;
    uint32_t m;
    uint32_t n;
    uint16_t col0;
    uint16_t row0;
    uint16_t col1;
    uint16_t row1;

    transform(0, 0, &col0, &row0);

    if ((currentOrientation == LCD_ORIENTATION_PORTRAIT_1) || (currentOrientation == LCD_ORIENTATION_PORTRAIT_2))
    {
        transform(SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, &col1, &row1);
    }
    else if ((currentOrientation == LCD_ORIENTATION_LANDSCAPE_1) || (currentOrientation == LCD_ORIENTATION_LANDSCAPE_2))
    {
        transform(SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, &col1, &row1);
    }

    SetAddrWindow(col0, row0, col1, row1);

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

void LCD_FillRectangle(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color)
{
    uint32_t i;
    uint32_t m;
    uint32_t n;
    uint16_t col0;
    uint16_t row0;
    uint16_t col1;
    uint16_t row1;

    transform(x0, y0, &col0, &row0);
    transform(x1, y1, &col1, &row1);

    SetAddrWindow(col0, row0, col1, row1);

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD); /* write memory write command */

    for (i = 0; i < ST7789_BUFFER_SIZE; i += 2) /* fill the buffer */
    {
        buffer[i] = (color >> 8) & 0xFF;     /* set the color */
        buffer[i + 1] = (color >> 0) & 0xFF; /* set the color */
    }

    m = (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1) * 2 /
        ST7789_BUFFER_SIZE; /* total times */
    n = ((uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1) * 2) %
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
    uint16_t col0;
    uint16_t row0;
    uint16_t col1;
    uint16_t row1;

    transform(left, top, &col0, &row0);
    transform(right, bottom, &col1, &row1);

    SetAddrWindow(col0, row0, col1, row1);

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD);

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

void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint32_t color, LCD_Font_t font)
{
    size_t len;

    len = strlen(str);

    while ((len != 0) && (*str <= '~') && (*str >= ' ')) /* write all string */
    {
        LCD_DrawChar(x, y, *str, font, color);

        x += (uint8_t)(font / 2);
        str++;
        len--;
    }
}

void LCD_ClearString(uint16_t x, uint16_t y, uint8_t length, uint32_t color, LCD_Font_t font)
{
    switch (font)
    {
    case LCD_FONT_12:
        LCD_FillRectangle(x, y, x + 6 * length, y + 12, color);
        break;
    case LCD_FONT_16:
        LCD_FillRectangle(x, y, x + 8 * length, y + 16, color);
        break;
    case LCD_FONT_24:
        LCD_FillRectangle(x, y, x + 12 * length, y + 24, color);
        break;
    default:
        break;
    }
}

void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color)
{
    uint8_t buf[4];
    uint16_t col;
    uint16_t row;

    transform(x, y, &col, &row);

    SetAddrWindow(col, row, col, row);

    WriteByte(ST7789_CMD_RAMWR, ST7789_CMD);

    buf[0] = (color >> 8) & 0xFF;    /* set the color */
    buf[1] = (color >> 0) & 0xFF;    /* set the color */
    WriteBytes(buf, 2, ST7789_DATA); /* write data */
}

void LCD_DrawChar(uint16_t x, uint16_t y, uint8_t chr, LCD_Font_t font, uint32_t color)
{
    uint8_t temp, t, t1;
    uint16_t y0 = y;
    uint8_t csize = (font / 8 + ((font % 8) ? 1 : 0)) * (font / 2); /* get size */

    chr = chr - ' ';            /* get index */
    for (t = 0; t < csize; t++) /* write size */
    {
        if (font == 12) /* if size 12 */
        {
            temp = gsc_st7789_ascii_1206[chr][t]; /* get ascii 1206 */
        }
        else if (font == 16) /* if size 16 */
        {
            temp = gsc_st7789_ascii_1608[chr][t]; /* get ascii 1608 */
        }
        else if (font == 24) /* if size 24 */
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
            if ((y - y0) == font) /* reset size */
            {
                y = y0; /* set y */
                x++;    /* x++ */

                break; /* break */
            }
        }
    }
}

static uint8_t WriteByte(uint8_t data, uint32_t cmd)
{
    spi_transaction_t transaction;

    memset(&transaction, 0, sizeof(transaction)); // Zero out the transaction
    transaction.length = 8;                       // Command is 8 bits
    transaction.tx_buffer = &data;                // The data is the cmd itself
    transaction.user = (void *)cmd;

    return spi_device_polling_transmit(spiDeviceHandle, &transaction);
}

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

static void TransformPortrait(uint16_t x, uint16_t y, uint16_t *col, uint16_t *row)
{
    *col = x + COL_OFFSET;
    *row = y + ROW_OFFSET;
}

static void TransformLandscape(uint16_t x, uint16_t y, uint16_t *col, uint16_t *row)
{
    *col = x + ROW_OFFSET;
    *row = y + COL_OFFSET;
}
