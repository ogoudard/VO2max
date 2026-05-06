#include "settings.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define SETTINGS_NVS_NAMESPACE "vo2max"
#define SETTINGS_NVS_KEYNAME "settings"

Settings_t g_settings;

static const char *TAG = "[SETTINGS]";
static nvs_handle_t nvsHandle;
static const Settings_t defaultSettings = {.bleOn = false, .co2Calibration = 400.0f, .flowCalibration = 1.0f, .o2Calibration = 20.9f, .userWeight = 70.0f};

void SETTINGS_Initialize(void)
{
    nvs_stats_t nvsStats;
    Settings_t settingsRead;
    size_t keySize = sizeof(Settings_t);

    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &nvsHandle));

    ESP_ERROR_CHECK(nvs_get_stats(NULL, &nvsStats));

    ESP_LOGI(TAG, "Count: UsedEntries = (%lu), FreeEntries = (%lu), AvailableEntries = (%lu), AllEntries = (%lu)",
             nvsStats.used_entries, nvsStats.free_entries, nvsStats.available_entries, nvsStats.total_entries);

    if (ESP_OK != nvs_find_key(nvsHandle, SETTINGS_NVS_KEYNAME, NULL))
    {
        ESP_LOGW(TAG, "No settings found in flash, writing default settings");
        g_settings = defaultSettings;
        SETTINGS_SaveSettings();
    }
    else
    {
        ESP_ERROR_CHECK(nvs_get_blob(nvsHandle, SETTINGS_NVS_KEYNAME, (void *)&settingsRead, &keySize));

        if (keySize != sizeof(Settings_t))
        {
            ESP_LOGE(TAG, "Invalid key size, settings default parameters");
            nvs_erase_key(nvsHandle, SETTINGS_NVS_KEYNAME);
            g_settings = defaultSettings;
            SETTINGS_SaveSettings();
        }
    }
}

void SETTINGS_LoadSettings()
{
    Settings_t settingsRead;
    size_t keySize = sizeof(Settings_t);
    esp_err_t res;

    res = nvs_get_blob(nvsHandle, SETTINGS_NVS_KEYNAME, (void *)&settingsRead, &keySize);

    if (ESP_OK != res)
    {
        ESP_LOGE(TAG, "Unable to read NVS key: %s", SETTINGS_NVS_KEYNAME);
    }
    else
    {
        if (keySize == sizeof(Settings_t))
        {
            g_settings = settingsRead;
        }
    }
}

void SETTINGS_SaveSettings()
{
    if (ESP_OK != nvs_set_blob(nvsHandle, SETTINGS_NVS_KEYNAME, (void *)&g_settings, sizeof(Settings_t)))
    {
        ESP_LOGE(TAG, "nvs_set_blob failed");
    }
    else
    {
        if (ESP_OK != nvs_commit(nvsHandle))
        {
            ESP_LOGE(TAG, "nvs_commit failed");
        }
    }
}