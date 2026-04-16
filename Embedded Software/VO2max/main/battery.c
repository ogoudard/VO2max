#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "soc/adc_channel.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"

static const char *TAG = "BATTERY";

#define ADC_CHANNEL_VBAT ADC1_GPIO33_CHANNEL

#define VOLTAGE_DIVIDER_VBAT_R6 100000.0f
#define VOLTAGE_DIVIDER_VBAT_R7 100000.0f

#define ADC_ATTENUATION_INV 5.0f
#define VOLTAGE_DIVIDER_RATIO_INV (1.0f + VOLTAGE_DIVIDER_VBAT_R6 / VOLTAGE_DIVIDER_VBAT_R7)

#define ADC_VOLTAGE_TO_BATTERY_VOLTAGE(x) (float)x * ADC_ATTENUATION_INV * VOLTAGE_DIVIDER_RATIO_INV / 1000.0f
static adc_oneshot_unit_handle_t adcHandle;
static adc_cali_handle_t adcCalibration;

void BATTERY_Initialize()
{
    adc_oneshot_unit_init_cfg_t unitConfig = {.unit_id = ADC_UNIT_1, .clk_src = 0, .ulp_mode = ADC_ULP_MODE_DISABLE};
    adc_oneshot_chan_cfg_t channelConfig = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12};

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitConfig, &adcHandle));

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcHandle, ADC_CHANNEL_VBAT, &channelConfig));
}

float BATTERY_MeasureSoc()
{
    int adcReading;
    float batterySoc;
    int adcVoltagemV;
    float batteryVoltage;

    adc_oneshot_read(adcHandle, ADC_CHANNEL_VBAT, &adcReading);

    batteryVoltage = ADC_VOLTAGE_TO_BATTERY_VOLTAGE(adcReading);

    ESP_LOGI(TAG, "Battery voltage = %f V", batteryVoltage);
    return batteryVoltage * 71.42f - 214.25f;
}