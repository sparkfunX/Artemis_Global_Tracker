/*
 Artemis Global Tracker
 Example: Geofence Alert

 Written by Paul Clark (PaulZC)
 30th January 2020

 ** Set the Board to "SparkFun Artemis Module" **

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
 https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library

 The ZOE-M8Q shares I2C Port 1 (Wire1) with the MS8607: SCL = D8; SDA = D9

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

#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
SFE_UBLOX_GPS myGPS;

// geofencePin Interrupt Service Routine
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
void geofenceISR(void)
{
  digitalWrite(LED, !digitalRead(geofencePin)); // Update the LED
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
  pinMode(LED, OUTPUT);

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input

  attachInterrupt(digitalPinToInterrupt(geofencePin), geofenceISR, CHANGE); // Call geofenceISR whenever geofencePin changes state

  // Set up the I2C pins
  Wire1.begin();

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
  
  if (myGPS.begin(Wire1) == false) //Connect to the Ublox module using Wire1 port
  {
    Serial.println(F("Ublox GPS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }

  //myGPS.enableDebugging(); // Enable debug messages
  myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

  Serial.println(F("Waiting for a 3D fix..."));

  byte fixType = 0;

  while (fixType != 3) // Wait for a 3D fix
  {
    fixType = myGPS.getFixType(); // Get the fix type
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

  long latitude = myGPS.getLatitude(); // Get the latitude in degrees * 10^-7
  Serial.print(F("Lat: "));
  Serial.print(latitude);

  long longitude = myGPS.getLongitude(); // Get the longitude in degrees * 10^-7
  Serial.print(F("   Long: "));
  Serial.println(longitude);

  uint32_t radius = 10000; // Set the radius to 100m (radius is in m * 10^-2 i.e. cm)
  byte confidence = 2; // Set the confidence level: 0=none, 1=68%, 2=95%, 3=99.7%, 4=99.99%
  byte pinPolarity = 0; // Set the PIO pin polarity: 0 = low means inside, 1 = low means outside (or unknown)
  byte pin = 14; // ZOE-M8Q PIO14 is connected to the geofencePin

  //myGPS.clearGeofences();// Clear all existing geofences.

  // It is possible to define up to four geofences.
  // Call addGeofence up to four times to define them.
  // The geofencePin will indicate the combined state of all active geofences.
  myGPS.addGeofence(latitude, longitude, radius, confidence, pinPolarity, pin); // Add the geofence

  delay(1000); // Let the geofence do its thing

  Serial.println(F("The LED will be lit while the tracker is inside the geofence."));

  digitalWrite(LED, !digitalRead(geofencePin)); // Update the LED.

  // Put the ZOE-M8Q into power save mode
  if (myGPS.powerSaveMode() == true)
  {
    Serial.println(F("GNSS Power Save Mode enabled."));
  }
  else
  {
    Serial.println(F("***!!! GNSS Power Save Mode may have FAILED !!!***"));
  }

  Serial.println(F("Going into deep sleep..."));

  delay(1000); // Wait for serial data to be sent so the Serial.end doesn't cause problems

  Serial.end(); // Close the serial console

  // Code taken from the LowPower_WithWake example
  // and the Ambiq deepsleep_wake example
  
  //Turn off ADC
  power_adc_disable();

  // Set the clock frequency.
  am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);

  // Set the default cache configuration
  am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
  am_hal_cachectrl_enable();

  // Configure the board for low power operation.
  // (This will disable the RTC)
  am_bsp_low_power_init();

  // Disabling the debugger GPIOs saves about 1.2 uA total:
  am_hal_gpio_pinconfig(20 /* SWDCLK */, g_AM_HAL_GPIO_DISABLE);
  am_hal_gpio_pinconfig(21 /* SWDIO */, g_AM_HAL_GPIO_DISABLE);

  // These two GPIOs are critical: the TX/RX connections between the Artemis module and the CH340S on the Blackboard
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

  // Enable GPIO interrupts to the NVIC. (redundant?)
  NVIC_EnableIRQ(GPIO_IRQn);

  // Enable interrupts to the core. (redundant?)
  am_hal_interrupt_master_enable();
}

void loop()
{
  // Go to Deep Sleep.
  am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
}
