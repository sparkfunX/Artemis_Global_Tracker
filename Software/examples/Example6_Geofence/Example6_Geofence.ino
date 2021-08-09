/*
  Artemis Global Tracker
  Example: Geofence
  
  Written by Paul Clark (PaulZC)
  August 7th 2021

 ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
 ** (At the time of writing, v2.1.1 of the core conatins a feature which makes communication with the u-blox GNSS problematic. Be sure to use v2.1.0) **

 ** Set the Board to "RedBoard Artemis ATP" **
 ** (The Artemis Module does not have a Wire port defined, which prevents the GNSS library from compiling) **
  
  This example powers up the ZOE-M8Q and reads the fix.
  Once a valid 3D fix has been found, the code reads the latitude and longitude.
  The code then sets four geofences around that position with a radii of 5m, 10m, 15m and 20m with 95% confidence.
  The code then monitors the geofence status.
  The LED will be illuminated if you are inside the _combined_ geofence (i.e. within the 20m radius).
  If you disconnect the antenna: the status should go unknown, the geofencePin will go high and the LED will go out.
  
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

  // Set up the I2C pins
  agtWire.begin();
  agtWire.setClock(100000); // Use 100kHz for best performance
  setAGTWirePullups(0); // Remove the pull-ups from the I2C pins (internal to the Artemis) for best performance

  // Start the console serial port
  Serial.begin(115200);
  while (!Serial) // Wait for the user to open the serial monitor
    ;
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Example: Geofence"));
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
  
  if (myGNSS.begin(agtWire) == false) //Connect to the u-blox module using pads 8 & 9
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing."));
    while (1)
      ;
  }

  //myGNSS.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial
  
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

  uint32_t radius = 500; // Set the radius to 5m (radius is in m * 10^-2 i.e. cm)
  byte confidence = 2; // Set the confidence level: 0=none, 1=68%, 2=95%, 3=99.7%, 4=99.99%
  byte pinPolarity = 0; // Set the PIO pin polarity: 0 = low means inside, 1 = low means outside (or unknown)
  byte pin = 14; // ZOE-M8Q PIO14 is connected to the geofencePin

  // Call clearGeofences() to clear all existing geofences.
  Serial.print(F("Clearing any existing geofences. clearGeofences returned: "));
  Serial.println(myGNSS.clearGeofences());

  // It is possible to define up to four geofences.
  // Call addGeofence up to four times to define them.
  // The geofencePin will indicate the combined state of all active geofences.
  Serial.println(F("Setting the geofences:"));
  
  Serial.print(F("addGeofence for geofence 1 returned: "));
  Serial.println(myGNSS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin));
  
  radius = 1000; // 10m
  Serial.print(F("addGeofence for geofence 2 returned: "));
  Serial.println(myGNSS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin));
  
  radius = 1500; // 15m
  Serial.print(F("addGeofence for geofence 3 returned: "));
  Serial.println(myGNSS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin));
  
  radius = 2000; // 20m
  Serial.print(F("addGeofence for geofence 4 returned: "));
  Serial.println(myGNSS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin));
}

void loop()
{
  geofenceState currentGeofenceState; // Create storage for the geofence state

  boolean result = myGNSS.getGeofenceState(currentGeofenceState);

  Serial.print(F("getGeofenceState returned: ")); // Print the combined state
  Serial.print(result); // Get the geofence state

  if (!result) // If getGeofenceState did not return true
  {
    Serial.println(F(".")); // Tidy up
    return; // and go round the loop again
  }

  // Print the Geofencing status
  // 0 - Geofencing not available or not reliable; 1 - Geofencing active
  Serial.print(F(". status is: "));
  Serial.print(currentGeofenceState.status);

  // Print the numFences
  Serial.print(F(". numFences is: "));
  Serial.print(currentGeofenceState.numFences);

  // Print the combined state
  // Combined (logical OR) state of all geofences: 0 - Unknown; 1 - Inside; 2 - Outside
  Serial.print(F(". combState is: "));
  Serial.print(currentGeofenceState.combState);

  // Print the state of each geofence
  // 0 - Unknown; 1 - Inside; 2 - Outside
  Serial.print(F(". The individual states are: "));
  for(int i = 0; i < currentGeofenceState.numFences; i++)
  {
    if (i > 0) Serial.print(F(","));
    Serial.print(currentGeofenceState.states[i]);
  }

  byte fenceStatus = digitalRead(geofencePin); // Read the geofence pin
  digitalWrite(LED, !fenceStatus); // Set the LED (inverted)
  Serial.print(F(". Geofence pin (PIO14) is: ")); // Print the pin state
  Serial.print(fenceStatus);
  Serial.println(F("."));
  
  delay(1000);
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
