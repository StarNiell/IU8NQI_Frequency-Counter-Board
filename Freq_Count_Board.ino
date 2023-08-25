// 25/08/2023 
// IU8NQUI - Arduino Frequency Meter
// Using DIY SAB6456 Prescaler Module
// Based on https://github.com/imsaiguy/Frequency-Counter-Board
// 

#include <FreqCount.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

#include "SevenSegmentsIU8NQI13pt7b.h"

#define button2 A0  // Calibration SW
#define div64256 2  // D2 to SAB6456 Prescaler Module PIN 3 

double frequency  = 0.0; // frequency calculated
double uncalfreq = 0.0;
float cal = 1.0000f;     // calibration error
float readcal = 0.000f;  // used to read cal value in EEPROM
unsigned long count = 0; // counter value
int mode = 1;            // instrument mode
int divisor = 64;        // set to 64 or 256
long sensorValue = 0L;   // Feedback (amplitude) input signal
int sql = 29;            // Squeltch: set it for ignore low level signals
long c = 0L;
long s = 0L;
double rawval = 0.000;
double calibrateKhz = 0.0; // Add or Subtract your value but it's better to use the calibrate function


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


//========================================================================
void setup() {
  Serial.begin(9600);
  SetupDisplayI2C();
  SetupFreqCount();
}

void loop() {
  getAmplitude(); // Read the amplitude signal (sensorValue) on A7 pin and let pass only values over sql (squelch) values
  getFrequency(); // Read and calculate the frequency
  
  // Calibration catch routine
  if (mode == 2) calibration();
  if (mode == 1) updatedisplay();
  if (digitalRead(button2) == 0) mode = 2;
}

void SetupFreqCount()  {

  pinMode(button2, INPUT_PULLUP);
  pinMode(A7, INPUT);
  pinMode(div64256, OUTPUT);

  if (divisor == 64)
  {
    digitalWrite(div64256, HIGH);
  }
  else {
    digitalWrite(div64256, LOW);
  }

  EEint();                              // get stored cal data
  FreqCount.begin(1000);                // start counter
}

void SetupDisplayI2C()
{
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(3); // Draw 3X-scale text
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(" IU8NQI");
  display.display();
  delay(2000);

  display.setTextSize(2); // Draw 2X-scale text
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(" Frequency   Meter");
  display.display();
  delay(2000);

  // Set the Digit 7 Segments Font
  display.setFont(&SevenSegmentsIU8NQI13pt7b);
  display.setTextSize(1); // Draw 1X-scale text
 
}

void getFrequency()
{
  if (FreqCount.available()) {
    // Read the prescaled frequency on D5 Pin
    count = FreqCount.read();

    // Apply divisor and calibration
    rawval = count / 1000000.0;
    count = count * divisor;
    uncalfreq = count / 1000000.0;

    // Let pass only values over sql 
    if (sensorValue > sql)
      frequency = uncalfreq * cal;
    else
      frequency = 0;
  }
}

//========================================================================
void updatedisplay(){

  double f = frequency * 1000;   // Trasform in Mhz

  // Prepare variables for printing on display
  String MhzString = "";
  String KhzString = "";
  String initString = "";
  String dotString = "";
  String midString = "";
  String finalString = "";
  int Khz = 0;

  // Print only if f > 0
  if (f > 0)
  {

    // apply the calibrateKhz (but it's bettere use the calibrate function!)
    f = f + calibrateKhz;

    // Format a string in Khz format, eg: 27.205
    int Mhz = f / 1000;
    MhzString = String(Mhz);
    Khz = round(f - (Mhz * 1000));
    KhzString = String(Khz);

    if (Mhz > 0)
    {
      dotString = ".";
      if (Mhz < 100)
        initString =  " ";
      else if (Mhz < 10)
      {
        initString = "  ";
        dotString  = "";
      }
    }

    if (Khz < 100)
      midString = "0";

    if (Khz < 10)
      midString = "00";

    finalString = initString + MhzString + dotString + midString + KhzString;
  }
  else 
  {
    finalString = "     0";
  }

  // Print on serial the frequency and the SensorValue for DEBUG
  Serial.println(String(f));
  Serial.println(sensorValue);

  // Printthe frequency value in Khz on Display
  display.clearDisplay();
  display.setCursor(0, 30);
  display.println(finalString);
  display.display();
  
}

void calibration(){
  display.clearDisplay();
  display.setFont();
  display.setTextSize(1); // Draw 2X-scale text

  display.clearDisplay();
  display.setCursor(0, 8);
  display.println("Calibration");
  display.println("Connect 27.205 Mhz");
  display.println("Then Press SW");
  display.display();
  delay(2000);

  while(digitalRead(button2) == 1) {} // wait until key press
  cal = 27.205 / uncalfreq;           // calculate error, new cal factor
  EEPROM.put(1,cal);                  // store it
  display.clearDisplay();
  display.setCursor(0, 9);
  display.println("Cal Complete");
  display.display();
  mode = 1;                           // exit mode 2
  delay(2000); 

  // Restore the Digit 7 Segments Font
  display.clearDisplay();
  display.setFont(&SevenSegmentsIU8NQI13pt7b);
  display.setTextSize(1); 
} 

void getAmplitude()
{
  // Read 50 times the analog value of A7 pin
  s = s + analogRead(A7);
  c = c + 1;
  if (c >= 50) 
  {
     // calculate the average value
     sensorValue = (s / 50);
     c = 0;
     s = 0;
  }
  
}

// Read the calibration value into the EEprom 
//========================================================================
void EEint(){
  byte value = EEPROM.read(0);             // check flag
  if (value == 0x55) {                     // already initialized
     EEPROM.get(1, readcal);               // read EEPROM data
     cal = readcal;                        
     Serial.println("Cal data found");
     Serial.print("Cal Value: ");
     Serial.println(cal);
     delay( 1000 ); 
     }
  else {                                   // first time use, initialize EEPROM
    EEPROM.write(0, 0x55);                 // write flag
    EEPROM.put(1,cal);                     // write initial cal value
    Serial.println("First time use");
    Serial.println("EEPROM initialized");
    delay( 1000 );     
    }
 }

//==============================================================
//==============================================================
