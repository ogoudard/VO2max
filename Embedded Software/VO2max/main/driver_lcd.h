#ifndef DRIVER_LCD_H
#define DRIVER_LCD_H

#include "driver_st7789.h"

#define LCD_COLOR_BLACK 0x0000
#define LCD_COLOR_RED 0xF800
#define LCD_COLOR_GREEN 0x07E0
#define LCD_COLOR_BLUE 0x001F
#define LCD_COLOR_WHITE 0xFFFF

/**
 * @brief st7789 basic example default definition
 */
#define LCD_DEFAULT_GAMMA_CURVE ST7789_GAMMA_CURVE_1                                            /**< access */
#define LCD_DEFAULT_RGB_INTERFACE_COLOR_FORMAT ST7789_RGB_INTERFACE_COLOR_FORMAT_65K            /**< 262K color format */
#define LCD_DEFAULT_CONTROL_INTERFACE_COLOR_FORMAT ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT /**< 16bit color format */
#define LCD_DEFAULT_BRIGHTNESS 0xFF                                                             /**< 0xFF brightness */
#define LCD_DEFAULT_BRIGHTNESS_BLOCK ST7789_BOOL_FALSE                                          /**< disable brightness block */
#define LCD_DEFAULT_DISPLAY_DIMMING ST7789_BOOL_FALSE                                           /**< disable display dimming */
#define LCD_DEFAULT_BACKLIGHT ST7789_BOOL_FALSE                                                 /**< disable backlight */
#define LCD_DEFAULT_COLOR_ENHANCEMENT ST7789_BOOL_TRUE                                          /**< enable color enhancement */
#define LCD_DEFAULT_COLOR_ENHANCEMENT_MODE ST7789_COLOR_ENHANCEMENT_MODE_USER_INTERFACE         /**< user interface */
#define LCD_DEFAULT_COLOR_ENHANCEMENT_LEVEL ST7789_COLOR_ENHANCEMENT_LEVEL_HIGH                 /**< high level */
#define LCD_DEFAULT_CABC_MINIMUM_BRIGHTNESS 0x00                                                /**< 0x00 */
#define LCD_DEFAULT_RAM_ACCESS ST7789_RAM_ACCESS_MCU                                            /**< mcu access */
#define LCD_DEFAULT_DISPLAY_MODE ST7789_DISPLAY_MODE_MCU                                        /**< mcu display mode */
#define LCD_DEFAULT_FRAME_TYPE ST7789_FRAME_TYPE_0                                              /**< frame type 0 */
#define LCD_DEFAULT_DATA_MODE ST7789_DATA_MODE_MSB                                              /**< data mode msb */
#define LCD_DEFAULT_RGB_BUS_WIDTH ST7789_RGB_BUS_WIDTH_18_BIT                                   /**< 18 bits */
#define LCD_DEFAULT_PIXEL_TYPE ST7789_PIXEL_TYPE_0                                              /**< pixel type 0 */
#define LCD_DEFAULT_DIRECT_RGB_MODE ST7789_DIRECT_RGB_MODE_MEM                                  /**< rgb mode mem */
#define LCD_DEFAULT_RGB_IF_ENABLE_MODE ST7789_RGB_IF_ENABLE_MODE_MCU                            /**< enable mode mcu */
#define LCD_DEFAULT_VSPL ST7789_PIN_LEVEL_LOW                                                   /**< level low */
#define LCD_DEFAULT_HSPL ST7789_PIN_LEVEL_LOW                                                   /**< level low */
#define LCD_DEFAULT_DPL ST7789_PIN_LEVEL_LOW                                                    /**< level low */
#define LCD_DEFAULT_EPL ST7789_PIN_LEVEL_LOW                                                    /**< level low */
#define LCD_DEFAULT_VBP 0x02                                                                    /**< 0x02 */
#define LCD_DEFAULT_HBP 0x14                                                                    /**< 0x14 */
#define LCD_DEFAULT_PORCH_NORMAL_BACK 0x0C                                                      /**< 0x0C */
#define LCD_DEFAULT_PORCH_NORMAL_FRONT 0x0C                                                     /**< 0x0C */
#define LCD_DEFAULT_PORCH_ENABLE ST7789_BOOL_FALSE                                              /**< disable porch */
#define LCD_DEFAULT_PORCH_IDEL_BACK 0x03                                                        /**< 0x03 */
#define LCD_DEFAULT_PORCH_IDEL_FRONT 0x03                                                       /**< 0x03 */
#define LCD_DEFAULT_PORCH_PART_BACK 0x03                                                        /**< 0x03 */
#define LCD_DEFAULT_PORCH_PART_FRONT 0x03                                                       /**< 0x03 */
#define LCD_DEFAULT_SEPARATE_FR ST7789_BOOL_FALSE                                               /**< disable fr */
#define LCD_DEFAULT_FRAME_RATE_DIVIDED ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_1                  /**< div 1 */
#define LCD_DEFAULT_INVERSION_IDLE_MODE ST7789_INVERSION_IDLE_MODE_DOT                          /**< dot mode */
#define LCD_DEFAULT_IDLE_FRAME_RATE 0x0F                                                        /**< 0x0F */
#define LCD_DEFAULT_INVERSION_PARTIAL_MODE ST7789_INVERSION_PARTIAL_MODE_DOT                    /**< dot mode */
#define LCD_DEFAULT_IDLE_PARTIAL_RATE 0x0F                                                      /**< 0x0F */
#define LCD_DEFAULT_NON_DISPLAY_SOURCE_OUTPUT_LEVEL ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V63  /**< v63 */
#define LCD_DEFAULT_NON_DISPLAY_AREA_SCAN_MODE ST7789_NON_DISPLAY_AREA_SCAN_MODE_NORMAL         /**< normal scan mode */
#define LCD_DEFAULT_NON_DISPLAY_FRAME_FREQUENCY ST7789_NON_DISPLAY_FRAME_FREQUENCY_EVERY        /**< every frame */
#define LCD_DEFAULT_VGHS ST7789_VGHS_14P97_V                                                    /**< 14.97V */
#define LCD_DEFAULT_VGLS_NEGATIVE ST7789_VGLS_NEGATIVE_8P23                                     /**< -8.23 */
#define LCD_DEFAULT_GATE_ON_TIMING 0x22                                                         /**< 0x22 */
#define LCD_DEFAULT_GATE_OFF_TIMING_RGB 0x07                                                    /**< 0x07 */
#define LCD_DEFAULT_GATE_OFF_TIMING 0x05                                                        /**< 0x05 */
#define LCD_DEFAULT_DIGITAL_GAMMA ST7789_BOOL_TRUE                                              /**< enable digital gamma */
#define LCD_DEFAULT_VCOMS 1.625f                                                                /**< 1.625 vcoms */
#define LCD_DEFAULT_XMY ST7789_BOOL_FALSE                                                       /**< disable xmy */
#define LCD_DEFAULT_XBGR ST7789_BOOL_TRUE                                                       /**< enable xbgr */
#define LCD_DEFAULT_XINV ST7789_BOOL_FALSE                                                      /**< disable xinv */
#define LCD_DEFAULT_XMX ST7789_BOOL_TRUE                                                        /**< enable xmx */
#define LCD_DEFAULT_XMH ST7789_BOOL_TRUE                                                        /**< enable xmh */
#define LCD_DEFAULT_XMV ST7789_BOOL_FALSE                                                       /**< disable xmv */
#define LCD_DEFAULT_XGS ST7789_BOOL_FALSE                                                       /**< disable xgs */
#define LCD_DEFAULT_VDV_VRH_FROM ST7789_VDV_VRH_FROM_CMD                                        /**< from cmd */
#define LCD_DEFAULT_VRHS 4.8f                                                                   /**< 4.8 */
#define LCD_DEFAULT_VDV 0.0f                                                                    /**< 0.0 */
#define LCD_DEFAULT_VCOMS_OFFSET 0.0f                                                           /**< 0.0 */
#define LCD_DEFAULT_INVERSION_SELECTION ST7789_INVERSION_SELECTION_DOT                          /**< dot */
#define LCD_DEFAULT_FRAME_RATE ST7789_FRAME_RATE_60_HZ                                          /**< frame rate 60Hz */
#define LCD_DEFAULT_LED_ON ST7789_BOOL_FALSE                                                    /**< disable led on */
#define LCD_DEFAULT_LED_PWM_INIT ST7789_BOOL_FALSE                                              /**< disable led pwm init */
#define LCD_DEFAULT_LED_PWM_FIX ST7789_BOOL_FALSE                                               /**< disable led pwm fix */
#define LCD_DEFAULT_LED_PWM_POLARITY ST7789_BOOL_FALSE                                          /**< disable led pwm polarity */
#define LCD_DEFAULT_PWM_FREQUENCY ST7789_PWM_FREQUENCY_9P8_KHZ                                  /**< pwm 9.8KHz */
#define LCD_DEFAULT_AVDD ST7789_AVDD_6P8_V                                                      /**< 6.8V */
#define LCD_DEFAULT_AVCL_NEGTIVE ST7789_AVCL_NEGTIVE_4P8_V                                      /**< -4.8V */
#define LCD_DEFAULT_VDS ST7789_VDS_2P3_V                                                        /**< 2.3V */
#define LCD_DEFAULT_COMMAND_2_ENABLE ST7789_BOOL_FALSE                                          /**< disable command 2 */
#define LCD_DEFAULT_POSITIVE_VOLTAGE_GAMMA {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, \
                                            0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23} /**< gamma */
#define LCD_DEFAULT_NEGATIVA_VOLTAGE_GAMMA {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, \
                                            0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23} /**< gamma */
#define LCD_DEFAULT_GATE_LINE 320                                                     /**< 320 */
#define LCD_DEFAULT_FIRST_SCAN_LINE 0x00                                              /**< 0x00 */
#define LCD_DEFAULT_GATE_SCAN_MODE ST7789_GATE_SCAN_MODE_INTERLACE                    /**< interlace */
#define LCD_DEFAULT_GATE_SCAN_DIRECTION ST7789_GATE_SCAN_DIRECTION_0_319              /**< 320 */
#define LCD_DEFAULT_SPI2_LANE ST7789_BOOL_FALSE                                       /**< disable */
#define LCD_DEFAULT_COMMAND_TABLE_2 ST7789_BOOL_FALSE                                 /**< disable command table2 */
#define LCD_DEFAULT_SBCLK_DIV ST7789_SBCLK_DIV_3                                      /**< div3 */
#define LCD_DEFAULT_STP14CK_DIV ST7789_STP14CK_DIV_6                                  /**< div6 */
#define LCD_DEFAULT_SOURCE_EQUALIZE_TIME 0x11                                         /**< 0x11 */
#define LCD_DEFAULT_SOURCE_PRE_DRIVE_TIME 0x11                                        /**< 0x11 */
#define LCD_DEFAULT_GATE_EQUALIZE_TIME 0x08                                           /**< 0x08 */
#define LCD_DEFAULT_PROGRAM_MODE ST7789_BOOL_FALSE                                    /**< disable program mode */

/**
 * @brief  basic example init
 * @return status code
 *         - 0 success
 *         - 1 init failed
 * @note   none
 */
uint8_t LCD_Initialize(void);

/**
 * @brief  basic example deinit
 * @return status code
 *         - 0 success
 *         - 1 deinit failed
 * @note   none
 */
uint8_t LCD_Deinitialize(void);

/**
 * @brief  basic example clear
 * @return status code
 *         - 0 success
 *         - 1 clear failed
 * @note   none
 */
uint8_t LCD_Clear(void);

/**
 * @brief  basic example display on
 * @return status code
 *         - 0 success
 *         - 1 display on failed
 * @note   none
 */
uint8_t LCD_DisplayOn(void);

/**
 * @brief  basic example display off
 * @return status code
 *         - 0 success
 *         - 1 display off failed
 * @note   none
 */
uint8_t LCD_DisplayOff(void);

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
uint8_t LCD_String(uint16_t x, uint16_t y, const char *str, uint16_t len, uint32_t color, uint32_t bgColor, ST7789_font_t font);

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
uint8_t LCD_WritePoint(uint16_t x, uint16_t y, uint32_t color);

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
uint8_t LCD_Rectangle(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint32_t color);

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
uint8_t LCD_DrawPicture16bits(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t *img);

#endif
