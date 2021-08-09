/*
  Artemis Global Tracker
  Example: Test Low Power
  
  Written by Paul Clark (PaulZC)
  August 7th 2021

  ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **

  ** Set the Board to "RedBoard Artemis ATP" **

  This example test the Artemis low power mode and in particular:
  tests that millis is still running afterwards; checks the RTC;
  and let's us figure out what we need to do when waking from sleep
  to put the Flash, RAM and clock speeds back to normal.

  It is also how we discover that millis gets corrupted somehow
  when going into or coming out of deep sleep. It is probably
  something to do with having to configure the stimer but I
  haven't found the root cause yet.

  It is also how we discover that pressing the Reset switch
  does _not_ reset the RTC. Nice!
  
  This example is based extensively on the Apollo3 Example6_Low_Power_Alarm
  
  SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun!

  Version history:
  August 7th 2021
    Update for Apollo3 v2.1 using the powerDown code from OpenLog Artemis v2
  December 15th, 2020:
    Adding the deep sleep code from OpenLog Artemis.
    Keep RAM powered up during sleep to prevent corruption above 64K.
    Restore busVoltagePin (Analog in) after deep sleep.
  January 30th, 2020:
    Original release

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

#include "RTC.h" //Include RTC library included with the Arduino_Apollo3 core

// Define how often to wake up in SECONDS
unsigned long INTERVAL = 15;

// Use this to keep track of the second alarms from the RTC
volatile unsigned long seconds_count = 0;

// This flag indicates an interval alarm has occurred
volatile bool interval_alarm = false;

// Loop Steps - these are used by the switch/case in the main loop
typedef enum {
  loop_init = 0,
  send_time,
  zzz,
  wakeUp
} loop_steps;
loop_steps loop_step = loop_init; // Make sure loop_step is set to loop_init

// RTC alarm Interrupt Service Routine
// Clear the interrupt flag and increment seconds_count
// If INTERVAL has been reached, set the interval_alarm flag and reset seconds_count
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt
  rtc.clearInterrupt();

  // Increment seconds_count
  seconds_count = seconds_count + 1;

  // Check if interval_alarm should be set
  if (seconds_count >= INTERVAL)
  {
    interval_alarm = true;
    seconds_count = 0;
  }
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
  loop_step = loop_init; // Make sure loop_step is set to loop_init
  seconds_count = 0; // Make sure seconds_count is reset
  interval_alarm = false; // Make sure the interval alarm is clear

  // Set up the RTC for 1 second interrupts
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
    // Print the welcome message
    case loop_init:

      digitalWrite(LED, HIGH); // Enable the LED
    
      // Start the console serial port and send the welcome message
      Serial.begin(115200);
      delay(1000); // Wait for the user to open the serial monitor (extend this delay if you need more time)
      Serial.println();
      Serial.println();
      Serial.println(F("Artemis Global Tracker"));
      Serial.println(F("Example: Test Low Power and RTC"));
      Serial.println();
      Serial.println(F("millis\tRTC"));

      Serial.flush(); // Wait for serial data to be sent so the next Serial.begin doesn't cause problems

      loop_step = send_time; // Move on, send the time
      
      break; // End of case loop_init

    // ************************************************************************************************
    // Print the time (millis and RTC)
    case send_time:

      digitalWrite(LED, HIGH); // Enable the LED

      // Start the console serial port again (zzz will have ended it)
      Serial.begin(115200);

      Serial.print(millis()); // Print millis - show the corruption after deep sleep
      Serial.print(F("\t"));

      // Print the RTC time
      // v2.1 of the Apollo3 does not support printf("%02d") correctly, so we need to assemble the timeString manually
      char timeString[23]; // 10/12/2019 09:14:37.41
      getTimeString(timeString);
      Serial.println(timeString);

      Serial.flush(); // Wait for serial data to be sent so the Serial.end doesn't cause problems

      digitalWrite(LED, LOW); // Disable the LED

      loop_step = zzz; // Move on, go to sleep
      
      break; // End of case send_time

    // ************************************************************************************************
    // Go to sleep
    case zzz:
      {
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
        //am_hal_pwrctrl_periph_disable(AM_HAL_PWRCTRL_PERIPH_UART0); // Leave UART0 on to avoid printing erroneous characters to Serial
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
      
        // This while loop keeps the processor asleep until INTERVAL seconds have passed
        while (!interval_alarm) // Wake up every INTERVAL seconds
        {
          // Go to Deep Sleep.
          am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
        }
        interval_alarm = false; // Clear the alarm flag now
  
  
        // And we're back!
        loop_step = wakeUp;
      }
      break; // End of case zzz
      
    // ************************************************************************************************
    // Wake from sleep
    case wakeUp:

      // Code taken (mostly) from Apollo3 Example6_Low_Power_Alarm
      
      // Go back to using the main clock
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
      am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ);
    
      // Power up SRAM, turn on entire Flash
      am_hal_pwrctrl_memory_deepsleep_powerdown(AM_HAL_PWRCTRL_MEM_MAX);
    
      // Go back to using the main clock
      am_hal_stimer_config(AM_HAL_STIMER_CFG_CLEAR | AM_HAL_STIMER_CFG_FREEZE);
      am_hal_stimer_config(AM_HAL_STIMER_HFRC_3MHZ);
    
      // Renable UART0 pins
      am_hal_gpio_pinconfig(48, g_AM_BSP_GPIO_COM_UART_TX);
      am_hal_gpio_pinconfig(49, g_AM_BSP_GPIO_COM_UART_RX);
    
      // Renable power to UART0
      am_hal_pwrctrl_periph_enable(AM_HAL_PWRCTRL_PERIPH_UART0);
      
      // Enable ADC
      initializeADC();
    
      // Do it all again!
      loop_step = send_time;
  
      break; // End of case wake

  } // End of switch (loop_step)
} // End of loop()

void getTimeString(char timeStringBuffer[])
{
  rtc.getTime();

  timeStringBuffer[0] = 0; // Clear the previous time

  char rtcDate[11]; // 10/12/2019
  char rtcDay[3];
  char rtcMonth[3];
  char rtcYear[5];
  if (rtc.dayOfMonth < 10)
    sprintf(rtcDay, "0%d", rtc.dayOfMonth);
  else
    sprintf(rtcDay, "%d", rtc.dayOfMonth);
  if (rtc.month < 10)
    sprintf(rtcMonth, "0%d", rtc.month);
  else
    sprintf(rtcMonth, "%d", rtc.month);
  if (rtc.year < 10)
    sprintf(rtcYear, "200%d", rtc.year);
  else
    sprintf(rtcYear, "20%d", rtc.year);
  //sprintf(rtcDate, "%s/%s/%s ", rtcMonth, rtcDay, rtcYear); // American format
  sprintf(rtcDate, "%s/%s/%s ", rtcDay, rtcMonth, rtcYear); // UK format
  strcat(timeStringBuffer, rtcDate);

  char rtcTime[12]; // 09:14:37.41
  char rtcHour[3];
  char rtcMin[3];
  char rtcSec[3];
  char rtcHundredths[3];
  if (rtc.hour < 10)
    sprintf(rtcHour, "0%d", rtc.hour);
  else
    sprintf(rtcHour, "%d", rtc.hour);
  if (rtc.minute < 10)
    sprintf(rtcMin, "0%d", rtc.minute);
  else
    sprintf(rtcMin, "%d", rtc.minute);
  if (rtc.seconds < 10)
    sprintf(rtcSec, "0%d", rtc.seconds);
  else
    sprintf(rtcSec, "%d", rtc.seconds);
  if (rtc.hundredths < 10)
    sprintf(rtcHundredths, "0%d", rtc.hundredths);
  else
    sprintf(rtcHundredths, "%d", rtc.hundredths);
  sprintf(rtcTime, "%s:%s:%s.%s", rtcHour, rtcMin, rtcSec, rtcHundredths);
  strcat(timeStringBuffer, rtcTime);
}
