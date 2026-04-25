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

#define SHORT_LONG_PRESS_THRESHOLD_US 300000

#define MENU_LIST_X_START 20
#define MENU_LIST_Y_START_POSITION 50
#define MENU_LIST_Y_STEP 20

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
static void NormalOperation(Menu_t *menu);
static void DisplayMenu(Menu_t *menu, uint8_t page);
static void DisplaySelected(uint8_t selected);
static PushButtonState_e GetPushButton1State(void);
static PushButtonState_e GetPushButton2State(void);
static void LiveValuesScreenEntry(void);
static void LiveValuesScreenAction(void);
static void SpirometerScreenAction(void);
static void SpirometerScreenEntry(void);

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
    char version[20];
    BaseType_t waitNotification = pdTRUE;
    Menu_t mainMenu;
    Menu_t calibrationMenu;
    Menu_t vo2maxMenu;
    Menu_t spirometerMenu;
    Menu_t settingsMenu;
    Menu_t liveValuesMenu;
    Menu_t o2CalibrationMenu;
    Menu_t co2CalibrationMenu;
    Menu_t flowCalibrationMenu;

    ESP_LOGI(hmiTaskTag, "HMI initialization...");

    MENU_Create(&mainMenu, "VO2max");
    MENU_Create(&calibrationMenu, "Calibration");
    MENU_Create(&flowCalibrationMenu, "Flow");
    MENU_Create(&o2CalibrationMenu, "O2");
    MENU_Create(&co2CalibrationMenu, "CO2");
    MENU_Create(&vo2maxMenu, "VO2max");
    MENU_Create(&spirometerMenu, "Spirometer");
    MENU_Create(&settingsMenu, "Settings");
    MENU_Create(&liveValuesMenu, "Live values");

    MENU_AddSubmenu(&mainMenu, &calibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &o2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &co2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &flowCalibrationMenu);
    MENU_AddSubmenu(&mainMenu, &vo2maxMenu);
    MENU_AddSubmenu(&mainMenu, &spirometerMenu);
    MENU_AddSubmenu(&mainMenu, &settingsMenu);
    MENU_AddSubmenu(&mainMenu, &liveValuesMenu);

    MENU_AddAction(&liveValuesMenu, LiveValuesScreenEntry, LiveValuesScreenAction, NULL);
    MENU_AddAction(&spirometerMenu, SpirometerScreenEntry, SpirometerScreenAction, NULL);

    LCD_Initialize();

    LCD_String(0, 25, "VO2max", strlen("VO2max"), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);

    sprintf(version, "Version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);
    LCD_String(0, 50, version, strlen(version), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);

    LCD_String(0, 75, "Initializing...", strlen("Initializing..."), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);

    waitNotification &= xSemaphoreTake(g_flowInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_o2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    waitNotification &= xSemaphoreTake(g_co2InitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));
    // waitNotification &= xSemaphoreTake(g_pressureInitializationSemaphore, pdMS_TO_TICKS(INITIALIZATION_TIMEOUT_MS));

    if (pdFALSE == waitNotification)
    {
        LCD_String(70, 110, "Error!", strlen("ERROR!"), LCD_COLOR_RED, LCD_NO_BG_COLOR, ST7789_FONT_24);

        vTaskSuspend(g_hmiTaskHandle);

        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
        }
    }
    else
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        LCD_String(60, 110, "Success!", strlen("Success!"), LCD_COLOR_GREEN, LCD_NO_BG_COLOR, ST7789_FONT_24);
        vTaskDelay(pdMS_TO_TICKS(3000));

        NormalOperation(&mainMenu);
    }
}

static void NormalOperation(Menu_t *menu)
{
    Menu_t *currentMenu = menu;
    PushButtonState_e pushButton1State;
    PushButtonState_e pushButton2State;
    uint8_t selectedMenu = 0;
    uint8_t page = 0;
    uint8_t lastPage = 0;

    DisplayMenu(currentMenu, page);
    DisplaySelected(selectedMenu);

    ESP_LOGI(hmiTaskTag, "HMI ready");

    while (1)
    {
        pushButton1State = GetPushButton1State();

        if (pushButton1State == BUTTON_SHORT_PRESS)
        {
            if (currentMenu->childrenCount > 0)
            {
                if (selectedMenu == 0)
                {
                    selectedMenu = currentMenu->childrenCount - 1;
                }
                else
                {
                    selectedMenu--;
                }

                page = selectedMenu / 4;

                if (page != lastPage)
                {
                    DisplayMenu(currentMenu, page);
                    lastPage = page;
                }

                DisplaySelected(selectedMenu);
            }
        }
        else if (pushButton1State == BUTTON_LONG_PRESS)
        {
            if (currentMenu->parent != NULL)
            {
                if (currentMenu->onExit != NULL)
                {
                    currentMenu->onExit();
                }

                currentMenu = currentMenu->parent;
                selectedMenu = 0;
                page = 0;
                lastPage = 0;
                DisplayMenu(currentMenu, page);
                DisplaySelected(selectedMenu);
            }
        }

        pushButton2State = GetPushButton2State();

        if (pushButton2State == BUTTON_SHORT_PRESS)
        {
            if (currentMenu->childrenCount > 0)
            {
                if (selectedMenu >= currentMenu->childrenCount - 1)
                {
                    selectedMenu = 0;
                }
                else
                {
                    selectedMenu++;
                }

                page = selectedMenu / 4;

                if (page != lastPage)
                {
                    DisplayMenu(currentMenu, page);
                    lastPage = page;
                }

                DisplaySelected(selectedMenu);
            }
        }
        else if (pushButton2State == BUTTON_LONG_PRESS)
        {
            if (currentMenu->children[selectedMenu] != NULL)
            {
                currentMenu = currentMenu->children[selectedMenu];

                if (currentMenu->onEntry != NULL)
                {
                    currentMenu->onEntry();
                }

                if (currentMenu->childrenCount > 0)
                {
                    selectedMenu = 0;
                    page = 0;
                    lastPage = 0;
                    DisplayMenu(currentMenu, page);
                    DisplaySelected(selectedMenu);
                }
            }
        }

        if (currentMenu->action != NULL)
        {
            currentMenu->action();
        }

        vTaskDelay(pdMS_TO_TICKS(HMI_TASK_PERIOD_MS));
    }
}

static void DisplayMenu(Menu_t *menu, uint8_t page)
{
    uint8_t yPos = 0;
    uint8_t endIndex;

    if (menu->childrenCount > 0)
    {
        LCD_Clear();

        LCD_String((SCREEN_HEIGHT - 16 * strlen(menu->name)) / 2, 16, menu->name, strlen(menu->name), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);

        yPos = 0;

        endIndex = page * 4 + 4;

        if (endIndex > menu->childrenCount)
        {
            endIndex = menu->childrenCount;
        }

        for (uint8_t i = page * 4; i < endIndex; i++)
        {
            LCD_String(MENU_LIST_X_START, MENU_LIST_Y_START_POSITION + yPos * MENU_LIST_Y_STEP, menu->children[i]->name, strlen(menu->children[i]->name), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
            yPos++;
        }
    }
}

static void DisplaySelected(uint8_t selected)
{
    uint8_t yPos = 0;
    uint8_t selectedPos = selected % 4;

    for (uint8_t i = 0; i < 4; i++)
    {
        if (i != selectedPos)
        {
            LCD_String(0, MENU_LIST_Y_START_POSITION + yPos * MENU_LIST_Y_STEP, " ", 1, LCD_COLOR_WHITE, LCD_COLOR_WHITE, ST7789_FONT_24);
        }
        else
        {
            LCD_String(0, MENU_LIST_Y_START_POSITION + yPos * MENU_LIST_Y_STEP, ">", 1, LCD_COLOR_RED, LCD_NO_BG_COLOR, ST7789_FONT_24);
        }

        yPos++;
    }
}

static PushButtonState_e GetPushButton1State(void)
{
    PushButtonState_e ret = BUTTON_NO_PRESS;
    bool pushButton1State = false;
    static bool lastPushButton1State = false;
    static int64_t pressDurationButton1 = 0;
    static int64_t pressTimestampButton1 = 0;

    pushButton1State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_1);

    if ((pushButton1State == true) && (lastPushButton1State == false))
    {
        pressTimestampButton1 = esp_timer_get_time();
    }
    else if ((pushButton1State == false) && (lastPushButton1State == true))
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

    lastPushButton1State = pushButton1State;

    return ret;
}

static PushButtonState_e GetPushButton2State(void)
{
    PushButtonState_e ret = BUTTON_NO_PRESS;
    bool pushButton2State = false;
    static bool lastPushButton2State = false;
    static int64_t pressDurationButton2 = 0;
    static int64_t pressTimestampButton2 = 0;

    pushButton2State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_2);

    if ((pushButton2State == true) && (lastPushButton2State == false))
    {
        pressTimestampButton2 = esp_timer_get_time();
    }
    else if ((pushButton2State == false) && (lastPushButton2State == true))
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

    lastPushButton2State = pushButton2State;

    return ret;
}

static void LiveValuesScreenEntry(void)
{
    LCD_Clear();

    LCD_String(0, 16, "O2   =           %", 18, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 40, "CO2  =         ppm", 18, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 64, "Flow =       L/min", 18, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 88, "T    =           C", 18, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 112, "H    =           %", 18, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
}

static void LiveValuesScreenAction(void)
{
    float flow;
    float o2;
    float co2;
    float temperature;
    float humidity;
    char string[30];

    if (pdPASS == xQueueReceive(g_o2Queue, (void *)&o2, (TickType_t)0))
    {
        sprintf(string, "%.1f", o2);
        LCD_String(80, 16, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
    if (pdPASS == xQueueReceive(g_co2Queue, (void *)&co2, (TickType_t)0))
    {
        sprintf(string, "%.0f", co2);
        LCD_String(80, 40, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
    if (pdPASS == xQueueReceive(g_flowQueue, (void *)&flow, (TickType_t)0))
    {
        sprintf(string, "%.1f", flow);
        LCD_String(80, 64, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
    if (pdPASS == xQueueReceive(g_temperatureQueue, (void *)&temperature, (TickType_t)0))
    {
        sprintf(string, "%.1f", temperature);
        LCD_String(80, 88, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
    if (pdPASS == xQueueReceive(g_humidityQueue, (void *)&humidity, (TickType_t)0))
    {
        sprintf(string, "%.0f", humidity);
        LCD_String(80, 112, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
}

static void SpirometerScreenEntry(void)
{
    LCD_Clear();

    LCD_String(60, 24, "Spirometer", strlen("Spirometer"), LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 72, "Vol (cyc) =     L", 17, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
    LCD_String(0, 96, "Vol (tot) =     L", 17, LCD_COLOR_BLACK, LCD_NO_BG_COLOR, ST7789_FONT_24);
}

static void SpirometerScreenAction(void)
{
    float cycleExhaledVolume;
    float totalExhaledVolume;
    char string[30];

    if (pdPASS == xQueueReceive(g_cycleExhaledVolumeQueue, (void *)&cycleExhaledVolume, (TickType_t)0))
    {
        sprintf(string, "%.1f", cycleExhaledVolume);
        LCD_String(120, 72, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
    if (pdPASS == xQueueReceive(g_totalExhaledVolumeQueue, (void *)&totalExhaledVolume, (TickType_t)0))
    {
        sprintf(string, "%.1f", totalExhaledVolume);
        LCD_String(120, 96, string, strlen(string), LCD_COLOR_BLACK, LCD_COLOR_WHITE, ST7789_FONT_24);
    }
}
