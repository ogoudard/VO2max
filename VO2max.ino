//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Spirometrie Project
// https://www.instructables.com/Accurate-VO2-Max-for-Zwift-and-Strava/
// BLE by Andreas Spiess https://github.com/SensorsIot/Bluetooth-BLE-on-Arduino-IDE
// Modifications by Ulrich Rissel
//
// TTGO T-Display: SDA-Pin21, SCL-Pin22
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/*
Board: ESP32 Dev Module
Upload Speed: 921600
CPU Frequency: 240Mhz (WiFi/BT)
Flash Frequency: 80Mhz
Flash Mode: QIO
Flash Size: 4MB (32Mb)
Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5 SPIFFS)
Core Debug Level: None
PSRAM: Disabled
*/
const String Version = "V2.3 2024/11/20";


#include "esp_adc_cal.h"          // ADC calibration data
#include <EEPROM.h>               // include library to read and write settings from flash
#include "DFRobot_OxygenSensor.h" //Library for Oxygen sensor
#include "SCD30.h"                //Library for CO2 sensor
#include "SensirionI2CSdp.h"      //Library for pressure sensor
#include <SPI.h>
#include <TFT_eSPI.h>             // Graphics and font library for ST7735 driver chip
#include <Wire.h>

// declarations for bluetooth serial --------------
#include "BluetoothSerial.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;

// declarations for BLE ---------------------
#include <BLE2902.h> // used for notifications 0x2902: Client Characteristic Configuration
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

//Library for barometric sensor  ---------------------
#include <Adafruit_BMP3XX.h> 

// Set this to the correct printed case venturi diameter
#define DIAMETER 16

//#define VERBOSE // additional debug logging

#define ADC_EN  14       // ADC_EN is the ADC detection enable port
#define ADC_PIN 34

// Oxygene sensor defines
#define COLLECT_NUMBER    10        // collect number, the collection range is 1-100.
#define Oxygen_IICAddress ADDRESS_3 // I2C  label for o2 address

byte hrmPos[1] = {2};
bool _BLEClientConnected = false;

// heart rate service -> Send VO2MAX value
// @TODO : change to randomly generate UUID and see if it s still working
// heart rate service -> Send VO2MAX value
// @TODO : change to randomly generate UUID and see if it s still working
#define vo2maxRateService BLEUUID("b354cf1b-2486-4f21-b4b1-ee4cd5cc3bf0")
BLECharacteristic vo2maxRateMeasurementCharacteristics(BLEUUID("c70c73fd-b5fb-4a5f-87f4-7a187590b0ef", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor     vo2maxRateDescriptor(BLEUUID((uint16_t)0x2901));

// Start Test bit to trig the begining of a new VO2max test protocol 
// @TODO  :change this when the heart data will be read from the POLAR heart sensor
BLECharacteristic startTestCharacteristic(BLEUUID((uint16_t)0x2A38), BLECharacteristic::PROPERTY_READ);
BLEDescriptor     startTestDescriptor(BLEUUID((uint16_t)0x2901)); // 0x2901: Characteristic User Description

// Vo2 service
#define vo2RateService BLEUUID("231c616b-32a6-4b93-9f0c-fe728deca0a5") 
BLECharacteristic vo2RateMeasurementCharacteristics(BLEUUID("4225d51b-f1c2-419a-9acc-bd8e70d960ae"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor vo2RateDescriptor(BLEUUID((uint16_t)0x2901));
// Vco2 service
#define vco2RateService BLEUUID("196c1c76-fe53-47e7-baab-a32b404d63df") 
BLECharacteristic vco2RateMeasurementCharacteristics(BLEUUID("a4534c9e-0ede-48ec-bee5-7b728ecb25df"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor vco2RateDescriptor(BLEUUID((uint16_t)0x2901));
// RQ service
#define RQRateService BLEUUID("8868ba7f-cada-4035-85d2-e0002aeb2be6") 
BLECharacteristic RQRateMeasurementCharacteristics(BLEUUID("b192eef4-a298-43fe-b85c-b04d934448f7"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor RQRateDescriptor(BLEUUID((uint16_t)0x2901));
// O2perc service
#define o2percRateService BLEUUID("f362445d-0824-40e1-a53a-8e90106f0ba3") 
BLECharacteristic o2percRateMeasurementCharacteristics(BLEUUID("f1329c52-97b8-4945-a8a2-2127426d71ba"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor o2percRateDescriptor(BLEUUID((uint16_t)0x2901));
// C02perc service
#define co2percRateService BLEUUID("4b1ad6ec-d8d3-426c-8d1e-0dea4d6b4ebc") 
BLECharacteristic co2percRateMeasurementCharacteristics(BLEUUID("86c8ca0e-885f-4987-82ba-a794574cc6cf"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor co2percRateDescriptor(BLEUUID((uint16_t)0x2901));
// kcal service
#define kcalRateService BLEUUID("3483923e-e94a-4a21-96c4-c12a442ee4cf") 
BLECharacteristic kcalRateMeasurementCharacteristics(BLEUUID("fb6fc35b-2896-4474-8647-bd206b3d0c50"), BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor kcalRateDescriptor(BLEUUID((uint16_t)0x2901));

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        //Serial.println("Client connected");
        _BLEClientConnected = true;
        pServer->startAdvertising();  // Relancer l'advertising
        //Serial.println("Start advertising");
    }

    void onDisconnect(BLEServer* pServer) {
        _BLEClientConnected = false;
        //Serial.println("Client disconnected");
        delay(100);  // Pause avant de recommencer à diffuser
        pServer->startAdvertising();  // Relancer l'advertising
        //Serial.println("Start advertising");
    }
};

// ------------------------------------------
// Barometer device
Adafruit_BMP3XX bmp;

// Starts Screen for TTGO device
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h

// Labels the pressure sensor: mySensor
SensirionI2CSdp mySensor;

// Label of oxygen sensor
DFRobot_OxygenSensor Oxygen;

// Defines button state for adding wt
const int buttonPin1 = 0;
const int buttonPin2 = 35;
int       wtTotal = 0;
int       buttonPushCounter1 = 0; // counter for the duration of button1 pressed
int       buttonState1 = 1;       // current state of the button
int       buttonPushCounter2 = 0; // counter for the duration of button2 pressed
int       buttonState2 = 1;       // current state of the button
int       screenChanged = 0;
int       screenNr = 1;
int       HeaderStreamed = 0;
int       HeaderStreamedBT = 0;
int       DEMO = 0;               // 0 = Normal mode / 1 = DEMO-mode
int       SCREEN_ONLY = 1;        // 0 = Normal mode / 1 = Screen only mode
int       vref = 1100;            // used for battery voltage reading

//############################################
// Select correct diameter depending on printed
// case dimensions:
//############################################

// Defines the size of the Venturi openings for the  calculations of AirFlow
float area_1 = 0.000531;    // = 26mm diameter
#if (DIAMETER == 20)
float area_2 = 0.000314;    // = 20mm diameter
#elif (DIAMETER == 19)
float area_2 = 0.000254;    // = 19mm diameter
#elif (DIAMETER == 16)
float area_2 = 0.000201;    // = 16mm diameter
#else // default
float area_2 = 0.000201;    // = 16mm diameter by default
#endif

float rho = 1.225;          // ATP conditions: density based on ambient conditions, dry air
float rhoSTPD = 1.292;      // STPD conditions: density at 0°C, MSL, 1013.25 hPa, dry air
float rhoBTPS = 1.123;      // BTPS conditions: density at ambient pressure, 35°C, 95% humidity
float massFlow = 0;
float volFlow = 0;
float volumeTotal = 0;      // variable for holding total volume of breath
float pressure = 0.0;       // differential pressure of the venturi nozzle
float pressThreshold = 0.2; // threshold for starting calculation of VE
float volumeVE = 0.0;
float volumeVEmean = 0.0;
float volumeExp = 0.0;

//######## Edit correction factor based on flow measurment with calibration syringe ############
// float correctionSensor = 1.0;   // correction factor

// Basic defaults in settings, saved to eeprom
struct {
    int   version = 1;            // Make sure saved data is right version
    float correctionSensor = 0.92;// calculated from 3L calibration syringe
    float weightkg = 62.0;        // Standard-body-weight
    bool  BLE_on = true;       // Output vo2 as a HRM
    bool  sens_on = false;         // Output as sensiron data
    bool  cheet_on = false;       // Output as vo2master for GoldenCheetah
    bool  co2_on = true;          // CO2 sensor active
} settings;

float  TimerVolCalc = 0.0;
float  Timer5s = 0.0;
float  Timer1min = 0.0;
float  TimerVO2calc = 0.0;
float  TimerVO2diff = 0.0; // used for integral of calories
float  TimerStart = 0.0;
float  TotalTime = 0.0;
String TotalTimeMin = String("00:00");
int    readVE = 0;
float  TimerVE = 0.0;
float  DurationVE = 0.0;

float lastO2 = 0;
float initialO2 = 0;
float co2 = 0;
float calTotal = 0;
float vo2Cal = 0;
float vo2CalH = 0;        // calories per hour
float vo2CalDay = 0.0;    // calories per day
float vo2CalDayMax = 0.0; // highest value of calories per day
float vo2Max = 0;         // value of vo2Max/min/kg, calculated every 30 seconds
float vo2Total = 0.0;     // value of total vo2Max/min
float vo2MaxMax = 0;      // Best value of vo2 max for whole time machine is on

float respq = 0.0;        // respiratory quotient in mol VCO2 / mol VO2
float co2ppm = 0.0;       // CO2 sensor in ppm
float co2perc = 0.0;      // = CO2ppm /10000
float initialCO2 = 0.0;   // initial value of CO2 in ppm
float vco2Total = 0.0;
float vco2Max = 0.0;
float co2temp = 0.0;      // temperature CO2 sensor
float co2hum = 0.0;       // humidity CO2 sensor (not used in calculations)

float freqVE = 0.0;       // ventilation frequency
float freqVEmean = 0.0;   // mean ventilation frequency

float expiratVol = 0.0;   // last expiratory volume in L
float volumeTotalOld = 0.0;
float volumeTotal2 = 0.0;
float TempC = 15.0;       // Air temperature in Celsius barometric sensor BMP180
float PresPa = 101325;    // uncorrected (absolute) barometric pressure
float Battery_Voltage = 0.0;

//----------------------------------------------------------------------------------------------------------
//                  SETUP
//----------------------------------------------------------------------------------------------------------

void setup() {

// Flow sensor variables
uint32_t productNumber;
uint8_t serialNumber[8];
uint8_t serialNumberSize = 8;
uint16_t error;
char errorMessage[256];

  EEPROM.begin(sizeof(settings));

  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);

  // defines ADC characteristics for battery voltage
  /*
    ADC_EN is the ADC detection enable port
    If the USB port is used for power supply, it is turned on by default.
    If it is powered by battery, it needs to be set to high level
  */
  // setup for analog digital converter used for battery voltage ---------
  pinMode(ADC_EN, OUTPUT);
  digitalWrite(ADC_EN, HIGH);
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  // Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
      // Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
      vref = adc_chars.vref;
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
      // Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n",
      // adc_chars.coeff_a, adc_chars.coeff_b);
  } else {
      // Serial.println("Default Vref: 1100mV");
  }

  // init display ----------
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  readVoltage();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("VO2max", 0, 25, 4);
  tft.drawString(Version, 0, 50, 4);
  tft.drawString("Initialising...", 0, 75, 4);
  // check for DEMO mode ---------
  if (!digitalRead(buttonPin2)) { // DEMO Mode if button2 is pressed during power on
      DEMO = 1;
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawString("DEMO-MODE!", 0, 100, 4);
  }
  delay(3000);
  tft.fillScreen(TFT_BLACK);

  // init serial communication  ----------
  Wire.begin();
  Serial.begin(9600); //drop to 9600 to see if improves reliability
  Serial.println("yMin,yMax,Time,VO2 ,VO2MAX,VCO2,RQ,Bvol,VEmin,Brate,outO2perc,CO2perc");

  if (!Serial) {
      tft.drawString("Serial Error!", 0, 0, 4);
  } else {
      tft.drawString("Serial ok", 0, 0, 4);
  }

  // init serial bluetooth -----------
  if (!SerialBT.begin("VO2max")) { // Start Bluetooth with device name
      tft.drawString("BT NOT ready!", 0, 25, 4);
  } else {
      tft.drawString("BT ready", 0, 25, 4);
  }

  // init barometric sensor BMP388 ---------
  if (!bmp.begin_I2C()) {
    tft.drawString("Temp/Pres. Error!", 0, 50, 4);
  }
  else {
    tft.drawString("Temp/Pres. ok", 0, 50, 4);
  }

  // init O2 sensor DF-Robot -----------
  if (!Oxygen.begin(Oxygen_IICAddress)) {
      tft.drawString("O2 Error!", 0, 75, 4);
  } else {
      tft.drawString("O2 ok", 0, 75, 4);
  }

  // init CO2 sensor Sensirion SCD30 -------------
  // check if sensor is connected?
  scd30.initialize();
  scd30.setAutoSelfCalibration(0);
  
  if(!scd30.isAvailable()) {
    tft.drawString("CO2 Error!", 120, 75, 4);
  }
  else{
    tft.drawString("CO2 ok", 120, 75, 4);
  }

// init flow/pressure sensor Sensirion SDP810-500Pa -------------

mySensor.begin(Wire, SDP8XX_I2C_ADDRESS_0);

mySensor.stopContinuousMeasurement();
error = mySensor.readProductIdentifier(productNumber, serialNumber, serialNumberSize);
if (error) {
    //Serial.print("Error trying to execute readProductIdentifier(): ");
    errorToString(error, errorMessage, 256);
    //Serial.println(errorMessage);
    tft.drawString("Flow-Sensor Error!", 0, 100, 4);
} else {
    for (size_t i = 0; i < serialNumberSize; i++) {
    }
  tft.drawString("Flow-Sensor ok", 0, 100, 4);  
  }
  
  InitBLE(); // init BLE for transmitting data VO2 live, VO2 max,co2,rq

  error = mySensor.startContinuousMeasurementWithDiffPressureTCompAndAveraging();

  delay (2000);
  if(SCREEN_ONLY == 1){
    initialO2 = 20.90; 
    initialCO2 = 1000;                              
  } 
  else {
    CheckInitialO2();
    CheckInitialCO2();
  }
  doMenu();

  showParameters();
  
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("Ready...", 120, 55, 4);

  TimerVolCalc = millis(); // timer for the volume (VE) integral function
  Timer5s = millis();
  Timer1min = millis();
  TimerVO2calc = millis(); // timer between VO2max calculations
  TimerStart = millis();   // holds the millis at start
  TotalTime = 0;
  // BatteryBT(); // TEST for battery discharge log
}

//----------------------------------------------------------------------------------------------------------
//                  MAIN PROGRAM
//----------------------------------------------------------------------------------------------------------
void loop() {
  TotalTime = millis() - TimerStart; // calculates actual total time
 
  VolumeCalc();                      // Starts integral function

  // VO2max calculation, tft display and excel csv every 1s --------------
  if ((millis() - TimerVO2calc) > 1000 && pressure < pressThreshold) { // calls vo2maxCalc() for calculation Vo2Max every 1 seconds.
      TimerVO2diff = millis() - TimerVO2calc;
      TimerVO2calc = millis(); // resets the timer

      //Are we using the co2 sensor?
      if (settings.co2_on) {
          readCO2();
      } else { // default co2values
          co2temp = TempC;
      }

      vo2maxCalc(); // vo2 max function call

      /*if (TotalTime >= 10000)*/ {
          showScreen();
          volumeTotal2 = 0; // resets volume2 to 0 (used for initial 10s sensor test)
          readVoltage();
      }
      ExcelStream();   // send csv data via wired com port

      delay(100);

      if (settings.BLE_on) {
          vo2maxRateMeasurementCharacteristics.setValue(vo2MaxMax); // set the new value for vo2max
          vo2maxRateMeasurementCharacteristics.notify();           // send a notification that value has changed
          startTestCharacteristic.setValue(hrmPos, 1);

          vo2RateMeasurementCharacteristics.setValue(vo2Max); // set the new value for vo2rate
          vo2RateMeasurementCharacteristics.notify();         // send a notification that value has changed
          
          vco2RateMeasurementCharacteristics.setValue(vco2Max);
          vco2RateMeasurementCharacteristics.notify();           // send a notification that value has changed
          
          RQRateMeasurementCharacteristics.setValue(respq);
          RQRateMeasurementCharacteristics.notify();           // send a notification that value has changed
          
      }
  }

  /*if (TotalTime >= 10000)*/ { // after 10 sec. activate the buttons for switching the screens
      ReadButtons();
      if (buttonPushCounter1 > 20 && buttonPushCounter2 > 20) ESP.restart();
      if (buttonPushCounter1 == 2) {
          screenNr--;
          screenChanged = 1;
      }
      if (buttonPushCounter2 == 2) {
          screenNr++;
          screenChanged = 1;
      }
      if (screenNr < 1) screenNr = 6;
      if (screenNr > 6) screenNr = 1;
      if (screenChanged == 1) {
          showScreen();
          screenChanged = 0;
      }
  }

  if (millis() - Timer1min > 30000) {
    Timer1min = millis(); // reset timer
    // BatteryBT(); //TEST für battery discharge log ++++++++++++++++++++++++++++++++++++++++++
  }

  TimerVolCalc = millis();  // part of the integral function to keep calculation volume over time
                            // Resets amount of time between calcs

}

//----------------------------------------------------------------------------------------------------------
//                  FUNCTIONS
//----------------------------------------------------------------------------------------------------------

void loadSettings() {
  // Check version first.
  int version = EEPROM.read(0);
  if (version == settings.version) {
      for (int i = 0; i < sizeof(settings); ++i)
          ((byte *)&settings)[i] = EEPROM.read(i);
  }
}

//--------------------------------------------------

void saveSettings() {
  bool changed = false;
  for (int i = 0; i < sizeof(settings); ++i) {
      byte b = EEPROM.read(i);
      if (b != ((byte *)&settings)[i]) {
          EEPROM.write(i, ((byte *)&settings)[i]);
          changed = true;
      }
  }
  if (changed) EEPROM.commit();
}

//--------------------------------------------------

void CheckInitialO2() {
  // check initial O2 value -----------
  initialO2 = Oxygen.ReadOxygenData(COLLECT_NUMBER); // read and check initial VO2%
  if (initialO2 < 20.00) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setCursor(5, 5, 4);
    tft.println("INITIAL O2% LOW!");
    tft.setCursor(5, 30, 4);
    tft.println("Wait to continue!");
    while (digitalRead(buttonPin1)) {
      initialO2 = Oxygen.ReadOxygenData(COLLECT_NUMBER);
      tft.setCursor(5, 67, 4);
      tft.print("O2: ");
      tft.print(initialO2);
      tft.println(" % ");
      tft.setCursor(5, 105, 4);
      tft.println("Continue              >>>");
      delay(500);
    }
    if (initialO2 < 20.00) initialO2 = 20.90;
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(5, 5, 4);
    tft.println("Initial O2% set to:");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(5, 55, 4);
    tft.print(initialO2);
    tft.println(" % ");
    delay(5000);
  }
}

//--------------------------------------------------

void CheckInitialCO2() { // check initial CO2 value
  readCO2();
  initialCO2 = co2ppm;

  if (initialCO2 > 1000) {
      tft.fillScreen(TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.setCursor(5, 5, 4);
      tft.println("INITIAL CO2 HIGH!");
      tft.setCursor(5, 30, 4);
      tft.println("Wait to continue!");
      while (digitalRead(buttonPin1)) {
          readCO2();
          initialCO2 = co2ppm;
          tft.setCursor(5, 67, 4);
          tft.print("CO2: ");
          tft.print(initialCO2, 0);
          tft.println(" ppm ");
          tft.setCursor(5, 105, 4);
          tft.println("Continue              >>>");
          delay(500);
      }
      if (initialCO2 > 1000) initialCO2 = 1000;
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(5, 5, 4);
      tft.println("Initial CO2 set to:");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(5, 55, 4);
      tft.print(initialCO2, 0);
      tft.println(" ppm");
      delay(5000);
  }
}

//--------------------------------------------------

void ConvertTime(float ms) {
  long   inms = long(ms);
  int    h, m, s;
  String strh, strm, strs;
  s = (inms / 1000) % 60;
  m = (inms / 60000) % 60;
  h = (inms / 3600000) % 24;
  strs = String(s);
  if (s < 10) strs = String("0") + strs;
  strm = String(m);
  if (m < 10) strm = String("0") + strm;
  strh = String(h);
  if (h < 10) strh = String("0") + strh;
  TotalTimeMin = String(strh) + String(":") + String(strm) + String(":") + String(strs);
}

//--------------------------------------------------

void VolumeCalc() {

  // Read pressure from Sensirion SDP810-500Pa
  char errorMessage[256];
  // Read Measurement
  float differentialPressure;
  float temperature;

  float error = mySensor.readMeasurement(differentialPressure, temperature);
  if(SCREEN_ONLY == 1){
      differentialPressure = random(1,3)/10.0;                                     
      temperature = random(100,200)/10.0; 
      error = 0;                                    
  } 
  if (error) {
      //Serial.print("Error trying to execute readMeasurement(): ");
      errorToString(error, errorMessage, 256);
      //Serial.println(errorMessage);
  } else {
      //Serial.print("DifferentialPressure[Pa]:");
      //Serial.print(differentialPressure);
      //Serial.print("\t");
      //Serial.print("Temperature[°C]:");
      //Serial.print(temperature);
      //Serial.println();
  }
 
 
  pressure = pressure/2 + differentialPressure/2;

  if (DEMO == 1) {
      pressure = 10;                                      // TEST+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
      if ((millis() - TimerVO2calc) > 2500) pressure = 0; // TEST++++++++++++++++++++++++++++
  }

  if (isnan(pressure)) { // isnan = is not a number,  unvalid sensor data
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawCentreString("Venturi Error!", 120, 55, 4);
  }
  if (pressure > 266) { // upper limit of flow sensor warning
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawCentreString("Sensor Limit!", 120, 55, 4);
  }
  if (pressure < 0) pressure = 0;

  if (pressure < pressThreshold && readVE == 1) { // read volumeVE
      readVE = 0;
      DurationVE = millis() - TimerVE;
      TimerVE = millis(); // start timerVE
      volumeExp = volumeTotal;
      volumeTotal = 0; // resets volume for next breath
      volumeVE = volumeExp / DurationVE * 60;
      volumeExp = volumeExp / 1000;
      volumeVEmean = (volumeVEmean * 3 / 4) + (volumeVE / 4); // running mean of one minute volume (VE)
      if (volumeVEmean < 1) volumeVEmean = 0;
      freqVE = 60000 / DurationVE;
      if (volumeVE < 0.1) freqVE = 0;
      freqVEmean = (freqVEmean * 3 / 4) + (freqVE / 4);
      if (freqVEmean < 1) freqVEmean = 0;

    #ifdef VERBOSE
            Serial.print("volumeExp: ");
            Serial.print(volumeExp);
            Serial.print("   VE: ");
            Serial.print(volumeVE);
            Serial.print("   VEmean: ");
            Serial.print(volumeVEmean);
            Serial.print("   freqVE: ");
            Serial.print(freqVE, 1);
            Serial.print("   freqVEmean: ");
            Serial.println(freqVEmean, 1);
    #endif
  }
  if (millis() - TimerVE > 5000) readVE = 1; // readVE at least every 5s

  if (pressure >= pressThreshold) { // ongoing integral of volumeTotal
      if (volumeTotal > 50) readVE = 1;
      massFlow = 1000 * sqrt((abs(pressure) * 2 * rho) / ((1 / (pow(area_2, 2))) - (1 / (pow(area_1, 2))))); // Bernoulli equation
      volFlow = massFlow / rho;                      // volumetric flow of air
      volFlow = volFlow * settings.correctionSensor; // correction of sensor calculations
      volumeTotal = volFlow * (millis() - TimerVolCalc) + volumeTotal;
      volumeTotal2 = volFlow * (millis() - TimerVolCalc) + volumeTotal2;
  } else if ((volumeTotal2 - volumeTotalOld) > 200) { // calculate actual expiratory volume
      expiratVol = (volumeTotal2 - volumeTotalOld) / 1000;
      volumeTotalOld = volumeTotal2;
  }
}



//--------------------------------------------------
void ExcelStream() {
  // HeaderStreamed = 1;// TEST: Deactivation of header
if (HeaderStreamed == 0) {
    Serial.print("yMax");
    Serial.print(",");
    Serial.print("yMin");
    Serial.print(",");
    Serial.print("Time");
    Serial.print(",");
    Serial.print("VO2");
    Serial.print(",");
    Serial.print("VO2MAX");
    Serial.print(",");
    Serial.print("VCO2");
    Serial.print(",");
    Serial.print("RQ");
    Serial.print(",");
    Serial.print("Bvol");
    Serial.print(",");
    Serial.print("VEmin");
    Serial.print(",");
    Serial.print("Brate");
    Serial.print(",");
    Serial.print("outO2perc");
    Serial.print(",");
    Serial.println("CO2perc");
    HeaderStreamed = 1;
}

// Test to resize serial plotter
// assuming no data goes above 40 and test will last less than 600 sec
    Serial.print("yMax:"); 
    Serial.print("40");
    Serial.print(", ");
    Serial.print("yMin:");    
    Serial.print("3");
    Serial.print(", ");
    Serial.print("Time:");    
    Serial.print(float(TotalTime / 1000), 0);
    Serial.print(", ");
    Serial.print("VO2:");    
    Serial.print(vo2Max);
    Serial.print(", ");
    Serial.print("VO2MAX:");    
    Serial.print(vo2MaxMax);
    Serial.print(", ");
    Serial.print("VCO2:");    
    Serial.print(vco2Max);
    Serial.print(", ");
    Serial.print("RQ:");    
    Serial.print(respq);
    Serial.print(", ");
    Serial.print("Bvol:");    
    Serial.print(volumeExp);
    Serial.print(", ");
    Serial.print("VEmin:");    
    Serial.print(volumeVEmean);
    Serial.print(", ");
    Serial.print("Brate:");    
    Serial.print(freqVEmean);
    Serial.print(", ");
    Serial.print("outO2perc:");    
    Serial.print(lastO2);
    Serial.print(", ");
    Serial.print("CO2perc:");    
    Serial.println(co2perc, 3);
}

//--------------------------------------------------

void BatteryBT() {
  // HeaderStreamedBT = 1;// TEST: Deactivation of header
  if (HeaderStreamedBT == 0) {
      SerialBT.print("Time");
      SerialBT.print(",");
      SerialBT.println("Voltage");
      HeaderStreamedBT = 1;
  }
  SerialBT.print(float(TotalTime / 1000), 0);
  SerialBT.print(",");
  SerialBT.println(Battery_Voltage);
}

//--------------------------------------------------

void ReadO2() {
  float oxygenData = Oxygen.ReadOxygenData(COLLECT_NUMBER);
 
  // Generate random value if screen only connected
  if(SCREEN_ONLY == 1) {
    oxygenData = random(1950, 2150) / 100.0;
  }

  lastO2 = oxygenData;
  if (lastO2 > initialO2) initialO2 = lastO2; // correction for drift of O2 sensor

  // DEMO Mode
  if (DEMO == 1) lastO2 = initialO2 - 4; 
  
  co2 = initialO2 - lastO2;
}

//--------------------------------------------------

void readCO2() {
  float result[3] = {0};
  // Generate random value if screen only connected
  if(SCREEN_ONLY == 1) {
    co2ppm = random(100000, 200000) / 10.0;
    co2temp = random(190, 210) / 10.0;
    co2hum = random(100, 900) / 10.0;
  }

  if (scd30.isAvailable()) {
      scd30.getCarbonDioxideConcentration(result);

      co2ppm = result[0];
 
      if (co2ppm >= 40000) { // upper limit of CO2 sensor warning
          // tft.fillScreen(TFT_RED);
          tft.setTextColor(TFT_WHITE, TFT_RED);
          tft.drawCentreString("CO2 LIMIT!", 120, 55, 4);
      }

      if (DEMO == 1) co2ppm = 30000; // TEST
      if (initialCO2 == 0) initialCO2 = co2ppm;
 
  
      co2perc = co2ppm / 10000;
      co2temp = result[1];
      co2hum = result[2];


      float co2percdiff = (co2ppm - initialCO2) / 10000; // calculates difference to initial CO2
      if (co2percdiff < 0) co2percdiff = 0;

      // VCO2 calculation is based on changes in CO2 concentration (difference to baseline)
      vco2Total = volumeVEmean * rhoBTPS / rhoSTPD * co2percdiff * 10; // = vco2 in ml/min (* co2% * 10 for L in ml)
      vco2Max = vco2Total / settings.weightkg;                         // correction for wt
      respq = (vco2Total * 44) / (vo2Total * 32);                      // respiratory quotient based on molarity
      // CO2: 44g/mol, O2: 32 g/mol
      if (isnan(respq)) respq = 0; // correction for errors/div by 0
      if (respq > 1.5) respq = 0;

      #ifdef VERBOSE
              Serial.print("Carbon Dioxide Concentration is: ");
              Serial.print(result[0]);
              Serial.println(" ppm");
              Serial.print("Temperature = ");
              Serial.print(result[1]);
              Serial.println(" ℃");
              Serial.print("Humidity = ");
              Serial.print(result[2]);
              Serial.println(" %");
      #endif
  }
}

//--------------------------------------------------

void AirDensity() {
  TempC = bmp.readTemperature(); // Temp from baro sensor BMP180
  // co2temp is temperature from CO2 sensor
  PresPa = bmp.readPressure();
  
  // Generate random values if screen only connected
  if(SCREEN_ONLY == 1) {
    PresPa = random(100000, 103325)/10.0;
    TempC = random(80, 250)/10.0;
  }
  
  rho = PresPa / (co2temp + 273.15) / 287.058; // calculation of air density
  rhoBTPS = PresPa / (35 + 273.15) / 292.9;    // density at BTPS: 35°C, 95% humidity
}

//--------------------------------------------------

void vo2maxCalc() { // V02max calculation every 5s

  ReadO2();
  AirDensity(); // calculates air density
 

  #ifdef VERBOSE
    // Debug. compare co2
    Serial.print("Calc co2 ");
    Serial.print(initialO2 - lastO2);
    Serial.print(" sens co2 ");
    Serial.println(co2perc);
  #endif

  co2 = initialO2 - lastO2; // calculated level of CO2 based on Oxygen level loss
  if (co2 < 0) co2 = 0;     // correction for sensor drift

  vo2Total = volumeVEmean * rhoBTPS / rhoSTPD * co2 * 10; // = vo2 in ml/min (* co2% * 10 for L in ml)
  vo2Max = vo2Total / settings.weightkg;                  // correction for wt
  if (vo2Max > vo2MaxMax) vo2MaxMax = vo2Max;

  vo2Cal = vo2Total / 1000 * 4.86;                     // vo2Max liters/min * 4.86 Kcal/liter = kcal/min
  calTotal = calTotal + vo2Cal * TimerVO2diff / 60000; // integral function of calories
  vo2CalH = vo2Cal * 60.0;                             // actual calories/min. * 60 min. = cal./hour
  vo2CalDay = vo2Cal * 1440.0;                         // actual calories/min. * 1440 min. = cal./day
  if (vo2CalDay > vo2CalDayMax) vo2CalDayMax = vo2CalDay;

  // Generate random values if screen only connected
  if(SCREEN_ONLY == 1) {
    vo2MaxMax = random(200, 700)/10.0;
    vo2Max = random(100, 700)/10.0;
    vco2Max = random(80, 250)/10.0;
  }
 
}

//--------------------------------------------------

void showScreen() { // select active screen
  ConvertTime(TotalTime);
  tft.setRotation(1);
  switch (screenNr) {
    case 1:
        tftScreen1();
        break;
    case 2:
        tftScreen2();
        break;
    case 3:
        tftScreen3();
        break;
    case 4:
        tftScreen4();
        break;
    case 5:
        // tft.setRotation(2);
        tftScreen5();
        break;
    case 6:
        tftParameters();
        break;
    default:
        // if nothing else matches, do the default
        // default is optional
        break;
  }
}

//--------------------------------------------------
void showParameters() {
  while (digitalRead(buttonPin2)) { // wait until button2 is pressed
      // Let stabilise
      AirDensity();
      tftParameters(); // show initial sensor parameters

      tft.setCursor(220, 5, 4);
      tft.print(">");
      delay(500);
      tft.setCursor(220, 5, 4);
      tft.print("    ");
      delay(500);
  }
  while (digitalRead(buttonPin2) == 0)
      ;
}

//--------------------------------------------------
// Reset O2 calibration value
void fnCalO2() {
  Oxygen.Calibrate(20.9, 0.0);
  showParameters();
}

//--------------------------------------------------
// Calibrate flow sensor
void fnCalAir() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(0, 5, 4);
  tft.println("Use 3L calib.pump");
  tft.setCursor(0, 30, 4);
  tft.println("for sensor check.");
  tft.setCursor(0, 105, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Press to start      >>>");

  while (digitalRead(buttonPin1))
      ; // Start measurement ---------

  tft.fillScreen(TFT_BLACK);

  TimerStart = millis();
  float orig = settings.correctionSensor;
  settings.correctionSensor = 1.16; // precalibration factor
  // timing of the integral of volume calculation differs
  // between this calibration loop and the main loop

  volumeTotal2 = 0;

  do {
      TotalTime = millis() - TimerStart; // calculates actual total time
      VolumeCalc();                      // Starts integral function

      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.setCursor(0, 5, 4);
      tft.println("Total Volume (ml):");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setCursor(0, 55, 7);
      tft.println(volumeTotal2, 0);
      tft.setCursor(0, 105, 4);
      tft.print(expiratVol, 3);
      tft.setCursor(100, 105, 4);
      tft.print(TotalTime / 1000, 1);
      // tft.setCursor(170, 105, 4);
      // tft.println(pressure, 1);

      TimerVolCalc = millis(); // part of the integral function to keep calculation volume over time
                               // Resets amount of time between calcs

  } while (TotalTime < 10000);

  settings.correctionSensor = 3000 / volumeTotal2;

  // leave alone if not sensible.
  if (settings.correctionSensor < 0.8 || settings.correctionSensor > 1.2) settings.correctionSensor = orig;

  showParameters();
}
//--------------------------------------------------

struct MenuItem {
  int   id;
  char *label;
  bool  toggle;
  void (*fn)();
  bool *val;
};

int      icount = 0;
MenuItem menuitems[] = {{icount++, "Recalibrate O2", false, &fnCalO2, 0},
                        {icount++, "Calibrate Flow", false, &fnCalAir, 0},
                        {icount++, "Set Weight", false, &GetWeightkg, 0},
                        {icount++, "Bluetooth", true, 0, &settings.BLE_on},
                        {icount++, "CO2 sensor", true, 0, &settings.co2_on},
                        {icount++, "", false, 0, &settings.sens_on},
                        {icount++, "", false, 0, &settings.cheet_on},
                        {icount++, "Done...", false, 0, 0}};

//--------------------------------------------------
void doMenu() {
  int total = 5; // max on screen
  int cur = 7;   // Default to Done.
  int first = 0; // 2
  first = (cur - (total - 1));

  loadSettings();

  while (1) {

      // Make sure buttons unpressed
      do {
          delay(100);
      } while ((digitalRead(buttonPin1) == 0) || (digitalRead(buttonPin2) == 0));

      tft.fillScreen(TFT_BLUE);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);

      tft.setCursor(220, 5, 4);
      tft.print(">");
      tft.setCursor(220, 105, 4);
      tft.print("+");

      // Display
      for (int i = 0; i < total; i++) {
          int y = 5 + i * 25;
          int x = 5;

          tft.setCursor(x, y, 4);

          int  item = i + first;
          bool sel;
          if (cur == item) {
              tft.setTextColor(TFT_BLUE, TFT_WHITE);
              sel = true;
          } else {
              tft.setTextColor(TFT_WHITE, TFT_BLUE);
              sel = false;
          }

          tft.print(" ");
          tft.print(menuitems[item].label);
          if (menuitems[item].toggle) {
              tft.print(*menuitems[item].val ? " [Yes]" : " [No]");
          } else {
              tft.print(" ");
          }
      }

      // Detect click
      do {
          ReadButtons();
          delay(100);
      } while (buttonPushCounter1 == 0 && buttonPushCounter2 == 0);

      do {
          delay(100);
      } while ((digitalRead(buttonPin1) == 0) || (digitalRead(buttonPin2) == 0));

      //Serial.printf("cur %d, %d, %d", cur, menuitems[cur].toggle, menuitems[cur].fn);
      
      if (buttonPushCounter2) {
          if (menuitems[cur].toggle) {
              *menuitems[cur].val = !*menuitems[cur].val;
          } else {
              if (menuitems[cur].fn) {
                  (menuitems[cur].fn)(); // call function
              } else {
                  // Done
                  saveSettings();
                  return;
              }
          }
      }

      if (buttonPushCounter1) {
          cur = cur + 1;
          if (cur >= icount) cur = 0; // wrap
          first = (cur - (total - 1));
          if (first < 0) first = 0;
      }
  }
}

//--------------------------------------------------------
void tftScreen1() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 5, 4);
  tft.print("Time  ");
  tft.setCursor(120, 5, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println(TotalTimeMin);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.setCursor(5, 30, 4);
  tft.print("VO2 ");
  tft.setCursor(120, 30, 4);
  tft.println(vo2Max);

  tft.setCursor(5, 55, 4);
  tft.print("VO2MAX ");
  tft.setCursor(120, 55, 4);
  tft.println(vo2MaxMax);

  if (settings.co2_on) {
      tft.setCursor(5, 80, 4);
      tft.print("VCO2 ");
      tft.setCursor(120, 80, 4);
      tft.println(vco2Max);
  }

  tft.setCursor(5, 105, 4);
  tft.print("RQ ");
  tft.setCursor(120, 105, 4);
  tft.println(respq);
}

//--------------------------------------------------------
void tftScreen2() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 5, 4);
  tft.print("Time  ");
  tft.setCursor(120, 5, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println(TotalTimeMin);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.setCursor(5, 30, 4);
  tft.print("outO2% ");
  tft.setCursor(120, 30, 4);
  tft.println(lastO2);

  if (settings.co2_on) {
      tft.setCursor(5, 55, 4);
      tft.print("CO2% ");
      tft.setCursor(120, 55, 4);
      tft.println(co2perc, 3);
  }

  tft.setCursor(5, 80, 4);
  tft.print("kcal ");
  tft.setCursor(120, 80, 4);
  tft.println(calTotal, 0);

  tft.setCursor(5, 105, 4);
  tft.print("kcal/h ");
  tft.setCursor(120, 105, 4);
  tft.println(vo2CalH, 0);
}

//--------------------------------------------------------
void tftScreen3() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 5, 4);
  tft.print("Time  ");
  tft.setCursor(120, 5, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println(TotalTimeMin);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.setCursor(5, 30, 4);
  tft.print("Bvol ");
  tft.setCursor(120, 30, 4);
  tft.println(volumeExp);

  tft.setCursor(5, 55, 4);
  tft.print("VEmin ");
  tft.setCursor(120, 55, 4);
  tft.println(volumeVEmean, 1);

  tft.setCursor(5, 80, 4);
  tft.print("Brate ");
  tft.setCursor(120, 80, 4);
  tft.println(freqVEmean, 1);

  tft.setCursor(5, 105, 4);
  tft.print("O2%diff ");
  tft.setCursor(120, 105, 4);
  float co2diff = lastO2 - initialO2;
  tft.println(co2diff);
}
//--------------------------------------------------------
void tftScreen4() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 5, 4);
  tft.print("Time ");
  tft.setCursor(120, 5, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println(TotalTimeMin);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.setCursor(5, 30, 4);
  tft.print("O2% ");
  tft.setCursor(120, 30, 4);
  tft.println(lastO2);

  tft.setCursor(5, 55, 4);
  tft.print("CO2ppm ");
  tft.setCursor(120, 55, 4);
  tft.println(co2ppm, 0);

  tft.setCursor(5, 80, 4);
  tft.print("Pressure ");
  tft.setCursor(120, 80, 4);
  tft.println((PresPa / 100));

  tft.setCursor(5, 105, 4);
  tft.print("Humidity ");
  tft.setCursor(120, 105, 4);
  float co2diff = lastO2 - initialO2;
  tft.println(co2hum, 0);
}

//--------------------------------------------------------
void tftScreen5() {

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 5, 4);
  tft.print("Time  ");
  tft.setCursor(120, 5, 4);
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.println(TotalTimeMin);

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 30, 4);
  tft.print("VO2 ");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(90, 30, 7);
  tft.println(vo2Max);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(5, 80, 4);
  tft.print("RQ ");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(90, 80, 7);
  tft.println(respq);
}

//--------------------------------------------------------
void tftParameters() {

  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);

  tft.setCursor(5, 5, 4);
  tft.print("deg. C");
  tft.setCursor(120, 5, 4);
  tft.println(co2temp, 1);

  tft.setCursor(5, 30, 4);
  tft.print("hPA");
  tft.setCursor(120, 30, 4);
  tft.println((PresPa / 100));

  tft.setCursor(5, 55, 4);
  tft.print("kg/m3");
  tft.setCursor(120, 55, 4);
  tft.println(rho, 4);

  tft.setCursor(5, 80, 4);
  tft.print("kg");
  tft.setCursor(45, 80, 4);
  tft.println(settings.weightkg, 1);

  tft.setCursor(120, 80, 4);
  tft.print("cor");
  tft.setCursor(180, 80, 4);
  tft.println(settings.correctionSensor, 2);

  tft.setCursor(5, 105, 4);
  tft.print("inO2%");
  tft.setCursor(120, 105, 4);
  tft.println(initialO2);
}

//--------------------------------------------------------
void ReadButtons() {
  buttonState1 = digitalRead(buttonPin1);
  buttonState2 = digitalRead(buttonPin2);
  if (buttonState1 == LOW) {
      buttonPushCounter1++;
  } else {
      buttonPushCounter1 = 0;
  }
  if (buttonState2 == LOW) {
      buttonPushCounter2++;
  } else {
      buttonPushCounter2 = 0;
  }
}

//---------------------------------------------------------

void GetWeightkg() {

  Timer5s = millis();
  int weightChanged = 0;
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("Enter weight in kg", 20, 10, 4);
  tft.drawString(String(settings.weightkg), 48, 48, 7);

  while ((millis() - Timer5s) < 5000) {
      ReadButtons();

      if (buttonPushCounter1 > 0) {
          settings.weightkg = settings.weightkg - 0.5;
          if (buttonPushCounter1 > 8) settings.weightkg = settings.weightkg - 1.5;
          weightChanged = 1;
      }

      if (buttonPushCounter2 > 0) {
          settings.weightkg = settings.weightkg + 0.5;
          if (buttonPushCounter2 > 8) settings.weightkg = settings.weightkg + 1.5;
          weightChanged = 1;
      }

      if (settings.weightkg < 20) settings.weightkg = 20;
      if (settings.weightkg > 200) settings.weightkg = 200;
      if (weightChanged > 0) {
          tft.fillScreen(TFT_BLUE);
          tft.drawString("New weight in kg is:", 10, 10, 4);
          tft.drawString(String(settings.weightkg), 48, 48, 7);
          weightChanged = 0;
          Timer5s = millis();
      }
      delay(200);
  }
}

//---------------------------------------------------------

void readVoltage() {
  uint16_t v = analogRead(ADC_PIN);
  Battery_Voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  if (Battery_Voltage >= 4.3) tft.setTextColor(TFT_BLACK, TFT_WHITE); // USB powered, charging
  if (Battery_Voltage < 4.3) tft.setTextColor(TFT_BLACK, TFT_GREEN);  // battery full
  if (Battery_Voltage < 3.9) tft.setTextColor(TFT_BLACK, TFT_YELLOW); // battery half
  if (Battery_Voltage < 3.7) tft.setTextColor(TFT_WHITE, TFT_RED);    // battery critical
  tft.setCursor(0, 0, 4);
  tft.print(String(Battery_Voltage) + "V");
}

//---------------------------------------------------------

void InitBLE() {
//Serial.println("In InitBLE");

  BLEDevice::init("VO2-HR"); // creates the device name

  // (1) Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer(); // creates the BLE server
  pServer->setCallbacks(new MyServerCallbacks()); // creates the server callback function

  // (2) Create the BLE Service "vo2maxRateService"
  if (settings.BLE_on) {
      BLEService *pHeart = pServer->createService(vo2maxRateService); // creates heatrate service with 0x180D

      // (3) Create the characteristics, descriptor, notification
      pHeart->addCharacteristic(&vo2maxRateMeasurementCharacteristics); // creates heartrate
      // characteristics 0x2837
      vo2maxRateDescriptor.setValue("Rate from 0 to 200"); // describtion of the characteristic
      vo2maxRateMeasurementCharacteristics.addDescriptor(&vo2maxRateDescriptor);
      vo2maxRateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      // client switches server notifications on/off via BLE2902 protocol

      // (4) Create additional characteristics
      pHeart->addCharacteristic(&startTestCharacteristic);
      startTestDescriptor.setValue("Start Test bit");
      startTestCharacteristic.addDescriptor(&startTestDescriptor);
      pHeart->start();
      
        // Vo2 service
      BLEService *pvo2 = pServer->createService(vo2RateService); // creates heatrate service with 0x180D
      pvo2->addCharacteristic(&vo2RateMeasurementCharacteristics); // creates vo2rate
      vo2RateDescriptor.setValue("VO2 Measurement");
      vo2RateMeasurementCharacteristics.addDescriptor(&vo2RateDescriptor);
      vo2RateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      pvo2->start();
      // Vco2 service
      BLEService *pvco2 = pServer->createService(vco2RateService); // creates heatrate service with 0x180D
      pvco2->addCharacteristic(&vco2RateMeasurementCharacteristics); // creates vo2rate
      vco2RateDescriptor.setValue("VCO2 Measurement");
      vco2RateMeasurementCharacteristics.addDescriptor(&vco2RateDescriptor);
      vco2RateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      pvco2->start();
      //RQ service
      BLEService *pRQ = pServer->createService(RQRateService); // creates heatrate service with 0x180D
      pRQ->addCharacteristic(&RQRateMeasurementCharacteristics); // creates vo2rate
      RQRateDescriptor.setValue("RQ Measurement");
      RQRateMeasurementCharacteristics.addDescriptor(&vo2RateDescriptor);
      RQRateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      pRQ->start();
      // // o2perc service
      // BLEService *po2perc = pServer->createService(o2percRateService); // creates heatrate service with 0x180D
      // po2perc->addCharacteristic(&o2percRateMeasurementCharacteristics); // creates vo2rate
      // o2percRateDescriptor.setValue("o2perc Measurement");
      // o2percRateMeasurementCharacteristics.addDescriptor(&o2percRateDescriptor);
      // o2percRateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      // po2perc->start();
      // // co2perc service
      // BLEService *pco2perc = pServer->createService(co2percRateService); // creates heatrate service with 0x180D
      // pco2perc->addCharacteristic(&co2percRateMeasurementCharacteristics); // creates vo2rate
      // co2percRateDescriptor.setValue("co2perc Measurement");
      // co2percRateMeasurementCharacteristics.addDescriptor(&vo2RateDescriptor);
      // co2percRateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      // pco2perc->start();
      // // kcal service
      // BLEService *pkcal = pServer->createService(kcalRateService); // creates heatrate service with 0x180D
      // pco2perc->addCharacteristic(&kcalRateMeasurementCharacteristics); // creates vo2rate
      // kcalRateDescriptor.setValue("kcal Measurement");
      // kcalRateMeasurementCharacteristics.addDescriptor(&vo2RateDescriptor);
      // kcalRateMeasurementCharacteristics.addDescriptor(new BLE2902()); // necessary for notifications
      // pkcal->start();
  }

  BLEAdvertising *pAdvertising = pServer->getAdvertising();

  if (settings.BLE_on) {
      pAdvertising->addServiceUUID(vo2maxRateService);
      pAdvertising->addServiceUUID(vo2RateService);
      pAdvertising->addServiceUUID(vco2RateService);
      pAdvertising->addServiceUUID(RQRateService);
  }
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  // (6) start the server and the advertising
  BLEDevice::startAdvertising();
}
