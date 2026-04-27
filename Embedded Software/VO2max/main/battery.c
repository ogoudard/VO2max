/************************************
 * INCLUDES
 ************************************/

#include "battery.h"
#include "esp_adc/adc_oneshot.h"
#include "soc/adc_channel.h"
#include "esp_log.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define ADC_CHANNEL_VBAT ADC1_GPIO34_CHANNEL

#define VOLTAGE_DIVIDER_VBAT_R6 100000.0f
#define VOLTAGE_DIVIDER_VBAT_R7 100000.0f

#define ADC_ATTENUATION_INV 5.0f
#define VOLTAGE_DIVIDER_RATIO_INV (1.0f + VOLTAGE_DIVIDER_VBAT_R6 / VOLTAGE_DIVIDER_VBAT_R7)

#define ADC_VOLTAGE_MV_TO_BATTERY_VOLTAGE_V(x) (float)x *VOLTAGE_DIVIDER_RATIO_INV / 1000.0f

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *TAG = "[BATTERY]";
static adc_oneshot_unit_handle_t adcHandle = NULL;
static adc_cali_handle_t calibrationHandle = NULL;

/************************************
 * PUBLIC VARIABLES
 ************************************/

QueueHandle_t g_batterySocQueue;

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void BATTERY_Initialize()
{
    adc_oneshot_unit_init_cfg_t unitConfig = {.unit_id = ADC_UNIT_1, .clk_src = 0, .ulp_mode = ADC_ULP_MODE_DISABLE};
    adc_oneshot_chan_cfg_t channelConfig = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    adc_cali_line_fitting_config_t calibrationConfig = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT};

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unitConfig, &adcHandle));

    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&calibrationConfig, &calibrationHandle));

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adcHandle, ADC_CHANNEL_VBAT, &channelConfig));

    g_batterySocQueue = xQueueCreate(1, sizeof(float));
}

bool BATTERY_MeasureSoc(float *soc)
{
    bool ret = false;
    int adcReading;

    if (ESP_OK != adc_oneshot_read(adcHandle, ADC_CHANNEL_VBAT, &adcReading))
    {
        ESP_LOGE(TAG, "ADC conversion failed");
    }
    else
    {
        float batteryVoltage;
        int adcVoltage;

        adc_cali_raw_to_voltage(calibrationHandle, adcReading, &adcVoltage);
        batteryVoltage = ADC_VOLTAGE_MV_TO_BATTERY_VOLTAGE_V(adcVoltage);

        if(batteryVoltage > 4.5f)
        {
            *soc = -1.0f; // Charging
        }
        else
        {
            *soc = batteryVoltage * 135.0f - 467.0f;
            if (*soc > 100)
            {
                *soc = 100;
            }
        }

        xQueueOverwrite(g_batterySocQueue, (void *)soc);

        ret = true;
    }

    return ret;
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/
