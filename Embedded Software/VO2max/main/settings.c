#include "settings.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define SETTINGS_NVS_NAMESPACE "vo2max"
#define SETTINGS_NVS_KEYNAME "settings"

static const char *TAG = "[SETTINGS]";
static nvs_handle_t nvsHandle;

void SETTINGS_Initialize(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle));
}

void SETTINGS_LoadSettings(Settings_t *settings)
{
    Settings_t settingsRead;
    size_t keySize;
    esp_err_t res;

    res = nvs_get_blob(nvsHandle, SETTINGS_NVS_KEYNAME, (void *)&settingsRead, &keySize);

    if ((ESP_OK != res) && (ESP_ERR_NVS_NOT_FOUND != res))
    {
        ESP_LOGE(TAG, "Unable to read NVS key: %d", res);
    }
    else
    {
        if (keySize == sizeof(Settings_t))
        {
            *settings = settingsRead;
        }
    }
}

void SETTINGS_SaveSettings(const Settings_t *settings)
{
    nvs_set_blob(nvsHandle, SETTINGS_NVS_KEYNAME, (void *)settings, sizeof(Settings_t));
}