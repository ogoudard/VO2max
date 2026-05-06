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
#include "driver/spi_master.h"
#include "settings.h"

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

#define MENU_NAME_POSITION_Y 10
#define MENU_LIST_X_START 30
#define MENU_LIST_Y_START 30
#define MENU_LIST_Y_STEP 24

#define CENTER_X(str) (SCREEN_HEIGHT - 12 * strlen(str)) / 2

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

static Menu_t mainMenu;
static Menu_t calibrationMenu;
static Menu_t vo2maxMenu;
static Menu_t spirometerMenu;
static Menu_t settingsMenu;
static Menu_t liveValuesMenu;
static Menu_t o2CalibrationMenu;
static Menu_t co2CalibrationMenu;
static Menu_t flowCalibrationMenu;
static Menu_t pressureCalibrationMenu;
static Menu_t bluetoothSettingsMenu;
static Menu_t measureSettingsMenu;
static Menu_t userSettingsMenu;
static Menu_t bluetoothEnableMenu;
static Menu_t bluetoothMacMenu;
static Menu_t bluetoothNameMenu;
static Menu_t bluetoothPairMenu;
static Menu_t weightMenu;
static Menu_t acquisitionMenu;

/************************************
 * PUBLIC VARIABLES
 ************************************/

TaskHandle_t g_hmiTaskHandle = NULL;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void HmiTask(void *pvParameters);
static void InitializeMenus(void);
static void NormalOperation(void);
static void DisplayBatterySoc(void);
static void DisplaySelected(uint8_t selected);
static void DisplayMenuName(Menu_t *menu);
static void DisplayPage(Menu_t *menu, uint8_t page);
static PushButtonState_e GetPushButton1State(void);
static PushButtonState_e GetPushButton2State(void);
static void LiveValuesScreenAction(void);
static void SpirometerScreenAction(void);
static void Vo2MaxScreenAction(void);
static void O2CalibrationScreenAction(void);
static void FlowCalibrationScreenAction(void);
static void PressureCalibrationScreenAction(void);
static void UserWeightScreenAction(void);
static void bleEnableScreenAction(void);

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
    LCD_SetBacklight(true);
    LCD_SetOrientation(LCD_ORIENTATION_LANDSCAPE_1);
    LCD_DisplayOn();
    LCD_Clear();

    LCD_DrawString(10, 10, "VO2max", LCD_COLOR_BLACK, LCD_FONT_24);

    snprintf(versionString, sizeof(versionString), "Version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    LCD_DrawString(10, 34, versionString, LCD_COLOR_BLACK, LCD_FONT_24);

    LCD_DrawString(10, 58, "Initializing...", LCD_COLOR_BLACK, LCD_FONT_24);

    waitNotification &= xSemaphoreTake(g_flowInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_o2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_co2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    // waitNotification &= xSemaphoreTake(g_pressureInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));

    if (pdFALSE == waitNotification)
    {
        LCD_DrawString((SCREEN_HEIGHT - 12 * strlen("Error!")) / 2, 90, "Error!", LCD_COLOR_RED, LCD_FONT_24);

        vTaskSuspend(g_hmiTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
        }
    }
    else
    {
        LCD_DrawString((SCREEN_HEIGHT - 12 * strlen("Success!")) / 2, 90, "Success!", LCD_COLOR_GREEN, LCD_FONT_24);
        vTaskDelay(pdMS_TO_TICKS(1000));
        InitializeMenus();
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

static void InitializeMenus(void)
{
    // Main menu
    MENU_Create(&mainMenu, "Main");
    MENU_Create(&vo2maxMenu, "VO2max");
    MENU_Create(&spirometerMenu, "Spirometer");
    MENU_Create(&liveValuesMenu, "Live values");
    MENU_Create(&settingsMenu, "Settings");

    MENU_AddSubmenu(&mainMenu, &vo2maxMenu);
    MENU_AddSubmenu(&mainMenu, &spirometerMenu);
    MENU_AddSubmenu(&mainMenu, &liveValuesMenu);
    MENU_AddSubmenu(&mainMenu, &settingsMenu);

    // Settings menu
    MENU_Create(&measureSettingsMenu, "Measure");
    MENU_Create(&bluetoothSettingsMenu, "Bluetooth");
    MENU_Create(&userSettingsMenu, "User");
    MENU_AddSubmenu(&settingsMenu, &measureSettingsMenu);
    MENU_AddSubmenu(&settingsMenu, &bluetoothSettingsMenu);
    MENU_AddSubmenu(&settingsMenu, &userSettingsMenu);

    // Bluetooth menu
    MENU_Create(&bluetoothEnableMenu, "BLE Enabling");
    MENU_Create(&bluetoothNameMenu, "Name");
    MENU_Create(&bluetoothMacMenu, "MAC");
    MENU_Create(&bluetoothPairMenu, "Pair");
    MENU_AddSubmenu(&bluetoothSettingsMenu, &bluetoothEnableMenu);
    MENU_AddSubmenu(&bluetoothSettingsMenu, &bluetoothNameMenu);
    MENU_AddSubmenu(&bluetoothSettingsMenu, &bluetoothMacMenu);
    MENU_AddSubmenu(&bluetoothSettingsMenu, &bluetoothPairMenu);

    // Measure menu
    MENU_Create(&calibrationMenu, "Calibration");
    MENU_Create(&acquisitionMenu, "Acquisition");
    MENU_AddSubmenu(&measureSettingsMenu, &calibrationMenu);
    MENU_AddSubmenu(&measureSettingsMenu, &acquisitionMenu);

    // Calibration Menu
    MENU_Create(&flowCalibrationMenu, "Flow");
    MENU_Create(&o2CalibrationMenu, "O2");
    MENU_Create(&co2CalibrationMenu, "CO2");
    MENU_Create(&pressureCalibrationMenu, "Pressure");
    MENU_AddSubmenu(&calibrationMenu, &o2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &co2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &flowCalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &pressureCalibrationMenu);

    // User settings menu
    MENU_Create(&weightMenu, "Weight");
    MENU_AddSubmenu(&userSettingsMenu, &weightMenu);

    // Actions
    MENU_AddAction(&liveValuesMenu, LiveValuesScreenAction);
    MENU_AddAction(&spirometerMenu, SpirometerScreenAction);
    MENU_AddAction(&o2CalibrationMenu, O2CalibrationScreenAction);
    MENU_AddAction(&pressureCalibrationMenu, PressureCalibrationScreenAction);
    MENU_AddAction(&weightMenu, UserWeightScreenAction);
    MENU_AddAction(&bluetoothEnableMenu, bleEnableScreenAction);
    MENU_AddAction(&flowCalibrationMenu, FlowCalibrationScreenAction);
    MENU_AddAction(&vo2maxMenu, Vo2MaxScreenAction);
}

static void NormalOperation(void)
{
    Menu_t *currentMenu;
    Menu_t *previousMenu;
    uint8_t selected = 0;
    uint8_t previousSelected = 0;
    uint8_t previousPage = 0;
    uint8_t page = 0;

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

    if (pdPASS == xQueuePeek(g_batterySocQueue, (void *)&batterySoc, (TickType_t)0))
    {
        if (batterySoc != previousBatterySoc)
        {
            char batterySocString[7] = "";

            if (batterySoc >= 0)
            {
                snprintf(batterySocString, sizeof(batterySocString), " %3d%%", batterySoc);
            }
            else if (batterySoc == BATTERY_SOC_CHARGING)
            {
                strcpy(batterySocString, "charge");
            }
            else if (batterySoc == BATTERY_SOC_USB_PLUGGED)
            {
                strcpy(batterySocString, "   usb");
            }
            else
            {
                ESP_LOGE(hmiTaskTag, "Invalid SOC value received: %d", batterySoc);
            }

            LCD_ClearString(190, 5, sizeof(batterySocString), LCD_COLOR_WHITE, LCD_FONT_16);
            LCD_DrawString(190, 5, batterySocString, LCD_COLOR_BLACK, LCD_FONT_16);
            previousBatterySoc = batterySoc;
        }
    }
}

static void DisplayMenuName(Menu_t *menu)
{
    LCD_ClearString(10, MENU_NAME_POSITION_Y, 18, LCD_COLOR_WHITE, LCD_FONT_24);
    LCD_DrawString(CENTER_X(menu->name), 5, menu->name, LCD_COLOR_BLACK, LCD_FONT_24);
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

    LCD_FillRectangle(MENU_LIST_X_START, MENU_LIST_Y_START, 239, 134, LCD_COLOR_WHITE);

    for (uint8_t i = startIndex; i < endIndex; i++)
    {
        LCD_DrawString(MENU_LIST_X_START, MENU_LIST_Y_START + yPos * MENU_LIST_Y_STEP, menu->children[i]->name, LCD_COLOR_BLACK, LCD_FONT_24);
        yPos++;
    }
}

static void DisplaySelected(uint8_t selected)
{
    uint8_t selectedPos = selected % 4;

    LCD_FillRectangle(0, MENU_LIST_Y_START, MENU_LIST_X_START, SCREEN_WIDTH - 1, LCD_COLOR_WHITE);

    LCD_DrawString(10, MENU_LIST_Y_START + selectedPos * MENU_LIST_Y_STEP, ">", LCD_COLOR_RED, LCD_FONT_24);
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

static void bleEnableScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    char enabledString[] = "ENABLED ";
    char disabledString[] = "DISABLED";
    bool exit = false;
    bool bleOn;

    bleOn = g_settings.bleOn;

    LCD_Clear();

    LCD_DrawString(CENTER_X("BLE Enable"), MENU_NAME_POSITION_Y, "BLE Enable", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(20, 48, "BLE", LCD_COLOR_BLACK, LCD_FONT_24);

    if (true == bleOn)
    {
        LCD_DrawString(80, 48, enabledString, LCD_COLOR_BLACK, LCD_FONT_24);
    }
    else
    {
        LCD_DrawString(80, 48, disabledString, LCD_COLOR_BLACK, LCD_FONT_24);
    }

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            if (true == bleOn)
            {
                bleOn = false;
                LCD_ClearString(80, 48, 9, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 48, disabledString, LCD_COLOR_BLACK, LCD_FONT_24);
            }
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            if (false == bleOn)
            {
                bleOn = true;
                LCD_ClearString(80, 48, 9, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 48, enabledString, LCD_COLOR_BLACK, LCD_FONT_24);
            }
        }

        exit = (BUTTON_LONG_PRESS == pushButton1State) || (BUTTON_LONG_PRESS == pushButton2State);

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }

    if (BUTTON_LONG_PRESS == pushButton2State)
    {
        g_settings.bleOn = bleOn;
        SETTINGS_SaveSettings();
    }
}

static void O2CalibrationScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    float o2CalValue;
    char o2CalString[6];
    bool exit = false;

    o2CalValue = g_settings.o2Calibration;

    LCD_Clear();

    LCD_DrawString(CENTER_X("O2 Calibration"), MENU_NAME_POSITION_Y, "O2 Calibration", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 48, "cal =          %", LCD_COLOR_BLACK, LCD_FONT_24);
    snprintf(o2CalString, sizeof(o2CalString), "%2.1f", o2CalValue);
    LCD_DrawString(90, 48, o2CalString, LCD_COLOR_BLACK, LCD_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            o2CalValue += 0.1f;
            snprintf(o2CalString, sizeof(o2CalString), "%2.1f", o2CalValue);
            LCD_ClearString(90, 48, 4, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(90, 48, o2CalString, LCD_COLOR_BLACK, LCD_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            o2CalValue -= 0.1f;
            snprintf(o2CalString, sizeof(o2CalString), "%2.1f", o2CalValue);
            LCD_ClearString(90, 48, 4, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(90, 48, o2CalString, LCD_COLOR_BLACK, LCD_FONT_24);
        }

        exit = (BUTTON_LONG_PRESS == pushButton1State) || (BUTTON_LONG_PRESS == pushButton2State);

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }

    if (BUTTON_LONG_PRESS == pushButton2State)
    {
        g_settings.o2Calibration = o2CalValue;
        SETTINGS_SaveSettings();
    }
}

static void FlowCalibrationScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    float flowCalValue;
    char flowCalString[6];
    bool exit = false;

    flowCalValue = g_settings.flowCalibration;

    LCD_Clear();

    LCD_DrawString(CENTER_X("Flow Calibration"), MENU_NAME_POSITION_Y, "Flow Calibration", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 48, "cal =          ", LCD_COLOR_BLACK, LCD_FONT_24);
    snprintf(flowCalString, sizeof(flowCalString), "%2.2f", flowCalValue);
    LCD_DrawString(90, 48, flowCalString, LCD_COLOR_BLACK, LCD_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            flowCalValue += 0.01f;
            snprintf(flowCalString, sizeof(flowCalString), "%2.2f", flowCalValue);
            LCD_ClearString(90, 48, sizeof(flowCalString), LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(90, 48, flowCalString, LCD_COLOR_BLACK, LCD_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            flowCalValue -= 0.01f;
            snprintf(flowCalString, sizeof(flowCalString), "%2.2f", flowCalValue);
            LCD_ClearString(90, 48, sizeof(flowCalString), LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(90, 48, flowCalString, LCD_COLOR_BLACK, LCD_FONT_24);
        }

        exit = (BUTTON_LONG_PRESS == pushButton1State) || (BUTTON_LONG_PRESS == pushButton2State);

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }

    if (BUTTON_LONG_PRESS == pushButton2State)
    {
        g_settings.flowCalibration = flowCalValue;
        SETTINGS_SaveSettings();
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

    LCD_DrawString(CENTER_X("Pressure"), MENU_NAME_POSITION_Y, "Pressure", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(CENTER_X("Calibration"), MENU_NAME_POSITION_Y + 24, "Calibration", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 72, "cal =          hPa", LCD_COLOR_BLACK, LCD_FONT_24);
    snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
    LCD_DrawString(80, 72, pressureString, LCD_COLOR_BLACK, LCD_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            pressureValue += 0.25f;
            snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
            LCD_ClearString(80, 72, 7, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(80, 72, pressureString, LCD_COLOR_BLACK, LCD_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            pressureValue -= 0.25f;
            snprintf(pressureString, sizeof(pressureString), "%4.2f", pressureValue);
            LCD_ClearString(80, 72, 7, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(80, 72, pressureString, LCD_COLOR_BLACK, LCD_FONT_24);
        }

        exit = (BUTTON_LONG_PRESS == pushButton1State);

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void UserWeightScreenAction(void)
{
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    float weightValue;
    char weightString[6];
    bool exit = false;

    weightValue = g_settings.userWeight;

    LCD_Clear();

    LCD_DrawString(CENTER_X("User Weight"), MENU_NAME_POSITION_Y, "User Weight", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 48, "weight =         kg", LCD_COLOR_BLACK, LCD_FONT_24);
    snprintf(weightString, sizeof(weightString), "%3.1f", weightValue);
    LCD_DrawString(120, 48, weightString, LCD_COLOR_BLACK, LCD_FONT_24);

    while (false == exit)
    {
        pushButton1State = GetPushButton1State();
        pushButton2State = GetPushButton2State();

        if (BUTTON_SHORT_PRESS == pushButton1State)
        {
            weightValue += 0.1f;
            snprintf(weightString, sizeof(weightString), "%3.1f", weightValue);
            LCD_ClearString(120, 48, 5, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(120, 48, weightString, LCD_COLOR_BLACK, LCD_FONT_24);
        }
        else if (BUTTON_SHORT_PRESS == pushButton2State)
        {
            weightValue -= 0.1f;
            snprintf(weightString, sizeof(weightString), "%3.1f", weightValue);
            LCD_ClearString(120, 48, 5, LCD_COLOR_WHITE, LCD_FONT_24);
            LCD_DrawString(120, 48, weightString, LCD_COLOR_BLACK, LCD_FONT_24);
        }

        exit = (BUTTON_LONG_PRESS == pushButton1State) || (BUTTON_LONG_PRESS == pushButton2State);

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }

    if (BUTTON_LONG_PRESS == pushButton2State)
    {
        g_settings.userWeight = weightValue;
        SETTINGS_SaveSettings();
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

    LCD_DrawString(5, 0, "O2  =             %", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(5, 22, "CO2 =           ppm", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(5, 44, "Q   =         L/min", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(5, 66, "T   =             C", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(5, 88, "H   =             %", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(5, 110, "P   =           hPa", LCD_COLOR_BLACK, LCD_FONT_24);

    while (BUTTON_LONG_PRESS != GetPushButton1State())
    {
        if (pdPASS == xQueuePeek(g_o2Queue, (void *)&o2, (TickType_t)0))
        {
            if (o2 != previousO2)
            {
                snprintf(string, sizeof(string), "%.1f", o2);
                LCD_ClearString(80, 0, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 0, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousO2 = o2;
            }
        }
        if (pdPASS == xQueuePeek(g_co2Queue, (void *)&co2, (TickType_t)0))
        {
            if (co2 != previousCo2)
            {
                snprintf(string, sizeof(string), "%-5.0f", co2);
                LCD_ClearString(80, 22, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 22, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousCo2 = co2;
            }
        }
        if (pdPASS == xQueuePeek(g_flowQueue, (void *)&flow, (TickType_t)0))
        {
            if (flow != previousFlow)
            {
                snprintf(string, sizeof(string), "%-5.1f", flow);
                LCD_ClearString(80, 44, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 44, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousFlow = flow;
            }
        }
        if (pdPASS == xQueuePeek(g_temperatureQueue, (void *)&temperature, (TickType_t)0))
        {
            if (temperature != previousTemperature)
            {
                snprintf(string, sizeof(string), "%-5.1f", temperature);
                LCD_ClearString(80, 66, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 66, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousTemperature = temperature;
            }
        }
        if (pdPASS == xQueuePeek(g_humidityQueue, (void *)&humidity, (TickType_t)0))
        {
            if (humidity != previousHumidity)
            {
                snprintf(string, sizeof(string), "%-5.0f", humidity);
                LCD_ClearString(80, 88, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 88, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousHumidity = humidity;
            }
        }
        if (pdPASS == xQueuePeek(g_pressureQueue, (void *)&pressure, (TickType_t)0))
        {
            if (pressure != previousPressure)
            {
                snprintf(string, sizeof(string), "%-5.1f", pressure);
                LCD_ClearString(80, 110, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                LCD_DrawString(80, 110, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousPressure = pressure;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void SpirometerScreenAction(void)
{
    float cycleExhaledVolume;
    float previousCycleExhaledVolume = -1.0f;
    float totalExhaledVolume;
    float previousTotalExhaledVolume = -1.0f;
    float respiratoryRate;
    float previousRespriatoryRate = -1.0f;
    char string[8];

    LCD_Clear();

    LCD_DrawString(CENTER_X("Spirometer"), MENU_NAME_POSITION_Y, "Spirometer", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 40, "VOLcyc =     0.0 L", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 64, "VOLtot =     0.0 L", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 88, "Rate =   0.0 br/min", LCD_COLOR_BLACK, LCD_FONT_24);

    LCD_DrawString(180, 120, "RESET >", LCD_COLOR_BLACK, LCD_FONT_16);

    while (BUTTON_LONG_PRESS != GetPushButton1State())
    {
        if (GetPushButton2State() == BUTTON_LONG_PRESS)
        {
            xSemaphoreGive(g_resetExhaledVolumeSemaphore);
        }

        if (pdPASS == xQueuePeek(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume, (TickType_t)0))
        {
            if (cycleExhaledVolume != previousCycleExhaledVolume)
            {
                LCD_ClearString(118, 40, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%7.1f", cycleExhaledVolume);
                LCD_DrawString(118, 40, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousCycleExhaledVolume = cycleExhaledVolume;
            }
        }
        if (pdPASS == xQueuePeek(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume, (TickType_t)0))
        {
            if (totalExhaledVolume != previousTotalExhaledVolume)
            {
                LCD_ClearString(118, 64, 7, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%7.1f", totalExhaledVolume);
                LCD_DrawString(118, 64, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousTotalExhaledVolume = totalExhaledVolume;
            }
        }
        if (pdPASS == xQueuePeek(g_respiratoryRateQueue, (void *)&respiratoryRate, (TickType_t)0))
        {
            if (respiratoryRate != previousRespriatoryRate)
            {
                LCD_ClearString(94, 88, 5, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%5.1f", respiratoryRate);
                LCD_DrawString(94, 88, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousRespriatoryRate = respiratoryRate;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void Vo2MaxScreenAction(void)
{
    float vo2;
    float previousVo2 = -1.0f;
    float vo2Max;
    float previousVo2Max = -1.0f;
    float vCo2;
    float previousVCo2 = -1.0f;

    char string[8];

    LCD_Clear();

    LCD_DrawString(CENTER_X("VO2max"), MENU_NAME_POSITION_Y, "VO2max", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(10, 36, "VO2 =     0.0", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(168, 42, "mL/min/kg", LCD_COLOR_BLACK, LCD_FONT_16);

    LCD_DrawString(10, 60, "VO2max =  0.0", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(168, 66, "mL/min/kg", LCD_COLOR_BLACK, LCD_FONT_16);

    LCD_DrawString(10, 84, "VCO2 =    0.0", LCD_COLOR_BLACK, LCD_FONT_24);
    LCD_DrawString(168, 90, "mL/min/kg", LCD_COLOR_BLACK, LCD_FONT_16);

    LCD_DrawString(180, 120, "RESET >", LCD_COLOR_BLACK, LCD_FONT_16);

    while (BUTTON_LONG_PRESS != GetPushButton1State())
    {
        if (GetPushButton2State() == BUTTON_LONG_PRESS)
        {
            xSemaphoreGive(g_resetVo2MaxSemaphore);
        }

        if (pdPASS == xQueuePeek(g_vo2Queue, (void *)&vo2, (TickType_t)0))
        {
            if (vo2 != previousVo2)
            {
                LCD_ClearString(118, 36, 4, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%4.1f", vo2);
                LCD_DrawString(118, 36, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousVo2 = vo2;
            }
        }
        if (pdPASS == xQueuePeek(g_vo2MaxQueue, (void *)&vo2Max, (TickType_t)0))
        {
            if (vo2Max != previousVo2Max)
            {
                LCD_ClearString(118, 60, 4, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%4.1f", vo2Max);
                LCD_DrawString(118, 60, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousVo2Max = vo2Max;
            }
        }
        if (pdPASS == xQueuePeek(g_vCo2Queue, (void *)&vCo2, (TickType_t)0))
        {
            if (vCo2 != previousVCo2)
            {
                LCD_ClearString(118, 84, 4, LCD_COLOR_WHITE, LCD_FONT_24);
                snprintf(string, sizeof(string), "%4.1f", vCo2);
                LCD_DrawString(118, 84, string, LCD_COLOR_BLACK, LCD_FONT_24);
                previousVCo2 = vCo2;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}
