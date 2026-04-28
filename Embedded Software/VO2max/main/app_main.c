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
#include "freertos/queue.h"
#include "bluetooth.h"
#include "settings.h"

#define MAIN_TASK_PERIOD_MS 3000

static const char *TAG = "[MAIN]";

void app_main(void)
{
    int8_t batterySoc;
    Settings_t settings;

    ESP_LOGI(TAG, "VO2max embedded software version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    ESP_LOGI(TAG, "Initializing system...");

    esp_timer_early_init(); // For time tracking

    SETTINGS_Initialize();
    SETTINGS_LoadSettings(&settings);

    BATTERY_Initialize();

    HMI_Initialize();

    MEASURE_Initialize();

    BLUETOOTH_Initialize();

    while (1)
    {
        BATTERY_MeasureSoc(&batterySoc);

        vTaskDelay(pdMS_TO_TICKS(MAIN_TASK_PERIOD_MS));
    }
}
