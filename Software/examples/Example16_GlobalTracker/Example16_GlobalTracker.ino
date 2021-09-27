/*
  Artemis Global Tracker
  
  Written by Paul Clark (PaulZC)
  September 7th 2021

  ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
  ** (At the time of writing, v2.1.1 of the core conatins a feature which makes communication with the u-blox GNSS problematic. Be sure to use v2.1.0) **

  ** Set the Board to "RedBoard Artemis ATP" **
  ** (The Artemis Module does not have a Wire port defined, which prevents the GNSS library from compiling) **

  This example builds on the BetterTracker example. Many settings are stored in EEPROM (Flash) and can be configured
  via the USB port (Serial Monitor) or via an Iridium binary message sent from Rock7 Operations.

  Messages can be sent automatically when:
  the tracker leaves or enters a geofenced area;
  pressure, humidity or temperature limits are exceeded;
  the battery voltage is low.

  Messages can be sent in text format (human-readable) or binary format (to save messages credits).

  You can configure which message fields are included in the message so you only send the data you need.
  You can find a Python3 PyQt5 configuration tool at:
  https://github.com/sparkfun/Artemis_Global_Tracker/tree/master/Tools

  You can trigger user-defined functions and include readings from additional sensors:
  https://github.com/sparkfun/Artemis_Global_Tracker/tree/master/Documentation/GlobalTracker_FAQs#How-do-I-define-and-trigger-a-user-function
  https://github.com/sparkfun/Artemis_Global_Tracker/tree/master/Documentation/GlobalTracker_FAQs#how-do-i-send-a-user-value

  You can have the Iridium 9603N monitor the ring channel continuously for new Mobile Terminated messages
  but this will increase the current draw considerably. This is not recommended for battery-powered
  applications. You will also need to increase WAKEINT, setting it to the same interval as both ALARMINT
  and TXINT.
  
  Likewise, if you want to enable geofence alarm messages, you will also need to increase WAKEINT.
  Set it to the same interval as ALARMINT.
  
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
  
  Basic information on how to install an Arduino library is available here:
  https://learn.sparkfun.com/tutorials/installing-an-arduino-library
  
  SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Version history:
  August 25th 2021
    Added a fix for https://github.com/sparkfun/Arduino_Apollo3/issues/423
  August 7th 2021
    Updated for v2.1 of the Apollo3 core
  December 15th, 2020:
    Adding the deep sleep code from OpenLog Artemis.
    Keep RAM powered up during sleep to prevent corruption above 64K.
    Restore busVoltagePin (Analog in) after deep sleep.
  June 7th, 2020:
    Original release

*/

// Artemis Global Tracker pin definitions
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

// Include the u-blox library first so the message fields know about the dynModel enum
#include "SparkFun_u-blox_GNSS_Arduino_Library.h" //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGPS;

#include "Tracker_Message_Fields.h" // Include the message field and storage definitions
trackerSettings myTrackerSettings; // Create storage for the tracker settings in RAM

//#define noTX // Uncomment this line to disable the Iridium SBD transmit if you want to test the code without using message credits
//#define skipGNSS // Uncomment this line to skip getting a GNSS fix (only valid if noTX is defined too)

#include "RTC.h" //Include RTC library included with the Arduino_Apollo3 core

#include <EEPROM.h> // Needed for EEPROM storage on the Artemis

// We use Serial1 to communicate with the Iridium modem. Serial1 on the ATP uses pin 24 for TX and 25 for RX. AGT uses the same pins.

#include <IridiumSBD.h> //http://librarymanager/All#IridiumSBDI2C
#define DIAGNOSTICS false // Change this to true to see IridiumSBD diagnostics
// Declare the IridiumSBD object (including the sleep (ON/OFF) and Ring Indicator pins)
IridiumSBD modem(Serial1, iridiumSleep, iridiumRI);

#include <Wire.h> // Needed for I2C
const byte PIN_AGTWIRE_SCL = 8;
const byte PIN_AGTWIRE_SDA = 9;
TwoWire agtWire(PIN_AGTWIRE_SDA, PIN_AGTWIRE_SCL); //Create an I2C port using pads 8 (SCL) and 9 (SDA)

#include <SparkFun_PHT_MS8607_Arduino_Library.h> //http://librarymanager/All#SparkFun_MS8607
MS8607 barometricSensor; //Create an instance of the MS8607 object

// iterationCounter is incremented each time a transmission is attempted.
// It helps keep track of whether messages are being sent successfully.
// It also indicates if the tracker has been reset (the count will go back to zero).
long iterationCounter = 0;

// Use these to keep a count of the seconds from the rtc
volatile unsigned long seconds_since_last_wake = 0;
volatile unsigned long seconds_since_last_alarmtx = 0;
volatile unsigned long seconds_since_last_tx = 0;

// This flag indicates an interval alarm has occurred
volatile bool interval_alarm = false;

// This flag indicates the geofence pin has changed state
volatile bool geofence_alarm = false;

// Volatile copy of the WAKEINT (wake up interval)
volatile uint32_t wake_int;

// More global variables
bool PGOOD = false; // Flag to indicate if LTC3225 PGOOD is HIGH
int err; // Error value returned by IridiumSBD.begin
bool firstTime = true; // Flag to indicate if this is the first time around the loop (so we go right round the loop and not just check the PHT)
int delayCount; // General-purpose delay count used by non-blocking delays
bool alarmState = false; // Use this to keep track of whether we are in an alarm state
uint8_t alarmType = 0; // Field to save which alarm has been set off
bool dynamicModelSet = false; // Flag to indicate if the ZOE-M8Q dynamic model has been set
bool geofencesSet = false; // Flag to indicate if the ZOE-M8Q geofences been set

// geofence_alarm will be set to true by the ISR
// but we only want to send geofence alarm messages every ALARMINT
// so let's use sendGeofenceAlarm to record if a geofence alarm message should be sent
// as geofence_alarm may have been cleared by then
bool sendGeofenceAlarm = false;

uint8_t tracker_serial_rx_buffer[1024]; // Define tracker_serial_rx_buffer which will store the incoming serial configuration data
size_t tracker_serial_rx_buffer_size; // The size of the buffer
unsigned long rx_start; // Used by check_for_serial_data: holds the value of millis after a fresh start
unsigned long last_rx; // Used by check_for_serial_data: holds the value of millis for the last time a byte was received
bool data_seen; // Used by check_for_serial_data: is set to true once byte(s) have been seen after a fresh start

uint8_t mt_buffer[(MTLIM + 1)]; // Buffer to store a Mobile Terminated SBD message (with space for a final NULL)
size_t mtBufferSize; // Size of MT buffer

bool _printDebug = false; // Flag to show if message field debug printing is enabled
Stream *_debugSerial; //The stream to send debug messages to (if enabled)

// Timeout after this many _minutes_ when waiting for a 3D GNSS fix
// (UL = unsigned long)
#define GNSS_timeout 5UL

// Timeout after this many _minutes_ when waiting for the super capacitors to charge
// 1 min should be OK for 1F capacitors at 150mA.
// Charging 10F capacitors at 60mA can take a long time! Could be as much as 10 mins.
#define CHG_timeout 2UL

// Top up the super capacitors for this many _seconds_ to make sure they are fully charged.
// 10 seconds should be OK for 1F capacitors at 150mA.
// Increase the value for 10F capacitors.
#define TOPUP_timeout 10UL

// Loop Steps - these are used by the switch/case in the main loop
// This structure makes it easy to go from any of the steps directly to zzz when (e.g.) the batteries are low
typedef enum {
  loop_init = 0, // Send the welcome message, check the battery voltage
  read_pressure, // Read the pressure and temperature from the MS8607
  start_GPS,     // Enable the ZOE-M8Q, check the battery voltage
  read_GPS,      // Wait for up to GNSS_timeout minutes for a valid 3D fix, check the battery voltage
  start_LTC3225, // Enable the LTC3225 super capacitor charger and wait for up to CHG_timeout minutes for PGOOD to go high
  wait_LTC3225,  // Wait TOPUP_timeout seconds to make sure the capacitors are fully charged
  start_9603,    // Power on the 9603N, send the message, check the battery voltage
  sleep_9603,    // Put the 9603N to sleep
  zzz,           // Turn everything off and put the processor into deep sleep
  wakeUp,        // Wake from deep sleep, restore the processor clock speed
  wait_for_ring, // Keep the 9603N powered up and wait for a ring indication
  configureMe    // Configure the tracker settings via USB Serial
} loop_steps;
volatile loop_steps loop_step = loop_init; // Make sure loop_step is set to loop_init
volatile loop_steps last_loop_step = loop_init; // Go back to this loop_step after doing a configure

// RTC alarm Interrupt Service Routine
// Clear the interrupt flag and increment the seconds_since_last_...
// If WAKEINT has been reached, set the interval_alarm flag and reset seconds_since_last_wake
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt
  rtc.clearInterrupt();

  // Increment seconds_since_last_...
  seconds_since_last_wake = seconds_since_last_wake + 1;
  seconds_since_last_alarmtx = seconds_since_last_alarmtx + 1;
  seconds_since_last_tx = seconds_since_last_tx + 1;

  // Check if interval_alarm should be set
  if (seconds_since_last_wake >= wake_int)
  {
    interval_alarm = true;
    seconds_since_last_wake = 0;
  }
}

// geofencePin Interrupt Service Routine
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
void geofenceISR(void)
{
  geofence_alarm = true; // Set the flag
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

  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  attachInterrupt(digitalPinToInterrupt(geofencePin), geofenceISR, FALLING); // Call geofenceISR whenever geofencePin goes low
  geofence_alarm = false; // In Apollo3 v2.1.0 attachInterrupt causes the interrupt to trigger. So, let's clear the flag.

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power (HIGH = enable; LOW = disable)
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger (HIGH = enable; LOW = disable)
  pinMode(iridiumSleep, OUTPUT); // Iridium 9603N On/Off (Sleep) pin
  digitalWrite(iridiumSleep, LOW); // Put the Iridium 9603N to sleep (HIGH = on; LOW = off/sleep)
  pinMode(iridiumRI, INPUT); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT); // Configure the Iridium Network Available as an input
  pinMode(superCapPGOOD, INPUT); // Configure the super capacitor charger PGOOD input

  // Make sure the Serial1 RX pin is disabled to prevent the power-on glitch on the modem's RX(OUT) pin
  // causing problems with v2.1.0 of the Apollo3 core. Requires v3.0.5 of IridiumSBDi2c.
  modem.endSerialPort();

  pinMode(busVoltageMonEN, OUTPUT); // Make the Bus Voltage Monitor Enable an output
  digitalWrite(busVoltageMonEN, LOW); // Set it low to disable the measurement to save power
  analogReadResolution(14); //Set resolution to 14 bit

  // Initialise the globals
  iterationCounter = 0; // Make sure iterationCounter is set to zero (indicating a reset)
  loop_step = loop_init; // Make sure loop_step is set to loop_init
  last_loop_step = loop_init; // Make sure last_loop_step is set to loop_init
  seconds_since_last_wake = 0; // Make sure seconds_since_last_wake is reset
  seconds_since_last_alarmtx = 0; // Make sure seconds_since_last_alarmtx is reset
  seconds_since_last_tx = 0; // Make sure seconds_since_last_tx is reset
  firstTime = true; // Make sure firstTime is set
  interval_alarm = false; // Make sure the interval alarm flag is clear
  geofence_alarm = false; // Make sure the geofence alarm flag is clear
  sendGeofenceAlarm = false; // Make sure the send-a-geofence-alarm-message flag is clear
  dynamicModelSet = false; // Make sure the dynamicModelSet flag is clear
  geofencesSet = false; // Make sure the geofencesSet flag is clear

  disableDebugging(); // Make sure the serial debug messages are disabled until the Serial port is open!

  EEPROM.init(); // Initialize the EEPROM

  // Initialise the tracker settings in RAM - before we enable the RTC
  initTrackerSettings(&myTrackerSettings);

  // If the EEPROM (Flash) already contains valid settings, they will always used - unless you
  // uncomment this line to reset the EEPROM with the default settings from Tracker_Message_Fields.h:
  //putTrackerSettings(&myTrackerSettings);

  // Check if the EEPROM data is valid (i.e. has already been initialised)
  if (checkEEPROM(&myTrackerSettings))
  {
    getTrackerSettings(&myTrackerSettings); // EEPROM data is valid so load it into RAM
  }
  else
  {
    putTrackerSettings(&myTrackerSettings); // EEPROM data is invalid so reset it with the default settings
  }

  wake_int = myTrackerSettings.WAKEINT.the_data; // Copy WAKEINT into wake_int (volatile for the ISR)

  // Set up the rtc for 1 second interrupts now that TXINT has been initialized
  /*
    0: Alarm interrupt disabled
    1: Alarm match every year   (hundredths, seconds, minutes, hour, day, month)
    2: Alarm match every month  (hundredths, seconds, minutes, hours, day)
    3: Alarm match every week   (hundredths, seconds, minutes, hours, weekday)
    4: Alarm match every day    (hundredths, seconds, minute, hours)
    5: Alarm match every hour   (hundredths, seconds, minutes)
    6: Alarm match every minute (hundredths, seconds)
    7: Alarm match every second (hundredths)
  */
  rtc.setAlarmMode(7); // Set the RTC alarm mode
  rtc.attachInterrupt(); // Attach RTC alarm interrupt  

}

void loop()
{
  // loop is one large switch/case that controls the sequencing of the code
  switch (loop_step) {

    // ************************************************************************************************
    // Initialise things
    case loop_init:
    {
      // Start the console serial port and send the welcome message
      Serial.begin(115200);
      Serial.println();
      Serial.println();
      Serial.println(F("Artemis Global Tracker"));
      Serial.print(F("Software Version: "));
      Serial.print((DEF_SWVER & 0xf0) >> 4); // DEF_SWVER is defined in Tracker_Message_Fields.h
      Serial.print(F("."));
      Serial.println(DEF_SWVER & 0x0f);
      Serial.println();
      Serial.println();

      //enableDebugging(Serial); // Uncomment this line to enable extra debug messages to Serial

      if (_printDebug == true)
      {
        // If debugging is enabled: print the tracker EEPROM contents as text
        Serial.println(F("EEPROM contents (remember that data is little endian!):"));
        displayEEPROMcontents();
        Serial.println();
      }

      // Print the tracker settings from RAM as text - if debugging is enabled
      printTrackerSettings(&myTrackerSettings);

      // Make sure the serial Rx buffer is empty - keep reading characters until available is false
      while (Serial.available() > 0)
      {
        Serial.read(); // Read a single character from the buffer and discard it
      }

      // Print the configuration message
      Serial.println(F("Ready to accept configuration settings via Serial..."));
      Serial.println();
      Serial.println();

      // Setup the IridiumSBD
      modem.setPowerProfile(IridiumSBD::USB_POWER_PROFILE); // Change power profile to "low current"

      loop_step = read_pressure; // Move on, check the PHT readings (in case we need to send an alarm message)
    }
    break; // End of case loop_init

    // ************************************************************************************************
    // Read the pressure and temperature from the MS8607
    case read_pressure:
    {
      Serial.println(F("Getting the PHT readings..."));

      agtWire.begin(); // Set up the I2C pins
      agtWire.setClock(100000); // Use 100kHz for best performance
      setAGTWirePullups(1); // MS8607 needs pull-ups

      bool barometricSensorOK;
      barometricSensorOK = barometricSensor.begin(agtWire); // Begin the PHT sensor
      if (barometricSensorOK == false)
      {
        // Send a warning message if we were unable to connect to the MS8607:
        Serial.println(F("*** Could not detect the MS8607 sensor. Trying again... ***"));
        barometricSensorOK = barometricSensor.begin(agtWire); // Re-begin the PHT sensor
        if (barometricSensorOK == false)
        {
          // Send a warning message if we were unable to connect to the MS8607:
          Serial.println(F("*** MS8607 sensor not detected at default I2C address ***"));
        }
      }

      if (barometricSensorOK == false) // If the sensor is not OK
      {
        // Set the pressure and temperature to default values
        myTrackerSettings.PRESS.the_data = DEF_PRESS;
        myTrackerSettings.TEMP.the_data = DEF_TEMP;
        myTrackerSettings.HUMID.the_data = DEF_HUMID;
      }
      else // The sensor is OK so let's read and print the PHT values
      {
        myTrackerSettings.PRESS.the_data = (uint16_t)(barometricSensor.getPressure()); // mbar
        myTrackerSettings.TEMP.the_data = (int16_t)(barometricSensor.getTemperature() * 100.0); // Convert to °C * 10^-2
        myTrackerSettings.HUMID.the_data = (uint16_t)(barometricSensor.getHumidity() * 100.0); // Convert to %RH * 10^-2

        Serial.print(F("Pressure (mbar): "));
        Serial.println(myTrackerSettings.PRESS.the_data);
        Serial.print(F("Temperature (C * 10^-2): "));
        Serial.println(myTrackerSettings.TEMP.the_data);
        Serial.print(F("Humidity (%RH * 10^-2): "));
        Serial.println(myTrackerSettings.HUMID.the_data);
      }

      alarmState = false; // Clear the alarm state
      alarmType = 0; // Clear the alarm type

      // Check if a PHT alarm has occurred
      if ( (myTrackerSettings.PRESS.the_data != DEF_PRESS) && // If measurement is valid
           (myTrackerSettings.PRESS.the_data > myTrackerSettings.HIPRESS.the_data) ) // Check if HIPRESS has been exceeded
      {
        alarmType |= ALARM_HIPRESS;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_HIPRESS) == FLAGS1_HIPRESS) // If the HIPRESS alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** HIPRESS Alarm! ***"));
        }
      }
      if ( (myTrackerSettings.PRESS.the_data != DEF_PRESS) && // If measurement is valid
           (myTrackerSettings.PRESS.the_data < myTrackerSettings.LOPRESS.the_data) ) // Check if LOPRESS has been exceeded
      {
        alarmType |= ALARM_LOPRESS;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_LOPRESS) == FLAGS1_LOPRESS) // If the LOPRESS alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** LOPRESS Alarm! ***"));
        }
      }
      if ( (myTrackerSettings.TEMP.the_data != DEF_TEMP) && // If measurement is valid
           (myTrackerSettings.TEMP.the_data > myTrackerSettings.HITEMP.the_data) ) // Check if HITEMP has been exceeded
      {
        alarmType |= ALARM_HITEMP;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_HITEMP) == FLAGS1_HITEMP) // If the HITEMP alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** HITEMP Alarm! ***"));
        }
      }
      if ( (myTrackerSettings.TEMP.the_data != DEF_TEMP) && // If measurement is valid
           (myTrackerSettings.TEMP.the_data < myTrackerSettings.LOTEMP.the_data) ) // Check if LOTEMP has been exceeded
      {
        alarmType |= ALARM_LOTEMP;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_LOTEMP) == FLAGS1_LOTEMP) // If the LOTEMP alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** LOTEMP Alarm! ***"));
        }
      }
      if ( (myTrackerSettings.HUMID.the_data != DEF_HUMID) && // If measurement is valid
           (myTrackerSettings.HUMID.the_data > myTrackerSettings.HIHUMID.the_data) ) // Check if HIHUMID has been exceeded
      {
        alarmType |= ALARM_HIHUMID;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_HIHUMID) == FLAGS1_HIHUMID) // If the HIHUMID alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** HIHUMID Alarm! ***"));
        }
      }
      if ( (myTrackerSettings.HUMID.the_data != DEF_HUMID) && // If measurement is valid
           (myTrackerSettings.HUMID.the_data < myTrackerSettings.LOHUMID.the_data) ) // Check if LOHUMID has been exceeded
      {
        alarmType |= ALARM_LOHUMID;
        if ((myTrackerSettings.FLAGS1 & FLAGS1_LOHUMID) == FLAGS1_LOHUMID) // If the LOHUMID alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** LOHUMID Alarm! ***"));
        }
      }
      // Check battery voltage
      // If voltage is lower than LOWBATT, send an alarm message
      get_vbat(); // Get the battery (bus) voltage
      if (myTrackerSettings.BATTV.the_data < myTrackerSettings.LOWBATT.the_data) {
        alarmType |= ALARM_LOBATT;
        Serial.print(F("*** LOWBATT: "));
        Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
        Serial.println(F("V ***"));
        if ((myTrackerSettings.FLAGS2 & FLAGS2_LOWBATT) == FLAGS2_LOWBATT) // If the LOWBATT alarm is enabled
        {
          alarmState = true;
          Serial.println(F("*** LOWBATT Alarm! ***"));
        }
      }
      // If an alarm has fired, execute alarm user function
      if (alarmType) {
        ALARM_FUNC(alarmType); // Execute alarm user function
      }

      loop_step = start_GPS; // Get ready to move on and start the GPS

      // Send a message:

      // If this is the first time around the loop
      if (firstTime == true)
      {
        firstTime = false; // Clear the firstTime flag
        seconds_since_last_alarmtx = 0; // Clear the time since the last alarm
        seconds_since_last_tx = 0; // Clear the time since the last routine transmit too (assumes ALARMINT is < TXINT)
      }
      // If a geofence alarm has occurred but only if geofence alarms are enabled and the alarmtx limit has been reached
      // otherwise we could be sending geofence alarm messages very frequently
      else if ((sendGeofenceAlarm == true)  && ((myTrackerSettings.FLAGS2 & FLAGS2_GEOFENCE) == FLAGS2_GEOFENCE))
      {
        if (seconds_since_last_alarmtx >= (myTrackerSettings.ALARMINT.the_data * 60))
        {
          Serial.println(F("*** Geofence Alarm! ***"));
          sendGeofenceAlarm = false; // Clear the send-a-geofence-alarm-message flag
          seconds_since_last_alarmtx = 0; // Clear the time since the last alarm
          seconds_since_last_tx = 0; // Clear the time since the last routine transmit too (assumes ALARMINT is < TXINT)
        }
        else
        {
          Serial.println(F("*** A Geofence Alarm was seen but it is too early to send the message! ***"));
          loop_step = zzz; // Go to sleep as it is too early to send a geofence alarm message
        }
      }
      // If we are monitoring the ring channel, either a ring indication was received or a WAKEINT interval_alarm occurred.
      // Either way, we want to do a full message send cycle so we can go back to monitoring the ring channel.
      // If we just Go To Sleep instead, we won't be monitoring the ring channel from now until the next transmit
      else if ((myTrackerSettings.FLAGS2 & FLAGS2_RING) == FLAGS2_RING)
      {
        Serial.println(F("*** Ring Alarm! ***"));
        seconds_since_last_alarmtx = 0; // Clear the time since the last alarm
        seconds_since_last_tx = 0; // Clear the time since the last routine transmit too (assumes ALARMINT is < TXINT)
      }
      // If we are in an alarm state and it is time for an alarm message
      else if ((alarmState == true) && (seconds_since_last_alarmtx >= (myTrackerSettings.ALARMINT.the_data * 60)))
      {
        Serial.println(F("Time for an alarm message!"));
        seconds_since_last_alarmtx = 0; // Clear the time since the last alarm
        seconds_since_last_tx = 0; // Clear the time since the last routine transmit too (assumes ALARMINT is < TXINT)
      }
      // Or if it is time for a routine transmit
      else if (seconds_since_last_tx >= (myTrackerSettings.TXINT.the_data * 60))
      {
        Serial.println(F("Time for a routine message."));
        seconds_since_last_tx = 0; // Clear the time since the last routine transmit      
      }

      // Else:

      // This is probably just a WAKEINT interval_alarm so let's go back to sleep now that we've checked the PHT readings
      else
      {
        loop_step = zzz; // Go to sleep
      }
    }
    break; // End of case read_pressure

    // ************************************************************************************************
    // Power up the GNSS (ZOE-M8Q)
    case start_GPS:
    {
      Serial.println(F("Powering up the GNSS..."));
      gnssON(); // Enable power for the GNSS

      // Give the GNSS 2secs to power up in a non-blocking way (so we can respond to config serial data as soon as it arrives)
      delayCount = 0;
      while ((delayCount < 2000) && (Serial.available() == 0))
      {
        delay(1);
        delayCount++;
      }
      // Check battery voltage now we are drawing current for the GPS
      get_vbat(); // Get the battery (bus) voltage
      if (battVlow() == true) {
        // If voltage is low, turn off the GNSS and go to sleep
        Serial.print(F("*** LOW VOLTAGE (start_GPS) "));
        Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
        Serial.println(F("V ***"));
        gnssOFF(); // Disable power for the GNSS
        loop_step = zzz; // Go to sleep
      }
      
      else // If the battery voltage is OK
      {
        setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance
        if (myGPS.begin(agtWire) == false) //Connect to the Ublox module using Wire port
        {
          // If we were unable to connect to the ZOE-M8Q:
          
          // Send a warning message
          Serial.println(F("*** Ublox GPS not detected at default I2C address ***"));
          
          // Set the lat, long etc. to default values
          myTrackerSettings.YEAR.the_data = DEF_YEAR;
          myTrackerSettings.MONTH = DEF_MONTH;
          myTrackerSettings.DAY = DEF_DAY;
          myTrackerSettings.HOUR = DEF_HOUR;
          myTrackerSettings.MIN = DEF_MIN;
          myTrackerSettings.SEC = DEF_SEC;
          myTrackerSettings.MILLIS.the_data = DEF_MILLIS;
          myTrackerSettings.LAT.the_data = DEF_LAT;
          myTrackerSettings.LON.the_data = DEF_LON;
          myTrackerSettings.ALT.the_data = DEF_ALT;
          myTrackerSettings.SPEED.the_data = DEF_SPEED;
          myTrackerSettings.HEAD.the_data = DEF_HEAD;
          myTrackerSettings.SATS = DEF_SATS;
          myTrackerSettings.PDOP.the_data = DEF_PDOP;
          myTrackerSettings.FIX = DEF_FIX;
          myTrackerSettings.GEOFSTAT[0] = DEF_GEOFSTAT;
          myTrackerSettings.GEOFSTAT[1] = DEF_GEOFSTAT;
          myTrackerSettings.GEOFSTAT[2] = DEF_GEOFSTAT;
  
          // Power down the GNSS
          gnssOFF(); // Disable power for the GNSS
  
          loop_step = start_LTC3225; // Move on, skip reading the GNSS fix
        }

        else // If the GNSS started up OK
        {
          
          //myGPS.enableDebugging(); // Enable debug messages
          myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

          // Change the dynamic platform model.
          // Possible values are:
          // 0(PORTABLE),2(STATIONARY),3(PEDESTRIAN),4(AUTOMOTIVE),5(SEA),6(AIRBORNE1g),7(AIRBORNE2g),8(AIRBORNE4g),9(WRIST),10(BIKE)

          if (dynamicModelSet == false)
          {
            if (myGPS.setDynamicModel(myTrackerSettings.DYNMODEL) == false)
            {
              Serial.println(F("*** Warning: setDynamicModel may have failed ***"));
            }
            else
            {
              dynamicModelSet = true; // Set the flag so we don't try to set the dynamic model again
              if (myGPS.saveConfigSelective(VAL_CFG_SUBSEC_NAVCONF) == true) // Save the NAV configuration to BBR
              {
                Serial.println(F("Dynamic Model updated and saved to BBR"));
              }
              else
              {
                Serial.println(F("*** Warning: saving the NAVCONF to BBR may have failed ***"));
              }
            }
          }

          // Set the geofences.
          if (geofencesSet == false) // If we have not set the geofences already
          {
            byte numf = (myTrackerSettings.GEOFNUM & 0xf0) >> 4; // Extract the number of geofences
            byte conf = myTrackerSettings.GEOFNUM & 0x0f; // Extract the confidence level
            // Always call clearGeofences() to clear all existing geofences.
            // We need to do this if numf is greater than zero to be able to define new geofences,
            // but we also want to do it if numf is zero to erase any previous geofences.
            Serial.print(F("Clearing any existing geofences. clearGeofences returned: "));
            Serial.println(myGPS.clearGeofences());              
            byte pinPolarity; // Define the pin polarity depending on whether FLAGS2_INSIDE is set
            if ((myTrackerSettings.FLAGS2 & FLAGS2_INSIDE) == FLAGS2_INSIDE) // If the INSIDE bit is set
            {
              pinPolarity = 0; // Set the PIO pin polarity to 0: the PIO pin will be low when inside the combined geofence; unknown state is always high
            }
            else
            {
              pinPolarity = 1; // Set the PIO pin polarity to 1: the PIO pin will be low when outside the combined geofence; unknown state is always high
            }
            byte pinNum = 14; // ZOE-M8Q PIO14 is connected to the geofencePin
  
            if (numf > 0) // If the number of geofences is not zero
            {
              if (numf >= 1)
              {
                // It is possible to define up to four geofences.
                // Call addGeofence up to four times to define them.
                Serial.println(F("Setting the geofences:"));
    
                Serial.print(F("addGeofence for geofence 1 returned: "));
                Serial.println(myGPS.addGeofence(myTrackerSettings.GEOF1LAT.the_data, myTrackerSettings.GEOF1LON.the_data, myTrackerSettings.GEOF1RAD.the_data, conf, pinPolarity, pinNum));
              }
              if (numf >= 2)
              {
                Serial.print(F("addGeofence for geofence 2 returned: "));
                Serial.println(myGPS.addGeofence(myTrackerSettings.GEOF2LAT.the_data, myTrackerSettings.GEOF2LON.the_data, myTrackerSettings.GEOF2RAD.the_data, conf, pinPolarity, pinNum));
              }            
              if (numf >= 3)
              {
                Serial.print(F("addGeofence for geofence 3 returned: "));
                Serial.println(myGPS.addGeofence(myTrackerSettings.GEOF3LAT.the_data, myTrackerSettings.GEOF3LON.the_data, myTrackerSettings.GEOF3RAD.the_data, conf, pinPolarity, pinNum));
              }            
              if (numf >= 4)
              {
                Serial.print(F("addGeofence for geofence 4 returned: "));
                Serial.println(myGPS.addGeofence(myTrackerSettings.GEOF4LAT.the_data, myTrackerSettings.GEOF4LON.the_data, myTrackerSettings.GEOF4RAD.the_data, conf, pinPolarity, pinNum));
              }            
            }
            geofencesSet = true; // Set the flag so we don't try to set the geofences again
            if (myGPS.saveConfigSelective(VAL_CFG_SUBSEC_NAVCONF) == true) // Save the NAV configuration to BBR
            {
              Serial.println(F("NAVCONF saved to BBR"));
            }
            else
            {
              Serial.println(F("*** Warning: saving the NAVCONF to BBR may have failed ***"));
            }
          }
          loop_step = read_GPS; // Move on, read the GNSS fix
        }
      }

      // Check if any serial data has arrived telling us to go into configure
      if (Serial.available() > 0) // Has any serial data arrived?
      {
        gnssOFF(); // Disable power for the GNSS
        last_loop_step = start_GPS; // Let's start the GPS again when leaving configure
        loop_step = configureMe; // Start the configure
      }
    }
    break; // End of case start_GPS

    // ************************************************************************************************
    // Read a fix from the ZOE-M8Q
    case read_GPS:
    {
      Serial.println(F("Waiting for a 3D GNSS fix..."));

      myTrackerSettings.FIX = 0; // Clear the fix type
      
#if !defined(noTX) || !defined(skipGNSS)
      // Look for GPS signal for up to GNSS_timeout minutes
      // Stop when we get a 3D fix, or we timeout, or if any serial data arrives (telling us to go into configure)
      for (unsigned long tnow = millis(); (myTrackerSettings.FIX != 3) && (millis() - tnow < GNSS_timeout * 60UL * 1000UL) && (Serial.available() == 0);)
      {
      
        myTrackerSettings.FIX = myGPS.getFixType(); // Get the GNSS fix type
        
        // Check battery voltage now we are drawing current for the GPS
        // If voltage is lower than 0.2V below LOWBATT, stop looking for GNSS and go to sleep
        get_vbat();
        if (battVlow() == true) {
          break; // Exit the for loop now
        }

        // Flash the LED at 1Hz
        if ((millis() / 1000) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }

        // Delay for 100msec in a non-blocking way so we don't pound the I2C bus too hard!
        delayCount = 0;
        while ((delayCount < 100) && (Serial.available() == 0))
        {
          delay(1);
          delayCount++;
        }

      }
#endif

      // If voltage is low then go straight to sleep
      if (battVlow() == true) {
        Serial.print(F("*** LOW VOLTAGE (read_GPS) "));
        Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
        Serial.println(F("V ***"));
        
        loop_step = zzz;
      }

      else if (myTrackerSettings.FIX == 3) // Check if we got a valid 3D fix
      {
        // Get the time and position etc.
        // Get the time first to hopefully avoid second roll-over problems
        myTrackerSettings.MILLIS.the_data = myGPS.getMillisecond();
        myTrackerSettings.SEC = myGPS.getSecond();
        myTrackerSettings.MIN = myGPS.getMinute();
        myTrackerSettings.HOUR = myGPS.getHour();
        myTrackerSettings.DAY = myGPS.getDay();
        myTrackerSettings.MONTH = myGPS.getMonth();
        myTrackerSettings.YEAR.the_data = myGPS.getYear(); // Get the year
        myTrackerSettings.LAT.the_data = myGPS.getLatitude(); // Get the latitude in degrees * 10^-7
        myTrackerSettings.LON.the_data = myGPS.getLongitude(); // Get the longitude in degrees * 10^-7
        myTrackerSettings.ALT.the_data = myGPS.getAltitudeMSL(); // Get the altitude above Mean Sea Level in mm
        myTrackerSettings.SPEED.the_data = myGPS.getGroundSpeed(); // Get the ground speed in mm/s
        myTrackerSettings.SATS = myGPS.getSIV(); // Get the number of satellites used in the fix
        myTrackerSettings.HEAD.the_data = myGPS.getHeading(); // Get the heading in degrees * 10^-7
        myTrackerSettings.PDOP.the_data = myGPS.getPDOP(); // Get the PDOP in cm
        geofenceState currentGeofenceState; // Create storage for the geofence state
        myGPS.getGeofenceState(currentGeofenceState); // Get the geofence state
        myTrackerSettings.GEOFSTAT[0] = ((currentGeofenceState.status) << 4) | (currentGeofenceState.combState); // Store the status and the combined state
        myTrackerSettings.GEOFSTAT[1] = ((currentGeofenceState.states[0]) << 4) | (currentGeofenceState.states[1]); // Store the individual geofence states
        myTrackerSettings.GEOFSTAT[2] = ((currentGeofenceState.states[2]) << 4) | (currentGeofenceState.states[3]);

        Serial.println(F("A 3D fix was found!"));
        Serial.print(F("Latitude (degrees * 10^-7): ")); Serial.println(myTrackerSettings.LAT.the_data);
        Serial.print(F("Longitude (degrees * 10^-7): ")); Serial.println(myTrackerSettings.LON.the_data);
        Serial.print(F("Altitude (mm): ")); Serial.println(myTrackerSettings.ALT.the_data);

        loop_step = start_LTC3225; // Move on, start the supercap charger
      }
      
      else
      {
        // We didn't get a 3D fix so
        // set the lat, long etc. to default values
        myTrackerSettings.YEAR.the_data = DEF_YEAR;
        myTrackerSettings.MONTH = DEF_MONTH;
        myTrackerSettings.DAY = DEF_DAY;
        myTrackerSettings.HOUR = DEF_HOUR;
        myTrackerSettings.MIN = DEF_MIN;
        myTrackerSettings.SEC = DEF_SEC;
        myTrackerSettings.MILLIS.the_data = DEF_MILLIS;
        myTrackerSettings.LAT.the_data = DEF_LAT;
        myTrackerSettings.LON.the_data = DEF_LON;
        myTrackerSettings.ALT.the_data = DEF_ALT;
        myTrackerSettings.SPEED.the_data = DEF_SPEED;
        myTrackerSettings.HEAD.the_data = DEF_HEAD;
        myTrackerSettings.SATS = DEF_SATS;
        myTrackerSettings.PDOP.the_data = DEF_PDOP;
        myTrackerSettings.FIX = DEF_FIX;
        myTrackerSettings.GEOFSTAT[0] = DEF_GEOFSTAT;
        myTrackerSettings.GEOFSTAT[1] = DEF_GEOFSTAT;
        myTrackerSettings.GEOFSTAT[2] = DEF_GEOFSTAT;

        Serial.println(F("A 3D fix was NOT found!"));
        Serial.println(F("Using default values..."));

        loop_step = start_LTC3225; // Move on, start the supercap charger
      }

      // Power down the GNSS
      Serial.println(F("Powering down the GNSS..."));
      gnssOFF(); // Disable power for the GNSS

      // Check if any serial data has arrived telling us to go into configure
      if (Serial.available() > 0) // Has any serial data arrived?
      {
        last_loop_step = start_GPS; // Let's read the GPS again when leaving configure
        loop_step = configureMe; // Start the configure
      }
    }
    break; // End of case read_GPS

    // ************************************************************************************************
    // Start the LTC3225 supercapacitor charger
    case start_LTC3225:
    {
      // Enable the supercapacitor charger
      Serial.println(F("Enabling the supercapacitor charger..."));
      digitalWrite(superCapChgEN, HIGH); // Enable the super capacitor charger

      Serial.println(F("Waiting for supercapacitors to charge..."));
      // Give the supercap charger 2secs to power up in a non-blocking way (so we can respond to config serial data as soon as it arrives)
      delayCount = 0;
      while ((delayCount < 2000) && (Serial.available() == 0))
      {
        delay(1);
        delayCount++;
      }

      PGOOD = false; // Flag to show if PGOOD is HIGH
      
      // Wait for PGOOD to go HIGH for up to CHG_timeout minutes
      // Stop when PGOOD goes high, or we timeout, or if any serial data arrives (telling us to go into configure)
      for (unsigned long tnow = millis(); (!PGOOD) && (millis() - tnow < CHG_timeout * 60UL * 1000UL) && (Serial.available() == 0);)
      {
      
        PGOOD = digitalRead(superCapPGOOD); // Read the PGOOD pin
        
        // Check battery voltage now we are drawing current for the LTC3225
        // If voltage is low, stop charging and go to sleep
        get_vbat();
        if (battVlow() == true) {
          break;
        }

        // Flash the LED at 2Hz
        if ((millis() / 500) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }

        // Delay for 100msec in a non-blocking way so we don't pound the I2C bus too hard!
        delayCount = 0;
        while ((delayCount < 100) && (Serial.available() == 0))
        {
          delay(1);
          delayCount++;
        }

      }

      // If voltage is low then go straight to sleep
      if (battVlow() == true) {
        Serial.print(F("*** LOW VOLTAGE (start_LTC3225) "));
        Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
        Serial.println(F("V ***"));
        
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
        Serial.println(F("*** Supercapacitors failed to charge ***"));

        loop_step = zzz;
      }
  
      // Check if any serial data has arrived telling us to go into configure
      if (Serial.available() > 0) // Has any serial data arrived?
      {
        digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
        last_loop_step = start_LTC3225; // Let's charge the capacitors again when leaving configure
        loop_step = configureMe; // Start the configure
      }
    }
    break; // End of case start_LTC3225

    // ************************************************************************************************
    // Give the super capacitors some extra time to charge
    case wait_LTC3225:
    {
      Serial.println(F("Giving the supercapacitors extra time to charge..."));
 
      // Wait for TOPUP_timeout seconds, keep checking PGOOD and the battery voltage
      for (unsigned long tnow = millis(); (millis() - tnow < TOPUP_timeout * 1000UL) && (Serial.available() == 0);)
      {
      
        // Check battery voltage now we are drawing current for the LTC3225
        // If voltage is low, stop charging and go to sleep
        get_vbat();
        if (battVlow() == true) {
          break;
        }

        // Flash the LED at 2Hz
        if ((millis() / 500) % 2 == 1) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }

        // Delay for 100msec in a non-blocking way so we don't pound the I2C bus too hard!
        delayCount = 0;
        while ((delayCount < 100) && (Serial.available() == 0))
        {
          delay(1);
          delayCount++;
        }

      }

      // If voltage is low then go straight to sleep
      if (battVlow() == true) {
        Serial.print(F("*** LOW VOLTAGE (wait_LTC3225) "));
        Serial.print((((float)myTrackerSettings.BATTV.the_data)/100.0),2);
        Serial.println(F("V ***"));
        
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
        Serial.println(F("*** Supercapacitors failed to hold charge in wait_LTC3225 ***"));

        loop_step = zzz;
      }
  
      // Check if any serial data has arrived telling us to go into configure
      // (This is the last time we'll do this. We'll ignore any serial data once the 9603 is enabled.)
      if (Serial.available() > 0) // Has any serial data arrived?
      {
        digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
        last_loop_step = start_LTC3225; // Let's charge the capacitors again when leaving configure
        loop_step = configureMe; // Start the configure
      }
    }
    break; // End of case wait_LTC3225
      
    // ************************************************************************************************
    // Enable the 9603N and attempt to send a message
    case start_9603:
    {
      // Enable power for the 9603N
      Serial.println(F("Enabling 9603N power..."));
      digitalWrite(iridiumPwrEN, HIGH); // Enable Iridium Power
      delay(1000);

      // Relax timing constraints waiting for the supercap to recharge.
      modem.setPowerProfile(IridiumSBD::USB_POWER_PROFILE);

      // Begin satellite modem operation
      // Also begin the serial port connected to the satellite modem via IridiumSBD::beginSerialPort
      Serial.println(F("Starting modem..."));
      err = modem.begin();

      // Check if the modem started correctly
      if (err != ISBD_SUCCESS)
      {
        // If the modem failed to start, disable it and go to sleep
        Serial.print(F("*** modem.begin failed with error "));
        Serial.print(err);
        Serial.println(F(" ***"));
        loop_step = zzz;
      }

      else
      {
        // The modem started OK so let's try to send the message
        size_t outBufferSize = MOLIM + 25; // Define the size of outBuffer (25 larger than the MO limit, just in case we go over the end of the buffer)
        char outBuffer[outBufferSize]; // Use outBuffer to store the message.
        uint8_t outBufferBinary[outBufferSize]; // Storage for the binary format message (the IridiumSBD library needs this...)
        size_t outBufferPtr = 0; // outBuffer pointer

        // Clear the text outBuffer (fill it with 0x00) to make it easy to find the end of each field as we add them
        for (size_t i = 0; i < outBufferSize; i++)
        {
          outBuffer[i] = 0x00;
        }

        // Check if we are sending a text or a binary message
        if ((myTrackerSettings.FLAGS1 & FLAGS1_BINARY) != FLAGS1_BINARY) // if bit 7 of FLAGS1 is clear we are sending text
        { // Construct the text message

// ##################################################
// Start of text message construction - the next ~430 lines construct the text message (they should really be in a separate .ino!)
// ##################################################

          // Check if we need to include a RockBLOCK gateway header (DEST)
          if ((myTrackerSettings.FLAGS1 & FLAGS1_DEST) == FLAGS1_DEST) // if bit 6 of FLAGS1 is set we need to add RB DEST first
          {
            char destStr[8];
            if (myTrackerSettings.DEST.the_data < 10)
              sprintf(destStr, "000000%d", myTrackerSettings.DEST.the_data);
            else if (myTrackerSettings.DEST.the_data < 100)
              sprintf(destStr, "00000%d", myTrackerSettings.DEST.the_data);
            else if (myTrackerSettings.DEST.the_data < 1000)
              sprintf(destStr, "0000%d", myTrackerSettings.DEST.the_data);
            else if (myTrackerSettings.DEST.the_data < 10000)
              sprintf(destStr, "000%d", myTrackerSettings.DEST.the_data);
            else if (myTrackerSettings.DEST.the_data < 100000)
              sprintf(destStr, "00%d", myTrackerSettings.DEST.the_data);
            else if (myTrackerSettings.DEST.the_data < 1000000)
              sprintf(destStr, "0%d", myTrackerSettings.DEST.the_data);
            else
              sprintf(destStr, "%d", myTrackerSettings.DEST.the_data);
        
            sprintf(outBuffer, "RB%s,", destStr); // Add RB DEST , to outBuffer
            outBufferPtr += 10; // increment the pointer by 10
          }

          // ---------- MOFIELDS0 ----------
          
          // Check the relevant bits of MOFIELDS[0]
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SWVER) ==  MOFIELDS0_SWVER) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d.%d,", ((myTrackerSettings.SWVER & 0xf0) >> 4), (myTrackerSettings.SWVER & 0x0f)); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SOURCE) == MOFIELDS0_SOURCE) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.SOURCE.the_data); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_BATTV) == MOFIELDS0_BATTV) // If the bit is set
          {
            char temp_str[6]; // temporary string
            ftoa((((float)myTrackerSettings.BATTV.the_data) / 100.0),temp_str,2,6); // Convert to V
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_PRESS) == MOFIELDS0_PRESS) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.PRESS.the_data); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_TEMP) == MOFIELDS0_TEMP) // If the bit is set
          {
            char temp_str[8]; // temporary string
            ftoa((((float)myTrackerSettings.TEMP.the_data) / 100.0),temp_str,2,8); // Convert to C
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HUMID) == MOFIELDS0_HUMID) // If the bit is set
          {
            char temp_str[8]; // temporary string
            ftoa((((float)myTrackerSettings.HUMID.the_data) / 100.0),temp_str,2,8); // Convert to %RH
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_YEAR) == MOFIELDS0_YEAR) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%4d,", myTrackerSettings.YEAR.the_data); // Add the field to outBuffer
            outBufferPtr += 5; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MONTH) == MOFIELDS0_MONTH) // If the bit is set
          {
            if (myTrackerSettings.MONTH < 10) 
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.MONTH); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.MONTH); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_DAY) == MOFIELDS0_DAY) // If the bit is set
          {
            if (myTrackerSettings.DAY < 10)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.DAY); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.DAY); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HOUR) == MOFIELDS0_HOUR) // If the bit is set
          {
            if (myTrackerSettings.HOUR < 10)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.HOUR); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.HOUR); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MIN) == MOFIELDS0_MIN) // If the bit is set
          {
            if (myTrackerSettings.MIN < 10)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.MIN); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.MIN); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SEC) == MOFIELDS0_SEC) // If the bit is set
          {
            if (myTrackerSettings.SEC < 10)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.SEC); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.SEC); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MILLIS) == MOFIELDS0_MILLIS) // If the bit is set
          {
            if (myTrackerSettings.MILLIS.the_data < 10)
              sprintf(outBuffer+outBufferPtr, "00%d,", myTrackerSettings.MILLIS.the_data); // Add the field to outBuffer
            else if (myTrackerSettings.MILLIS.the_data < 100)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.MILLIS.the_data); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.MILLIS.the_data); // Add the field to outBuffer
            outBufferPtr += 4;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_DATETIME) == MOFIELDS0_DATETIME) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%4d", myTrackerSettings.YEAR.the_data); // Add the field to outBuffer
            outBufferPtr += 4; // increment the pointer
            if (myTrackerSettings.MONTH < 10) 
              sprintf(outBuffer+outBufferPtr, "0%d", myTrackerSettings.MONTH); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d", myTrackerSettings.MONTH); // Add the field to outBuffer
            outBufferPtr += 2;
            if (myTrackerSettings.DAY < 10)
              sprintf(outBuffer+outBufferPtr, "0%d", myTrackerSettings.DAY); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d", myTrackerSettings.DAY); // Add the field to outBuffer
            outBufferPtr += 2;
            if (myTrackerSettings.HOUR < 10)
              sprintf(outBuffer+outBufferPtr, "0%d", myTrackerSettings.HOUR); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d", myTrackerSettings.HOUR); // Add the field to outBuffer
            outBufferPtr += 2;
            if (myTrackerSettings.MIN < 10)
              sprintf(outBuffer+outBufferPtr, "0%d", myTrackerSettings.MIN); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d", myTrackerSettings.MIN); // Add the field to outBuffer
            outBufferPtr += 2;
            if (myTrackerSettings.SEC < 10)
              sprintf(outBuffer+outBufferPtr, "0%d,", myTrackerSettings.SEC); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.SEC); // Add the field to outBuffer
            outBufferPtr += 3;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_LAT) == MOFIELDS0_LAT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.LAT.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_LON) == MOFIELDS0_LON) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.LON.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_ALT) == MOFIELDS0_ALT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.ALT.the_data) / 1000.0),temp_str,3,16); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SPEED) == MOFIELDS0_SPEED) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.SPEED.the_data) / 1000.0),temp_str,3,10); // Convert to m/s
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HEAD) == MOFIELDS0_HEAD) // If the bit is set
          {
            char temp_str[8]; // temporary string
            ftoa((((float)myTrackerSettings.HEAD.the_data) / 10000000.0),temp_str,1,8); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SATS) == MOFIELDS0_SATS) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.SATS); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_PDOP) == MOFIELDS0_PDOP) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.PDOP.the_data) / 100.0),temp_str,2,10); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_FIX) == MOFIELDS0_FIX) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%1d,", myTrackerSettings.FIX); // Add the field to outBuffer
            outBufferPtr += 2; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_GEOFSTAT) == MOFIELDS0_GEOFSTAT) // If the bit is set
          {
            if (myTrackerSettings.GEOFSTAT[0] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.GEOFSTAT[0]); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.GEOFSTAT[0]); // Add the field to outBuffer
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.GEOFSTAT[1] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.GEOFSTAT[1]); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.GEOFSTAT[1]); // Add the field to outBuffer
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.GEOFSTAT[2] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X,", myTrackerSettings.GEOFSTAT[2]); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%X,", myTrackerSettings.GEOFSTAT[2]); // Add the field to outBuffer
            outBufferPtr += 3; // increment the pointer
          }

          // ---------- MOFIELDS1 ----------

          // USERVAL1-8: these call the function and append the returned value to the message
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL1) == MOFIELDS1_USERVAL1) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_1()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL2) == MOFIELDS1_USERVAL2) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_2()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL3) == MOFIELDS1_USERVAL3) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_3()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL4) == MOFIELDS1_USERVAL4) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_4()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL5) == MOFIELDS1_USERVAL5) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_5()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL6) == MOFIELDS1_USERVAL6) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", USER_VAL_6()); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL7) == MOFIELDS1_USERVAL7) // If the bit is set
          {
            char temp_str[20]; // temporary string
            ftoa((USER_VAL_7()),temp_str,3,20);
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL8) == MOFIELDS1_USERVAL8) // If the bit is set
          {
            char temp_str[20]; // temporary string
            ftoa((USER_VAL_8()),temp_str,3,20);
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_MOFIELDS) == MOFIELDS1_MOFIELDS) // If the bit is set
          {
            if (myTrackerSettings.MOFIELDS[0].the_bytes[0] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[0].the_bytes[0]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[0].the_bytes[0]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[0].the_bytes[1] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[0].the_bytes[1]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[0].the_bytes[1]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[0].the_bytes[2] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[0].the_bytes[2]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[0].the_bytes[2]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[0].the_bytes[3] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[0].the_bytes[3]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[0].the_bytes[3]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[1].the_bytes[0] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[1].the_bytes[0]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[1].the_bytes[0]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[1].the_bytes[1] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[1].the_bytes[1]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[1].the_bytes[1]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[1].the_bytes[2] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[1].the_bytes[2]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[1].the_bytes[2]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[1].the_bytes[3] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[1].the_bytes[3]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[1].the_bytes[3]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[2].the_bytes[0] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[2].the_bytes[0]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[2].the_bytes[0]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[2].the_bytes[1] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[2].the_bytes[1]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[2].the_bytes[1]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[2].the_bytes[2] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X", myTrackerSettings.MOFIELDS[2].the_bytes[2]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X", myTrackerSettings.MOFIELDS[2].the_bytes[2]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 2; // increment the pointer
            if (myTrackerSettings.MOFIELDS[2].the_bytes[3] < 16)
              sprintf(outBuffer+outBufferPtr, "0%X,", myTrackerSettings.MOFIELDS[2].the_bytes[3]); // Add the field to outBuffer (little endian!)
            else
              sprintf(outBuffer+outBufferPtr, "%X,", myTrackerSettings.MOFIELDS[2].the_bytes[3]); // Add the field to outBuffer (little endian!)
            outBufferPtr += 3; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_FLAGS1) == MOFIELDS1_FLAGS1) // If the bit is set
          {
            if (myTrackerSettings.FLAGS1 < 16)
              sprintf(outBuffer+outBufferPtr, "0%X,", myTrackerSettings.FLAGS1); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%X,", myTrackerSettings.FLAGS1); // Add the field to outBuffer
            outBufferPtr += 3; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_FLAGS2) == MOFIELDS1_FLAGS2) // If the bit is set
          {
            if (myTrackerSettings.FLAGS2)
              sprintf(outBuffer+outBufferPtr, "0%X,", myTrackerSettings.FLAGS2); // Add the field to outBuffer
            else
              sprintf(outBuffer+outBufferPtr, "%X,", myTrackerSettings.FLAGS2); // Add the field to outBuffer
            outBufferPtr += 3; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_DEST) == MOFIELDS1_DEST) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.DEST.the_data); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HIPRESS) == MOFIELDS1_HIPRESS) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.HIPRESS.the_data); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOPRESS) == MOFIELDS1_LOPRESS) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.LOPRESS.the_data); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HITEMP) == MOFIELDS1_HITEMP) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.HITEMP.the_data) / 100.0),temp_str,2,10); // Convert to C
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOTEMP) == MOFIELDS1_LOTEMP) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.LOTEMP.the_data) / 100.0),temp_str,2,10); // Convert to C
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HIHUMID) == MOFIELDS1_HIHUMID) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.HIHUMID.the_data) / 100.0),temp_str,2,10); // Convert to %RH
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOHUMID) == MOFIELDS1_LOHUMID) // If the bit is set
          {
            char temp_str[10]; // temporary string
            ftoa((((float)myTrackerSettings.LOHUMID.the_data) / 100.0),temp_str,2,10); // Convert to %RH
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++; // increment the pointer
          }

          // From ~here we need to check that we have not exceeded the MO buffer length.
          // If we have, any remaining message fields will be ignored.
          
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOFNUM) ==  MOFIELDS1_GEOFNUM) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d.%d,", ((myTrackerSettings.GEOFNUM & 0xf0) >> 4), (myTrackerSettings.GEOFNUM & 0x0f)); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-4)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1LAT) == MOFIELDS1_GEOF1LAT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF1LAT.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-12)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1LON) == MOFIELDS1_GEOF1LON) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF1LON.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-13)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1RAD) == MOFIELDS1_GEOF1RAD) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF1RAD.the_data) / 100.0),temp_str,2,16); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-10)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF2LAT) == MOFIELDS1_GEOF2LAT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF2LAT.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-12)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF2LON) == MOFIELDS1_GEOF2LON) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF2LON.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-13)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }

          // ---------- MOFIELDS2 ----------

          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF2RAD) == MOFIELDS2_GEOF2RAD) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF2RAD.the_data) / 100.0),temp_str,2,16); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-10)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3LAT) == MOFIELDS2_GEOF3LAT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF3LAT.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-12)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3LON) == MOFIELDS2_GEOF3LON) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF3LON.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-13)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3RAD) == MOFIELDS2_GEOF3RAD) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF3RAD.the_data) / 100.0),temp_str,2,16); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-10)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4LAT) == MOFIELDS2_GEOF4LAT) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF4LAT.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-12)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4LON) == MOFIELDS2_GEOF4LON) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF4LON.the_data) / 10000000.0),temp_str,7,16); // Convert to degrees
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-13)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4RAD) == MOFIELDS2_GEOF4RAD) // If the bit is set
          {
            char temp_str[16]; // temporary string
            ftoa((((float)myTrackerSettings.GEOF4RAD.the_data) / 100.0),temp_str,2,16); // Convert to m
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-10)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_WAKEINT) == MOFIELDS2_WAKEINT) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.WAKEINT.the_data); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-6)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room (maximum is 86400)
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_ALARMINT) == MOFIELDS2_ALARMINT) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.ALARMINT.the_data); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-5)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room (maximum is 1440)
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_TXINT) == MOFIELDS2_TXINT) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.TXINT.the_data); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-5)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room (maximum is 1440)
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_LOWBATT) == MOFIELDS2_LOWBATT) // If the bit is set
          {
            char temp_str[8]; // temporary string
            ftoa((((float)myTrackerSettings.LOWBATT.the_data) / 100.0),temp_str,2,8); // Convert to V
            sprintf(outBuffer+outBufferPtr, "%s,", temp_str); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-5)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_DYNMODEL) == MOFIELDS2_DYNMODEL) // If the bit is set
          {
            sprintf(outBuffer+outBufferPtr, "%d,", myTrackerSettings.DYNMODEL); // Add the field to outBuffer
            if (outBufferPtr < (MOLIM-3)) {while (outBuffer[outBufferPtr] != 0x00) outBufferPtr++;} // increment the pointer if there is room
            else {outBuffer[outBufferPtr] = 0x00;} // if there is not enough room, ignore this message (set the first character to NULL)
          }
         
// ##################################################
// End of text message construction
// ##################################################

          // Remove the final comma
          outBufferPtr -= 1; // Reduce the pointer by one to point at the final comma
          outBuffer[outBufferPtr] = 0x00; // Clear the comma
          
          // Print the message
          Serial.print(F("Text message is '"));
          Serial.print(outBuffer);
          Serial.println(F("'"));
        
        } // End of text message construction

        else // we are sending binary
        {
                    
// ##################################################
// Start of binary message construction - the next ~470 lines construct the binary message (they should really be in a separate .ino!)
// ##################################################

          // Check if we need to include a RockBLOCK gateway header (DEST)
          if ((myTrackerSettings.FLAGS1 & FLAGS1_DEST) == FLAGS1_DEST) // if bit 6 of FLAGS1 is set we need to add RB DEST first
          {
            outBufferBinary[outBufferPtr++] = 0x52; // Add the 'R'
            outBufferBinary[outBufferPtr++] = 0x42; // Add the 'B'
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[2]; // Add the DEST
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[0];
          }

          size_t stxLoc = outBufferPtr; // Use this to record the location of the STX
          outBufferBinary[outBufferPtr++] = STX; // Add the STX

          // ---------- MOFIELDS0 ----------
          
          // Check the relevant bits of MOFIELDS[0]
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SWVER) == MOFIELDS0_SWVER) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = SWVER; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SWVER; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SOURCE) == MOFIELDS0_SOURCE) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = SOURCE; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SOURCE.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SOURCE.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SOURCE.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SOURCE.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_BATTV) == MOFIELDS0_BATTV) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = BATTV; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.BATTV.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.BATTV.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_PRESS) == MOFIELDS0_PRESS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = PRESS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.PRESS.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.PRESS.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_TEMP) == MOFIELDS0_TEMP) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = TEMP; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.TEMP.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.TEMP.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HUMID) == MOFIELDS0_HUMID) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HUMID; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HUMID.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HUMID.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_YEAR) == MOFIELDS0_YEAR) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = YEAR; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.YEAR.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.YEAR.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MONTH) == MOFIELDS0_MONTH) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = MONTH; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MONTH; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_DAY) == MOFIELDS0_DAY) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = DAY; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DAY; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HOUR) == MOFIELDS0_HOUR) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HOUR; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HOUR; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MIN) == MOFIELDS0_MIN) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = MIN; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MIN; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SEC) == MOFIELDS0_SEC) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = SEC; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SEC; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_MILLIS) == MOFIELDS0_MILLIS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = MILLIS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MILLIS.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MILLIS.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_DATETIME) == MOFIELDS0_DATETIME) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = DATETIME; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.YEAR.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.YEAR.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MONTH;
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DAY;
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HOUR;
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MIN;
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SEC;
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_LAT) == MOFIELDS0_LAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LAT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LAT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LAT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LAT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_LON) == MOFIELDS0_LON) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LON; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LON.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LON.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LON.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LON.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_ALT) == MOFIELDS0_ALT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = ALT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SPEED) == MOFIELDS0_SPEED) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = SPEED; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SPEED.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SPEED.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SPEED.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SPEED.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_HEAD) == MOFIELDS0_HEAD) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HEAD; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HEAD.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HEAD.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HEAD.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HEAD.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_SATS) == MOFIELDS0_SATS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = SATS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.SATS; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_PDOP) == MOFIELDS0_PDOP) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = PDOP; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.PDOP.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.PDOP.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_FIX) == MOFIELDS0_FIX) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = FIX; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.FIX; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[0].the_data & MOFIELDS0_GEOFSTAT) == MOFIELDS0_GEOFSTAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOFSTAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOFSTAT[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOFSTAT[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOFSTAT[2];
          }

          // ---------- MOFIELDS1 ----------

          // USERVAL1-8: these call the function and append the returned value to the message
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL1) == MOFIELDS1_USERVAL1) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = USERVAL1; // Add the field ID
            outBufferBinary[outBufferPtr++] = USER_VAL_1(); // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL2) == MOFIELDS1_USERVAL2) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = USERVAL2; // Add the field ID
            outBufferBinary[outBufferPtr++] = USER_VAL_2(); // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL3) == MOFIELDS1_USERVAL3) // If the bit is set
          {
            union_uint16t uint16t;
            uint16t.the_data = USER_VAL_3();
            outBufferBinary[outBufferPtr++] = USERVAL3; // Add the field ID
            outBufferBinary[outBufferPtr++] = uint16t.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = uint16t.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL4) == MOFIELDS1_USERVAL4) // If the bit is set
          {
            union_uint16t uint16t;
            uint16t.the_data = USER_VAL_4();
            outBufferBinary[outBufferPtr++] = USERVAL4; // Add the field ID
            outBufferBinary[outBufferPtr++] = uint16t.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = uint16t.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL5) == MOFIELDS1_USERVAL5) // If the bit is set
          {
            union_uint32t uint32t;
            uint32t.the_data = USER_VAL_5();
            outBufferBinary[outBufferPtr++] = USERVAL5; // Add the field ID
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[1];
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[2];
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL6) == MOFIELDS1_USERVAL6) // If the bit is set
          {
            union_uint32t uint32t;
            uint32t.the_data = USER_VAL_6();
            outBufferBinary[outBufferPtr++] = USERVAL6; // Add the field ID
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[1];
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[2];
            outBufferBinary[outBufferPtr++] = uint32t.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL7) == MOFIELDS1_USERVAL7) // If the bit is set
          {
            union_uint32t union_float;
            union_float.the_data = USER_VAL_7();
            outBufferBinary[outBufferPtr++] = USERVAL7; // Add the field ID
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[1];
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[2];
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_USERVAL8) == MOFIELDS1_USERVAL8) // If the bit is set
          {
            union_uint32t union_float;
            union_float.the_data = USER_VAL_8();
            outBufferBinary[outBufferPtr++] = USERVAL8; // Add the field ID
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[1];
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[2];
            outBufferBinary[outBufferPtr++] = union_float.the_bytes[3];
          }
          
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_MOFIELDS) == MOFIELDS1_MOFIELDS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = MOFIELDS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[0].the_bytes[0]; // Add the data (little endian!)
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[0].the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[0].the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[0].the_bytes[3];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[1].the_bytes[0]; // Add the data (little endian!)
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[1].the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[1].the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[1].the_bytes[3];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[2].the_bytes[0]; // Add the data (little endian!)
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[2].the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[2].the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.MOFIELDS[2].the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_FLAGS1) == MOFIELDS1_FLAGS1) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = FLAGS1; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.FLAGS1; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_FLAGS2) == MOFIELDS1_FLAGS2) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = FLAGS2; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.FLAGS2; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_DEST) == MOFIELDS1_DEST) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = DEST; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DEST.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HIPRESS) == MOFIELDS1_HIPRESS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HIPRESS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIPRESS.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIPRESS.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIPRESS.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIPRESS.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOPRESS) == MOFIELDS1_LOPRESS) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LOPRESS; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOPRESS.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOPRESS.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOPRESS.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOPRESS.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HITEMP) == MOFIELDS1_HITEMP) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HITEMP; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HITEMP.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HITEMP.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HITEMP.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HITEMP.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOTEMP) == MOFIELDS1_LOTEMP) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LOTEMP; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOTEMP.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOTEMP.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOTEMP.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOTEMP.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_HIHUMID) == MOFIELDS1_HIHUMID) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = HIHUMID; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIHUMID.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIHUMID.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIHUMID.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.HIHUMID.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_LOHUMID) == MOFIELDS1_LOHUMID) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LOHUMID; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOHUMID.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOHUMID.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOHUMID.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOHUMID.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOFNUM) ==  MOFIELDS1_GEOFNUM) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOFNUM; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOFNUM; // Add the data
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1LAT) == MOFIELDS1_GEOF1LAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF1LAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LAT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LAT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LAT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LAT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1LON) == MOFIELDS1_GEOF1LON) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF1LON; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LON.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LON.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LON.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1LON.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF1RAD) == MOFIELDS1_GEOF1RAD) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF1RAD; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1RAD.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1RAD.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1RAD.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF1RAD.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF2LAT) == MOFIELDS1_GEOF2LAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF2LAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LAT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LAT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LAT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LAT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[1].the_data & MOFIELDS1_GEOF2LON) == MOFIELDS1_GEOF2LON) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF2LON; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LON.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LON.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LON.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2LON.the_bytes[3];
          }

          // ---------- MOFIELDS2 ----------

          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF2RAD) == MOFIELDS2_GEOF2RAD) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF2RAD; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2RAD.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2RAD.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2RAD.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF2RAD.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3LAT) == MOFIELDS2_GEOF3LAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF3LAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LAT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LAT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LAT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LAT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3LON) == MOFIELDS2_GEOF3LON) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF3LON; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LON.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LON.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LON.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3LON.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF3RAD) == MOFIELDS2_GEOF3RAD) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF3RAD; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3RAD.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3RAD.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3RAD.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF3RAD.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4LAT) == MOFIELDS2_GEOF4LAT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF4LAT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LAT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LAT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LAT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LAT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4LON) == MOFIELDS2_GEOF4LON) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF4LON; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LON.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LON.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LON.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4LON.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_GEOF4RAD) == MOFIELDS2_GEOF4RAD) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = GEOF4RAD; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4RAD.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4RAD.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4RAD.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.GEOF4RAD.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_WAKEINT) == MOFIELDS2_WAKEINT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = WAKEINT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.WAKEINT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.WAKEINT.the_bytes[1];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.WAKEINT.the_bytes[2];
            outBufferBinary[outBufferPtr++] = myTrackerSettings.WAKEINT.the_bytes[3];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_ALARMINT) == MOFIELDS2_ALARMINT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = ALARMINT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALARMINT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.ALARMINT.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_TXINT) == MOFIELDS2_TXINT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = TXINT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.TXINT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.TXINT.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_LOWBATT) == MOFIELDS2_LOWBATT) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = LOWBATT; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOWBATT.the_bytes[0]; // Add the data
            outBufferBinary[outBufferPtr++] = myTrackerSettings.LOWBATT.the_bytes[1];
          }
          if ((myTrackerSettings.MOFIELDS[2].the_data & MOFIELDS2_DYNMODEL) == MOFIELDS2_DYNMODEL) // If the bit is set
          {
            outBufferBinary[outBufferPtr++] = DYNMODEL; // Add the field ID
            outBufferBinary[outBufferPtr++] = myTrackerSettings.DYNMODEL; // Add the data
          }

          outBufferBinary[outBufferPtr++] = ETX; // Add the ETX

          // Calculate the RFC 1145 Checksum bytes
          uint32_t csuma = 0;
          uint32_t csumb = 0;
          for (size_t x = stxLoc; x < outBufferPtr; x++) // Calculate a sum of sums for every byte from STX to ETX
          {
            csuma = csuma + outBufferBinary[x];
            csumb = csumb + csuma;
          }
          
          // Add the two checksum bytes to the message
          outBufferBinary[outBufferPtr++] = (byte)(csuma & 0x000000ff);          
          outBufferBinary[outBufferPtr++] = (byte)(csumb & 0x000000ff);
                  
// ##################################################
// End of binary message construction
// ##################################################

          // Print the message
          Serial.print(F("Binary message is '"));
          for (size_t i = 0; i < outBufferPtr; i++)
          {
            if (outBufferBinary[i] < 16)
              Serial.printf("0%X", outBufferBinary[i]);
            else
              Serial.printf("%X", outBufferBinary[i]);
          }
          Serial.println(F("'"));

        } // End of binary message construction

        // Send the message
        Serial.println(F("Transmitting message..."));

        mtBufferSize = sizeof(mt_buffer);

        int waitingMessages = 1; // Keep sending/receiving until the waiting mesage count is zero
        bool firstTX = true; // Send the text/binary message the first time, NULL messages thereafter

        while (waitingMessages > 0) // Keep sending/receiving until the waiting mesage count is zero
        {

#ifndef noTX
          if (firstTX == true) // If this is the first time around the loop, send the message
          {
            // Check if we are sending a text or a binary message
            if ((myTrackerSettings.FLAGS1 & FLAGS1_BINARY) != FLAGS1_BINARY) // if bit 7 of FLAGS1 is clear we are sending text
            {
              err = modem.sendReceiveSBDText(outBuffer, mt_buffer, mtBufferSize); // This could take many seconds to complete and will call ISBDCallback() periodically
            }
            else
            {
              err = modem.sendReceiveSBDBinary(outBufferBinary, outBufferPtr, mt_buffer, mtBufferSize); // This could take many seconds to complete and will call ISBDCallback() periodically
            }
          }
          
          else // This is not the first time around the loop, so send a NULL message (to avoid wasted credits)
          {
            err = modem.sendReceiveSBDText(NULL, mt_buffer, mtBufferSize); // Send a NULL message
          }
#else
          err = ISBD_SUCCESS; // Fake successful transmit
          mtBufferSize = 0; // and with no MT message received
#endif

          // Check if the message sent OK
          if (err != ISBD_SUCCESS)
          {
            Serial.print(F("Transmission failed with error code "));
            Serial.println(err);
            // Turn on LED to indicate failed send
            digitalWrite(LED, HIGH);
          }
          else
          {
            Serial.println(F(">>> Message sent! <<<"));
            // Flash LED rapidly to indicate successful send
            for (int i = 0; i < 10; i++)
            {
              digitalWrite(LED, HIGH);
              delay(100);
              digitalWrite(LED, LOW);
              delay(100);
            }
            if (mtBufferSize > 0) { // Was an MT message received?
              // Check message content
              mt_buffer[mtBufferSize] = 0; // Make sure message is NULL terminated
              String mt_str = String((char *)mt_buffer); // Convert message into a String
              Serial.println(F("Received a MT message! Checking if it is valid..."));
              tracker_parsing_result presult = check_data(mt_buffer, mtBufferSize);
              if (presult == DATA_VALID) // If the data is valid, parse it (and update the values in RAM)
              {
                Serial.println(F("Data is valid! Parsing it..."));
                // Parse the data with the serial flag set to false so SOURCE cannot be changed
                parse_data(mt_buffer, mtBufferSize, &myTrackerSettings, false);
                Serial.println(F("Parsing complete. Updating values in EEPROM."));
                putTrackerSettings(&myTrackerSettings); // Update the settings in EEPROM
                wake_int = myTrackerSettings.WAKEINT.the_data; // Copy WAKEINT into wake_int in case it has changed
              }
  
              if (_printDebug == true)
              {
                // If debugging is enabled: print the tracker EEPROM contents as text
                Serial.println(F("EEPROM contents (remember that data is little endian!):"));
                displayEEPROMcontents();
                Serial.println();
                Serial.println();
              }
            
              printTrackerSettings(&myTrackerSettings); // Print the tracker settings if debug is enabled
            }
          }
  
          // Clear the Mobile Originated message buffer
          Serial.println(F("Clearing the MO buffer."));
          err = modem.clearBuffers(ISBD_CLEAR_MO); // Clear MO buffer
          if (err != ISBD_SUCCESS)
          {
            Serial.print(F("*** modem.clearBuffers failed with error "));
            Serial.print(err);
            Serial.println(F(" ***"));
          }

          // Update the waiting message count
          // Do multiple send/receives (with empty MO messages) until no more MT messages are waiting
#ifndef noTX
          waitingMessages = modem.getWaitingMessageCount();
#else
          waitingMessages = 0; // Fake the remaining message count
#endif
          firstTX = false; // Clear the flag so we send NULL messages from now on
          Serial.print(F("The number of messages in the MT queue is: "));
          Serial.println(waitingMessages);

        } // End of waiting messages loop

        iterationCounter = iterationCounter + 1; // Increment the iterationCounter

        // Check if the monitor-the-ring-channel bit is set
        if ((myTrackerSettings.FLAGS2 & FLAGS2_RING) == FLAGS2_RING)
        {
          loop_step = wait_for_ring; // Start monitoring the ring indicator
        }
        else
        {
          loop_step = sleep_9603; // Put the modem to sleep
        }
      }
    }
    break; // End of case start_9603
      
    // ************************************************************************************************
    // Put the modem to sleep
    case sleep_9603: 
    {
      // Power down the modem
      // Also disable the Serial1 RX pin via IridiumSBD::endSerialPort
      Serial.println(F("Putting the 9603N to sleep."));
      err = modem.sleep();
      if (err != ISBD_SUCCESS)
      {
        Serial.print(F("*** modem.sleep failed with error "));
        Serial.print(err);
        Serial.println(F(" ***"));
      }

      loop_step = zzz; // Now go to sleep
    }
    break; // End of case sleep_9603

    // ************************************************************************************************
    // Go to sleep
    case zzz:
    {
      Serial.println(F("Getting ready to put the Apollo3 into deep sleep..."));

      // Make sure the Serial1 RX pin is disabled
      modem.endSerialPort();
  
      // Disable 9603N power
      Serial.println(F("Disabling 9603N power..."));
      digitalWrite(iridiumSleep, LOW); // Disable 9603N via its ON/OFF pin (modem.sleep should have done this already)
      delay(1);
      digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
      delay(1);
    
      // Disable the supercapacitor charger
      Serial.println(F("Disabling the supercapacitor charger..."));
      digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

      // Close the Iridium serial port
      Serial1.end();

      // Check if we should leave the GNSS powered up: if geofence alerts are enabled and 1 or more geofences is defined
      if (((myTrackerSettings.FLAGS2 & FLAGS2_GEOFENCE) == FLAGS2_GEOFENCE) && (((myTrackerSettings.GEOFNUM & 0xf0) >> 4) > 0))
      {
        // The FLAGS2_GEOFENCE bit is set so we should power up the GNSS, wait for a 3D fix,
        // wait for the geofence status to go active, put the ZOE into powersave mode
        // and then put the Artemis into deep sleep. If the geofence pin changes state,
        // it will wake the Artemis and cause it to send a message.

        Serial.println(F("Powering up the GNSS for geofence monitoring..."));
        gnssON(); // Enable power for the GNSS
        delay(2000); // Give the GNSS time to power up

        setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance
        if (myGPS.begin(agtWire) == true) //Connect to the Ublox module using Wire port
        {
          Serial.println(F("Waiting for a 3D GNSS fix..."));
    
          myTrackerSettings.FIX = 0; // Clear the fix type
          
          // Look for GPS signal for up to GNSS_timeout minutes
          // Stop when we get a 3D fix, or we timeout
          for (unsigned long tnow = millis(); (myTrackerSettings.FIX != 3) && (millis() - tnow < GNSS_timeout * 60UL * 1000UL);)
          {
          
            myTrackerSettings.FIX = myGPS.getFixType(); // Get the GNSS fix type
            
            // Check battery voltage now we are drawing current for the GPS
            // If voltage is lower than 0.2V below LOWBATT, stop looking for GNSS and go to sleep
            get_vbat();
            if (battVlow() == true) {
              break; // Exit the for loop now
            }
    
            // Flash the LED at 1Hz
            if ((millis() / 1000) % 2 == 1) {
              digitalWrite(LED, HIGH);
            }
            else {
              digitalWrite(LED, LOW);
            }
    
            // Delay for 100msec so we don't pound the I2C bus too hard!
            delay(100);
          }
  
          Serial.println(F("Waiting for the geofence status to go active..."));
          byte geofenceStatus = 0; // Use this to hold the geofence status

          // Wait for the geofence status to go active for up to GNSS_timeout minutes
          // Stop when it goes active, or we timeout
          for (unsigned long tnow = millis(); (geofenceStatus != 1) && (millis() - tnow < GNSS_timeout * 60UL * 1000UL);)
          {
          
            geofenceState currentGeofenceState; // Create storage for the geofence state
            myGPS.getGeofenceState(currentGeofenceState); // Get the geofence state
            geofenceStatus = currentGeofenceState.status; // Get the status
                    
            // Check battery voltage now we are drawing current for the GPS
            // If voltage is lower than 0.2V below LOWBATT, stop looking for GNSS and go to sleep
            get_vbat();
            if (battVlow() == true) {
              break; // Exit the for loop now
            }
    
            // Flash the LED at 1Hz
            if ((millis() / 1000) % 2 == 1) {
              digitalWrite(LED, HIGH);
            }
            else {
              digitalWrite(LED, LOW);
            }
    
            // Delay for 100msec so we don't pound the I2C bus too hard!
            delay(100);
          }

          // Put the ZOE-M8Q into power save mode
          if (myGPS.powerSaveMode() == true)
          {
            Serial.println(F("GNSS Power Save Mode enabled."));
          }
          else
          {
            Serial.println(F("*** GNSS Power Save Mode may have FAILED ***"));
          }

        }
        else
        {
          // GNSS failed to start so power it off again
        Serial.println(F("The GNSS failed to start. Powering it down again..."));
        gnssOFF(); // Disable power for the GNSS
        }
      }
      else
      {
      // Make sure the GNSS is powered off
      Serial.println(F("Powering down the GNSS..."));
      gnssOFF(); // Disable power for the GNSS
      }

      // Close the I2C port
      //agtWire.end(); //DO NOT Power down I2C - causes badness with v2.1 of the core: https://github.com/sparkfun/Arduino_Apollo3/issues/412

      digitalWrite(busVoltageMonEN, LOW); // Disable the bus voltage monitor

      digitalWrite(LED, LOW); // Disable the LED

      disableDebugging(); // Disable the serial debug messages

      // Close and detach the serial console
      Serial.print(F("Going into deep sleep until next WAKEINT ("));
      Serial.print(wake_int);
      Serial.println(F(" seconds)."));
      Serial.flush(); //Finish any prints
      Serial.end(); // Close the serial console

      // Code taken (mostly) from Apollo3 Example6_Low_Power_Alarm
      
      // Disable ADC
      powerControlADC(false);
    
      // Force the peripherals off
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM0); // SPI
      //am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM1); // agtWire I2C
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM2);
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM3);
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM4); // Qwiic I2C
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM5);
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_ADC);
      //am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART0); // Leave UART0 on to avoid printing erroneous characters to Serial
      am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART1); // Serial1
    
      // Disable all unused pins - including: SCL (8), SDA (9), UART0 TX (48) and RX (49) and UART1 TX (24) and RX (25)
      const int pinsToDisable[] = {0,1,2,8,9,11,12,14,15,16,20,21,24,25,29,31,32,33,36,37,38,42,43,44,45,48,49,-1};
      for (int x = 0; pinsToDisable[x] >= 0; x++)
      {
        pin_config(PinName(pinsToDisable[x]), g_AM_HAL_GPIO_DISABLE);
      }
    
      //Power down CACHE, flashand SRAM
      am_hal_pwrctrl_memory_deepsleep_powerdown(AM_HAL_PWRCTRL_MEM_ALL); // Power down all flash and cache
      am_hal_pwrctrl_memory_deepsleep_retain(AM_HAL_PWRCTRL_MEM_SRAM_384K); // Retain all SRAM (0.6 uA)
      
      // Keep the 32kHz clock running for RTC
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
      am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
      
      geofence_alarm = false; // The geofence alarm pin will have been bouncing around so let's make sure the flag is clear

      // This while loop keeps the processor asleep until WAKEINT seconds have passed
      // Or a geofence alarm takes place (but only if geofence alarms are enabled)
      while ((interval_alarm == false) && ((geofence_alarm == false) || ((myTrackerSettings.FLAGS2 & FLAGS2_GEOFENCE) != FLAGS2_GEOFENCE)))
      {
        // Go to Deep Sleep.
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
      }
      
      interval_alarm = false; // Clear the interval alarm flag now (even if a geofence alarm caused us to come out of sleep)
      
      // If a geofence alarm took place, let's record that we need to send a geofence alarm message
      // so it doesn't matter what geofence_alarm does before then
      if (geofence_alarm == true)
      {
        sendGeofenceAlarm = true;
        geofence_alarm = false;
      }

      // Wake up!
      loop_step = wakeUp;
    }
    break; // End of case zzz
      
    // ************************************************************************************************
    // Wake from sleep
    case wakeUp:
    {
      // Code taken (mostly) from Apollo3 Example6_Low_Power_Alarm
      
      // Go back to using the main clock
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
      am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ);
    
      // Power up SRAM, turn on entire Flash
      am_hal_pwrctrl_memory_deepsleep_powerdown(AM_HAL_PWRCTRL_MEM_MAX);
    
      // Renable UART0 pins: TX (48) and RX (49)
      pin_config(PinName(48), g_AM_BSP_GPIO_COM_UART_TX);
      pin_config(PinName(49), g_AM_BSP_GPIO_COM_UART_RX);
    
      // Do not renable the UART1 pins here as the modem is still powered off. Let modem.begin do it via beginSerialPort.
    
      // Enable ADC
      powerControlADC(true);

      // Disable power for the GNSS (which could make the geofence interrupt pin bounce around causing false alerts)
      gnssOFF();

      // Do it all again!
      loop_step = loop_init;
    }
    break; // End of case wakeUp

    // ************************************************************************************************
    // Leave the 9603N powered up and monitor the ring indicator continuously
    // Exit and send a message (to download new MT messages) every WAKEINT seconds or earlier when we see a ring indication
    // The user should have set WAKEINT to the same interval as TXINT or ALARMINT
    case wait_for_ring:
    {
      Serial.println(F("Waiting for Ring Indication..."));

      while ((interval_alarm == false) && (modem.hasRingAsserted() == false)) // Exit every WAKEINT seconds or when we see a ring indication
      {
        // Flash the LED for 100msec every 5 seconds
        if (millis() % 5000 < 100) {
          digitalWrite(LED, HIGH);
        }
        else {
          digitalWrite(LED, LOW);
        }
      }
      interval_alarm = false; // Clear the alarm flag now

      if (modem.hasRingAsserted() == true)
      {
        Serial.println(F("Ring Indication received!"));
      }

      // Power down the 9603N as we are going to want to use the GNSS next and don't want bad things to happen to the RF switch!
      // Also disable the Serial1 RX pin via IridiumSBD::endSerialPort
      Serial.println(F("Putting the 9603N to sleep."));
      err = modem.sleep();
      if (err != ISBD_SUCCESS)
      {
        Serial.print(F("*** modem.sleep failed with error "));
        Serial.print(err);
        Serial.println(F(" ***"));
      }
      // Disable 9603N power
      Serial.println(F("Disabling 9603N power..."));
      digitalWrite(iridiumSleep, LOW); // Disable 9603N via its ON/OFF pin (modem.sleep should have done this already)
      delay(100);
      digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
      delay(100);
      // Disable the supercapacitor charger
      Serial.println(F("Disabling the supercapacitor charger..."));
      digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

      delay(100); // Wait for serial port to clear

      // Do it all again!
      loop_step = loop_init;
    }
    break; // End of case wait_for_ring

    // ************************************************************************************************
    // Configure the tracker settings via Serial (USB)
    case configureMe:
    {
      Serial.println(F("*** Tracker Configuration ***"));
      Serial.println(F("Waiting for data..."));

      tracker_serial_rx_status stat = check_for_serial_data(true); // Start checking for the arrival of new serial data

      while ((stat != DATA_RECEIVED) && (stat != DATA_TIMEOUT))
      {
        stat = check_for_serial_data(false); // Keep checking for the arrival of serial data
      }
    
      if (stat == DATA_RECEIVED) // If we received some data then parse it
      {
        Serial.println(F("Data received! Checking if it is valid..."));
        tracker_parsing_result presult = check_data(tracker_serial_rx_buffer, tracker_serial_rx_buffer_size);
        if (presult == DATA_VALID) // If the data is valid, parse it (and update the values in RAM)
        {
          Serial.println(F("Data is valid! Parsing it..."));
          // Parse the data with the serial flag set to true so SOURCE can be changed
          parse_data(tracker_serial_rx_buffer, tracker_serial_rx_buffer_size, &myTrackerSettings, true);
          Serial.println(F("Parsing complete. Updating values in EEPROM."));
          putTrackerSettings(&myTrackerSettings); // Update the settings in EEPROM
          wake_int = myTrackerSettings.WAKEINT.the_data; // Copy WAKEINT into wake_int in case it has changed
        }
      }
    
      if (_printDebug == true)
      {
        // If debugging is enabled: print the tracker EEPROM contents as text
        Serial.println(F("EEPROM contents (remember that data is little endian!):"));
        displayEEPROMcontents();
        Serial.println();
      }
    
      printTrackerSettings(&myTrackerSettings); // Print the tracker settings (if debug is enabled)
    
      Serial.println(F("Done!"));
      
      // Go back where we came from... (Or so help me!)
      loop_step = last_loop_step;
    }
    break; // End of case configureMe

  } // End of switch (loop_step)
} // End of loop()

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
