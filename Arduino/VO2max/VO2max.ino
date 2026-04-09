const String Version = "V3.0 2026/04/09";

#include <TFT_eSPI.h>             // Graphics and font library for ST7735 driver chip
#include <SPI.h>
#include <Wire.h>

#define BUTTON_1_PIN 0  
#define BUTTON_2_PIN 35
// Starts Screen for TTGO device
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h


//----------------------------------------------------------------------------------------------------------
//                  SETUP
//----------------------------------------------------------------------------------------------------------

void setup() 
{
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);

  // init display ----------
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("VO2max", 0, 25, 4);
  tft.drawString(Version, 0, 50, 4);
  tft.drawString("Initializing...", 0, 75, 4);

  delay(10000);
  tft.fillScreen(TFT_BLACK);
}

//----------------------------------------------------------------------------------------------------------
//                  MAIN PROGRAM
//----------------------------------------------------------------------------------------------------------
void loop() {
  delay (2000);
}

//----------------------------------------------------------------------------------------------------------
//                  FUNCTIONS
//----------------------------------------------------------------------------------------------------------

//--------------------------------------------------



//--------------------------------------------------
