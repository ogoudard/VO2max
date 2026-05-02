#ifndef __DRIVER_LCD_H__
#define __DRIVER_LCD_H__

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver_st7789_settings.h"

#define LCD_COLOR_BLACK 0x0000
#define LCD_COLOR_RED 0xF800
#define LCD_COLOR_GREEN 0x07E0
#define LCD_COLOR_BLUE 0x001F
#define LCD_COLOR_WHITE 0xFFFF

#define LCD_NO_BG_COLOR 0xFFFFFFFF

#ifndef ST7789_BUFFER_SIZE
#define ST7789_BUFFER_SIZE (4096) /**< 4096 */
#endif

/**
 * @brief st7789 bool enumeration definition
 */
typedef enum
{
    ST7789_BOOL_FALSE = 0x00, /**< false */
    ST7789_BOOL_TRUE = 0x01,  /**< true */
} ST7789_bool_t;

typedef enum
{
    LCD_ORIENTATION_PORTRAIT_1,
    LCD_ORIENTATION_PORTRAIT_2,
    LCD_ORIENTATION_LANDSCAPE_1,
    LCD_ORIENTATION_LANDSCAPE_2
} LCD_Orientation_e;

/**
 * @brief st7789 font size enumeration definition
 */
typedef enum
{
    LCD_FONT_12 = 0x0C, /**< font 12 */
    LCD_FONT_16 = 0x10, /**< font 16 */
    LCD_FONT_24 = 0x18, /**< font 24 */
} LCD_Font_t;

/**
 * @brief st7789 gamma curve enumeration definition
 */
typedef enum
{
    ST7789_GAMMA_CURVE_1 = 0x1, /**< g2.2 */
    ST7789_GAMMA_CURVE_2 = 0x2, /**< g1.8 */
    ST7789_GAMMA_CURVE_3 = 0x4, /**< g2.5 */
    ST7789_GAMMA_CURVE_4 = 0x8, /**< g1.0 */
} ST7789_gamma_curve_t;

/**
 * @brief st7789 tearing effect enumeration definition
 */
typedef enum
{
    ST7789_TEARING_EFFECT_V_BLANKING = 0x0,                /**< v-blanking */
    ST7789_TEARING_EFFECT_V_BLANKING_AND_H_BLANKING = 0x1, /**< v-blanking and h-blanking */
} ST7789_tearing_effect_t;

/**
 * @brief st7789 order enumeration definition
 */
typedef enum
{
    ST7789_ORDER_PAGE_TOP_TO_BOTTOM = (0 << 7),    /**< top to bottom */
    ST7789_ORDER_PAGE_BOTTOM_TO_TOP = (1 << 7),    /**< bottom to top */
    ST7789_ORDER_COLUMN_LEFT_TO_RIGHT = (0 << 6),  /**< left to right */
    ST7789_ORDER_COLUMN_RIGHT_TO_LEFT = (1 << 6),  /**< right to left */
    ST7789_ORDER_PAGE_COLUMN_NORMAL = (0 << 5),    /**< normal mode */
    ST7789_ORDER_PAGE_COLUMN_REVERSE = (1 << 5),   /**< reverse mode */
    ST7789_ORDER_LINE_TOP_TO_BOTTOM = (0 << 4),    /**< lcd refresh top to bottom */
    ST7789_ORDER_LINE_BOTTOM_TO_TOP = (1 << 4),    /**< lcd refresh bottom to top */
    ST7789_ORDER_COLOR_RGB = (0 << 3),             /**< rgb */
    ST7789_ORDER_COLOR_BGR = (1 << 3),             /**< bgr */
    ST7789_ORDER_REFRESH_LEFT_TO_RIGHT = (0 << 2), /**< lcd refresh left to right */
    ST7789_ORDER_REFRESH_RIGHT_TO_LEFT = (1 << 2), /**< lcd refresh right to left */
} ST7789_order_t;

/**
 * @brief st7789 rgb interface color format enumeration definition
 */
typedef enum
{
    ST7789_RGB_INTERFACE_COLOR_FORMAT_65K = 0x5,  /**< 65k of rgb interface */
    ST7789_RGB_INTERFACE_COLOR_FORMAT_262K = 0x6, /**< 262k of rgb interface */
} ST7789_rgb_interface_color_format_t;

/**
 * @brief st7789 control interface color format enumeration definition
 */
typedef enum
{
    ST7789_CONTROL_INTERFACE_COLOR_FORMAT_12_BIT = 0x3, /**< 12bit/pixel */
    ST7789_CONTROL_INTERFACE_COLOR_FORMAT_16_BIT = 0x5, /**< 16bit/pixel */
    ST7789_CONTROL_INTERFACE_COLOR_FORMAT_18_BIT = 0x6, /**< 18bit/pixel */
} ST7789_control_interface_color_format_t;

/**
 * @brief st7789 color enhancement mode enumeration definition
 */
typedef enum
{
    ST7789_COLOR_ENHANCEMENT_MODE_OFF = 0x0,            /**< off */
    ST7789_COLOR_ENHANCEMENT_MODE_USER_INTERFACE = 0x1, /**< user interface mode */
    ST7789_COLOR_ENHANCEMENT_MODE_STILL_PICTURE = 0x2,  /**< still picture */
    ST7789_COLOR_ENHANCEMENT_MODE_MOVING_IMAGE = 0x3,   /**< moving image */
} ST7789_color_enhancement_mode_t;

/**
 * @brief st7789 color enhancement level enumeration definition
 */
typedef enum
{
    ST7789_COLOR_ENHANCEMENT_LEVEL_LOW = 0x0,    /**< low enhancement */
    ST7789_COLOR_ENHANCEMENT_LEVEL_MEDIUM = 0x1, /**< medium enhancement */
    ST7789_COLOR_ENHANCEMENT_LEVEL_HIGH = 0x3,   /**< high enhancement */
} ST7789_color_enhancement_level_t;

/**
 * @}
 */

/**
 * @addtogroup ST7789_advance_driver
 * @{
 */

/**
 * @brief st7789 ram access enumeration definition
 */
typedef enum
{
    ST7789_RAM_ACCESS_MCU = 0x0, /**< mcu interface */
    ST7789_RAM_ACCESS_RGB = 0x1, /**< rgb interface */
} ST7789_ram_access_t;

/**
 * @brief st7789 display mode enumeration definition
 */
typedef enum
{
    ST7789_DISPLAY_MODE_MCU = 0x0,   /**< mcu interface */
    ST7789_DISPLAY_MODE_RGB = 0x1,   /**< rgb interface */
    ST7789_DISPLAY_MODE_VSYNC = 0x2, /**< vsync interface */
} ST7789_display_mode_t;

/**
 * @brief st7789 data mode enumeration definition
 */
typedef enum
{
    ST7789_DATA_MODE_MSB = 0x0, /**< big endian */
    ST7789_DATA_MODE_LSB = 0x1, /**< little endian */
} ST7789_data_mode_t;

/**
 * @brief st7789 rgb bus width enumeration definition
 */
typedef enum
{
    ST7789_RGB_BUS_WIDTH_18_BIT = 0x0, /**< 18 bit bus width */
    ST7789_RGB_BUS_WIDTH_6_BIT = 0x1,  /**< 6 bit bus width */
} ST7789_rgb_bus_width_t;

/**
 * @brief st7789 frame type enumeration definition
 */
typedef enum
{
    ST7789_FRAME_TYPE_0 = 0x0, /**< type0 */
    ST7789_FRAME_TYPE_1 = 0x1, /**< type1 */
    ST7789_FRAME_TYPE_2 = 0x2, /**< type2 */
    ST7789_FRAME_TYPE_3 = 0x3, /**< type3 */
} ST7789_frame_type_t;

/**
 * @brief st7789 pixel type enumeration definition
 */
typedef enum
{
    ST7789_PIXEL_TYPE_0 = 0x0, /**< type0 */
    ST7789_PIXEL_TYPE_1 = 0x1, /**< type1 */
    ST7789_PIXEL_TYPE_2 = 0x2, /**< type2 */
    ST7789_PIXEL_TYPE_3 = 0x3, /**< type3 */
} ST7789_pixel_type_t;

/**
 * @brief st7789 direct rgb mode enumeration definition
 */
typedef enum
{
    ST7789_DIRECT_RGB_MODE_MEM = 0x0,   /**< memory mode */
    ST7789_DIRECT_RGB_MODE_SHIFT = 0x1, /**< shift register mode */
} ST7789_direct_rgb_mode_t;

/**
 * @brief st7789 rgb if enable mode enumeration definition
 */
typedef enum
{
    ST7789_RGB_IF_ENABLE_MODE_MCU = 0x0, /**< memory mode */
    ST7789_RGB_IF_ENABLE_MODE_DE = 0x2,  /**< rgb de mode */
    ST7789_RGB_IF_ENABLE_MODE_HV = 0x3,  /**< rgb hv mode */
} ST7789_rgb_if_enable_mode_t;

/**
 * @brief st7789 pin level enumeration definition
 */
typedef enum
{
    ST7789_PIN_LEVEL_LOW = 0x0,  /**< low active */
    ST7789_PIN_LEVEL_HIGH = 0x1, /**< high active */
} ST7789_pin_level_t;

/**
 * @brief st7789 frame rate divided control enumeration definition
 */
typedef enum
{
    ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_1 = 0x0, /**< divide by 1 */
    ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_2 = 0x1, /**< divide by 2 */
    ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_4 = 0x2, /**< divide by 4 */
    ST7789_FRAME_RATE_DIVIDED_CONTROL_DIV_8 = 0x3, /**< divide by 8 */
} ST7789_frame_rate_divided_control_t;

/**
 * @brief st7789 inversion idle mode enumeration definition
 */
typedef enum
{
    ST7789_INVERSION_IDLE_MODE_DOT = 0x0,    /**< dot inversion */
    ST7789_INVERSION_IDLE_MODE_COLUMN = 0x7, /**< column inversion */
} ST7789_inversion_idle_mode_t;

/**
 * @brief st7789 inversion partial mode enumeration definition
 */
typedef enum
{
    ST7789_INVERSION_PARTIAL_MODE_DOT = 0x0,    /**< dot inversion */
    ST7789_INVERSION_PARTIAL_MODE_COLUMN = 0x7, /**< column inversion */
} ST7789_inversion_partial_mode_t;

/**
 * @brief st7789 non display source output level enumeration definition
 */
typedef enum
{
    ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V63 = 0x0, /**< v63 */
    ST7789_NON_DISPLAY_SOURCE_OUTPUT_LEVEL_V0 = 0x1,  /**< v0 */
} ST7789_non_display_source_output_level_t;

/**
 * @brief st7789 non display area scan mode enumeration definition
 */
typedef enum
{
    ST7789_NON_DISPLAY_AREA_SCAN_MODE_NORMAL = 0x0,   /**< normal mode */
    ST7789_NON_DISPLAY_AREA_SCAN_MODE_INTERVAL = 0x1, /**< interval scan mode */
} ST7789_non_display_area_scan_mode_t;

/**
 * @brief st7789 non display frame frequency enumeration definition
 */
typedef enum
{
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_EVERY = 0x0,    /**< every frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_3 = 0x1,  /**< 1/3 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_5 = 0x2,  /**< 1/5 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_7 = 0x3,  /**< 1/7 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_9 = 0x4,  /**< 1/9 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_11 = 0x5, /**< 1/11 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_13 = 0x6, /**< 1/13 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_15 = 0x7, /**< 1/15 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_17 = 0x8, /**< 1/17 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_19 = 0x9, /**< 1/19 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_21 = 0xA, /**< 1/21 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_23 = 0xB, /**< 1/23 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_25 = 0xC, /**< 1/25 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_27 = 0xD, /**< 1/27 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_29 = 0xE, /**< 1/29 frame */
    ST7789_NON_DISPLAY_FRAME_FREQUENCY_1_DIV_31 = 0xF, /**< 1/31 frame */
} ST7789_non_display_frame_frequency_t;

/**
 * @brief st7789 vghs enumeration definition
 */
typedef enum
{
    ST7789_VGHS_12P20_V = 0x0, /**< 12.20V */
    ST7789_VGHS_12P54_V = 0x1, /**< 12.54V */
    ST7789_VGHS_12P89_V = 0x2, /**< 12.89V */
    ST7789_VGHS_13P26_V = 0x3, /**< 13.26V */
    ST7789_VGHS_13P65_V = 0x4, /**< 13.65V */
    ST7789_VGHS_14P06_V = 0x5, /**< 14.06V */
    ST7789_VGHS_14P50_V = 0x6, /**< 14.50V */
    ST7789_VGHS_14P97_V = 0x7, /**< 14.97V */
} ST7789_vghs_t;

/**
 * @brief st7789 vgls enumeration definition
 */
typedef enum
{
    ST7789_VGLS_NEGATIVE_7P16 = 0x0,  /**< -7.16V */
    ST7789_VGLS_NEGATIVE_7P67 = 0x1,  /**< -7.67V */
    ST7789_VGLS_NEGATIVE_8P23 = 0x2,  /**< -8.23V */
    ST7789_VGLS_NEGATIVE_8P87 = 0x3,  /**< -8.87V */
    ST7789_VGLS_NEGATIVE_9P60 = 0x4,  /**< -9.60V */
    ST7789_VGLS_NEGATIVE_10P43 = 0x5, /**< -10.43V */
    ST7789_VGLS_NEGATIVE_11P38 = 0x6, /**< -11.38V */
    ST7789_VGLS_NEGATIVE_12P50 = 0x7, /**< -12.50V */
} ST7789_vgls_t;

/**
 * @brief st7789 vdv vrh from enumeration definition
 */
typedef enum
{
    ST7789_VDV_VRH_FROM_NVM = 0x0, /**< from nvm */
    ST7789_VDV_VRH_FROM_CMD = 0x1, /**< from command write */
} ST7789_vdv_vrh_from_t;

/**
 * @brief st7789 inversion selection enumeration definition
 */
typedef enum
{
    ST7789_INVERSION_SELECTION_DOT = 0x0,    /**< dot inversion */
    ST7789_INVERSION_SELECTION_COLUMN = 0x7, /**< column inversion */
} ST7789_inversion_selection_t;

/**
 * @brief st7789 frame rate enumeration definition
 */
typedef enum
{
    ST7789_FRAME_RATE_119_HZ = 0x00, /**< 119Hz */
    ST7789_FRAME_RATE_111_HZ = 0x01, /**< 111Hz */
    ST7789_FRAME_RATE_105_HZ = 0x02, /**< 105Hz */
    ST7789_FRAME_RATE_99_HZ = 0x03,  /**< 99Hz */
    ST7789_FRAME_RATE_94_HZ = 0x04,  /**< 94Hz */
    ST7789_FRAME_RATE_90_HZ = 0x05,  /**< 90Hz */
    ST7789_FRAME_RATE_86_HZ = 0x06,  /**< 86Hz */
    ST7789_FRAME_RATE_82_HZ = 0x07,  /**< 82Hz */
    ST7789_FRAME_RATE_78_HZ = 0x08,  /**< 78Hz */
    ST7789_FRAME_RATE_75_HZ = 0x09,  /**< 75Hz */
    ST7789_FRAME_RATE_72_HZ = 0x0A,  /**< 72Hz */
    ST7789_FRAME_RATE_69_HZ = 0x0B,  /**< 69Hz */
    ST7789_FRAME_RATE_67_HZ = 0x0C,  /**< 67Hz */
    ST7789_FRAME_RATE_64_HZ = 0x0D,  /**< 64Hz */
    ST7789_FRAME_RATE_62_HZ = 0x0E,  /**< 62Hz */
    ST7789_FRAME_RATE_60_HZ = 0x0F,  /**< 60Hz */
    ST7789_FRAME_RATE_58_HZ = 0x10,  /**< 58Hz */
    ST7789_FRAME_RATE_57_HZ = 0x11,  /**< 57Hz */
    ST7789_FRAME_RATE_55_HZ = 0x12,  /**< 55Hz */
    ST7789_FRAME_RATE_53_HZ = 0x13,  /**< 53Hz */
    ST7789_FRAME_RATE_52_HZ = 0x14,  /**< 52Hz */
    ST7789_FRAME_RATE_50_HZ = 0x15,  /**< 50Hz */
    ST7789_FRAME_RATE_49_HZ = 0x16,  /**< 49Hz */
    ST7789_FRAME_RATE_48_HZ = 0x17,  /**< 48Hz */
    ST7789_FRAME_RATE_46_HZ = 0x18,  /**< 46Hz */
    ST7789_FRAME_RATE_45_HZ = 0x19,  /**< 45Hz */
    ST7789_FRAME_RATE_44_HZ = 0x1A,  /**< 44Hz */
    ST7789_FRAME_RATE_43_HZ = 0x1B,  /**< 43Hz */
    ST7789_FRAME_RATE_42_HZ = 0x1C,  /**< 42Hz */
    ST7789_FRAME_RATE_41_HZ = 0x1D,  /**< 41Hz */
    ST7789_FRAME_RATE_40_HZ = 0x1E,  /**< 40Hz */
    ST7789_FRAME_RATE_39_HZ = 0x1F,  /**< 39Hz */
} ST7789_frame_rate_t;

/**
 * @brief st7789 pwm frequency enumeration definition
 */
typedef enum
{
    ST7789_PWM_FREQUENCY_39P2_KHZ = (0x0 << 3) | (0x0 << 0),   /**< 39.2 KHz */
    ST7789_PWM_FREQUENCY_78P7_KHZ = (0x1 << 3) | (0x0 << 0),   /**< 78.7 KHz */
    ST7789_PWM_FREQUENCY_158P7_KHZ = (0x2 << 3) | (0x0 << 0),  /**< 158.7 KHz */
    ST7789_PWM_FREQUENCY_322P6_KHZ = (0x3 << 3) | (0x0 << 0),  /**< 322.6 KHz */
    ST7789_PWM_FREQUENCY_666P7_KHZ = (0x4 << 3) | (0x0 << 0),  /**< 666.7 KHz */
    ST7789_PWM_FREQUENCY_1428P6_KHZ = (0x5 << 3) | (0x0 << 0), /**< 1428.6 KHz */
    ST7789_PWM_FREQUENCY_19P6_KHZ = (0x0 << 3) | (0x1 << 0),   /**< 19.6 KHz */
    ST7789_PWM_FREQUENCY_39P4_KHZ = (0x1 << 3) | (0x1 << 0),   /**< 39.4 KHz */
    ST7789_PWM_FREQUENCY_79P4_KHZ = (0x2 << 3) | (0x1 << 0),   /**< 79.4 KHz */
    ST7789_PWM_FREQUENCY_161P3_KHZ = (0x3 << 3) | (0x1 << 0),  /**< 161.3 KHz */
    ST7789_PWM_FREQUENCY_333P3_KHZ = (0x4 << 3) | (0x1 << 0),  /**< 333.3 KHz */
    ST7789_PWM_FREQUENCY_714P3_KHZ = (0x5 << 3) | (0x1 << 0),  /**< 714.3 KHz */
    ST7789_PWM_FREQUENCY_9P8_KHZ = (0x0 << 3) | (0x2 << 0),    /**< 9.8 KHz */
    ST7789_PWM_FREQUENCY_19P7_KHZ = (0x1 << 3) | (0x2 << 0),   /**< 19.7 KHz */
    ST7789_PWM_FREQUENCY_39P7_KHZ = (0x2 << 3) | (0x2 << 0),   /**< 39.7 KHz */
    ST7789_PWM_FREQUENCY_80P6_KHZ = (0x3 << 3) | (0x2 << 0),   /**< 80.6 KHz */
    ST7789_PWM_FREQUENCY_166P7_KHZ = (0x4 << 3) | (0x2 << 0),  /**< 166.7 KHz */
    ST7789_PWM_FREQUENCY_357P1_KHZ = (0x5 << 3) | (0x2 << 0),  /**< 357.1 KHz */
    ST7789_PWM_FREQUENCY_4P9_KHZ = (0x0 << 3) | (0x3 << 0),    /**< 4.9 KHz */
    ST7789_PWM_FREQUENCY_9P80_KHZ = (0x1 << 3) | (0x3 << 0),   /**< 9.80 KHz */
    ST7789_PWM_FREQUENCY_19P8_KHZ = (0x2 << 3) | (0x3 << 0),   /**< 19.8 KHz */
    ST7789_PWM_FREQUENCY_40P3_KHZ = (0x3 << 3) | (0x3 << 0),   /**< 40.3 KHz */
    ST7789_PWM_FREQUENCY_83P3_KHZ = (0x4 << 3) | (0x3 << 0),   /**< 83.3 KHz */
    ST7789_PWM_FREQUENCY_178P6_KHZ = (0x5 << 3) | (0x3 << 0),  /**< 178.6 KHz */
    ST7789_PWM_FREQUENCY_2P45_KHZ = (0x0 << 3) | (0x4 << 0),   /**< 2.45 KHz */
    ST7789_PWM_FREQUENCY_4P90_KHZ = (0x1 << 3) | (0x4 << 0),   /**< 4.90 KHz */
    ST7789_PWM_FREQUENCY_9P9_KHZ = (0x2 << 3) | (0x4 << 0),    /**< 9.9 KHz */
    ST7789_PWM_FREQUENCY_20P2_KHZ = (0x3 << 3) | (0x4 << 0),   /**< 20.2 KHz */
    ST7789_PWM_FREQUENCY_41P7_KHZ = (0x4 << 3) | (0x4 << 0),   /**< 41.7 KHz */
    ST7789_PWM_FREQUENCY_89P3_KHZ = (0x5 << 3) | (0x4 << 0),   /**< 89.3 KHz */
    ST7789_PWM_FREQUENCY_1P23_KHZ = (0x0 << 3) | (0x5 << 0),   /**< 1.23 KHz */
    ST7789_PWM_FREQUENCY_2P5_KHZ = (0x1 << 3) | (0x5 << 0),    /**< 2.5 KHz */
    ST7789_PWM_FREQUENCY_5P0_KHZ = (0x2 << 3) | (0x5 << 0),    /**< 5.0 KHz */
    ST7789_PWM_FREQUENCY_10P1_KHZ = (0x3 << 3) | (0x5 << 0),   /**< 10.1 KHz */
    ST7789_PWM_FREQUENCY_20P8_KHZ = (0x4 << 3) | (0x5 << 0),   /**< 20.8 KHz */
    ST7789_PWM_FREQUENCY_44P6_KHZ = (0x5 << 3) | (0x5 << 0),   /**< 44.6 KHz */
    ST7789_PWM_FREQUENCY_0P61_KHZ = (0x0 << 3) | (0x6 << 0),   /**< 0.61 KHz */
    ST7789_PWM_FREQUENCY_1P230_KHZ = (0x1 << 3) | (0x6 << 0),  /**< 1.230 KHz */
    ST7789_PWM_FREQUENCY_2P48_KHZ = (0x2 << 3) | (0x6 << 0),   /**< 2.48 KHz */
    ST7789_PWM_FREQUENCY_5P00_KHZ = (0x3 << 3) | (0x6 << 0),   /**< 5.00 KHz */
    ST7789_PWM_FREQUENCY_10P4_KHZ = (0x4 << 3) | (0x6 << 0),   /**< 10.4 KHz */
    ST7789_PWM_FREQUENCY_22P3_KHZ = (0x5 << 3) | (0x6 << 0),   /**< 22.3 KHz */
    ST7789_PWM_FREQUENCY_0P31_KHZ = (0x0 << 3) | (0x7 << 0),   /**< 0.31 KHz */
    ST7789_PWM_FREQUENCY_0P62_KHZ = (0x1 << 3) | (0x7 << 0),   /**< 0.62 KHz */
    ST7789_PWM_FREQUENCY_1P24_KHZ = (0x2 << 3) | (0x7 << 0),   /**< 1.24 KHz */
    ST7789_PWM_FREQUENCY_2P25_KHZ = (0x3 << 3) | (0x7 << 0),   /**< 2.25 KHz */
    ST7789_PWM_FREQUENCY_5P2_KHZ = (0x4 << 3) | (0x7 << 0),    /**< 5.2 KHz */
    ST7789_PWM_FREQUENCY_11P2_KHZ = (0x5 << 3) | (0x7 << 0),   /**< 11.2 KHz */
} ST7789_pwm_frequency_t;

/**
 * @brief st7789 avdd enumeration definition
 */
typedef enum
{
    ST7789_AVDD_6P4_V = 0x0, /**< 6.4V */
    ST7789_AVDD_6P6_V = 0x1, /**< 6.6V */
    ST7789_AVDD_6P8_V = 0x2, /**< 6.8V */
} ST7789_avdd_t;

/**
 * @brief st7789 avcl enumeration definition
 */
typedef enum
{
    ST7789_AVCL_NEGTIVE_4P4_V = 0x0, /**< -4.4V */
    ST7789_AVCL_NEGTIVE_4P6_V = 0x1, /**< -4.6V */
    ST7789_AVCL_NEGTIVE_4P8_V = 0x2, /**< -4.8V */
    ST7789_AVCL_NEGTIVE_5P0_V = 0x3, /**< -5.0V */
} ST7789_avcl_t;

/**
 * @brief st7789 vds enumeration definition
 */
typedef enum
{
    ST7789_VDS_2P19_V = 0x0, /**< 2.19V */
    ST7789_VDS_2P3_V = 0x1,  /**< 2.3V */
    ST7789_VDS_2P4_V = 0x2,  /**< 2.4V */
    ST7789_VDS_2P51_V = 0x3, /**< 2.51V */
} ST7789_vds_t;

/**
 * @brief st7789 gate scan mode enumeration definition
 */
typedef enum
{
    ST7789_GATE_SCAN_MODE_INTERLACE = 0x0,     /**< interlace mode */
    ST7789_GATE_SCAN_MODE_NON_INTERLACE = 0x1, /**< non-interlace mode */
} ST7789_gate_scan_mode_t;

/**
 * @brief st7789 gate scan direction enumeration definition
 */
typedef enum
{
    ST7789_GATE_SCAN_DIRECTION_0_319 = 0x0, /**< 0 - 319 */
    ST7789_GATE_SCAN_DIRECTION_319_0 = 0x1, /**< 319 - 0 */
} ST7789_gate_scan_direction_t;

/**
 * @brief st7789 sbclk div enumeration definition
 */
typedef enum
{
    ST7789_SBCLK_DIV_2 = 0x0, /**< div2 */
    ST7789_SBCLK_DIV_3 = 0x1, /**< div3 */
    ST7789_SBCLK_DIV_4 = 0x2, /**< div4 */
    ST7789_SBCLK_DIV_6 = 0x3, /**< div6 */
} ST7789_sbclk_div_t;

/**
 * @brief st7789 stp14ck div enumeration definition
 */
typedef enum
{
    ST7789_STP14CK_DIV_2 = 0x0, /**< div2 */
    ST7789_STP14CK_DIV_3 = 0x1, /**< div3 */
    ST7789_STP14CK_DIV_4 = 0x2, /**< div4 */
    ST7789_STP14CK_DIV_6 = 0x3, /**< div6 */
} ST7789_stp14ck_div_t;

/**
 * @}
 */

/**
 * @addtogroup ST7789_basic_driver
 * @{
 */

/**
 * @brief st7789 handle structure definition
 */
typedef struct ST7789_handle_s
{
    bool inited;                         /**< inited flag */
    uint8_t format;                      /**< format */
    uint8_t buf[ST7789_BUFFER_SIZE + 8]; /**< inner buffer */
} ST7789_handle_t;

void LCD_Initialize();
void LCD_SetBacklight(bool on);
void LCD_Clear();
void LCD_SetOrientation(LCD_Orientation_e orientation);
void LCD_DrawPoint(uint16_t x, uint16_t y, uint32_t color);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t len, uint32_t color, LCD_Font_t font);
void LCD_ClearString(uint16_t x, uint16_t y, uint8_t length, uint32_t color, LCD_Font_t font);
void LCD_FillRectangle(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint32_t color);
void LCD_DrawPicture16bits(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t *image);
void LCD_DisplayOff();
void LCD_DisplayOn();
void LCD_DisplayInversionOff();
void LCD_DisplayInversionOn();
void LCD_DrawChar(uint16_t x, uint16_t y, uint8_t chr, LCD_Font_t font, uint32_t color);

#endif
