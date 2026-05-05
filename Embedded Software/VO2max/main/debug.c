#include "debug.h"
#include "esp_log.h"

// Pins are on T4 connector header on Lilygo T-Display board
#define GPIO_PIN_NUM_DEBUG_OUT_1 25
#define GPIO_REGISTER_MASK_OUT_1 0x2000000
#define GPIO_PIN_NUM_DEBUG_OUT_2 26
#define GPIO_REGISTER_MASK_OUT_2 0x4000000
#define GPIO_PIN_NUM_DEBUG_IN_1 27
#define GPIO_REGISTER_MASK_IN_1 0x8000000

static const char *TAG = "[DEBUG]";

static gpio_dev_t *dev = GPIO_HAL_GET_HW(GPIO_PORT_0);

void DEBUG_Initialize(void)
{
    gpio_config_t debugGpioOutConf = {.intr_type = GPIO_INTR_DISABLE,
                                      .mode = GPIO_MODE_OUTPUT,
                                      .pin_bit_mask = ((uint64_t)1 << GPIO_PIN_NUM_DEBUG_OUT_1) |
                                                      ((uint64_t)1 << GPIO_PIN_NUM_DEBUG_OUT_2),
                                      .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                      .pull_up_en = GPIO_PULLUP_DISABLE};

    gpio_config_t debugGpioInConf = {.intr_type = GPIO_INTR_DISABLE,
                                     .mode = GPIO_MODE_INPUT,
                                     .pin_bit_mask = (uint64_t)1 << GPIO_PIN_NUM_DEBUG_IN_1,
                                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                     .pull_up_en = GPIO_PULLUP_DISABLE};

    ESP_LOGI(TAG, "Initialize debug GPIOs");
    ESP_ERROR_CHECK(gpio_config(&debugGpioOutConf));
    ESP_ERROR_CHECK(gpio_config(&debugGpioInConf));
}

void DEBUG_SetGpioDebugOut1(void)
{
    dev->out_w1ts = GPIO_REGISTER_MASK_OUT_1;
}

void DEBUG_ResetGpioDebugOut1(void)
{
    dev->out_w1tc = GPIO_REGISTER_MASK_OUT_1;
}

void DEBUG_TogglepioDebugOut1(void)
{
    if (dev->out & GPIO_REGISTER_MASK_OUT_1)
    {
        dev->out_w1tc = GPIO_REGISTER_MASK_OUT_1;
    }
    else
    {
        dev->out_w1ts = GPIO_REGISTER_MASK_OUT_1;
    }
}

void DEBUG_SetGpioDebugOut2(void)
{
    dev->out_w1ts = GPIO_REGISTER_MASK_OUT_2;
}

void DEBUG_ResetGpioDebugOut2(void)
{
    dev->out_w1tc = GPIO_REGISTER_MASK_OUT_2;
}

void DEBUG_ToggleGpioDebugOut2(void)
{
    if (dev->out & GPIO_REGISTER_MASK_OUT_2)
    {
        dev->out_w1tc = GPIO_REGISTER_MASK_OUT_2;
    }
    else
    {
        dev->out_w1ts = GPIO_REGISTER_MASK_OUT_2;
    }
}

bool DEBUG_GetGpioDebugInState(void)
{
    return (bool)gpio_get_level(GPIO_PIN_NUM_DEBUG_IN_1);
}