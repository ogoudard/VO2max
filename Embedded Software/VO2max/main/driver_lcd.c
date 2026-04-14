/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      driver_ST7789_basic.c
 * @brief     driver st7789 basic source file
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2023-04-15
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2023/04/15  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_lcd.h"
#include "esp_log.h"

static const char *TAG = "LCD";

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
    uint16_t i;
    uint8_t param_positive[14] = LCD_DEFAULT_POSITIVE_VOLTAGE_GAMMA;
    uint8_t param_negative[14] = LCD_DEFAULT_NEGATIVA_VOLTAGE_GAMMA;
    uint8_t params[64];

    ESP_LOGI(TAG, "Initialize ST7789...");
    res = ST7789_Initialize(SPI2_HOST);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: init failed.\n");

        return 1;
    }

    /* sleep out */
    ESP_LOGI(TAG, "Setting sleep mode out");
    res = ST7789_sleep_out();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: sleep out failed.\n");

        return 1;
    }

    /* idle mode off */
    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_idle_mode_off();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: idle mode off failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_normal_display_mode_on();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: normal display mode on failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_display_inversion_on();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: display inversion on failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_gamma(LCD_DEFAULT_GAMMA_CURVE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gamma failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_memory_data_access_control(LCD_DEFAULT_ACCESS);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set memory data access control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_interface_pixel_format(LCD_DEFAULT_RGB_INTERFACE_COLOR_FORMAT,
                                            LCD_DEFAULT_CONTROL_INTERFACE_COLOR_FORMAT);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set interface pixel format failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_display_brightness(LCD_DEFAULT_BRIGHTNESS);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set display brightness failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_display_control(LCD_DEFAULT_BRIGHTNESS_BLOCK,
                                     LCD_DEFAULT_DISPLAY_DIMMING,
                                     LCD_DEFAULT_BACKLIGHT);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set display control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_brightness_control_and_color_enhancement(LCD_DEFAULT_COLOR_ENHANCEMENT,
                                                              LCD_DEFAULT_COLOR_ENHANCEMENT_MODE,
                                                              LCD_DEFAULT_COLOR_ENHANCEMENT_LEVEL);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set brightness control and color enhancement failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_cabc_minimum_brightness(LCD_DEFAULT_CABC_MINIMUM_BRIGHTNESS);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set cabc minimum brightness failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_ram_control(LCD_DEFAULT_RAM_ACCESS,
                                 LCD_DEFAULT_DISPLAY_MODE,
                                 LCD_DEFAULT_FRAME_TYPE,
                                 LCD_DEFAULT_DATA_MODE,
                                 LCD_DEFAULT_RGB_BUS_WIDTH,
                                 LCD_DEFAULT_PIXEL_TYPE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set ram control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_rgb_interface_control(LCD_DEFAULT_DIRECT_RGB_MODE,
                                           LCD_DEFAULT_RGB_IF_ENABLE_MODE,
                                           LCD_DEFAULT_VSPL,
                                           LCD_DEFAULT_HSPL,
                                           LCD_DEFAULT_DPL,
                                           LCD_DEFAULT_EPL,
                                           LCD_DEFAULT_VBP,
                                           LCD_DEFAULT_HBP);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set rgb interface control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_porch(LCD_DEFAULT_PORCH_NORMAL_BACK,
                           LCD_DEFAULT_PORCH_NORMAL_FRONT,
                           LCD_DEFAULT_PORCH_ENABLE,
                           LCD_DEFAULT_PORCH_IDEL_BACK,
                           LCD_DEFAULT_PORCH_IDEL_FRONT,
                           LCD_DEFAULT_PORCH_PART_BACK,
                           LCD_DEFAULT_PORCH_PART_FRONT);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set porch failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_frame_rate_control(LCD_DEFAULT_SEPARATE_FR,
                                        LCD_DEFAULT_FRAME_RATE_DIVIDED,
                                        LCD_DEFAULT_INVERSION_IDLE_MODE,
                                        LCD_DEFAULT_IDLE_FRAME_RATE,
                                        LCD_DEFAULT_INVERSION_PARTIAL_MODE,
                                        LCD_DEFAULT_IDLE_PARTIAL_RATE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set frame rate control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_partial_mode_control(LCD_DEFAULT_NON_DISPLAY_SOURCE_OUTPUT_LEVEL,
                                          LCD_DEFAULT_NON_DISPLAY_AREA_SCAN_MODE,
                                          LCD_DEFAULT_NON_DISPLAY_FRAME_FREQUENCY);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set partial mode control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_gate_control(LCD_DEFAULT_VGHS, LCD_DEFAULT_VGLS_NEGATIVE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_gate_on_timing_adjustment(LCD_DEFAULT_GATE_ON_TIMING,
                                               LCD_DEFAULT_GATE_OFF_TIMING_RGB,
                                               LCD_DEFAULT_GATE_OFF_TIMING);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate on timing adjustment failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_digital_gamma(LCD_DEFAULT_DIGITAL_GAMMA);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set digital gamma failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_vcom_convert_to_register(LCD_DEFAULT_VCOMS, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vcom convert to register failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_vcoms(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vcoms failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_lcm_control(LCD_DEFAULT_XMY,
                                 LCD_DEFAULT_XBGR,
                                 LCD_DEFAULT_XINV,
                                 LCD_DEFAULT_XMX,
                                 LCD_DEFAULT_XMH,
                                 LCD_DEFAULT_XMV,
                                 LCD_DEFAULT_XGS);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set lcm control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_vdv_vrh_from(LCD_DEFAULT_VDV_VRH_FROM);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vdv vrh from failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_vrhs_convert_to_register(LCD_DEFAULT_VRHS, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vrhs convert to register failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_vrhs(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vrhs failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_vdv_convert_to_register(LCD_DEFAULT_VDV, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vdv convert to register failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_vdv(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vdv failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_vcoms_offset_convert_to_register(LCD_DEFAULT_VCOMS_OFFSET, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: vcoms offset convert to register failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_vcoms_offset(reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set vcoms offset failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_frame_rate(LCD_DEFAULT_INVERSION_SELECTION, LCD_DEFAULT_FRAME_RATE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set frame rate failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_cabc_control(LCD_DEFAULT_LED_ON,
                                  LCD_DEFAULT_LED_PWM_INIT,
                                  LCD_DEFAULT_LED_PWM_FIX,
                                  LCD_DEFAULT_LED_PWM_POLARITY);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set cabc control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_pwm_frequency(LCD_DEFAULT_PWM_FREQUENCY);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set pwm frequency failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_power_control_1(LCD_DEFAULT_AVDD,
                                     LCD_DEFAULT_AVCL_NEGTIVE,
                                     LCD_DEFAULT_VDS);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set power control 1 failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_command_2_enable(LCD_DEFAULT_COMMAND_2_ENABLE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set command 2 enable failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_positive_voltage_gamma_control(param_positive);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set positive voltage gamma control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_negative_voltage_gamma_control(param_negative);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set negative voltage gamma control failed.\n");

        return 1;
    }

    /* create the table */
    for (i = 0; i < 64; i++)
    {
        params[i] = i * 4;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_digital_gamma_look_up_table_red(params);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set digital gamma look up table red ailed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_digital_gamma_look_up_table_blue(params);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set digital gamma look up table blue ailed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_gate_line_convert_to_register(LCD_DEFAULT_GATE_LINE, &reg);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: gate line convert to register failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_gate(reg,
                          LCD_DEFAULT_FIRST_SCAN_LINE,
                          LCD_DEFAULT_GATE_SCAN_MODE,
                          LCD_DEFAULT_GATE_SCAN_DIRECTION);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set gate failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_spi2_enable(LCD_DEFAULT_SPI2_LANE, LCD_DEFAULT_COMMAND_TABLE_2);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set spi2 enable failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting idle mode off");
    res = ST7789_set_power_control_2(LCD_DEFAULT_SBCLK_DIV, LCD_DEFAULT_STP14CK_DIV);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set power control 2 failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting equalize time control");
    res = ST7789_set_equalize_time_control(LCD_DEFAULT_SOURCE_EQUALIZE_TIME,
                                           LCD_DEFAULT_SOURCE_PRE_DRIVE_TIME,
                                           LCD_DEFAULT_GATE_EQUALIZE_TIME);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set equalize time control failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Setting program mode");
    res = ST7789_set_program_mode_enable(LCD_DEFAULT_PROGRAM_MODE);
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: set program mode enable ailed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Display ON");
    res = ST7789_display_on();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: display on failed.\n");

        return 1;
    }

    ESP_LOGI(TAG, "Clearing screen");
    res = ST7789_clear();
    if (res != 0)
    {
        ESP_LOGI(TAG, "st7789: clear failed.\n");

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
uint8_t LCD_deinit(void)
{
    /* st7789 deinit */
    if (ST7789_deinit() != 0)
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
uint8_t LCD_clear(void)
{
    /* st7789 clear */
    if (ST7789_clear() != 0)
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
uint8_t LCD_display_on(void)
{
    /* display on */
    if (ST7789_display_on() != 0)
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
uint8_t LCD_display_off(void)
{
    /* display off */
    if (ST7789_display_off() != 0)
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
uint8_t LCD_string(uint16_t x, uint16_t y, char *str, uint16_t len, uint32_t color, ST7789_font_t font)
{
    /* write string */
    if (ST7789_write_string(x, y, str, len, color, font) != 0)
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
uint8_t LCD_write_point(uint16_t x, uint16_t y, uint32_t color)
{
    /* draw point */
    if (ST7789_draw_point(x, y, color) != 0)
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
uint8_t LCD_rect(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint32_t color)
{
    /* fill rect */
    if (ST7789_fill_rect(left, top, right, bottom, color) != 0)
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
uint8_t LCD_draw_picture_16bits(uint16_t left, uint16_t top, uint16_t right, uint16_t bottom, uint16_t *img)
{
    /* draw picture in 16 bits */
    if (ST7789_draw_picture_16bits(left, top, right, bottom, img) != 0)
    {
        return 1;
    }

    return 0;
}
