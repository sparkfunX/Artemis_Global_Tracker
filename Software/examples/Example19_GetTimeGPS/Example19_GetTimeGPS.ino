/*
 Artemis Global Tracker
 Example: GetTimeGPS

 Simple mod from Paul Clark (PaulZC)'s Sketch from 30th January 2020

 ** Set the Board to "SparkFun Artemis Module" **

 This example powers up the ZOE-M8Q and reads the fix, latitude, longitude and altitude.
 
 You will need to install the SparkFun u-blox library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library

 The ZOE-M8Q shares I2C Port 1 (Wire1) with the MS8607: SCL = D8; SDA = D9

 Power for the ZOE is switched via Q2.
 D26 needs to be pulled low to enable the GNSS power.

 The ZOE's PIO14 pin (geofence signal) is conected to D10,
 so let's configure D10 as an input even though we aren't using it yet.

 To prevent bad tings happening to the AS179 RF antenna switch,
 D27 and D22 should also be pulled low to disable the 5.3V rail.

*/

// Artemis Tracker pin definitions
#define spiCS1              4  // D4 can be used as an SPI chip select or as a general purpose IO pin
#define geofencePin         10 // Input for the ZOE-M8Q's PIO14 (geofence) pin
#define busVoltagePin       13 // Bus voltage divided by 3 (Analog in)
#define iridiumSleep        17 // Iridium 9603N ON/OFF (sleep) pin: pull high to enable the 9603N
#define iridiumNA           18 // Input for the Iridium 9603N Network Available
#define LED                 19 // White LED
#define iridiumPwrEN        22 // ADM4210 ON: pull high to enable power for the Iridium 9603N
#define gnssEN              26 // GNSS Enable: pull low to enable power for the GNSS (via Q2)
#define superCapChgEN       27 // LTC3225 super capacitor charger: pull high to enable the super capacitor charger
#define superCapPGOOD       28 // Input for the LTC3225 super capacitor charger PGOOD signal
#define busVoltageMonEN     34 // Bus voltage monitor enable: pull high to enable bus voltage monitoring (via Q4 and Q3)
#define spiCS2              35 // D35 can be used as an SPI chip select or as a general purpose IO pin
#define iridiumRI           41 // Input for the Iridium 9603N Ring Indicator
// Make sure you do not have gnssEN and iridiumPwrEN enabled at the same time!
// If you do, bad things might happen to the AS179 RF switch!

#include <Wire.h> // Needed for I2C

#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
SFE_UBLOX_GPS myGPS;

void gnssON(void) // Enable power for the GNSS
{
  digitalWrite(gnssEN, LOW); // Disable GNSS power (HIGH = disable; LOW = enable)
  pinMode(gnssEN, OUTPUT); // Configure the pin which enables power for the ZOE-M8Q GNSS
}

void gnssOFF(void) // Disable power for the GNSS
{
  pinMode(gnssEN, INPUT_PULLUP); // Configure the pin which enables power for the ZOE-M8Q GNSS
  digitalWrite(gnssEN, HIGH); // Disable GNSS power (HIGH = disable; LOW = enable)
}

void setup()
{
  pinMode(LED, OUTPUT);

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  // Set up the I2C pins
  Wire1.begin();

  // Start the console serial port
  Serial.begin(115200);
  while (!Serial) // Wait for the user to open the serial monitor
    ;
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Example: GetTimeGPS"));
  Serial.println();

  //empty the serial buffer
  while(Serial.available() > 0)
    Serial.read();

  //wait for the user to press any key before beginning
  Serial.println(F("Please check that the Serial Monitor is set to 115200 Baud"));
  Serial.println(F("and that the line ending is set to Newline."));
  Serial.println(F("Then click Send to start the example."));
  Serial.println();
  while(Serial.available() == 0)
    ;

  gnssON(); // Enable power for the GNSS
  delay(1000); // Let the ZOE power up

  if (myGPS.begin(Wire1) == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("Ublox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }

  //myGPS.enableDebugging(); // Enable debug messages
  myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)
}

void loop()
{
  digitalWrite(LED, LOW);

  byte fixType = myGPS.getFixType();
  Serial.print(F("Fix: "));
  if(fixType == 0) Serial.print(F("No fix"));
  else if(fixType == 1) Serial.print(F("Dead reckoning"));
  else if(fixType == 2) Serial.print(F("2D"));
  else if(fixType == 3) Serial.print(F("3D"));
  else if(fixType == 4) Serial.print(F("GNSS+Dead reckoning"));

  if(fixType > 0)
  {
    byte SIV = myGPS.getSIV();
    Serial.print(F(" SIV: "));
    Serial.print(SIV);
    Serial.print(" ");
    Serial.print(myGPS.getYear());
    Serial.print("-");
    Serial.print(myGPS.getMonth());
    Serial.print("-");
    Serial.print(myGPS.getDay());
    Serial.print(" ");
    Serial.print(myGPS.getHour());
    Serial.print(":");
    Serial.print(myGPS.getMinute());
    Serial.print(":");
    Serial.print(myGPS.getSecond());

    Serial.print("  Time is ");
    if (myGPS.getTimeValid() == false)
    {
      Serial.print("not ");
    }
    Serial.print("valid  Date is ");
    if (myGPS.getDateValid() == false)
    {
      Serial.print("not ");
    }
    Serial.print("valid");
  }

  Serial.println();

  digitalWrite(LED, HIGH);
  delay(1000);
}
