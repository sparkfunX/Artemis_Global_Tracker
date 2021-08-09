/*
 Artemis Global Tracker
 Example: Geofence Alert

 Written by Paul Clark (PaulZC)
 August 7th 2021

 ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
 ** (At the time of writing, v2.1.1 of the core conatins a feature which makes communication with the u-blox GNSS problematic. Be sure to use v2.1.0) **

 ** Set the Board to "RedBoard Artemis ATP" **
 ** (The Artemis Module does not have a Wire port defined, which prevents the GNSS library from compiling) **

 This example powers up the ZOE-M8Q and reads the fix.
 Once a valid 3D fix has been found, the code reads the latitude and longitude.
 The code then sets a geofence around that position with a radius of 100m with 95% confidence.
 The Artemis then goes into deep sleep to save current but will be woken up whenever the geofence
 pin changes state. The LED is illuminated when the tracker is inside the geofence.

 Once the geofence has been set, you can test the code by disconnecting the antenna.
 After a few seconds the geofence status will go to "unknown" and the LED will go out.

 This example also engages the ZOE's Power Save Mode.
 ** When it is able to ** the ZOE will approximately halve its current draw.
 
 You will need to install the SparkFun u-blox library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library

 The ZOE-M8Q shares I2C Port 1 with the MS8607: SCL = D8; SDA = D9

 Power for the ZOE is switched via Q2.
 D26 needs to be pulled low to enable the GNSS power.

 The ZOE's PIO14 pin (geofence signal) is conected to D10,
 so let's configure D10 as an input.

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
const byte PIN_AGTWIRE_SCL = 8;
const byte PIN_AGTWIRE_SDA = 9;
TwoWire agtWire(PIN_AGTWIRE_SDA, PIN_AGTWIRE_SCL); //Create an I2C port using pads 8 (SCL) and 9 (SDA)

#include "SparkFun_u-blox_GNSS_Arduino_Library.h" //http://librarymanager/All#SparkFun_u-blox_GNSS
SFE_UBLOX_GNSS myGNSS;

// geofencePin Interrupt Service Routine
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
void geofenceISR(void)
{
  digitalWrite(LED, !digitalRead(geofencePin)); // Update the LED
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

void setup()
{
  pinMode(LED, OUTPUT);

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  // Call geofenceISR whenever geofencePin changes state
  // Note that in v2.1.0 of the core, this causes an immediate interrupt
  attachInterrupt(geofencePin, geofenceISR, CHANGE);

  // Set up the I2C pins
  agtWire.begin();
  agtWire.setClock(100000); // Use 100kHz for best performance
  setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance

  // Start the console serial port
  Serial.begin(115200);
  delay(1000); // Wait for the user to open the serial monitor (extend this delay if you need more time)
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Example: Geofence Alert"));
  Serial.println();

  gnssON(); // Enable power for the GNSS
  delay(1000); // Let the ZOE power up
  
  if (myGNSS.begin(agtWire) == false) //Connect to the u-blox module using agtWire
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }

  //myGNSS.enableDebugging(); // Enable debug messages
  myGNSS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

  Serial.println(F("Waiting for a 3D fix..."));

  byte fixType = 0;

  while (fixType != 3) // Wait for a 3D fix
  {
    fixType = myGNSS.getFixType(); // Get the fix type
    Serial.print(F("Fix: "));
    Serial.print(fixType);
    if(fixType == 0) Serial.print(F(" = No fix"));
    else if(fixType == 1) Serial.print(F(" = Dead reckoning"));
    else if(fixType == 2) Serial.print(F(" = 2D"));
    else if(fixType == 3) Serial.print(F(" = 3D"));
    else if(fixType == 4) Serial.print(F(" = GNSS + Dead reckoning"));
    Serial.println();
    delay(1000);
  }

  Serial.println(F("3D fix found! Setting the geofence..."));

  long latitude = myGNSS.getLatitude(); // Get the latitude in degrees * 10^-7
  Serial.print(F("Lat: "));
  Serial.print(latitude);

  long longitude = myGNSS.getLongitude(); // Get the longitude in degrees * 10^-7
  Serial.print(F("   Long: "));
  Serial.println(longitude);

  uint32_t radius = 10000; // Set the radius to 100m (radius is in m * 10^-2 i.e. cm)
  byte confidence = 2; // Set the confidence level: 0=none, 1=68%, 2=95%, 3=99.7%, 4=99.99%
  byte pinPolarity = 0; // Set the PIO pin polarity: 0 = low means inside, 1 = low means outside (or unknown)
  byte pin = 14; // ZOE-M8Q PIO14 is connected to the geofencePin

  //myGNSS.clearGeofences();// Clear all existing geofences.

  // It is possible to define up to four geofences.
  // Call addGeofence up to four times to define them.
  // The geofencePin will indicate the combined state of all active geofences.
  myGNSS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin); // Add the geofence

  delay(1000); // Let the geofence do its thing

  Serial.println(F("The LED will be lit while the tracker is inside the geofence."));

  digitalWrite(LED, !digitalRead(geofencePin)); // Update the LED.

  // Put the ZOE-M8Q into power save mode
  if (myGNSS.powerSaveMode() == true)
  {
    Serial.println(F("GNSS Power Save Mode enabled."));
  }
  else
  {
    Serial.println(F("***!!! GNSS Power Save Mode may have FAILED !!!***"));
  }

  Serial.println(F("Going into deep sleep..."));
  Serial.println();

  Serial.flush(); // Wait for serial data to be sent so the Serial.end doesn't cause problems

  // Code taken (mostly) from Apollo3 Example6_Low_Power_Alarm
  
  // Disable UART
  Serial.end();

  // Disable ADC
  powerControlADC(false);

  // Force the peripherals off
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM0);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM1);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM2);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM3);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM4);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_IOM5);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_ADC);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART0);
  am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART1);

  // Disable all unused pins - including UART0 TX (48) and RX (49)
  const int pinsToDisable[] = {0,1,2,11,12,14,15,16,20,21,29,31,32,36,37,38,42,43,44,45,48,49,-1};
  for (int x = 0; pinsToDisable[x] >= 0; x++)
  {
    am_hal_gpio_pinconfig(pinsToDisable[x], g_AM_HAL_GPIO_DISABLE);
  }

  //Power down CACHE, flashand SRAM
  am_hal_pwrctrl_memory_deepsleep_powerdown(AM_HAL_PWRCTRL_MEM_ALL); // Power down all flash and cache
  am_hal_pwrctrl_memory_deepsleep_retain(AM_HAL_PWRCTRL_MEM_SRAM_384K); // Retain all SRAM (0.6 uA)
  
  // Keep the 32kHz clock running for RTC
  am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
  am_hal_stimer_config(AM_HAL_STIMER_XTAL_32KHZ);
}

void loop()
{
  // Go to Deep Sleep.
  am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}

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
