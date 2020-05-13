/*
  Artemis Global Tracker
  Example: A Better Tracker
  
  Written by Paul Clark (PaulZC)
  9th February 2020

  This example builds on the SimpleTracker example. The functionality is basically the same, except:
  * The transmit interval is stored in 'EEPROM' (flash memory) and can be updated via Iridium SBD message
  * The code supports message forwarding to another tracker/RockBLOCK via the Rock7 RockBLOCK Gateway

  You can change the message interval by changing the value of DEF_TXINT below. Or send a text
  message to the tracker from Rock7 Operations containing the text [INTERVAL=nnn]
  where nnn is the message interval in _minutes_.

  If you bought your 9603N from Rock7, you can have your messages delivered to another RockBLOCK automatically:
  http://www.rock7mobile.com/downloads/RockBLOCK-9603-Developers-Guide.pdf (see last page)
  Change DEF_DEST (below) to the serial number of the destination RockBLOCK.
  Change DEF_SOURCE (below) to the serial number of the RockBLOCK on the tracker.
  Both source and destination can be updated via a Mobile-Terminated SBD text message:
  Send a text message to the tracker containing the text [RBDESTINATION=nnnnn]
  where nnnnn is the RockBLOCK serial number of the destination RockBLOCK.
  Set RBDESTINATION back to zero to disable message forwarding: [RBDESTINATION=0]
  The destination tracker/RockBLOCK won't know who sent the message unless you also set RBSOURCE:
  Send a text message to the tracker containing the text [RBSOURCE=nnnnn]
  where nnnnn is the RockBLOCK serial number of the 9603N on the tracker.
  Note: the RockBLOCK gateway does not remove the destination RockBLOCK address from the SBD message
  so, in this code, it is included as a full CSV field.
  You can concatenate the configuration messages if required: [INTERVAL=5][RBDESTINATION=12345][RBSOURCE=54321]

  The message is transmitted in text and has the format:
  
    DateTime,Latitude,Longitude,Altitude,Speed,Course,PDOP,Satellites,Pressure,Temperature,Battery,Count

  If message forwarding is enabled, the format will be (using the above example):

    RB0012345,DateTime,Latitude,Longitude,Altitude,Speed,Course,PDOP,Satellites,Pressure,Temperature,Battery,Count,RB0054321

  If message forwarding is enabled, you will be charged _twice_ for each message. Once to send it and once to receive it.
  
  ** Set the Board to "SparkFun Artemis Module" **
  
  You will need to install this version of the Iridium SBD library
  before this example will run successfully:
  https://github.com/PaulZC/IridiumSBD
  
  You will also need to install the Qwiic_PHT_MS8607_Library:
  https://github.com/PaulZC/Qwiic_PHT_MS8607_Library
  (Available through the Arduino Library Manager: search for MS8607)
  
  Basic information on how to install an Arduino library is available here:
  https://learn.sparkfun.com/tutorials/installing-an-arduino-library
  
  This example is based extensively on the Artemis Low Power With Wake example
  By: Nathan Seidle
  SparkFun Electronics
  (Date: October 17th, 2019)
  License: This code is public domain. Based on deepsleep_wake.c from Ambiq SDK v2.2.0.
  Also the OpenLog_Artemis PowerDownRTC example
  Also the EEPROM GetPut example
  By: Nathan Seidle
  SparkFun Electronics
  Date: June 24th, 2019

  PaulZC and SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun!

*/

// Define how often messages are sent in MINUTES (max 1440)
// This is the _quickest_ messages will be sent. Could be much slower than this depending on:
// capacitor charge time; gnss fix time; Iridium timeout; etc.
// The default value will be overwritten with the one stored in Flash EEPROM - if one exists
// The value can be changed via a Mobile Terminated message: [INTERVAL=nnn]
#define DEF_TXINT 5 // DEFault TX INTerval (Minutes)

// Both the source and destination RockBLOCK serial numbers are stored in flash.
// These can be changed via MT message too: [RBSOURCE=nnnnn] and [RBDESTINATION=nnnnn]
// Set RBDESTINATION to zero to disable message forwarding
// The default values are:
#define DEF_SOURCE    0 // DEFault SOURCE RockBLOCK serial number (the serial number of the 9603N on this tracker)
#define DEF_DEST      0 // DEFault DESTination RockBLOCK (the serial number of the desination RockBLOCK)

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

//#define noLED // Uncomment this line to disable the White LED
//#define noTX // Uncomment this line to disable the Iridium SBD transmit if you want to test the code without using message credits

#include <EEPROM.h> // Needed for EEPROM storage on the Artemis

// Declares a Uart object called Serial using instance 1 of Apollo3 UART peripherals with RX on variant pin 25 and TX on pin 24
// (note, in this variant the pins map directly to pad, so pin === pad when talking about the pure Artemis module)
Uart iridiumSerial(1, 25, 24);

#include <IridiumSBD.h> // https://github.com/PaulZC/IridiumSBD
#define DIAGNOSTICS false // Change this to true to see IridiumSBD diagnostics
// Declare the IridiumSBD object (including the sleep (ON/OFF) and Ring Indicator pins)
IridiumSBD modem(iridiumSerial, iridiumSleep, iridiumRI);

#include <Wire.h> // Needed for I2C

#include <SparkFun_Ublox_Arduino_Library.h> //http://librarymanager/All#SparkFun_Ublox_GPS
SFE_UBLOX_GPS myGPS;

#include <MS8607_Library.h> // https://github.com/PaulZC/Qwiic_PHT_MS8607_Library
MS8607 barometricSensor; //Create an instance of the MS8607 object

// Include dtostrf
#include <avr/dtostrf.h>

#include "Tracker_EEPROM_Storage.h" // Include the trackerSettings
trackerSettings myTrackerSettings; // Define storage for the tracker settings

// Use this to keep a count of the second alarms from the rtc
volatile unsigned long seconds_count = 0;

// This flag indicates an interval alarm has occurred
volatile bool interval_alarm = false;

// iterationCounter is incremented each time a transmission is attempted.
// It helps keep track of whether messages are being sent successfully.
// It also indicates if the tracker has been reset (the count will go back to zero).
long iterationCounter = 0;

// More global variables
float vbat = 5.0; // Battery voltage
float latitude = 0.0; // Latitude in degrees
float longitude = 0.0; // Longitude in degrees
long altitude = 0; // Altitude above Median Seal Level in m
float speed = 0.0; // Ground speed in m/s
byte satellites = 0; // Number of satellites (SVs) used in the solution
long course = 0; // Course (heading) in degrees
int pdop = 0;  // Positional Dilution of Precision in m
int year = 1970; // GNSS Year
byte month = 1; // GNSS month
byte day = 1; // GNSS day
byte hour = 0; // GNSS hours
byte minute = 0; // GNSS minutes
byte second = 0; // GNSS seconds
int milliseconds = 0; // GNSS milliseconds
float pascals = 0.0; // Atmospheric pressure in Pascals
float tempC = 0.0; // Temperature in Celcius
byte fixType = 0; // GNSS fix type: 0=No fix, 1=Dead reckoning, 2=2D, 3=3D, 4=GNSS+Dead reckoning
bool PGOOD = false; // Flag to indicate if LTC3225 PGOOD is HIGH
int err; // Error value returned by IridiumSBD.begin

#define VBAT_LOW 2.8 // Minimum voltage for LTC3225

// Timeout after this many _minutes_ when waiting for a 3D GNSS fix
// (UL = unsigned long)
#define GNSS_timeout 5UL

// Timeout after this many _minutes_ when waiting for the super capacitors to charge
// 1 min should be OK for 1F capacitors at 150mA.
// Charging 10F capacitors at 60mA can take a long time! Could be as much as 10 mins.
#define CHG_timeout 2UL

// Top up the super capacitors for this many _seconds_.
// 10 seconds should be OK for 1F capacitors at 150mA.
// Increase the value for 10F capacitors.
#define TOPUP_timeout 10UL

// Loop Steps - these are used by the switch/case in the main loop
// This structure makes it easy to go from any of the steps directly to zzz when (e.g.) the batteries are low
#define loop_init     0 // Send the welcome message, check the battery voltage
#define start_GPS     1 // Enable the ZOE-M8Q, check the battery voltage
#define read_GPS      2 // Wait for up to GNSS_timeout minutes for a valid 3D fix, check the battery voltage
#define read_pressure 3 // Read the pressure and temperature from the MS8607
#define start_LTC3225 4 // Enable the LTC3225 super capacitor charger and wait for up to CHG_timeout minutes for PGOOD to go high
#define wait_LTC3225  5 // Wait TOPUP_timeout seconds to make sure the capacitors are fully charged
#define start_9603    6 // Power on the 9603N, send the message, check the battery voltage
#define zzz           7 // Turn everything off and put the processor into deep sleep
#define wake          8 // Wake from deep sleep, restore the processor clock speed
int loop_step = loop_init; // Make sure loop_step is set to loop_init

// Set up the RTC and generate interrupts every second
void setupRTC()
{
  // Enable the XT for the RTC.
  am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_XTAL_START, 0);
  
  // Select XT for RTC clock source
  am_hal_rtc_osc_select(AM_HAL_RTC_OSC_XT);
  
  // Enable the RTC.
  am_hal_rtc_osc_enable();
  
  // Set the alarm interval to 1 second
  am_hal_rtc_alarm_interval_set(AM_HAL_RTC_ALM_RPT_SEC);
  
  // Clear the RTC alarm interrupt.
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);
  
  // Enable the RTC alarm interrupt.
  am_hal_rtc_int_enable(AM_HAL_RTC_INT_ALM);
  
  // Enable RTC interrupts to the NVIC.
  NVIC_EnableIRQ(RTC_IRQn);

  // Enable interrupts to the core.
  am_hal_interrupt_master_enable();
}

// RTC alarm Interrupt Service Routine
// Clear the interrupt flag and increment seconds_count
// If TXINT has been reached, set the interval_alarm flag and reset seconds_count
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt.
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Increment seconds_count
  seconds_count = seconds_count + 1;

  // Check if interval_alarm should be set
  if (seconds_count >= (myTrackerSettings.TXINT * 60))
  {
    interval_alarm = true;
    seconds_count = 0;
  }
}

// Get the battery (bus) voltage
// Enable the bus voltage monitor
// Read the bus voltage and store it in vbat
// Disable the bus voltage monitor to save power
// Converts the analogread into Volts, compensating for
// the voltage divider (/3) and the Apollo voltage reference (2.0V)
void get_vbat()
{
  digitalWrite(busVoltageMonEN, HIGH); // Enable the bus voltage monitor
  analogReadResolution(14); //Set resolution to 14 bit
  delay(10); // Let the voltage settle
  vbat = ((float)analogRead(busVoltagePin)) * 3.0 * 2.0 / 16384.0;
  digitalWrite(busVoltageMonEN, LOW); // Disable the bus voltage monitor
}

// IridiumSBD Callback - this code is called while the 9603N is trying to transmit
bool ISBDCallback()
{
#ifndef noLED
  // Flash the LED at 4 Hz
  if ((millis() / 250) % 2 == 1) {
    digitalWrite(LED, HIGH);
  }
  else {
    digitalWrite(LED, LOW);
  }
#endif

  // Check the battery voltage now we are drawing current for the 9603
  // If voltage is low, stop Iridium send
  get_vbat(); // Read the battery (bus) voltage
  if (vbat < VBAT_LOW) {
    Serial.print(F("***!!! LOW VOLTAGE (ISBDCallback) "));
    Serial.print(vbat,2);
    Serial.println(F("V !!!***"));
    return false; // Returning false causes IridiumSBD to terminate
  }
  else {     
    return true;
  }
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
  // Let's begin by setting up the I/O pins
   
  pinMode(LED, OUTPUT); // Make the LED pin an output

  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power (HIGH = enable; LOW = disable)
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger (HIGH = enable; LOW = disable)
  pinMode(iridiumSleep, OUTPUT); // Iridium 9603N On/Off (Sleep) pin
  digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
  pinMode(iridiumRI, INPUT); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT); // Configure the Iridium Network Available as an input
  pinMode(superCapPGOOD, INPUT); // Configure the super capacitor charger PGOOD input

  pinMode(busVoltageMonEN, OUTPUT); // Make the Bus Voltage Monitor Enable an output
  digitalWrite(busVoltageMonEN, LOW); // Set it low to disable the measurement to save power
  analogReadResolution(14); //Set resolution to 14 bit

  // Initialise the globals
  iterationCounter = 0; // Make sure iterationCounter is set to zero (indicating a reset)
  loop_step = loop_init; // Make sure loop_step is set to loop_init
  seconds_count = 0; // Make sure seconds_count is reset
  interval_alarm = false; // Make sure the interval alarm flag is clear

  // Initialise the tracker settings in RAM - before we enable the RTC
  initTrackerSettings(&myTrackerSettings);
  
  //putTrackerSettings(&myTrackerSettings); // Uncomment this line to reset the EEPROM with the default settings

  // Check if the EEPROM data is valid (i.e. has already been initialised)
  if (checkEEPROM(&myTrackerSettings))
  {
    getTrackerSettings(&myTrackerSettings);
  }
  else
  {
    putTrackerSettings(&myTrackerSettings);
  }

  // Set up the rtc for 1 second interrupts now that TXINT has been initialized
  setupRTC();
}

void loop()
{
  // loop is one large switch/case that controls the sequencing of the code
  switch (loop_step) {

    // ************************************************************************************************
    // Initialise things
    case loop_init:
    
      // Start the console serial port and send the welcome message
      Serial.begin(115200);
      delay(1000); // Wait for the user to open the serial monitor (extend this delay if you need more time)
      Serial.println();
      Serial.println();
      Serial.println(F("Artemis Global Tracker"));
      Serial.println(F("Example: A Better Tracker"));
      Serial.println();

      // Print the TXINT, DEST and SOURCE
      Serial.print(F("Using a transmit interval of "));
      Serial.print(myTrackerSettings.TXINT);
      Serial.println(F(" minutes"));
      if (myTrackerSettings.DEST > 0)
      {
        Serial.print(F("Using a DEST of RB"));
        Serial.print(myTrackerSettings.DEST);
        Serial.print(F(" and a SOURCE of RB"));
        Serial.println(myTrackerSettings.SOURCE);
      }

      // Setup the IridiumSBD
      modem.setPowerProfile(IridiumSBD::USB_POWER_PROFILE); // Change power profile to "low current"

      // Check battery voltage
      // If voltage is low, go to sleep
      get_vbat(); // Get the battery (bus) voltage
      if (vbat < VBAT_LOW) {
        Serial.print(F("***!!! LOW VOLTAGE (init) "));
        Serial.print(vbat,2);
        Serial.println(F(" !!!***"));
        loop_step = zzz; // Go to sleep
      }
      else {
        loop_step = start_GPS; // Move on, start the GNSS
      }
      
      break; // End of case loop_init

    // ************************************************************************************************
    // Power up the GNSS (ZOE-M8Q)
    case start_GPS:

      Serial.println(F("Powering up the GNSS..."));
      Wire1.begin(); // Set up the I2C pins
      gnssON(); // Enable power for the GNSS

      delay(2000); // Give it time to power up
    
      // Check battery voltage now we are drawing current for the GPS
      get_vbat(); // Get the battery (bus) voltage
      if (vbat < VBAT_LOW) {
        // If voltage is low, turn off the GNSS and go to sleep
        Serial.print(F("***!!! LOW VOLTAGE (start_GPS) "));
        Serial.print(vbat,2);
        Serial.println(F("V !!!***"));
        gnssOFF(); // Disable power for the GNSS
        loop_step = zzz; // Go to sleep
      }
      
      else { // If the battery voltage is OK
      
        if (myGPS.begin(Wire1) == false) //Connect to the Ublox module using Wire port
        {
          // If we were unable to connect to the ZOE-M8Q:
          
          // Send a warning message
          Serial.println(F("***!!! Ublox GPS not detected at default I2C address !!!***"));
          
          // Set the lat, long etc. to default values
          latitude = 0.0; // Latitude in degrees
          longitude = 0.0; // Longitude in degrees
          altitude = 0; // Altitude above Median Seal Level in m
          speed = 0.0; // Ground speed in m/s
          satellites = 0; // Number of satellites (SVs) used in the solution
          course = 0; // Course (heading) in degrees
          pdop = 0;  // Positional Dilution of Precision in m
          year = 1970; // GNSS Year
          month = 1; // GNSS month
          day = 1; // GNSS day
          hour = 0; // GNSS hours
          minute = 0; // GNSS minutes
          second = 0; // GNSS seconds
          milliseconds = 0; // GNSS milliseconds
  
          // Power down the GNSS
          gnssOFF(); // Disable power for the GNSS
  
          loop_step = read_pressure; // Move on, skip reading the GNSS fix
        }

        else { // If the GNSS started up OK
          
          //myGPS.enableDebugging(); // Enable debug messages
          myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

          // If we are going to change the dynamic platform model, let's do it here.
          // Possible values are:
          // PORTABLE,STATIONARY,PEDESTRIAN,AUTOMOTIVE,SEA,AIRBORNE1g,AIRBORNE2g,AIRBORNE4g,WRIST,BIKE
          if (myGPS.setDynamicModel(DYN_MODEL_PORTABLE) == false)
          {
            Serial.println(F("***!!! Warning: setDynamicModel may have failed !!!***"));
          }
          else
          {
            Serial.println(F("Dynamic Model updated"));
          }
        
          loop_step = read_GPS; // Move on, read the GNSS fix
        }
        
      }

      break; // End of case start_GPS

    // ************************************************************************************************
    // Read a fix from the ZOE-M8Q
    case read_GPS:

      Serial.println(F("Waiting for a 3D GNSS fix..."));

      fixType = 0; // Clear the fix type
      
      // Look for GPS signal for up to GNSS_timeout minutes
      for (unsigned long tnow = millis(); (fixType != 3) && (millis() - tnow < GNSS_timeout * 60UL * 1000UL);)
      {
      
        fixType = myGPS.getFixType(); // Get the GNSS fix type
        
        // Check battery voltage now we are drawing current for the GPS
        // If voltage is low, stop looking for GNSS and go to sleep
        get_vbat();
        if (vbat < VBAT_LOW) {
          break; // Exit the for loop now
        }

#ifndef noLED
        // Flash the LED at 1Hz
        if ((millis() / 1000) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }
#endif

        delay(100); // Don't pound the I2C bus too hard!

      }

      // If voltage is low then go straight to sleep
      if (vbat < VBAT_LOW) {
        Serial.print(F("***!!! LOW VOLTAGE (read_GPS) "));
        Serial.print(vbat,2);
        Serial.println(F("V !!!***"));
        
        loop_step = zzz;
      }

      else if (fixType == 3) // Check if we got a valid 3D fix
      {
        // Get the time and position etc.
        // Get the time first to hopefully avoid second roll-over problems
        milliseconds = myGPS.getMillisecond();
        second = myGPS.getSecond();
        minute = myGPS.getMinute();
        hour = myGPS.getHour();
        day = myGPS.getDay();
        month = myGPS.getMonth();
        year = myGPS.getYear(); // Get the year
        latitude = (float)myGPS.getLatitude() / 10000000.0; // Get the latitude in degrees
        longitude = (float)myGPS.getLongitude() / 10000000.0; // Get the longitude in degrees
        altitude = myGPS.getAltitudeMSL() / 1000; // Get the altitude above Mean Sea Level in m
        speed = (float)myGPS.getGroundSpeed() / 1000.0; // Get the ground speed in m/s
        satellites = myGPS.getSIV(); // Get the number of satellites used in the fix
        course = myGPS.getHeading() / 10000000; // Get the heading in degrees
        pdop = myGPS.getPDOP() / 100; // Get the PDOP in m

        Serial.println(F("A 3D fix was found!"));
        Serial.print(F("Latitude (degrees): ")); Serial.println(latitude, 6);
        Serial.print(F("Longitude (degrees): ")); Serial.println(longitude, 6);
        Serial.print(F("Altitude (m): ")); Serial.println(altitude);

        loop_step = read_pressure; // Move on, read the pressure and temperature
      }
      
      else
      {
        // We didn't get a 3D fix so
        // set the lat, long etc. to default values
        latitude = 0.0; // Latitude in degrees
        longitude = 0.0; // Longitude in degrees
        altitude = 0; // Altitude above Median Seal Level in m
        speed = 0.0; // Ground speed in m/s
        satellites = 0; // Number of satellites (SVs) used in the solution
        course = 0; // Course (heading) in degrees
        pdop = 0;  // Positional Dilution of Precision in m
        year = 1970; // GNSS Year
        month = 1; // GNSS month
        day = 1; // GNSS day
        hour = 0; // GNSS hours
        minute = 0; // GNSS minutes
        second = 0; // GNSS seconds
        milliseconds = 0; // GNSS milliseconds

        Serial.println(F("A 3D fix was NOT found!"));
        Serial.println(F("Using default values..."));

        loop_step = read_pressure; // Move on, read the pressure and temperature
      }

      // Power down the GNSS
      Serial.println(F("Powering down the GNSS..."));
      gnssOFF(); // Disable power for the GNSS

      break; // End of case read_GPS

    // ************************************************************************************************
    // Read the pressure and temperature from the MS8607
    case read_pressure:

      Serial.println(F("Getting the pressure and temperature readings..."));

      bool barometricSensorOK;

      barometricSensorOK = barometricSensor.begin(Wire1); // Begin the PHT sensor
      if (barometricSensorOK == false)
      {
        // Send a warning message if we were unable to connect to the MS8607:
        Serial.println(F("*! Could not detect the MS8607 sensor. Trying again... !*"));
        barometricSensorOK = barometricSensor.begin(Wire1); // Re-begin the PHT sensor
        if (barometricSensorOK == false)
        {
          // Send a warning message if we were unable to connect to the MS8607:
          Serial.println(F("***!!! MS8607 sensor not detected at default I2C address !!!***"));
        }
      }

      if (barometricSensorOK == false)
      {
         // Set the pressure and temperature to default values
        pascals = 0.0;
        tempC = 0.0;
      }
      else
      {
        tempC = barometricSensor.getTemperature();
        pascals = barometricSensor.getPressure() * 100.0; // Convert pressure from hPa to Pascals

        Serial.print(F("Temperature="));
        Serial.print(tempC, 1);
        Serial.print(F("(C)"));
      
        Serial.print(F(" Pressure="));
        Serial.print(pascals, 1);
        Serial.println(F("(Pa)"));
      }

      loop_step = start_LTC3225; // Move on, start the super capacitor charger

      break; // End of case read_pressure

    // ************************************************************************************************
    // Start the LTC3225 supercapacitor charger
    case start_LTC3225:

      // Enable the supercapacitor charger
      Serial.println(F("Enabling the supercapacitor charger..."));
      digitalWrite(superCapChgEN, HIGH); // Enable the super capacitor charger

      Serial.println(F("Waiting for supercapacitors to charge..."));
      delay(2000);

      PGOOD = false; // Flag to show if PGOOD is HIGH
      
      // Wait for PGOOD to go HIGH for up to CHG_timeout minutes
      for (unsigned long tnow = millis(); (!PGOOD) && (millis() - tnow < CHG_timeout * 60UL * 1000UL);)
      {
      
        PGOOD = digitalRead(superCapPGOOD); // Read the PGOOD pin
        
        // Check battery voltage now we are drawing current for the LTC3225
        // If voltage is low, stop charging and go to sleep
        get_vbat();
        if (vbat < VBAT_LOW) {
          break;
        }

#ifndef noLED
        // Flash the LED at 2Hz
        if ((millis() / 500) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }
#endif

        delay(100); // Let's not pound the bus voltage monitor too hard!

      }

      // If voltage is low then go straight to sleep
      if (vbat < VBAT_LOW) {
        Serial.print(F("***!!! LOW VOLTAGE (start_LTC3225) "));
        Serial.print(vbat,2);
        Serial.println(F("V !!!***"));
        
        loop_step = zzz;
      }

      else if (PGOOD)
      {
        // If the capacitors charged OK
        Serial.println(F("Supercapacitors charged!"));
        
        loop_step = wait_LTC3225; // Move on and give the capacitors extra charging time
      }

      else
      {
        // The super capacitors did not charge so power down and go to sleep
        Serial.println(F("***!!! Supercapacitors failed to charge !!!***"));

        loop_step = zzz;
      }
  
      break; // End of case start_LTC3225

    // ************************************************************************************************
    // Give the super capacitors some extra time to charge
    case wait_LTC3225:
    
      Serial.println(F("Giving the supercapacitors extra time to charge..."));
 
      // Wait for TOPUP_timeout seconds, keep checking PGOOD and the battery voltage
      for (unsigned long tnow = millis(); millis() - tnow < TOPUP_timeout * 1000UL;)
      {
      
        // Check battery voltage now we are drawing current for the LTC3225
        // If voltage is low, stop charging and go to sleep
        get_vbat();
        if (vbat < VBAT_LOW) {
          break;
        }

#ifndef noLED
        // Flash the LED at 2Hz
        if ((millis() / 500) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }
#endif

        delay(100); // Let's not pound the bus voltage monitor too hard!

      }

      // If voltage is low then go straight to sleep
      if (vbat < VBAT_LOW) {
        Serial.print(F("***!!! LOW VOLTAGE (wait_LTC3225) "));
        Serial.print(vbat,2);
        Serial.println(F("V !!!***"));
        
        loop_step = zzz;
      }

      else if (PGOOD)
      {
        // If the capacitors are still charged OK
        Serial.println(F("Supercapacitors charged!"));
        
        loop_step = start_9603; // Move on and start the 9603N
      }

      else
      {
        // The super capacitors did not charge so power down and go to sleep
        Serial.println(F("***!!! Supercapacitors failed to hold charge in wait_LTC3225 !!!***"));

        loop_step = zzz;
      }
  
      break; // End of case wait_LTC3225
      
    // ************************************************************************************************
    // Enable the 9603N and attempt to send a message
    case start_9603:
    
      // Enable power for the 9603N
      Serial.println(F("Enabling 9603N power..."));
      digitalWrite(iridiumPwrEN, HIGH); // Enable Iridium Power
      delay(1000);

      // Enable the 9603N and start talking to it
      Serial.println(F("Beginning to talk to the 9603..."));

      // Start the serial port connected to the satellite modem
      iridiumSerial.begin(19200);
      delay(1000);

      // Relax timing constraints waiting for the supercap to recharge.
      modem.setPowerProfile(IridiumSBD::USB_POWER_PROFILE);

      // Begin satellite modem operation
      Serial.println(F("Starting modem..."));
      err = modem.begin();

      // Check if the modem started correctly
      if (err != ISBD_SUCCESS)
      {
        // If the modem failed to start, disable it and go to sleep
        Serial.print(F("***!!! modem.begin failed with error "));
        Serial.print(err);
        Serial.println(F(" !!!***"));
        loop_step = zzz;
      }

      else
      {
        // The modem started OK so let's try to send the message
        char outBuffer[120]; // Use outBuffer to store the message. Always try to keep message short to avoid wasting credits

        // Convert the floating point values into strings using dtostrf
        char lat_str[15]; // latitude string
        dtostrf(latitude,8,6,lat_str);
        char lon_str[15]; // longitude string
        dtostrf(longitude,8,6,lon_str);
        char alt_str[15]; // altitude string
        dtostrf(altitude,4,2,alt_str);
        char vbat_str[6]; // battery voltage string
        dtostrf(vbat,4,2,vbat_str);
        char speed_str[8]; // speed string
        dtostrf(speed,4,2,speed_str);
        char pressure_str[8]; // pressure string
        dtostrf(pascals,1,0,pressure_str);
        char temperature_str[10]; // temperature string
        dtostrf(tempC,3,1,temperature_str);

        // Assemble the message using sprintf
        if (myTrackerSettings.DEST > 0) {
          sprintf(outBuffer, "RB%07d,%d%02d%02d%02d%02d%02d,%s,%s,%s,%s,%d,%d,%d,%s,%s,%s,%d,RB%07d", myTrackerSettings.DEST, year, month, day, hour, minute, second, 
            lat_str, lon_str, alt_str, speed_str, course, pdop, satellites, pressure_str, temperature_str, vbat_str, iterationCounter, myTrackerSettings.SOURCE);
        }
        else {
          sprintf(outBuffer, "%d%02d%02d%02d%02d%02d,%s,%s,%s,%s,%d,%d,%d,%s,%s,%s,%d", year, month, day, hour, minute, second, 
            lat_str, lon_str, alt_str, speed_str, course, pdop, satellites, pressure_str, temperature_str, vbat_str, iterationCounter);
        }

        // Send the message
        Serial.print(F("Transmitting message '"));
        Serial.print(outBuffer);
        Serial.println(F("'"));

        uint8_t mt_buffer[100]; // Buffer to store Mobile Terminated SBD message
        size_t mtBufferSize = sizeof(mt_buffer); // Size of MT buffer

#ifndef noTX
        err = modem.sendReceiveSBDText(outBuffer, mt_buffer, mtBufferSize); // This could take many seconds to complete and will call ISBDCallback() periodically
#else
        err = ISBD_SUCCESS; // Fake success
        mtBufferSize = 0;
#endif

        // Check if the message sent OK
        if (err != ISBD_SUCCESS)
        {
          Serial.print(F("Transmission failed with error code "));
          Serial.println(err);
#ifndef noLED
          // Turn on LED to indicate failed send
          digitalWrite(LED, HIGH);
#endif
        }
        else
        {
          Serial.println(F(">>> Message sent! <<<"));
#ifndef noLED
          // Flash LED rapidly to indicate successful send
          for (int i = 0; i < 10; i++)
          {
            digitalWrite(LED, HIGH);
            delay(100);
            digitalWrite(LED, LOW);
            delay(100);
          }
#endif
          if (mtBufferSize > 0) { // Was an MT message received?
            // Check message content
            mt_buffer[mtBufferSize] = 0; // Make sure message is NULL terminated
            String mt_str = String((char *)mt_buffer); // Convert message into a String
            Serial.print(F("Received a MT message: ")); Serial.println(mt_str);

            // Check if the message contains a correctly formatted interval: "[INTERVAL=nnn]"
            int new_interval = 0;
            int starts_at = -1;
            int ends_at = -1;
            starts_at = mt_str.indexOf("[INTERVAL="); // See is message contains "[INTERVAL="
            if (starts_at >= 0) { // If it does:
              ends_at = mt_str.indexOf("]", starts_at); // Find the following "]"
              if (ends_at > starts_at) { // If the message contains both "[INTERVAL=" and "]"
                String new_interval_str = mt_str.substring((starts_at + 10),ends_at); // Extract the value after the "="
                Serial.print(F("Extracted an INTERVAL of: ")); Serial.println(new_interval_str);
                new_interval = (int)new_interval_str.toInt(); // Convert it to int
              }
            }
            if ((new_interval > 0) and (new_interval <= 1440)) { // Check new interval is valid
              Serial.print(F("New INTERVAL received. Setting TXINT to "));
              Serial.print(new_interval);
              Serial.println(F(" minutes."));
              myTrackerSettings.TXINT = new_interval; // Update the transmit interval
              putTrackerSettings(&myTrackerSettings); // Update flash memory
            }

            // Check if the message contains a correctly formatted RBSOURCE: "[RBSOURCE=nnnnn]"
            int new_source = -1;
            starts_at = -1;
            ends_at = -1;
            starts_at = mt_str.indexOf("[RBSOURCE="); // See is message contains "[RBSOURCE="
            if (starts_at >= 0) { // If it does:
              ends_at = mt_str.indexOf("]", starts_at); // Find the following "]"
              if (ends_at > starts_at) { // If the message contains both "[RBSOURCE=" and "]"
                String new_source_str = mt_str.substring((starts_at + 10),ends_at); // Extract the value after the "="
                Serial.print(F("Extracted an RBSOURCE of: ")); Serial.println(new_source_str);
                new_source = (int)new_source_str.toInt(); // Convert it to int
              }
            }
            // toInt returns zero if the conversion fails, so it is not possible to distinguish between a source of zero and an invalid value!
            // An invalid value will cause RBSOURCE to be set to zero
            if (new_source >= 0) { // If new_source was received
              Serial.print(F("New RBSOURCE received. Setting SOURCE to "));
              Serial.println(new_source);
              myTrackerSettings.SOURCE = new_source; // Update the source RockBLOCK serial number
              putTrackerSettings(&myTrackerSettings); // Update flash memory
            }

            // Check if the message contains a correctly formatted RBDESTINATION: "[RBDESTINATION=nnnnn]"
            int new_destination = -1;
            starts_at = -1;
            ends_at = -1;
            starts_at = mt_str.indexOf("[RBDESTINATION="); // See is message contains "[RBDESTINATION="
            if (starts_at >= 0) { // If it does:
              ends_at = mt_str.indexOf("]", starts_at); // Find the following "]"
              if (ends_at > starts_at) { // If the message contains both "[RBDESTINATION=" and "]"
                String new_destination_str = mt_str.substring((starts_at + 15),ends_at); // Extract the value after the "="
                Serial.print(F("Extracted an RBDESTINATION of: ")); Serial.println(new_destination_str);
                new_destination = (int)new_destination_str.toInt(); // Convert it to int
              }
            }
            // toInt returns zero if the conversion fails, so it is not possible to distinguish between a destination of zero and an invalid value!
            // An invalid value will cause RBDESTINATION to be set to zero
            if (new_destination >= 0) { // If new_destination was received
              Serial.print(F("New RBDESTINATION received. Setting DEST to "));
              Serial.println(new_destination);
              myTrackerSettings.DEST = new_destination; // Update the destination RockBLOCK serial number
              putTrackerSettings(&myTrackerSettings); // Update flash memory
            }
          }
        }

        // Clear the Mobile Originated message buffer
        Serial.println(F("Clearing the MO buffer."));
        err = modem.clearBuffers(ISBD_CLEAR_MO); // Clear MO buffer
        if (err != ISBD_SUCCESS)
        {
          Serial.print(F("***!!! modem.clearBuffers failed with error "));
          Serial.print(err);
          Serial.println(F(" !!!***"));
        }

        // Power down the modem
        Serial.println(F("Putting the 9603N to sleep."));
        err = modem.sleep();
        if (err != ISBD_SUCCESS)
        {
          Serial.print(F("***!!! modem.sleep failed with error "));
          Serial.print(err);
          Serial.println(F(" !!!***"));
        }

        iterationCounter = iterationCounter + 1; // Increment the iterationCounter
  
        loop_step = zzz; // Now go to sleep
      }

      break; // End of case start_9603
      
    // ************************************************************************************************
    // Go to sleep
    case zzz:
    
      Serial.println(F("Getting ready to put the Apollo3 into deep sleep..."));

      // Power down the GNSS
      Serial.println(F("Powering down the GNSS..."));
      gnssOFF(); // Disable power for the GNSS

      // Disable 9603N power
      Serial.println(F("Disabling 9603N power..."));
      digitalWrite(iridiumSleep, LOW); // Disable 9603N via its ON/OFF pin (modem.sleep should have done this already)
      delay(1000);
      digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
      delay(1000);
    
      // Disable the supercapacitor charger
      Serial.println(F("Disabling the supercapacitor charger..."));
      digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

      // Close the Iridium serial port
      iridiumSerial.end();

      // Close the I2C port
      Wire1.end();

      digitalWrite(busVoltageMonEN, LOW); // Disable the bus voltage monitor

      digitalWrite(LED, LOW); // Disable the LED

      // Close and detach the serial console
      Serial.println(F("Going into deep sleep until next TXINT..."));
      delay(1000); // Wait for serial port to clear
      Serial.end(); // Close the serial console

      // Code taken (mostly) from the LowPower_WithWake example and the and OpenLog_Artemis PowerDownRTC example
      
      // Turn off ADC
      power_adc_disable();
        
      // Set the clock frequency.
      am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);
  
      // Set the default cache configuration
      am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
      am_hal_cachectrl_enable();
  
      // Note: because we called setupRTC earlier,
      // we do NOT want to call am_bsp_low_power_init() here.
      // It would configure the board for low power operation
      // and calls am_hal_pwrctrl_low_power_init()
      // but it also stops the RTC oscillator!
      // (BSP = Board Support Package)

      // Initialize for low power in the power control block
      // "Initialize BLE Buck Trims for Lowest Power"
      am_hal_pwrctrl_low_power_init();
  
      // Disabling the debugger GPIOs saves about 1.2 uA total:
      am_hal_gpio_pinconfig(20 /* SWDCLK */, g_AM_HAL_GPIO_DISABLE);
      am_hal_gpio_pinconfig(21 /* SWDIO */, g_AM_HAL_GPIO_DISABLE);
  
      // These two GPIOs are critical: the TX/RX connections between the Artemis module and the CH340S
      // are prone to backfeeding each other. To stop this from happening, we must reconfigure those pins as GPIOs
      // and then disable them completely:
      am_hal_gpio_pinconfig(48 /* TXO-0 */, g_AM_HAL_GPIO_DISABLE);
      am_hal_gpio_pinconfig(49 /* RXI-0 */, g_AM_HAL_GPIO_DISABLE);
  
      // The default Arduino environment runs the System Timer (STIMER) off the 48 MHZ HFRC clock source.
      // The HFRC appears to take over 60 uA when it is running, so this is a big source of extra
      // current consumption in deep sleep.
      // For systems that might want to use the STIMER to generate a periodic wakeup, it needs to be left running.
      // However, it does not have to run at 48 MHz. If we reconfigure STIMER (system timer) to use the 32768 Hz
      // XTAL clock source instead the measured deepsleep power drops by about 64 uA.
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
  
      // This option selects 32768 Hz via crystal osc. This appears to cost about 0.1 uA versus selecting "no clock"
      am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
  
      // Turn OFF Flash1
      // There is a chance this could fail but I guess we should move on regardless and not do a while(1);
      am_hal_pwrctrl_memory_enable(AM_HAL_PWRCTRL_MEM_FLASH_512K);
  
      // Power down SRAM
      // Nathan seems to have gone a little off script here and isn't using
      // am_hal_pwrctrl_memory_deepsleep_powerdown or 
      // am_hal_pwrctrl_memory_deepsleep_retain. I wonder why?
      PWRCTRL->MEMPWDINSLEEP_b.SRAMPWDSLP = PWRCTRL_MEMPWDINSLEEP_SRAMPWDSLP_ALLBUTLOWER64K;


      // This while loop keeps the processor asleep until TXINT minutes have passed
      while (!interval_alarm) // Wake up every TXINT minutes
      {
        // Go to Deep Sleep.
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
      }
      interval_alarm = false; // Clear the alarm flag now


      // Wake up!
      loop_step = wake;

      break; // End of case zzz
      
    // ************************************************************************************************
    // Wake from sleep
    case wake:

      // Set the clock frequency. (redundant?)
      am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);
  
      // Set the default cache configuration. (redundant?)
      am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
      am_hal_cachectrl_enable();
  
      // Note: because we called setupRTC earlier,
      // we do NOT want to call am_bsp_low_power_init() here.
      // It would configure the board for low power operation
      // and calls am_hal_pwrctrl_low_power_init()
      // but it also stops the RTC oscillator!
      // (BSP = Board Support Package)

      // Initialize for low power in the power control block. (redundant?)
      // "Initialize BLE Buck Trims for Lowest Power"
      am_hal_pwrctrl_low_power_init();
  
      // Power up SRAM
      PWRCTRL->MEMPWDINSLEEP_b.SRAMPWDSLP = PWRCTRL_MEMPWDINSLEEP_SRAMPWDSLP_NONE;
      
      // Turn on Flash
      // There is a chance this could fail but I guess we should move on regardless and not do a while(1);
      am_hal_pwrctrl_memory_enable(AM_HAL_PWRCTRL_MEM_ALL);
      
      // Go back to using the main clock
      am_hal_stimer_int_enable(AM_HAL_STIMER_INT_OVERFLOW); // (posssibly redundant?)
      NVIC_EnableIRQ(STIMER_IRQn); // (posssibly redundant?)
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
      am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ);

      // Restore the TX/RX connections between the Artemis module and the CH340S on the Blackboard
      am_hal_gpio_pinconfig(48 /* TXO-0 */, g_AM_BSP_GPIO_COM_UART_TX);
      am_hal_gpio_pinconfig(49 /* RXI-0 */, g_AM_BSP_GPIO_COM_UART_RX);

      // Reenable the debugger GPIOs
      am_hal_gpio_pinconfig(20 /* SWDCLK */, g_AM_BSP_GPIO_SWDCK);
      am_hal_gpio_pinconfig(21 /* SWDIO */, g_AM_BSP_GPIO_SWDIO);
  
      // Turn on ADC
      ap3_adc_setup();
      
      // Do it all again!
      loop_step = loop_init;

      break; // End of case wake

  } // End of switch (loop_step)
} // End of loop()

// IridiumSBD Callbacks

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
