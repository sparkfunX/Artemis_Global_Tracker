/*
 Artemis Global Tracker
 Production Test Code - Part 2
 
 Written by Paul Clark (PaulZC)
 22nd July 2020

 This code tests that both the ZOE GNSS receiver and Iridium modem can receive an RF signal.
 It is the same as running Example5_GNSS followed by Example9_GetTime but instead
 flashes the white LED to indicate progress.
 
 You can power the AGT from a LiPo instead of USB-C for this test.

 The white LED will flash at:
  1Hz (0.1s on, 0.9s off) while a 3D GPS fix is acquired
  2Hz (0.1s on, 0.4s off) while the Iridium time is acquired
  
 If the LED stays on continuously, the test has passed
 If the LED goes off, the test has failed

 ** Set the Board to "SparkFun Artemis Module" **

 You will need to install the Qwiic_PHT_MS8607_Library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_PHT_MS8607_Arduino_Library
 (Search for MS8607 in the Library Manager)

 You will need to install the SparkFun u-blox library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library

 Version History:
 V1.0: 22nd July 2020
   First release

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
#define iridiumTx           25 // The Iridium 9603N Tx pin (RX_OUT) (Serial1 Rx)
#define iridiumRx           24 // The Iridium 9603N Rx pin (TX_IN) (Serial1 Tx)
// Make sure you do not have gnssEN and iridiumPwrEN enabled at the same time!
// If you do, bad things might happen to the AS179 RF switch!

#include <Wire.h> // Needed for I2C

#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
//The ZOE-M8Q shares I2C Port 1 (Wire1) with the MS8607: SCL = D8; SDA = D9
SFE_UBLOX_GPS myGPS;

// Declares a Uart object called Serial using instance 1 of Apollo3 UART peripherals with RX on variant pin 25 and TX on pin 24
// (note, in this variant the pins map directly to pad, so pin === pad when talking about the pure Artemis module)
Uart iridiumSerial(1, iridiumTx, iridiumRx);

#include <IridiumSBD.h> //http://librarymanager/All#IridiumSBDI2C
#include <time.h>
#define DIAGNOSTICS false // Change this to true see IridiumSBD diagnostics
// Declare the IridiumSBD object
IridiumSBD modem(iridiumSerial, iridiumSleep, iridiumRI);

// IridiumSBD Callback - this code is called while the 9603N is trying to get the time
bool ISBDCallback()
{
  if ((millis() % 500) < 100) // Check if the LED should be on
  {
    digitalWrite(LED, HIGH); // Turn the LED on
  }
  else
  {
    digitalWrite(LED, LOW); // Turn the LED off
  }

  return true;
}

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
  // Configure the I/O pins
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW); // Turn the LED off

  gnssOFF(); // Disable power for the GNSS

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
  pinMode(iridiumSleep, OUTPUT); // Iridium 9603N On/Off (Sleep) pin
  digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
  pinMode(iridiumRI, INPUT); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT); // Configure the Iridium Network Available as an input
  pinMode(superCapPGOOD, INPUT); // Configure the super capacitor charger PGOOD input

  // Set up the I2C pins for the ZOE
  Wire1.begin();

  // Start the console serial port
  Serial.begin(115200);

  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Production Test Part 2"));
  Serial.println();
  Serial.println(F("The white LED will flash at:"));
  Serial.println(F("1Hz (0.1s on, 0.9s off) while a 3D GPS fix is acquired"));
  Serial.println(F("2Hz (0.1s on, 0.4s off) while the Iridium time is acquired"));
  Serial.println();
  Serial.println(F("If the LED stays on continuously, the test has passed"));
  Serial.println(F("If the LED goes off, the test has failed"));
  Serial.println();

// -----------------------------------------------------------------------
// Test 1 - check that the ZOE GNSS can establish a 3D fix
// -----------------------------------------------------------------------

  gnssON(); // Enable power for the GNSS
  delay(1000); // Let the ZOE power up

  if (myGPS.begin(Wire1) == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("Test 1 Step 1: The ZOE GNSS did not begin!"));
    fail();
  }

  //myGPS.enableDebugging(); // Enable debug messages
  myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

  bool keep_going = true;
  int previousLEDstate = LOW;
  int currentLEDstate = LOW;
  while (keep_going)
  {
    if ((millis() % 1000) < 100) // Check if the LED should be on
    {
      digitalWrite(LED, HIGH); // Turn the LED on
      currentLEDstate = HIGH; // Record the LED state
    }
    else
    {
      digitalWrite(LED, LOW); // Turn the LED off
      currentLEDstate = LOW; // Record the LED state      
      if ((currentLEDstate == LOW) && (previousLEDstate == HIGH)) // Has the LED just turned off?
      {
        byte fixType = myGPS.getFixType(); // Check the GPS fix once per second while the LED is off (getFixType can take longer than 0.1s)
        if(fixType == 3) keep_going = false; // Check if a 3D fix has been established
      }
    }
    previousLEDstate = currentLEDstate; // Update the previous LED state
  }

  Serial.println(F("Test 1: Passed"));
  
  gnssOFF(); // Disable power for the GNSS
  
  digitalWrite(LED, LOW); // Turn the LED off
  
// -----------------------------------------------------------------------
// Test 2 - check that the Iridium modem can receive the time
// -----------------------------------------------------------------------

  // Enable the supercapacitor charger
  digitalWrite(superCapChgEN, HIGH); // Enable the super capacitor charger
  unsigned long startMillis = millis();
  while ((millis() - startMillis) < 1000) // Wait for 1s but keep flashing the LED
  {
    if ((millis() % 500) < 100) // Check if the LED should be on
    {
      digitalWrite(LED, HIGH); // Turn the LED on
    }
    else
    {
      digitalWrite(LED, LOW); // Turn the LED off
    }
  }

  // Wait for the supercapacitor charger PGOOD signal to go high
  while (digitalRead(superCapPGOOD) == false)
  {
    if ((millis() % 500) < 100) // Check if the LED should be on
    {
      digitalWrite(LED, HIGH); // Turn the LED on
    }
    else
    {
      digitalWrite(LED, LOW); // Turn the LED off
    }
  }

  // Enable power for the 9603N
  digitalWrite(iridiumPwrEN, HIGH); // Enable Iridium Power
  startMillis = millis();
  while ((millis() - startMillis) < 1000) // Wait for 1s but keep flashing the LED
  {
    if ((millis() % 500) < 100) // Check if the LED should be on
    {
      digitalWrite(LED, HIGH); // Turn the LED on
    }
    else
    {
      digitalWrite(LED, LOW); // Turn the LED off
    }
  }

  // Start the serial port connected to the satellite modem
  iridiumSerial.begin(19200);

  // Begin satellite modem operation
  int err = modem.begin();
  if (err != ISBD_SUCCESS)
  {
    Serial.println(F("Test 2 Step 1: The Iridium modem did not begin!"));
    if (err == ISBD_NO_MODEM_DETECTED)
      Serial.println(F("Test 2 Step 1: No modem detected: check wiring."));
    fail();
  }

  keep_going = true;
  while (keep_going)
  {
    startMillis = millis();
    while ((millis() - startMillis) < 5000) // Wait for 5s but keep flashing the LED
    {
      if ((millis() % 500) < 100) // Check if the LED should be on
      {
        digitalWrite(LED, HIGH); // Turn the LED on
      }
      else
      {
        digitalWrite(LED, LOW); // Turn the LED off
      }
    }
    
   struct tm t;
   err = modem.getSystemTime(t);
   if (err == ISBD_SUCCESS)
   {
      if (t.tm_year >= 120) // Check if the year is valid (Iridium years start at 1900)
        keep_going = false;
   }
  }

  Serial.println(F("Test 2: Passed"));

  modem.sleep(); // Power down the modem
  digitalWrite(iridiumSleep, LOW); // Make sure the Iridium 9603N is asleep (HIGH = on; LOW = off/sleep)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

  digitalWrite(LED, LOW); // Turn the LED off
  
// -----------------------------------------------------------------------
// Tests complete!
// -----------------------------------------------------------------------

  Serial.println();
  Serial.println(F("*** Tests complete ***"));
  Serial.println();

  digitalWrite(LED, HIGH); // Turn the LED on continuously to incidate success

}

void loop()
{
  ; // Do nothing
}

void fail() // Stop the test
{
  Serial.println(F("!!! TEST FAILED! !!!"));
  gnssOFF(); // Disable power for the GNSS
  modem.sleep(); // Power down the modem
  digitalWrite(iridiumSleep, LOW); // Make sure the Iridium 9603N is asleep (HIGH = on; LOW = off/sleep)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
  digitalWrite(LED, LOW); // Turn the LED off to indicate a fail
  while(1)
    ; // Do nothing more
}

#if DIAGNOSTICS
void ISBDConsoleCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}

void ISBDDiagsCallback(IridiumSBD *device, char c)
{
  Serial.write(c);
}
#endif
