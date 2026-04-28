/************************************
 * INCLUDES
 ************************************/

#include <math.h>

#include "hmi.h"
#include "driver_lcd.h"
#include "menu.h"
#include "driver/gpio.h"
#include "version.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "measure.h"
#include "battery.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define HMI_TASK_STACK_SIZE 8192
#define HMI_TASK_PERIOD_MS 100
#define HMI_TASK_PRIORITY 0

#define SPI_HOST SPI2_HOST

#define GPIO_PIN_NUM_SPI_MISO 25
#define GPIO_PIN_NUM_SPI_MOSI 19
#define GPIO_PIN_NUM_SPI_CLK 18

#define GPIO_PIN_NUM_BUTTON_1 35
#define GPIO_PIN_NUM_BUTTON_2 0

#define INITIALIZATION_TIMEOUT_MS 3000

#define SHORT_LONG_PRESS_THRESHOLD_US 400000

#define MENU_LIST_X_START 20
#define MENU_LIST_Y_START_POSITION 50
#define MENU_LIST_Y_STEP 24

/************************************
 * PRIVATE TYPEDEFS
 ************************************/

typedef enum
{
    BUTTON_NO_PRESS,
    BUTTON_SHORT_PRESS,
    BUTTON_LONG_PRESS
} PushButtonState_e;

/************************************
 * PRIVATE VARIABLES
 ************************************/

static const char *hmiTaskTag = "[HMI Task]";

/************************************
 * PUBLIC VARIABLES
 ************************************/

TaskHandle_t g_hmiTaskHandle = NULL;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void HmiTask(void *pvParameters);
static void NormalOperation();
static void DisplayBatterySoc(void);
static void DisplaySelected(uint8_t selected);
static void DisplayMenuName(Menu_t *menu);
static void DisplayPage(Menu_t *menu, uint8_t page);
static PushButtonState_e GetPushButton1State(void);
static PushButtonState_e GetPushButton2State(void);
static void LiveValuesScreenAction(void);
static void SpirometerScreenAction(void);
static void O2CalibrationScreenAction(void);
static void PressureCalibrationScreenAction(void);

/************************************
 * PUBLIC FUNCTION DEFINITIONS
 ************************************/

void HMI_Initialize()
{
    const char *hmiTag = "[HMI]";
    gpio_config_t pushButton1Conf = {.intr_type = GPIO_INTR_DISABLE,
                                     .mode = GPIO_MODE_INPUT,
                                     .pin_bit_mask = (uint64_t)1 << GPIO_PIN_NUM_BUTTON_1,
                                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                     .pull_up_en = GPIO_PULLUP_DISABLE};
    gpio_config_t pushButton2Conf = {.intr_type = GPIO_INTR_DISABLE,
                                     .mode = GPIO_MODE_INPUT,
                                     .pin_bit_mask = (uint64_t)1 << GPIO_PIN_NUM_BUTTON_2,
                                     .pull_down_en = GPIO_PULLDOWN_DISABLE,
                                     .pull_up_en = GPIO_PULLUP_ENABLE};
    spi_bus_config_t busConfig = {// SPI bus for LCD
                                  .miso_io_num = -1,
                                  .mosi_io_num = GPIO_PIN_NUM_SPI_MOSI,
                                  .sclk_io_num = GPIO_PIN_NUM_SPI_CLK,
                                  .quadwp_io_num = -1,
                                  .quadhd_io_num = -1,
                                  .max_transfer_sz = SCREEN_WIDTH * SCREEN_HEIGHT * 2 + 8};

    ESP_LOGI(hmiTag, "Initialize push-button 1");
    ESP_ERROR_CHECK(gpio_config(&pushButton1Conf));

    ESP_LOGI(hmiTag, "Initialize push-button 2");
    ESP_ERROR_CHECK(gpio_config(&pushButton2Conf));

    ESP_LOGI(hmiTag, "Initialize SPI bus for LCD display");
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &busConfig, SPI_DMA_CH1));

    xTaskCreate(HmiTask, "HMI Task", HMI_TASK_STACK_SIZE, NULL, HMI_TASK_PRIORITY, &g_hmiTaskHandle);
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static void HmiTask(void *pvParameters)
{
    char versionString[20];
    BaseType_t waitNotification = pdTRUE;

    ESP_LOGI(hmiTaskTag, "HMI initialization...");

    LCD_Initialize();

    LCD_String(0, 25, "VO2max", strlen("VO2max"), LCD_COLOR_BLACK, ST7789_FONT_24);

    snprintf(versionString, sizeof(versionString), "Version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    LCD_String(0, 50, versionString, strlen(versionString), LCD_COLOR_BLACK, ST7789_FONT_24);

    LCD_String(0, 75, "Initializing...", strlen("Initializing..."), LCD_COLOR_BLACK, ST7789_FONT_24);

    waitNotification &= xSemaphoreTake(g_flowInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_o2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_co2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    // waitNotification &= xSemaphoreTake(g_pressureInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));

    vTaskDelay(pdMS_TO_TICKS(1000));

    if (pdFALSE == waitNotification)
    {
        LCD_String(70, 110, "Error!", strlen("ERROR!"), LCD_COLOR_RED, ST7789_FONT_24);

        vTaskSuspend(g_hmiTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
        }
    }
    else
    {
        LCD_String(60, 110, "Success!", strlen("Success!"), LCD_COLOR_GREEN, ST7789_FONT_24);
        vTaskDelay(pdMS_TO_TICKS(3000));
        LCD_Clear();
        NormalOperation();
    }
}

static void MenuNavigate(Menu_t **menu, uint8_t *selected)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    pushButton1State = GetPushButton1State();

    if (BUTTON_SHORT_PRESS == pushButton1State)
    {
        if (0 < (*menu)->childrenCount)
        {
            if (0 == *selected)
            {
                *selected = (*menu)->childrenCount - 1;
            }
            else
            {
                (*selected)--;
            }
        }
    }
    else if (BUTTON_LONG_PRESS == pushButton1State)
    {
        if (NULL != (*menu)->parent)
        {
            *menu = (*menu)->parent;
            *selected = 0;
        }
    }

    pushButton2State = GetPushButton2State();

    if (BUTTON_SHORT_PRESS == pushButton2State)
    {
        if (0 < (*menu)->childrenCount)
        {
            if (*selected >= (*menu)->childrenCount - 1)
            {
                *selected = 0;
            }
            else
            {
                (*selected)++;
            }
        }
    }
    else if (BUTTON_LONG_PRESS == pushButton2State)
    {
        if (NULL != (*menu)->children[*selected])
        {
            *menu = (*menu)->children[*selected];
            *selected = 0;
        }
    }
}

static void NormalOperation(void)
{
    Menu_t mainMenu;
    Menu_t calibrationMenu;
    Menu_t vo2maxMenu;
    Menu_t spirometerMenu;
    Menu_t settingsMenu;
    Menu_t liveValuesMenu;
    Menu_t o2CalibrationMenu;
    Menu_t co2CalibrationMenu;
    Menu_t flowCalibrationMenu;
    Menu_t pressureCalibrationMenu;
    Menu_t *currentMenu;
    Menu_t *previousMenu;
    uint8_t selected = 0;
    uint8_t previousSelected = 0;
    uint8_t previousPage = 0;
    uint8_t page = 0;

    MENU_Create(&mainMenu, "VO2max");
    MENU_Create(&calibrationMenu, "Calibration");
    MENU_Create(&flowCalibrationMenu, "Flow");
    MENU_Create(&o2CalibrationMenu, "O2");
    MENU_Create(&co2CalibrationMenu, "CO2");
    MENU_Create(&pressureCalibrationMenu, "Pressure");
    MENU_Create(&vo2maxMenu, "VO2max");
    MENU_Create(&spirometerMenu, "Spirometer");
    MENU_Create(&settingsMenu, "Settings");
    MENU_Create(&liveValuesMenu, "Live values");

    MENU_AddSubmenu(&mainMenu, &calibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &o2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &co2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &flowCalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &pressureCalibrationMenu);
    MENU_AddSubmenu(&mainMenu, &vo2maxMenu);
    MENU_AddSubmenu(&mainMenu, &spirometerMenu);
    MENU_AddSubmenu(&mainMenu, &settingsMenu);
    MENU_AddSubmenu(&mainMenu, &liveValuesMenu);

    MENU_AddAction(&liveValuesMenu, LiveValuesScreenAction);
    MENU_AddAction(&spirometerMenu, SpirometerScreenAction);
    MENU_AddAction(&o2CalibrationMenu, O2CalibrationScreenAction);
    MENU_AddAction(&pressureCalibrationMenu, PressureCalibrationScreenAction);

    currentMenu = &mainMenu;

    while (1)
    {
        if (currentMenu->childrenCount == 0)
        {
            if (NULL != currentMenu->action)
            {
                currentMenu->action();
            }

            LCD_Clear();

            if (NULL != currentMenu->parent)
            {
                currentMenu = currentMenu->parent;
            }
            else // Should never happen
            {
                ESP_LOGE(hmiTaskTag, "Inconsistant state in HMI");
            }
        }
        else
        {
            DisplayMenuName(currentMenu);
            DisplayPage(currentMenu, 0);
            DisplaySelected(selected);

            previousMenu = currentMenu;

            do
            {
                DisplayBatterySoc();
                previousSelected = selected;
                previousMenu = currentMenu;
                previousPage = page;
                MenuNavigate(&currentMenu, &selected);
                page = selected / 4;

                if (page != previousPage)
                {
                    DisplayPage(currentMenu, page);
                }

                if (previousSelected != selected)
                {
                    DisplaySelected(selected);
                }

                vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
            } while (currentMenu == previousMenu);
        }
    }
}

static void DisplayBatterySoc(void)
{
    static int8_t previousBatterySoc = -128;
    int8_t batterySoc;

    if (pdPASS == xQueueReceive(g_batterySocQueue, (void *)&batterySoc, (TickType_t)0))
    {
        if (batterySoc != previousBatterySoc)
        {
            char batterySocString[7] = "";

            if (batterySoc >= 0)
            {
                snprintf(batterySocString, sizeof(batterySocString), "%3d%%", batterySoc);
            }
            else if (batterySoc == BATTERY_SOC_CHARGING)
            {
                snprintf(batterySocString, sizeof(batterySocString), "charge");
            }
            else if (batterySoc == BATTERY_SOC_USB_PLUGGED)
            {
                snprintf(batterySocString, sizeof(batterySocString), "usb");
            }
            else
            {
                ESP_LOGE(hmiTaskTag, "Invalid SOC value received: %d", batterySoc);
            }

            LCD_ClearString(170, 20, 7, LCD_COLOR_WHITE, ST7789_FONT_16);
            LCD_String(170, 20, batterySocString, strlen(batterySocString), LCD_COLOR_BLACK, ST7789_FONT_16);
            previousBatterySoc = batterySoc;
        }
    }
}

static void DisplayMenuName(Menu_t *menu)
{
    uint8_t length = strlen(menu->name);
    uint8_t xPos;

    xPos = (SCREEN_HEIGHT - 16 * length) / 2;

    ST7789_ClearString(0, 16, 18, LCD_COLOR_WHITE, ST7789_FONT_24);
    LCD_String(xPos, 16, menu->name, length, LCD_COLOR_BLACK, ST7789_FONT_24);
}

static void DisplayPage(Menu_t *menu, uint8_t page)
{
    uint8_t yPos = 0;
    uint8_t startIndex = 0;
    uint8_t endIndex = 0;

    yPos = 0;

    startIndex = page * 4;
    endIndex = startIndex + 4;

    if (endIndex > menu->childrenCount)
    {
        endIndex = menu->childrenCount;
    }

    LCD_Rectangle(30, 40, 239, 134, LCD_COLOR_WHITE);

    for (uint8_t i = startIndex; i < endIndex; i++)
    {
        LCD_String(MENU_LIST_X_START, MENU_LIST_Y_START_POSITION + yPos * MENU_LIST_Y_STEP, menu->children[i]->name, strlen(menu->children[i]->name), LCD_COLOR_BLACK, ST7789_FONT_24);
        yPos++;
    }
}

static void DisplaySelected(uint8_t selected)
{
    uint8_t selectedPos = selected % 4;

    LCD_Rectangle(0, 40, 25, 134, LCD_COLOR_WHITE);

    LCD_String(0, MENU_LIST_Y_START_POSITION + selectedPos * MENU_LIST_Y_STEP, ">", 1, LCD_COLOR_RED, ST7789_FONT_24);
}

static PushButtonState_e GetPushButton1State(void)
{
    PushButtonState_e ret = BUTTON_NO_PRESS;
    bool pushButton1State = false;
    static bool previousPushButton1State = false;
    static int64_t pressDurationButton1 = 0;
    static int64_t pressTimestampButton1 = 0;

    pushButton1State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_1);

    if ((pushButton1State == true) && (previousPushButton1State == false))
    {
        pressTimestampButton1 = esp_timer_get_time();
    }
    else if ((pushButton1State == false) && (previousPushButton1State == true))
    {
        pressDurationButton1 = esp_timer_get_time() - pressTimestampButton1;

        if (pressDurationButton1 < SHORT_LONG_PRESS_THRESHOLD_US) // Short click
        {
            ret = BUTTON_SHORT_PRESS;
        }
        else
        {
            ret = BUTTON_LONG_PRESS;
        }
    }

    previousPushButton1State = pushButton1State;

    return ret;
}

static PushButtonState_e GetPushButton2State(void)
{
    PushButtonState_e ret = BUTTON_NO_PRESS;
    bool pushButton2State = false;
    static bool previousPushButton2State = false;
    static int64_t pressDurationButton2 = 0;
    static int64_t pressTimestampButton2 = 0;

    pushButton2State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_2);

    if ((pushButton2State == true) && (previousPushButton2State == false))
    {
        pressTimestampButton2 = esp_timer_get_time();
    }
    else if ((pushButton2State == false) && (previousPushButton2State == true))
    {
        pressDurationButton2 = esp_timer_get_time() - pressTimestampButton2;

        if (pressDurationButton2 < SHORT_LONG_PRESS_THRESHOLD_US) // Short click
        {
            ret = BUTTON_SHORT_PRESS;
        }
        else
        {
            ret = BUTTON_LONG_PRESS;
        }
    }

    previousPushButton2State = pushButton2State;

    return ret;
}

static void O2CalibrationScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    static float o2Value = 20.9f;
    char o2String[6];
    bool exit = false;

    LCD_Clear();

    LCD_String(30, 16, "O2 Calibration", 14, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 48, "cal =          %", 16, LCD_COLOR_BLACK, ST7789_FONT_24);
    snprintf(o2String, sizeof(o2String), "%2.1f", o2Value);
    LCD_String(80, 48, o2String, 4, LCD_COLOR_BLACK, ST7789_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            o2Value += 0.1f;
            snprintf(o2String, sizeof(o2String), "%2.1f", o2Value);
            ST7789_ClearString(80, 48, 4, LCD_COLOR_WHITE, ST7789_FONT_24);
            LCD_String(80, 48, o2String, 4, LCD_COLOR_BLACK, ST7789_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            o2Value -= 0.1f;
            snprintf(o2String, sizeof(o2String), "%2.1f", o2Value);
            ST7789_ClearString(80, 48, 4, LCD_COLOR_WHITE, ST7789_FONT_24);
            LCD_String(80, 48, o2String, 4, LCD_COLOR_BLACK, ST7789_FONT_24);
        }
        else if (BUTTON_LONG_PRESS == pushButton1State)
        {
            exit = true;
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void PressureCalibrationScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    static float pressureValue = 1013.0f;
    char pressureString[8];
    bool exit = false;

    LCD_Clear();

    LCD_String(50, 12, "Pressure", 8, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(40, 36, "Calibration", 11, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 72, "cal =          hPa", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
    LCD_String(80, 72, pressureString, 7, LCD_COLOR_BLACK, ST7789_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            pressureValue += 0.25f;
            snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
            ST7789_ClearString(80, 72, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
            LCD_String(80, 72, pressureString, 7, LCD_COLOR_BLACK, ST7789_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            pressureValue -= 0.25f;
            snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
            ST7789_ClearString(80, 72, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
            LCD_String(80, 72, pressureString, 7, LCD_COLOR_BLACK, ST7789_FONT_24);
        }
        else if (BUTTON_LONG_PRESS == pushButton1State)
        {
            exit = true;
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void LiveValuesScreenAction(void)
{
    float flow;
    float previousFlow = FLT_MAX;
    float o2;
    float previousO2 = FLT_MAX;
    float co2;
    float previousCo2 = FLT_MAX;
    float temperature;
    float previousTemperature = FLT_MAX;
    float humidity;
    float previousHumidity = FLT_MAX;
    float pressure;
    float previousPressure = FLT_MAX;
    char string[10];

    LCD_Clear();

    LCD_String(0, 12, "O2  =            %", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 34, "CO2 =          ppm", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 57, "Q   =        L/min", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 80, "T   =            C", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 103, "H   =            %", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 126, "P   =          hPa", 18, LCD_COLOR_BLACK, ST7789_FONT_24);

    while (BUTTON_LONG_PRESS != GetPushButton1State())
    {
        if (pdPASS == xQueueReceive(g_o2Queue, (void *)&o2, (TickType_t)0))
        {
            if (o2 != previousO2)
            {
                snprintf(string, sizeof(string), "%.1f", o2);
                LCD_ClearString(70, 12, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 12, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousO2 = o2;
            }
        }
        if (pdPASS == xQueueReceive(g_co2Queue, (void *)&co2, (TickType_t)0))
        {
            if (co2 != previousCo2)
            {
                snprintf(string, sizeof(string), "%-5.0f", co2);
                LCD_ClearString(70, 34, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 34, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousCo2 = co2;
            }
        }
        if (pdPASS == xQueueReceive(g_flowQueue, (void *)&flow, (TickType_t)0))
        {
            if (flow != previousFlow)
            {
                snprintf(string, sizeof(string), "%-5.1f", flow);
                LCD_ClearString(70, 57, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 57, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousFlow = flow;
            }
        }
        if (pdPASS == xQueueReceive(g_temperatureQueue, (void *)&temperature, (TickType_t)0))
        {
            if (temperature != previousTemperature)
            {
                snprintf(string, sizeof(string), "%-5.1f", temperature);
                LCD_ClearString(70, 80, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 80, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousTemperature = temperature;
            }
        }
        if (pdPASS == xQueueReceive(g_humidityQueue, (void *)&humidity, (TickType_t)0))
        {
            if (humidity != previousHumidity)
            {
                snprintf(string, sizeof(string), "%-5.0f", humidity);
                LCD_ClearString(70, 103, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 103, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousHumidity = humidity;
            }
        }
        if (pdPASS == xQueueReceive(g_pressureQueue, (void *)&pressure, (TickType_t)0))
        {
            if (pressure != previousPressure)
            {
                snprintf(string, sizeof(string), "%-5.1f", pressure);
                LCD_ClearString(70, 126, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
                LCD_String(70, 126, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
                previousPressure = pressure;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void SpirometerScreenAction(void)
{
    float cycleExhaledVolume;
    float totalExhaledVolume;
    char string[30];

    LCD_Clear();

    LCD_String(60, 24, "Spirometer", strlen("Spirometer"), LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 72, "VOLcyc =         L", 18, LCD_COLOR_BLACK, ST7789_FONT_24);
    LCD_String(0, 96, "VOLtot =         L", 18, LCD_COLOR_BLACK, ST7789_FONT_24);

    while (BUTTON_LONG_PRESS != GetPushButton1State())
    {
        if (pdPASS == xQueueReceive(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume, (TickType_t)0))
        {
            LCD_ClearString(120, 72, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
            snprintf(string, sizeof(string), "%5.1f", cycleExhaledVolume);
            LCD_String(130, 72, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
        }
        if (pdPASS == xQueueReceive(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume, (TickType_t)0))
        {
            LCD_ClearString(120, 96, 7, LCD_COLOR_WHITE, ST7789_FONT_24);
            snprintf(string, sizeof(string), "%5.1f", totalExhaledVolume);
            LCD_String(130, 96, string, strlen(string), LCD_COLOR_BLACK, ST7789_FONT_24);
        }
    }
}
