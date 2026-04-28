#include "bluetooth.h"
#include "esp_bt.h"
#include "esp_log.h"

const char *TAG = "BLE";

bool BLUETOOTH_Initialize()
{
    bool ret = false;

    esp_bt_controller_config_t btConfig = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    btConfig.mode = ESP_BT_MODE_BLE;

    ESP_ERROR_CHECK(esp_bt_controller_init(&btConfig));

    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)
    {
    };

    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_INITED)
    {
        ESP_LOGE(TAG, "Failed initializing BLE");
    }
    else
    {
        ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

        if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED)
        {
            ESP_LOGE(TAG, "Failed initializing BLE");
        }
        else
        {
            ret = true;
        }
    }

    return ret;
}