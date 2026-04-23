/************************************
 * INCLUDES
 ************************************/

#include "hmi.h"
#include "driver_lcd.h"
#include "menu.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "version.h"
#include "esp_timer.h"
#include "esp_log.h"

/************************************
 * PRIVATE MACROS AND DEFINES
 ************************************/

#define SPI_HOST SPI2_HOST

#define GPIO_PIN_NUM_SPI_MISO 25
#define GPIO_PIN_NUM_SPI_MOSI 19
#define GPIO_PIN_NUM_SPI_CLK 18

#define GPIO_PIN_NUM_BUTTON_1 35
#define GPIO_PIN_NUM_BUTTON_2 0

#define SHORT_LONG_PRESS_THRESHOLD_US 300000

#define MENU_LIST_X_START 20
#define MENU_LIST_Y_START_POSITION 50
#define MENU_LIST_Y_STEP 20

/************************************
 * PRIVATE VARIABLES
 ************************************/

static TaskHandle_t hmiTaskHandle;

/************************************
 * PRIVATE FUNCTION PROTOTYPES
 ************************************/

static void DisplayMenu(Menu_t *menu);
static void DisplaySelected(uint8_t selected);

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

    xTaskCreate(HMI_Task, "HMI Task", 4096, NULL, 0, &hmiTaskHandle);
}

void HMI_Task(void *pvParameters)
{
    const char *hmiTaskTag = "[HMI Task]";
    char version[20];
    int64_t pressDurationButton1 = 0;
    int64_t pressDurationButton2 = 0;
    bool pushButton1State = false;
    bool pushButton2State = false;
    bool lastPushButton1State = false;
    bool lastPushButton2State = false;
    int64_t pressTimestampButton1 = 0;
    int64_t pressTimestampButton2 = 0;
    uint8_t selectedMenu = 0;
    Menu_t mainMenu;
    Menu_t calibrationMenu;
    Menu_t vo2maxMenu;
    Menu_t spirometerMenu;
    Menu_t settingsMenu;
    Menu_t o2CalibrationMenu;
    Menu_t co2CalibrationMenu;
    Menu_t massFlowCalibrationMenu;
    Menu_t *currentMenu;

    ESP_LOGI(hmiTaskTag, "HMI initialization...");

    MENU_Create(&mainMenu, "VO2max");
    MENU_Create(&calibrationMenu, "Calibration");
    MENU_Create(&massFlowCalibrationMenu, "Mass Flow");
    MENU_Create(&o2CalibrationMenu, "O2");
    MENU_Create(&co2CalibrationMenu, "CO2");
    MENU_Create(&vo2maxMenu, "VO2max");
    MENU_Create(&spirometerMenu, "Spirometer");
    MENU_Create(&settingsMenu, "Settings");

    MENU_AddSubmenu(&mainMenu, &calibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &o2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &co2CalibrationMenu);
    MENU_AddSubmenu(&calibrationMenu, &massFlowCalibrationMenu);
    MENU_AddSubmenu(&mainMenu, &vo2maxMenu);
    MENU_AddSubmenu(&mainMenu, &spirometerMenu);
    MENU_AddSubmenu(&mainMenu, &settingsMenu);

    sprintf(version, "version: %d.%d.%d", VO2MAX_VERSION_MAJOR, VO2MAX_VERSION_MINOR, VO2MAX_VERSION_PATCH);

    LCD_Initialize();

    LCD_String(0, 25, "VO2max", strlen("VO2max"), 0, ST7789_FONT_24, false);
    LCD_String(0, 50, version, strlen(version), 0, ST7789_FONT_24, false);
    LCD_String(0, 75, "Initializing...", strlen("Initializing..."), 0, ST7789_FONT_24, false);

    vTaskDelay(pdMS_TO_TICKS(2000));

    currentMenu = &mainMenu;
    DisplayMenu(currentMenu);
    DisplaySelected(selectedMenu);

    ESP_LOGI(hmiTaskTag, "HMI ready");

    while (1)
    {
        pushButton1State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_1);
        pushButton2State = !(bool)gpio_get_level(GPIO_PIN_NUM_BUTTON_2);

        if ((pushButton1State == true) && (lastPushButton1State == false))
        {
            pressTimestampButton1 = esp_timer_get_time();
        }
        else if ((pushButton1State == false) && (lastPushButton1State == true))
        {
            pressDurationButton1 = esp_timer_get_time() - pressTimestampButton1;

            if (pressDurationButton1 < SHORT_LONG_PRESS_THRESHOLD_US) // Short click
            {
                if (selectedMenu == 0)
                {
                    selectedMenu = currentMenu->childrenCount - 1;
                }
                else
                {
                    selectedMenu--;
                }

                DisplaySelected(selectedMenu);
            }
            else
            {
                if (currentMenu->parent != NULL)
                {
                    currentMenu = currentMenu->parent;
                    selectedMenu = 0;
                    DisplayMenu(currentMenu);
                    DisplaySelected(selectedMenu);
                }
            }
        }

        if ((pushButton2State == true) && (lastPushButton2State == false))
        {
            pressTimestampButton2 = esp_timer_get_time();
        }
        else if ((pushButton2State == false) && (lastPushButton2State == true))
        {
            pressDurationButton2 = esp_timer_get_time() - pressTimestampButton2;

            if (pressDurationButton2 < SHORT_LONG_PRESS_THRESHOLD_US) // Short click
            {
                if (selectedMenu >= currentMenu->childrenCount - 1)
                {
                    selectedMenu = 0;
                }
                else
                {
                    selectedMenu++;
                }

                DisplaySelected(selectedMenu);
            }
            else
            {
                if (currentMenu->children[selectedMenu]->childrenCount > 0)
                {
                    currentMenu = currentMenu->children[selectedMenu];
                    DisplayMenu(currentMenu);
                    selectedMenu = 0;
                    DisplaySelected(selectedMenu);
                }
            }
        }

        lastPushButton1State = pushButton1State;
        lastPushButton2State = pushButton2State;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/************************************
 * PRIVATE FUNCTION DEFINITIONS
 ************************************/

static void DisplayMenu(Menu_t *menu)
{
    LCD_Clear();

    LCD_String((SCREEN_HEIGHT - 14 * strlen(menu->name)) / 2, 20, menu->name, strlen(menu->name), 0, ST7789_FONT_24, false);

    for (uint8_t i = 0; i < menu->childrenCount; i++)
    {
        LCD_String(MENU_LIST_X_START, MENU_LIST_Y_START_POSITION + i * MENU_LIST_Y_STEP, menu->children[i]->name, strlen(menu->children[i]->name), 0, ST7789_FONT_24, false);
    }
}

static void DisplaySelected(uint8_t selected)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        if (i != selected)
        {
            LCD_String(0, MENU_LIST_Y_START_POSITION + i * MENU_LIST_Y_STEP, " ", 1, 0xFFFFFFFF, ST7789_FONT_24, true);
        }
        else
        {
            LCD_String(0, MENU_LIST_Y_START_POSITION + i * MENU_LIST_Y_STEP, ">", 1, 0, ST7789_FONT_24, false);
        }
    }
}
