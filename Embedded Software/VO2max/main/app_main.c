/************************************
 * INCLUDES
 ************************************/

#include <stdio.h>

#include "version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "battery.h"
#include "esp_timer.h"
#include "hmi.h"
#include "measure.h"

static const char *TAG = "[MAIN]";

void app_main(void)
{
    float batterySoc;

    ESP_LOGI(TAG, "VO2max embedded software version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    ESP_LOGI(TAG, "Initializing system...");

    esp_timer_early_init(); // For time tracking

    BATTERY_Initialize();
    batterySoc = BATTERY_MeasureSoc();
    ESP_LOGI(TAG, "Battery SOC = %.0f %%", batterySoc);

    HMI_Initialize();

    MEASURE_Initialize();

    vTaskSuspend(xTaskGetCurrentTaskHandle());

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
