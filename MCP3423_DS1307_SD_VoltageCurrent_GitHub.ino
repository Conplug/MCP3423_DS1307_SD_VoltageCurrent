//
// Copyright (c) 2019 Conplug
// Author: Hartman Hsieh
//
// Components :
//   ADC : MCP3423
//   Storage : MicroSD
//   RTC : DS1307
//   Display : Gravity:LCD1602 Module V1.0
//
// Required Library :
//   https://github.com/uChip/MCP342X
//   https://github.com/bearwaterfall/DFRobot_LCD-master
//

const int ADC_MCP_ADDRESS = 0x6E; // IIC Address of MCP3423

const int SD_CARD_CHIP_SELECT_PIN = 6; // SD CARD SPI Chipset Select Pin
const String LogFileName = "vc.txt";

const double RVH = 51.0 * 1000; // Resistance Divider
const double RVL = 1.1 * 1000; // Resistance Divider
const double RI = 0.005; // Shunt Resistance

//
// MCP3423
//
#include  <Wire.h>
#include  <MCP342X.h>
MCP342X AdcMcp (ADC_MCP_ADDRESS);

//
// IIC LCD Module
//
#include "DFRobot_LCD.h"
DFRobot_LCD Lcd(16, 2);  //16 characters and 2 lines of show

//
// SD Card
//
#include <SPI.h>
#include <SD.h>
File SdFile;
Sd2Card Card;

//
// RTC DS1307
//
#include <DS1307.h> // IIC Address is 0x68
int Rtc[7];
//char* WeekDays[]={"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};//week 

static int16_t Result1 = 0, Result2 = 0;
double Voltage1 = 0, PreVoltage = 0;
double Current1 = 0, PreCurrent = 0;

unsigned long CurrentTime = 0, PreviousTime = 0;
unsigned long  UpdateInterval = 5 * 60; // seconds

int LoopIndex = 0;

void setup() {

  Lcd.init();
  Lcd.display();

  Lcd.setCursor(0, 0);
  Lcd.print("                ");
  Lcd.setCursor(0, 1);
  Lcd.print("                ");
  
  Wire.begin();  // join I2C bus
  TWBR = 12;  // 400 kHz (maximum)
  
  Serial.begin(9600); // Open serial connection to send info to the host
  while (!Serial) {}  // wait for Serial comms to become ready
  Serial.println("Starting up");
  Serial.println("Testing device connection...");
  Serial.println(AdcMcp.testConnection() ? "MCP342X connection successful" : "MCP342X connection failed");

  //
  // Init SD Card
  //
  pinMode (SD_CARD_CHIP_SELECT_PIN, OUTPUT);

  if (!Card.init(SPI_HALF_SPEED, SD_CARD_CHIP_SELECT_PIN)) {
    Serial.println("initialization failed. Check: SD Card");
    return;
  } else {
    Serial.println("============= Card Information ==================");
  }

  //
  // Print Card Type
  //
  Serial.print("Card type: ");
  switch (Card.type()) {
    case SD_CARD_TYPE_SD1:
      Serial.println("SD1");
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println("SD2");
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println("SDHC");
      break;
    default:
      Serial.println("Unknow");
  }

  Serial.println("Initializing SD card...");

  if (!SD.begin(SD_CARD_CHIP_SELECT_PIN)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

}  // End of setup()

void loop() {

    unsigned long file_size = 0;

    SdFile = SD.open(LogFileName, FILE_READ);
    file_size = SdFile.size();
    SdFile.close();

    AdcMcp.configure( MCP342X_MODE_CONTINUOUS |
                     MCP342X_CHANNEL_1 |
                     MCP342X_SIZE_16BIT |
                     MCP342X_GAIN_1X
                   );
    AdcMcp.startConversion();
    AdcMcp.getResult(&Result1);
    Voltage1 = (((RVH + RVL) * 2.048) / (RVL * 32767.0)) * Result1;

    AdcMcp.configure( MCP342X_MODE_CONTINUOUS |
                     MCP342X_CHANNEL_2 |
                     MCP342X_SIZE_16BIT |
                     MCP342X_GAIN_1X
                   );
    AdcMcp.startConversion();
    AdcMcp.getResult(&Result2);
    Current1 = ((2.048 * Result2) / 32767.0) / RI;
    if (Current1 >= -0.02 && Current1 <= 0.02) Current1 = 0.0;

    Lcd.setCursor(0, 0);
    Lcd.print(Voltage1);
    Lcd.print("V,");
    Lcd.print(Current1);
    Lcd.print("A");
    Lcd.print("        ");
    //Lcd.setCursor(13, 0);
    //Lcd.print(LoopIndex);

    Lcd.setCursor(0, 1);
    if ((LoopIndex / 10) % 2 == 0) { // Every 10 loop, exchange display.
      Lcd.print("Size = ");
      Lcd.print(file_size);
      Lcd.print("        ");
    }
    else {
      RTC.get(Rtc, true);

      Lcd.print( Rtc[5]);
      Lcd.print("-");   
      Lcd.print( Rtc[4]);
      Lcd.print(" ");

      Lcd.print( Rtc[2]);
      Lcd.print(":");
      Lcd.print( Rtc[1]);
      Lcd.print(":");    
      Lcd.print( Rtc[0]);
      Lcd.print("  ");
    }

    if (abs(Voltage1 - PreVoltage) >= 0.01 || abs(Current1 - PreCurrent) >= 0.01) {

      SaveToSd(Voltage1, Current1);
      
      PreVoltage = Voltage1;
      PreCurrent = Current1;
      PreviousTime = millis();
    }
    else {
      CurrentTime = millis();
      if (PreviousTime == 0 || (CurrentTime - PreviousTime) > (UpdateInterval * 1000)) {

        SaveToSd(Voltage1, Current1);
      
        PreviousTime = millis();
      }
    }
    
    LoopIndex++;
    if (LoopIndex >= 99) LoopIndex = 0; // This yields a range of -32,768 to 32,767 (minimum value of -2^15 and a maximum value of (2^15) - 1)

}  // End of loop()

void SaveToSd(double Voltage, double Current)
{
  RTC.get(Rtc, true);
  SdFile = SD.open(LogFileName, FILE_WRITE);
  
  SdFile.print( Rtc[6]); // YEAR
  SdFile.print("-");   
  SdFile.print( Rtc[5]); // MONTH
  SdFile.print("-");   
  SdFile.print( Rtc[4]); // DATE
  SdFile.print(" ");
  SdFile.print( Rtc[2]); // HOUR
  SdFile.print(":");
  SdFile.print( Rtc[1]); // MIN
  SdFile.print(":");    
  SdFile.print( Rtc[0]); //SEC
  SdFile.print(",");
  SdFile.print(Voltage, 6);
  SdFile.print(",");
  SdFile.print(Current, 6);
  SdFile.println("");
  SdFile.flush();
  SdFile.close();
  
  Serial.print(millis());
  Serial.print(",");
  Serial.print(Current, 6);
  Serial.print("V,");
  Serial.print(Current, 6);
  Serial.println("A");

}
