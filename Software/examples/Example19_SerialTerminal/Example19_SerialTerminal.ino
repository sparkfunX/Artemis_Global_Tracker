/*
  Artemis Global Tracker
  Example: Serial Terminal
  
  Written by Paul Clark (PaulZC)
  September 7th 2021

  ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
  ** (At the time of writing, v2.1.1 of the core conatins a feature which makes communication with the u-blox GNSS problematic. Be sure to use v2.1.0) **

  ** Set the Board to "RedBoard Artemis ATP" **
  ** (The Artemis Module does not have a Wire port defined, which prevents the GNSS library from compiling) **

  This example provides access to all of the features of the Artemis Iridium Tracker
  via simple serial menus. You can connect via a serial monitor or terminal emulator,
  or write your own applications to use the serial interface.
  
  You will need to install version 3.0.5 of the Iridium SBD I2C library
  before this example will run successfully:
  https://github.com/sparkfun/SparkFun_IridiumSBD_I2C_Arduino_Library
  (Available through the Arduino Library Manager: search for IridiumSBDi2c)
  
  You will also need to install the Qwiic_PHT_MS8607_Library:
  https://github.com/sparkfun/SparkFun_PHT_MS8607_Arduino_Library
  (Available through the Arduino Library Manager: search for SparkFun MS8607)
  
  You will need to install the SparkFun u-blox library before this example will run successfully:
  https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library
  (Available through the Arduino Library Manager: search for SparkFun u-blox GNSS)
  
  SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Version history:
  August 25th 2021
    Added a fix for https://github.com/sparkfun/Arduino_Apollo3/issues/423
  August 14th 2021
    First commit using v2.1 of the Apollo3 core

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

// We use Serial1 to communicate with the Iridium modem. Serial1 on the ATP uses pin 24 for TX and 25 for RX. AGT uses the same pins.

#include <IridiumSBD.h> //http://librarymanager/All#IridiumSBDI2C
#define DIAGNOSTICS false
// Declare the IridiumSBD object (including the sleep (ON/OFF) and Ring Indicator pins)
IridiumSBD modem(Serial1, iridiumSleep, iridiumRI);

#include <Wire.h> // Needed for I2C
const byte PIN_AGTWIRE_SCL = 8;
const byte PIN_AGTWIRE_SDA = 9;
TwoWire agtWire(PIN_AGTWIRE_SDA, PIN_AGTWIRE_SCL); //Create an I2C port using pads 8 (SCL) and 9 (SDA)

#include "SparkFun_u-blox_GNSS_Arduino_Library.h" //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

#include <SparkFun_PHT_MS8607_Arduino_Library.h> //http://librarymanager/All#SparkFun_MS8607
MS8607 barometricSensor; //Create an instance of the MS8607 object

#include <EEPROM.h> // Needed for EEPROM storage on the Artemis

// More global variables
float agtLatitude = 0.0; // Latitude in degrees
float agtLongitude = 0.0; // Longitude in degrees
long  agtAltitude = 0; // Altitude above Median Seal Level in m
float agtSpeed = 0.0; // Ground speed in m/s
long  agtCourse = 0; // Course (heading) in degrees
int   agtYear = 1970; // GNSS Year
byte  agtMonth = 1; // GNSS month
byte  agtDay = 1; // GNSS day
byte  agtHour = 0; // GNSS hours
byte  agtMinute = 0; // GNSS minutes
byte  agtSecond = 0; // GNSS seconds
float agtPascals = 0.0; // Atmospheric pressure in Pascals
float agtTempC = 0.0; // Temperature in Celcius
byte  agtFixType = 0; // GNSS fix type: 0=No fix, 1=Dead reckoning, 2=2D, 3=3D, 4=GNSS+Dead reckoning
bool  agtPGOOD = false; // Flag to indicate if LTC3225 PGOOD is HIGH
int   agtErr; // Error value returned by IridiumSBD.begin
bool  ms8607Begun = false; // Flag to indicate if the MS8607 has been begun
bool  gnssBegun = false; // Flag to indicate if the ZOE-M8Q has been begun
bool  iridiumBegun = false; // Flag to indicate if the Iridium modem has been begun
unsigned long superCapChargerStartedAt; // Record when the super capacitor charger was started
const uint16_t iridiumMTbufferLength = 270; // Maximum MT buffer length
uint8_t  iridiumMTbuffer[iridiumMTbufferLength + 1]; // Maximum MT buffer length plus trailing NULL
const uint16_t iridiumMObufferLength = 340; // Maximum MO buffer length
uint8_t  iridiumMObuffer[iridiumMObufferLength + 1]; // Maximum MO buffer length plus trailing NULL

// Timeout after this many _minutes_ when waiting for the super capacitors to charge
// 1 min should be OK for 1F capacitors at 150mA.
// Charging 10F capacitors at 60mA can take a long time! Could be as much as 10 mins.
#define CHG_timeout 2UL

// Top up the super capacitors for this many _seconds_.
// 10 seconds should be OK for 1F capacitors at 150mA.
// Increase the value for 10F capacitors.
#define TOPUP_timeout 10UL

// Loop Steps - these are used by the switch/case in the main loop
typedef enum {
  main_menu = 0,        // Display the main menu
  read_MS8607,          // Read the temperature and pressure
  read_GNSS,            // Read PVT data from the GNSS
  check_Iridium,        // Check for a new Iridium message
  send_Iridium_text,    // Send an Iridium text message (and check for a new message)
  send_Iridium_binary,  // Send an Iridium text message (and check for a new message)
  flush_Iridium_MT,     // Flush the Mobile Terminated message queue (RockBLOCK only)
  set_dynModel,         // Set the GNSS dynamic model
  power_down            // Power down and wait for reset
} loop_steps;
loop_steps loop_step = main_menu; // Make sure loop_step is set to main_menu

bool iridiumTransfer(const char *bufferPointer, bool binary = false, size_t binaryLength = 0); // Header

// IridiumSBD Callback - this code is called while the 9603N is trying to transmit
bool ISBDCallback()
{
  // Toggle the LED (0.5Hz)
  digitalWrite(LED, !digitalRead(LED));
  
  Serial.println(F("IRIDIUM SBD   : Iridium SBD transfer in progress. Enter 0 to terminate"));
  Serial.print(F("IRIDIUM SBD   > "));

  char menuString[] = "IRIDIUM SBD   > ";

  // Get the menu choice using a 1 second timeout and a 750ms inter-character delay
  if (getMenuChoice(menuString, 1, 750) == 0)
    return false; // Returning false causes IridiumSBD to terminate

  return true;
}

void gnssON(void) // Enable power for the GNSS
{
  am_hal_gpio_pincfg_t pinCfg = g_AM_HAL_GPIO_OUTPUT; // Begin by making the gnssEN pin an open-drain output
  pinCfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN;
  pin_config(PinName(gnssEN), pinCfg);
  delay(1);
  
  digitalWrite(gnssEN, LOW); // Enable GNSS power (HIGH = disable; LOW = enable)
}

void gnssOFF(void) // Disable power for the GNSS
{
  am_hal_gpio_pincfg_t pinCfg = g_AM_HAL_GPIO_OUTPUT; // Begin by making the gnssEN pin an open-drain output
  pinCfg.eGPOutcfg = AM_HAL_GPIO_PIN_OUTCFG_OPENDRAIN;
  pin_config(PinName(gnssEN), pinCfg);
  delay(1);
  
  digitalWrite(gnssEN, HIGH); // Disable GNSS power (HIGH = disable; LOW = enable)
}

// Overwrite the IridiumSBD beginSerialPort function - a fix for https://github.com/sparkfun/Arduino_Apollo3/issues/423
void IridiumSBD::beginSerialPort() // Start the serial port connected to the satellite modem
{
  diagprint(F("custom IridiumSBD::beginSerialPort\r\n"));
  
  // Configure the standard ATP pins for UART1 TX and RX - endSerialPort may have disabled the RX pin
  
  am_hal_gpio_pincfg_t pinConfigTx = g_AM_BSP_GPIO_COM_UART_TX;
  pinConfigTx.uFuncSel = AM_HAL_PIN_24_UART1TX;
  pin_config(D24, pinConfigTx);
  
  am_hal_gpio_pincfg_t pinConfigRx = g_AM_BSP_GPIO_COM_UART_RX;
  pinConfigRx.uFuncSel = AM_HAL_PIN_25_UART1RX;
  pinConfigRx.ePullup = AM_HAL_GPIO_PIN_PULLUP_WEAK; // Put a weak pull-up on the Rx pin
  pin_config(D25, pinConfigRx);
  
  Serial1.begin(19200);
}

// Overwrite the IridiumSBD endSerialPort function - a fix for https://github.com/sparkfun/Arduino_Apollo3/issues/423
void IridiumSBD::endSerialPort()
{
  diagprint(F("custom IridiumSBD::endSerialPort\r\n"));
  
  // Disable the Serial1 RX pin to avoid the code hang
  am_hal_gpio_pinconfig(PinName(D25), g_AM_HAL_GPIO_DISABLE);
}

void setup()
{
  // Let's begin by setting up the I/O pins
   
  pinMode(LED, OUTPUT); // Make the LED pin an output

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power (HIGH = enable; LOW = disable)
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, HIGH); // Enable the super capacitor charger (HIGH = enable; LOW = disable)
  superCapChargerStartedAt = millis(); // Record when the charger was started
  pinMode(iridiumSleep, OUTPUT); // Iridium 9603N On/Off (Sleep) pin
  digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
  pinMode(iridiumRI, INPUT); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT); // Configure the Iridium Network Available as an input
  pinMode(superCapPGOOD, INPUT); // Configure the super capacitor charger PGOOD input

  // Make sure the Serial1 RX pin is disabled to prevent the power-on glitch on the modem's RX(OUT) pin
  // causing problems with v2.1.0 of the Apollo3 core. Requires v3.0.5 of IridiumSBDi2c.
  modem.endSerialPort();

  gnssON(); // Enable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  pinMode(busVoltageMonEN, OUTPUT); // Make the Bus Voltage Monitor Enable an output
  digitalWrite(busVoltageMonEN, LOW); // Set it low to disable the measurement to save power
  analogReadResolution(14); //Set resolution to 14 bit

  agtWire.begin(); // Set up the I2C pins
  agtWire.setClock(100000); // Use 100kHz for best performance

  // Check the dynamic model is valid
  // Possible values are:
  // 0: portable
  // 2: stationary
  // 3: pedestrian
  // 4: automotive
  // 5: sea
  // 6: airborne with <1g acceleration
  // 7: airborne with <2g acceleration
  // 8: airborne with <4g acceleration
  // 9: wrist-worn watch (not supported in protocol versions less than 18)
  // 10: bike (only supported in protocol versions 19.2 - ZOE-M8Q is 18.00)
  EEPROM.init(); // Initialize the EEPROM
  byte dynamicModel = EEPROM.read(0);
  if ((dynamicModel == 1) || (dynamicModel > 9))
    EEPROM.write(0, 0); // Default to portable

  // Start the console serial port
  Serial.begin(115200);
  Serial.println();
  delay(1000);
}

#define menuStringWidth 16 // The width of "MAIN MENU     > "

void loop()
{
  // loop is one large switch/case that controls the sequencing of the code
  switch (loop_step) {

    // ************************************************************************************************
    // Initialise things
    case main_menu:
    {
      Serial.println(F("MAIN MENU     : "));
      Serial.println(F("MAIN MENU     : Artemis Global Tracker"));
      Serial.println(F("MAIN MENU     : Serial Terminal"));
      Serial.println(F("MAIN MENU     : "));
      Serial.println(F("MAIN MENU     : Set your line ending to CR or CR+LF"));
      Serial.println(F("MAIN MENU     : "));
      Serial.println(F("MAIN MENU     : 1 - Read temperature and pressure"));
      Serial.println(F("MAIN MENU     : 2 - Read GNSS data"));
      Serial.println(F("MAIN MENU     : 3 - Check for a new Iridium message (uses 1 message credit if no messages are waiting)"));
      Serial.println(F("MAIN MENU     : 4 - Send an Iridium text message (and check for a new message)"));
      Serial.println(F("MAIN MENU     : 5 - Send an Iridium binary message (and check for a new message)"));
      Serial.println(F("MAIN MENU     : 6 - Flush the Mobile Terminated message queue (RockBLOCK only)"));
      Serial.println(F("MAIN MENU     : 7 - Set the GNSS dynamic model"));
      Serial.println(F("MAIN MENU     : 8 - Power down"));
      Serial.println(F("MAIN MENU     : "));
      Serial.print(F("MAIN MENU     > "));

      char menuString[] = "MAIN MENU     > ";

      // Get the menu choice using a 60 second timeout and a 250ms inter-character delay
      int menuChoice = getMenuChoice(menuString, 60, 250);

      switch (menuChoice)
      {
        case -1: // getMenuChoice will return -1 if nothing was entered within the timeout
          break; // Do nothing. Just display the menu again
        case 1:
          loop_step = read_MS8607;
          break;
        case 2:
          loop_step = read_GNSS;
          break;
        case 3:
          loop_step = check_Iridium;
          break;
        case 4:
          loop_step = send_Iridium_text;
          break;
        case 5:
          loop_step = send_Iridium_binary;
          break;
        case 6:
          loop_step = flush_Iridium_MT;
          break;
        case 7:
          loop_step = set_dynModel;
          break;
        case 8:
          loop_step = power_down;
          break;
        default:
          break; // Do nothing. Just display the menu again
      }
    }
    break; // End of case main_menu

    // ************************************************************************************************
    // Read the pressure and temperature from the MS8607
    case read_MS8607:
    {
      setAGTWirePullups(1); // MS8607 needs pull-ups

      if (ms8607Begun == false) // Do we need to begin the sensor?
      {
        Serial.println(F("INFO          : Starting the MS8607 sensor"));
        if (barometricSensor.begin(agtWire)) // Begin the PHT sensor
        {
          ms8607Begun = true;
        }
        else
        {
          // Display a warning message if we were unable to connect to the MS8607:
          Serial.println(F("WARNING       : Could not detect the MS8607 sensor. Trying again..."));
          if (barometricSensor.begin(agtWire)) // Re-begin the PHT sensor
          {
            ms8607Begun = true;
          }
          else
          {
            // Display an error message if we were unable to connect to the MS8607:
            Serial.println(F("ERROR         : MS8607 sensor not detected at default I2C address"));
          }
        }
      }

      if (ms8607Begun == true)
      {
        agtTempC = barometricSensor.getTemperature();
        agtPascals = barometricSensor.getPressure() * 100.0; // Convert pressure from hPa to Pascals

        Serial.print(F("TEMPERATURE   : "));
        Serial.println(agtTempC, 1);
      
        Serial.print(F("PRESSURE      : "));
        Serial.println(agtPascals, 0);
      }

      loop_step = main_menu; // Return to the main menu
    }
    break; // End of case read_ms8607

    // ************************************************************************************************
    // Read the GNSS (ZOE-M8Q)
    case read_GNSS:
    {
      if (iridiumBegun == true) // Do we need to power down the Iridium modem?
      {
        endIridium(); // Disable Iridium, enable power for the GNSS
        delay(1000); // Give the GNSS time to power up
      }
      
      setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance
      
      if (gnssBegun == false) // Do we need to begin the sensor?
      {
        Serial.println(F("INFO          : Starting the GNSS"));
        if (myGNSS.begin(agtWire)) //Connect to the u-blox module using agtWire port
        {
          gnssBegun = true;
          myGNSS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)
          if (myGNSS.setDynamicModel((dynModel)EEPROM.read(0))) // Set the dynamic model
          {
            uint8_t newModel = myGNSS.getDynamicModel();
            if (newModel < 255)
            {
              Serial.print(F("INFO          : GNSS dynamic model is "));
              Serial.println(newModel);
            }
            else
            {   
              Serial.println(F("ERROR         : GNSS dynamic model update failed"));
            }
          }
          else
          {   
            Serial.println(F("ERROR         : GNSS dynamic model update failed"));
          }
        }
        else
        {
          // Display an error message if we were unable to connect to the ZOE-M8Q
          Serial.println(F("ERROR         : u-blox GNSS not detected at default I2C address"));
        }
      }

      if (gnssBegun == true)
      {
        Serial.println(F("INFO          : Requesting GNSS data (1/2)"));
        agtFixType = myGNSS.getFixType(); // Get the GNSS fix type twice to ensure we have fresh data
        Serial.println(F("INFO          : Requesting GNSS data (2/2)"));
        agtFixType = myGNSS.getFixType();
        agtSecond = myGNSS.getSecond();
        agtMinute = myGNSS.getMinute();
        agtHour = myGNSS.getHour();
        agtDay = myGNSS.getDay();
        agtMonth = myGNSS.getMonth();
        agtYear = myGNSS.getYear(); // Get the year
        agtLatitude = (float)myGNSS.getLatitude() / 10000000.0; // Get the latitude in degrees
        agtLongitude = (float)myGNSS.getLongitude() / 10000000.0; // Get the longitude in degrees
        agtAltitude = myGNSS.getAltitudeMSL() / 1000; // Get the altitude above Mean Sea Level in m
        agtSpeed = (float)myGNSS.getGroundSpeed() / 1000.0; // Get the ground speed in m/s
        agtCourse = myGNSS.getHeading() / 10000000; // Get the heading in degrees

        // Apollo3 v2.1 does not support printf or sprintf correctly. We need to manually add preceeding zeros
        // and convert floats to strings

        // Convert the date and time into strings
        char gnssDay[3];
        char gnssMonth[3];
        if (agtDay < 10)
          sprintf(gnssDay, "0%d", agtDay);
        else
          sprintf(gnssDay, "%d", agtDay);
        if (agtMonth < 10)
          sprintf(gnssMonth, "0%d", agtMonth);
        else
          sprintf(gnssMonth, "%d", agtMonth);
        char gnssDate[20];
        sprintf(gnssDate, "DATE          : %d/%s/%s", agtYear, gnssMonth, gnssDay);
        Serial.println(gnssDate);
      
        char gnssHour[3];
        char gnssMin[3];
        char gnssSec[3];
        if (agtHour < 10)
          sprintf(gnssHour, "0%d", agtHour);
        else
          sprintf(gnssHour, "%d", agtHour);
        if (agtMinute < 10)
          sprintf(gnssMin, "0%d", agtMinute);
        else
          sprintf(gnssMin, "%d", agtMinute);
        if (agtSecond < 10)
          sprintf(gnssSec, "0%d", agtSecond);
        else
          sprintf(gnssSec, "%d", agtSecond);
        char gnssTime[20];
        sprintf(gnssTime, "TIME          : %s:%s:%s", gnssHour, gnssMin, gnssSec);
        Serial.println(gnssTime);
        
        Serial.print(F("FIX           : "));
        Serial.println(agtFixType);

        // Convert the floating point values into strings
        char latStr[15]; // latitude string
        ftoa(agtLatitude,latStr,6,15);
        Serial.print(F("LATITUDE      : "));
        Serial.println(latStr);

        char lonStr[15]; // longitude string
        ftoa(agtLongitude,lonStr,6,15);
        Serial.print(F("LONGITUDE     : "));
        Serial.println(lonStr);

        char altStr[15]; // altitude string
        ftoa(agtAltitude,altStr,2,15);
        Serial.print(F("ALTITUDE      : "));
        Serial.println(altStr);

        char speedStr[8]; // speed string
        ftoa(agtSpeed,speedStr,2,8);
        Serial.print(F("SPEED         : "));
        Serial.println(speedStr);

        Serial.print(F("COURSE        : "));
        Serial.println(agtCourse);
      }

      loop_step = main_menu; // Return to the main menu
    }
    break;

    // ************************************************************************************************
    // Check for a new Iridium message
    case check_Iridium:
    {
      if (beginIridium()) // Try to begin the Iridium modem
      {
        iridiumTransfer(NULL); // Perform an Iridium transfer with no MO message
      }

      loop_step = main_menu; // Go back to the main menu
    }
    break;
    
    // ************************************************************************************************
    // Send an Iridium text message (and check for a new message)
    case send_Iridium_text:
    {
      Serial.println(F("IRIDIUM TX TXT: "));
      Serial.println(F("IRIDIUM TX TXT: Please enter the text message"));
      Serial.println(F("IRIDIUM TX TXT: "));
      Serial.print(F("IRIDIUM TX TXT> "));

      char menuString[] = "IRIDIUM TX TXT> ";

      int msgLen = getUserString(menuString, 60, 250, iridiumMObuffer, iridiumMObufferLength, false);

      if (msgLen > 0)
      {
        if (beginIridium()) // Try to begin the Iridium modem
        {
          iridiumTransfer((const char *)iridiumMObuffer); // Perform an Iridium text transfer
        }
      }

      loop_step = main_menu; // Go back to the main menu      
    }
    break;
    
    // ************************************************************************************************
    // Send an Iridium text message (and check for a new message)
    case send_Iridium_binary:
    {
      Serial.println(F("IRIDIUM TX BIN: "));
      Serial.println(F("IRIDIUM TX BIN: Please enter the binary message as ASCII hex"));
      Serial.println(F("IRIDIUM TX BIN: "));
      Serial.print(F("IRIDIUM TX BIN> "));

      char menuString[] = "IRIDIUM TX BIN> ";

      int msgLen = getUserString(menuString, 60, 250, iridiumMObuffer, iridiumMObufferLength, true);

      if (msgLen > 0)
      {
        for (int i = 0; i < msgLen; i++) // Convert the ASCII hex to binary
        {
          uint8_t theByte = iridiumMObuffer[i]; // Get the byte, convert it to binary
          if ((theByte >= '0') && (theByte <= '9'))
            theByte = theByte - '0';
          else if ((theByte >= 'A') && (theByte <= 'F'))
            theByte = theByte + 10 - 'A';
          else // if ((theByte >= 'a') && (theByte <= 'f'))
            theByte = theByte + 10 - 'a';
          
          if (i % 2 == 0) // Are we on an even byte?
          {
            iridiumMObuffer[i / 2] = theByte << 4; // Overwrite the buffer
          }
          else
          {
            iridiumMObuffer[i / 2] |= theByte; // OR in the this odd byte
          }
        }

        msgLen = (msgLen + 1) / 2; // Divide len by 2 (round up if len is odd)
        
        if (beginIridium()) // Try to begin the Iridium modem
        {
          iridiumTransfer((const char *)iridiumMObuffer, true, (size_t)msgLen); // Perform an Iridium binary transfer
        }
      }

      loop_step = main_menu; // Go back to the main menu      
    }
    break;
    
    // ************************************************************************************************
    // Flush the Mobile Terminated message queue (RockBLOCK only)
    case flush_Iridium_MT:
    {
      Serial.println(F("FLUSH MT Q    : "));
      Serial.println(F("FLUSH MT Q    : Flushing the Mobile Terminated queue will delete all messages waiting in the Iridium system"));
      Serial.println(F("FLUSH MT Q    : You will still be charged for those messages"));
      Serial.println(F("FLUSH MT Q    : Enter 1 to confirm"));
      Serial.println(F("FLUSH MT Q    : "));
      Serial.print(F("FLUSH MT Q    > "));

      char menuString[] = "FLUSH MT Q    > ";

      if (getMenuChoice(menuString, 60, 250) == 1)
      {
        Serial.println(F("INFO          : Flushing MT queue"));
        if (beginIridium()) // Try to begin the Iridium modem
        {
          sprintf((char *)iridiumMObuffer, "FLUSH_MT");
          iridiumTransfer((const char *)iridiumMObuffer); // Perform an Iridium text transfer
        }
      }

      loop_step = main_menu; // Go back to the main menu      
    }
    break;
    
    // ************************************************************************************************
    // Set the GNSS dynamic model
    case set_dynModel:
    {
      Serial.println(F("DYNMODEL      : "));
      Serial.println(F("DYNMODEL      : Please enter the new dynamic model. Possible values are:"));
      Serial.println(F("DYNMODEL      : "));
      Serial.println(F("DYNMODEL      : 0: portable"));
      Serial.println(F("DYNMODEL      : 2: stationary"));
      Serial.println(F("DYNMODEL      : 3: pedestrian"));
      Serial.println(F("DYNMODEL      : 4: automotive"));
      Serial.println(F("DYNMODEL      : 5: sea"));
      Serial.println(F("DYNMODEL      : 6: airborne with <1g acceleration"));
      Serial.println(F("DYNMODEL      : 7: airborne with <2g acceleration"));
      Serial.println(F("DYNMODEL      : 8: airborne with <4g acceleration"));
      Serial.println(F("DYNMODEL      : "));
      Serial.print(F("DYNMODEL      > "));

      char menuString[] = "DYNMODEL      > ";

      // Get the menu choice using a 60 second timeout and a 250ms inter-character delay
      int menuChoice = getMenuChoice(menuString, 60, 250);

      switch (menuChoice)
      {
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        {
          EEPROM.write(0, menuChoice); // Model is valid. Update it in EEPROM
          Serial.println(F("INFO          : Dynamic model updated in EEPROM"));
          if (gnssBegun == true)
          {
            setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance
            if (myGNSS.setDynamicModel((dynModel)EEPROM.read(0))) // Set the dynamic model
            {
              uint8_t newModel = myGNSS.getDynamicModel();
              if (newModel < 255)
              {
                Serial.print(F("INFO          : GNSS dynamic model is "));
                Serial.println(newModel);
              }
              else
              {   
                Serial.println(F("ERROR         : GNSS dynamic model update failed"));
              }
            }
            else
            {   
              Serial.println(F("ERROR         : GNSS dynamic model update failed"));
            }
          }
        }
        break;
        case -1: // getMenuChoice will return -1 if nothing was entered within the timeout
        default:
          break; // Do nothing
      }

    loop_step = main_menu; // Return to the main menu
    }
    break;
    
    // ************************************************************************************************
    // Power down and wait for reset
    case power_down:
    {
      digitalWrite(LED, LOW); // Turn the LED off
      digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
      digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power (HIGH = enable; LOW = disable)
      digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger (HIGH = enable; LOW = disable)
      gnssOFF(); // Disable power for the GNSS
      digitalWrite(busVoltageMonEN, LOW); // Disable the bus voltage monitor

      Serial.println(F("INFO          : AGT has been powered off"));
      Serial.println(F("INFO          : Please disconnect and reconnect"));
      Serial.flush();
      Serial.end();
      agtWire.end();

      while (1)
        ; // Do nothing more
    }
    break;
    
  } // End of switch (loop_step)
} // End of loop()

// Begin the Iridium modem
bool beginIridium()
{
  if (iridiumBegun == true) // Is the Iridium modem already running?
    return (true); // Nothing to do!
    
  agtPGOOD = digitalRead(superCapPGOOD); // Read the PGOOD pin
      
  // Wait for PGOOD to go HIGH for up to CHG_timeout minutes
  for (unsigned long tnow = millis(); (!agtPGOOD) && ((millis() - tnow) < (CHG_timeout * 60UL * 1000UL)); )
  {
    agtPGOOD = digitalRead(superCapPGOOD); // Read the PGOOD pin

    if (agtPGOOD)
      superCapChargerStartedAt = millis(); // If the PGOOD pin has just gone high, update superCapChargerStartedAt
    
    // Toggle the LED (1Hz)
    digitalWrite(LED, !digitalRead(LED));

    Serial.println(F("INFO          : Waiting for supercapacitors to charge"));

    delay(500);
  }

  // Is PGOOD high?
  if (!agtPGOOD)
  {
    digitalWrite(LED, LOW); // Turn the LED off
    Serial.println(F("ERROR         : Supercapacitors failed to charge"));
    return (false);
  }

  // Was the supercapacitor charger started at least TOPUP_timeout seconds ago?
  // (This doesn't test for PGOOD going high just before beginIridium was called)
  while (millis() < (superCapChargerStartedAt + (TOPUP_timeout * 1000UL)))
  {
    // Toggle the LED (1Hz)
    digitalWrite(LED, !digitalRead(LED));

    Serial.println(F("INFO          : Topping up the supercapacitors"));

    delay(500);
  }

  digitalWrite(LED, LOW); // Turn the LED off
  
  Serial.println(F("INFO          : Disabling power for the GNSS"));
  gnssOFF(); // Disable power for the GNSS
  gnssBegun = false; // Flag the GNSS as not begun
  delay(100); // Wait for power to decay

  // Enable power for the 9603N
  Serial.println(F("INFO          : Enabling power for the Iridium modem"));
  digitalWrite(iridiumPwrEN, HIGH); // Enable Iridium Power
  delay(1000);

  // Enable the 9603N and start talking to it
  Serial.println(F("INFO          : Beginning to talk to the Iridium modem"));

  modem.setPowerProfile(IridiumSBD::DEFAULT_POWER_PROFILE); // Use the default power profile

  // Begin satellite modem operation
  // Also begin the serial port connected to the satellite modem via IridiumSBD::beginSerialPort
  agtErr = modem.begin();

  // Check if the modem started correctly
  if (agtErr != ISBD_SUCCESS)
  {
    // If the modem failed to start, disable it and go to sleep
    Serial.print(F("ERROR         : modem.begin failed with error "));
    Serial.println(agtErr);

    // Make sure the Serial1 RX pin is disabled
    modem.endSerialPort();
  
    Serial.println(F("INFO          : Disabling power for the Iridium modem"));
    digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
    digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
    delay(100); // Wait for power to decay
    
    Serial.println(F("INFO          : Enabling power for the GNSS"));
    gnssON(); // Re-enable power for the GNSS

    digitalWrite(LED, LOW); // Turn the LED off
  
    return (false);
  }

  digitalWrite(LED, LOW); // Turn the LED off
  
  iridiumBegun = true;
  return (true); // Modem is good to go!
}

// Peform an Iridium SBD transfer
// bufferPointer can be set to NULL if we have no data to send
bool iridiumTransfer(const char *bufferPointer, bool binary, size_t binaryLength)
{
  bool retVal;
  
  if (binary == false) // Are we sending text (if any)?
  {
    Serial.println(F("INFO          : Starting Iridium modem text transfer"));

    size_t bufferSize = sizeof(iridiumMTbuffer);
    agtErr = modem.sendReceiveSBDText(bufferPointer, iridiumMTbuffer, bufferSize); // Do the SBD transfer
    
    if (agtErr == ISBD_SUCCESS) // Was the transfer successful?
    {
      Serial.println(F("INFO          : Iridium transfer was successful"));
      
      if (bufferSize > 0) // Did we receive any data?
      {
        Serial.print(F("IRIDIUM RX HEX: "));
        for (int i = 0; i < bufferSize; i++) // Print the data as ASCII hex
        {
          Serial.write((iridiumMTbuffer[i] >> 4) <= 0x09 ? (iridiumMTbuffer[i] >> 4) + '0' : (iridiumMTbuffer[i] >> 4) + 'A' - 10); // Print the MS nibble
          Serial.write((iridiumMTbuffer[i] & 0x0F) <= 0x09 ? (iridiumMTbuffer[i] & 0x0F) + '0' : (iridiumMTbuffer[i] & 0x0F) + 'A' - 10); // Print the LS nibble
        }
        Serial.println();
        Serial.print(F("IRIDIUM RX TXT: "));
        for (int i = 0; i < bufferSize; i++) // Print the data as ASCII text if possible
        {
          if ((iridiumMTbuffer[i] >= ' ') && (iridiumMTbuffer[i] <= '~'))
            Serial.write(iridiumMTbuffer[i]);
        }
        Serial.println();
      }

      Serial.print(F("IRIDIUM MT Q  : "));
      Serial.println(modem.getWaitingMessageCount());

      retVal = true;
    }
    else
    {
      Serial.print(F("ERROR         : Iridium transfer failed with error "));
      Serial.println(agtErr);
      retVal = false;
    }
  }
  else // We are sending binary data
  {
    Serial.println(F("INFO          : Starting Iridium modem binary transfer"));

    size_t bufferSize = sizeof(iridiumMTbuffer);
    agtErr = modem.sendReceiveSBDBinary((const uint8_t *)bufferPointer, binaryLength, iridiumMTbuffer, bufferSize); // Do the SBD transfer
    
    if (agtErr == ISBD_SUCCESS) // Was the transfer successful?
    {
      Serial.println(F("INFO          : Iridium transfer was successful"));
      
      if (bufferSize > 0) // Did we receive any data?
      {
        Serial.print(F("IRIDIUM RX HEX: "));
        for (int i = 0; i < bufferSize; i++) // Print the data as ASCII hex
        {
          Serial.write((iridiumMTbuffer[i] >> 4) <= 0x09 ? (iridiumMTbuffer[i] >> 4) + '0' : (iridiumMTbuffer[i] >> 4) + 'A' - 10); // Print the MS nibble
          Serial.write((iridiumMTbuffer[i] & 0x0F) <= 0x09 ? (iridiumMTbuffer[i] & 0x0F) + '0' : (iridiumMTbuffer[i] & 0x0F) + 'A' - 10); // Print the LS nibble
        }
        Serial.println();
        Serial.print(F("IRIDIUM RX TXT: "));
        for (int i = 0; i < bufferSize; i++) // Print the data as ASCII text if possible
        {
          if ((iridiumMTbuffer[i] >= ' ') && (iridiumMTbuffer[i] <= '~'))
            Serial.write(iridiumMTbuffer[i]);
        }
        Serial.println();
      }

      Serial.print(F("IRIDIUM MT Q  : "));
      Serial.println(modem.getWaitingMessageCount());

      retVal = true;
    }
    else
    {
      Serial.print(F("ERROR         : Iridium transfer failed with error "));
      Serial.println(agtErr);
      retVal = false;
    }
  }

  // Clear the Mobile Originated message buffer
  Serial.println(F("INFO          : Clearing the MO buffer"));
  agtErr = modem.clearBuffers(ISBD_CLEAR_MO); // Clear MO buffer
  if (agtErr != ISBD_SUCCESS)
  {
    Serial.print(F("ERROR         : modem.clearBuffers failed with error "));
    Serial.println(agtErr);
    retVal = false;
  }

  digitalWrite(LED, LOW); // Turn the LED off
  
  return (retVal);
}

bool endIridium()
{
  // Power down the modem
  // Also disable the Serial1 RX pin via IridiumSBD::endSerialPort
  Serial.println(F("INFO          : Putting the Iridium modem to sleep"));
  agtErr = modem.sleep();
  if (agtErr != ISBD_SUCCESS)
  {
    Serial.print(F("ERROR         : modem.sleep failed with error "));
    Serial.println(agtErr);
  }

  digitalWrite(LED, LOW); // Turn the LED off
  
  Serial.println(F("INFO          : Disabling Iridium modem power"));
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  delay(100); // Wait for power to decay

  iridiumBegun = false;
  
  Serial.println(F("INFO          : Enabling power for the GNSS"));
  gnssON(); // Re-enable power for the GNSS

  return (agtErr == ISBD_SUCCESS);
}

// Get the menu choice using a timeout and an inter-character delay
// Return -1 if nothing was entered within the timeout
int getMenuChoice(char *menuString, unsigned long timeout, unsigned long interCharDelay)
{
  Serial.flush(); // Flush any characters in the serial Tx buffer
  while (Serial.available() > 0) Serial.read(); //Clear Rx buffer
  
  unsigned long lastCharArrival = millis();
  bool keepGoing = true;
  int charBufferWidth = 10;
  char charBuffer[charBufferWidth];
  unsigned int bufferPointer = 0;
  charBuffer[bufferPointer] = 0; // NULL-terminate the charBuffer
  bool hasChanged = false;

  // Check for timeout (seconds)
  while (keepGoing && (millis() < (lastCharArrival + (timeout * 1000))))
  {
    // Check for a gap between characters
    while (keepGoing && ((millis() < (lastCharArrival + interCharDelay)) || (Serial.available() > 0)))
    {
      while (Serial.available() > 0) // Check for the arrival of serial data
      {
        char theChar = Serial.read();
        if (theChar == '\r') // Check for a carriage return - line feeds will be ignored
        {
          keepGoing = false; // We are done
        }
        else if ((theChar >= '0') && (theChar <= '9')) // Only accept numeric chars
        {
          charBuffer[bufferPointer] = theChar; // Store theChar
          if (bufferPointer < (charBufferWidth - 2))
            bufferPointer++; // Increment the pointer (unless the buffer is full)
          charBuffer[bufferPointer] = 0; // NULL-terminate the charBuffer
          hasChanged = true; // Flag that the buffer has changed
        }
        else if (theChar == 8) // Backspace
        {
          if (bufferPointer > 0)
          {
            bufferPointer--; // Decrement the pointer (unless the buffer is empty)
            charBuffer[bufferPointer] = 0; // Erase the previous character
            hasChanged = true; // Flag that the buffer has changed
          }
        }
        lastCharArrival = millis(); // We received new data so update lastCharArrival
      }
    }

    // Has the buffer changed?
    if (hasChanged == true)
    {
      char terminalStr[charBufferWidth + menuStringWidth];
      Serial.println();
      sprintf(terminalStr, "%s%s", menuString, charBuffer);
      Serial.print(terminalStr); // Print the received characters
      hasChanged = false;
    }
  }

  Serial.println();

  if (keepGoing == true) // If keepGoing is still true, we must have reached the end of timeout
  {
    return (-1); // Return -1 to indicate a timeout
  }

  if (bufferPointer == 0) // If the buffer is empty
  {
    return (-1); // Return -1
  }

  // Convert charBuffer into an integer
  int result = 0;
  for (unsigned int x = 0; x < bufferPointer; x++)
  {
    result *= 10;
    result += (charBuffer[x] - '0');
  }
  return (result);
}

// Get the user string using a timeout and an inter-character delay
// Return zero if nothing was entered within the timeout, otherwise return the number of characters entered
int getUserString(char *menuString, unsigned long timeout, unsigned long interCharDelay, uint8_t *charBuffer, int charBufferWidth, bool onlyHex)
{
  Serial.flush(); // Flush any characters in the serial Tx buffer
  while (Serial.available() > 0) Serial.read(); //Clear Rx buffer
  
  unsigned long lastCharArrival = millis();
  bool keepGoing = true;
  unsigned int bufferPointer = 0;
  charBuffer[bufferPointer] = 0; // NULL-terminate the charBuffer
  bool hasChanged = false;

  // Check for timeout (seconds)
  while (keepGoing && (millis() < (lastCharArrival + (timeout * 1000))))
  {
    // Check for a gap between characters
    while (keepGoing && ((millis() < (lastCharArrival + interCharDelay)) || (Serial.available() > 0)))
    {
      while (Serial.available() > 0) // Check for the arrival of serial data
      {
        char theChar = Serial.read();
        if (theChar == '\r') // Check for a carriage return - line feeds will be ignored
        {
          keepGoing = false; // We are done
        }
        else if ( // Validate theChar
          ((onlyHex == true) && (((theChar >= '0') && (theChar <= '9')) || ((theChar >= 'a') && (theChar <= 'f')) || ((theChar >= 'A') && (theChar <= 'F'))))
          ||
          ((onlyHex == false) && ((theChar >= ' ') && (theChar <= '~')))
          )
        {
          charBuffer[bufferPointer] = theChar; // Store theChar
          if (bufferPointer < (charBufferWidth - 2))
            bufferPointer++; // Increment the pointer (unless the buffer is full)
          charBuffer[bufferPointer] = 0; // NULL-terminate the charBuffer
          hasChanged = true; // Flag that the buffer has changed
        }
        else if (theChar == 8) // Backspace
        {
          if (bufferPointer > 0)
          {
            bufferPointer--; // Decrement the pointer (unless the buffer is empty)
            charBuffer[bufferPointer] = 0; // Erase the previous character
            hasChanged = true; // Flag that the buffer has changed
          }
        }
        lastCharArrival = millis(); // We received new data so update lastCharArrival
      }
    }

    // Has the buffer changed?
    if (hasChanged == true)
    {
      char terminalStr[charBufferWidth + menuStringWidth];
      Serial.println();
      sprintf(terminalStr, "%s%s", menuString, charBuffer);
      Serial.print(terminalStr); // Print the received characters
      hasChanged = false;
    }
  }

  Serial.println();

  if (keepGoing == true) // If keepGoing is still true, we must have reached the end of timeout
  {
    return (0); // Return 0 to indicate a timeout
  }

  return (bufferPointer); // Return the number of characters entered
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

void setAGTWirePullups(uint32_t i2cBusPullUps)
{
  //Change SCL and SDA pull-ups manually using pin_config
  am_hal_gpio_pincfg_t sclPinCfg = g_AM_BSP_GPIO_IOM1_SCL;
  am_hal_gpio_pincfg_t sdaPinCfg = g_AM_BSP_GPIO_IOM1_SDA;

  if (i2cBusPullUps == 0)
  {
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE; // No pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_NONE;
  }
  else if (i2cBusPullUps == 1)
  {
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K; // Use 1K5 pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_1_5K;
  }
  else if (i2cBusPullUps == 6)
  {
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_6K; // Use 6K pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_6K;
  }
  else if (i2cBusPullUps == 12)
  {
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_12K; // Use 12K pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_12K;
  }
  else
  {
    sclPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_24K; // Use 24K pull-ups
    sdaPinCfg.ePullup = AM_HAL_GPIO_PIN_PULLUP_24K;
  }

  pin_config(PinName(PIN_AGTWIRE_SCL), sclPinCfg);
  pin_config(PinName(PIN_AGTWIRE_SDA), sdaPinCfg);
}

//*****************************************************************************
//
//  Divide an unsigned 32-bit value by 10.
//
//  Note: Adapted from Ch10 of Hackers Delight (hackersdelight.org).
//
//*****************************************************************************
static uint64_t divu64_10(uint64_t ui64Val)
{
    uint64_t q64, r64;
    uint32_t q32, r32, ui32Val;

    //
    // If a 32-bit value, use the more optimal 32-bit routine.
    //
    if ( ui64Val >> 32 )
    {
        q64 = (ui64Val>>1) + (ui64Val>>2);
        q64 += (q64 >> 4);
        q64 += (q64 >> 8);
        q64 += (q64 >> 16);
        q64 += (q64 >> 32);
        q64 >>= 3;
        r64 = ui64Val - q64*10;
        return q64 + ((r64 + 6) >> 4);
    }
    else
    {
        ui32Val = (uint32_t)(ui64Val & 0xffffffff);
        q32 = (ui32Val>>1) + (ui32Val>>2);
        q32 += (q32 >> 4);
        q32 += (q32 >> 8);
        q32 += (q32 >> 16);
        q32 >>= 3;
        r32 = ui32Val - q32*10;
        return (uint64_t)(q32 + ((r32 + 6) >> 4));
    }
}

//*****************************************************************************
//
// Converts ui64Val to a string.
// Note: pcBuf[] must be sized for a minimum of 21 characters.
//
// Returns the number of decimal digits in the string.
//
// NOTE: If pcBuf is NULL, will compute a return ui64Val only (no chars
// written).
//
//*****************************************************************************
static int uint64_to_str(uint64_t ui64Val, char *pcBuf)
{
    char tbuf[25];
    int ix = 0, iNumDig = 0;
    unsigned uMod;
    uint64_t u64Tmp;

    do
    {
        //
        // Divide by 10
        //
        u64Tmp = divu64_10(ui64Val);

        //
        // Get modulus
        //
        uMod = ui64Val - (u64Tmp * 10);

        tbuf[ix++] = uMod + '0';
        ui64Val = u64Tmp;
    } while ( ui64Val );

    //
    // Save the total number of digits
    //
    iNumDig = ix;

    //
    // Now, reverse the buffer when saving to the caller's buffer.
    //
    if ( pcBuf )
    {
        while ( ix-- )
        {
            *pcBuf++ = tbuf[ix];
        }

        //
        // Terminate the caller's buffer
        //
        *pcBuf = 0x00;
    }

    return iNumDig;
}

//*****************************************************************************
//
//  Float to ASCII text. A basic implementation for providing support for
//  single-precision %f.
//
//  param
//      fValue     = Float value to be converted.
//      pcBuf      = Buffer to place string AND input of buffer size.
//      iPrecision = Desired number of decimal places.
//      bufSize    = The size (in bytes) of the buffer.
//                   The recommended size is at least 16 bytes.
//
//  This function performs a basic translation of a floating point single
//  precision value to a string.
//
//  return Number of chars printed to the buffer.
//
//*****************************************************************************
#define OLA_FTOA_ERR_VAL_TOO_SMALL   -1
#define OLA_FTOA_ERR_VAL_TOO_LARGE   -2
#define OLA_FTOA_ERR_BUFSIZE         -3

typedef union
{
    int32_t I32;
    float F;
} ola_i32fl_t;

static int ftoa(float fValue, char *pcBuf, int iPrecision, int bufSize)
{
    ola_i32fl_t unFloatValue;
    int iExp2, iBufSize;
    int32_t i32Significand, i32IntPart, i32FracPart;
    char *pcBufInitial, *pcBuftmp;

    iBufSize = bufSize; // *(uint32_t*)pcBuf;
    if (iBufSize < 4)
    {
        return OLA_FTOA_ERR_BUFSIZE;
    }

    if (fValue == 0.0f)
    {
        // "0.0"
        *(uint32_t*)pcBuf = 0x00 << 24 | ('0' << 16) | ('.' << 8) | ('0' << 0);
        return 3;
    }

    pcBufInitial = pcBuf;

    unFloatValue.F = fValue;

    iExp2 = ((unFloatValue.I32 >> 23) & 0x000000FF) - 127;
    i32Significand = (unFloatValue.I32 & 0x00FFFFFF) | 0x00800000;
    i32FracPart = 0;
    i32IntPart = 0;

    if (iExp2 >= 31)
    {
        return OLA_FTOA_ERR_VAL_TOO_LARGE;
    }
    else if (iExp2 < -23)
    {
        return OLA_FTOA_ERR_VAL_TOO_SMALL;
    }
    else if (iExp2 >= 23)
    {
        i32IntPart = i32Significand << (iExp2 - 23);
    }
    else if (iExp2 >= 0)
    {
        i32IntPart = i32Significand >> (23 - iExp2);
        i32FracPart = (i32Significand << (iExp2 + 1)) & 0x00FFFFFF;
    }
    else // if (iExp2 < 0)
    {
        i32FracPart = (i32Significand & 0x00FFFFFF) >> -(iExp2 + 1);
    }

    if (unFloatValue.I32 < 0)
    {
        *pcBuf++ = '-';
    }

    if (i32IntPart == 0)
    {
        *pcBuf++ = '0';
    }
    else
    {
        if (i32IntPart > 0)
        {
            uint64_to_str(i32IntPart, pcBuf);
        }
        else
        {
            *pcBuf++ = '-';
            uint64_to_str(-i32IntPart, pcBuf);
        }
        while (*pcBuf)    // Get to end of new string
        {
            pcBuf++;
        }
    }

    //
    // Now, begin the fractional part
    //
    *pcBuf++ = '.';

    if (i32FracPart == 0)
    {
        *pcBuf++ = '0';
    }
    else
    {
        int jx, iMax;

        iMax = iBufSize - (pcBuf - pcBufInitial) - 1;
        iMax = (iMax > iPrecision) ? iPrecision : iMax;

        for (jx = 0; jx < iMax; jx++)
        {
            i32FracPart *= 10;
            *pcBuf++ = (i32FracPart >> 24) + '0';
            i32FracPart &= 0x00FFFFFF;
        }

        //
        // Per the printf spec, the number of digits printed to the right of the
        // decimal point (i.e. iPrecision) should be rounded.
        // Some examples:
        // Value        iPrecision          Formatted value
        // 1.36399      Unspecified (6)     1.363990
        // 1.36399      3                   1.364
        // 1.36399      4                   1.3640
        // 1.36399      5                   1.36399
        // 1.363994     Unspecified (6)     1.363994
        // 1.363994     3                   1.364
        // 1.363994     4                   1.3640
        // 1.363994     5                   1.36399
        // 1.363995     Unspecified (6)     1.363995
        // 1.363995     3                   1.364
        // 1.363995     4                   1.3640
        // 1.363995     5                   1.36400
        // 1.996        Unspecified (6)     1.996000
        // 1.996        2                   2.00
        // 1.996        3                   1.996
        // 1.996        4                   1.9960
        //
        // To determine whether to round up, we'll look at what the next
        // decimal value would have been.
        //
        if ( ((i32FracPart * 10) >> 24) >= 5 )
        {
            //
            // Yes, we need to round up.
            // Go back through the string and make adjustments as necessary.
            //
            pcBuftmp = pcBuf - 1;
            while ( pcBuftmp >= pcBufInitial )
            {
                if ( *pcBuftmp == '.' )
                {
                }
                else if ( *pcBuftmp == '9' )
                {
                    *pcBuftmp = '0';
                }
                else
                {
                    *pcBuftmp += 1;
                    break;
                }
                pcBuftmp--;
            }
        }
    }

    //
    // Terminate the string and we're done
    //
    *pcBuf = 0x00;

    return (pcBuf - pcBufInitial);
} // ftoa()
