/*
  Artemis Global Tracker
  Example: Test Low Power
  
  Written by Paul Clark (PaulZC)
  30th January 2020

  This example test the Artemis low power mode and in particular:
  tests that millis is still running afterwards; checks the RTC;
  and let's us figure out what we need to do when waking from sleep
  to put the Flash, RAM and clock speeds back to normal.

  It is also how we discover that millis gets zero'd somehow
  when going into or coming out of deep sleep. It is probably
  something to do with having to configure the stimer but I
  haven't found the root cause yet.

  It is also how we discover that pressing the Reset switch
  does _not_ reset the RTC. Nice!
  
  ** Set the Board to "SparkFun Artemis Module" **
  
  This example is based extensively on the Artemis Low Power With Wake example
  By: Nathan Seidle
  SparkFun Electronics
  (Date: October 17th, 2019)
  Also the OpenLog_Artemis PowerDownRTC example
  
  License: This code is public domain. Based on deepsleep_wake.c from Ambiq SDK v2.3.2.

  SparkFun labored with love to create this code. Feel like supporting open source hardware?
  Buy a board from SparkFun!

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

// Define how often to wake up in SECONDS
unsigned long INTERVAL = 15;

// Use this to keep track of the second alarms from the RTC
volatile unsigned long seconds_count = 0;

// This flag indicates an interval alarm has occurred
volatile bool interval_alarm = false;

// Loop Steps - these are used by the switch/case in the main loop
#define loop_init     0
#define send_time     1
#define zzz           2
#define wake          3
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
// If INTERVAL has been reached, set the interval_alarm flag and reset seconds_count
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
extern "C" void am_rtc_isr(void)
{
  // Clear the RTC alarm interrupt.
  am_hal_rtc_int_clear(AM_HAL_RTC_INT_ALM);

  // Increment seconds_count
  seconds_count = seconds_count + 1;

  // Check if interval_alarm should be set
  if (seconds_count >= INTERVAL)
  {
    interval_alarm = true;
    seconds_count = 0;
  }
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
  loop_step = loop_init; // Make sure loop_step is set to loop_init
  seconds_count = 0; // Make sure seconds_count is reset
  interval_alarm = false; // Make sure the interval alarm is clear

  // Set up the RTC for 1 second interrupts
  setupRTC();
  
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

      delay(200); // Wait for serial data to be sent so the next Serial.begin doesn't cause problems

      loop_step = send_time; // Move on, send the time
      
      break; // End of case loop_init

    // ************************************************************************************************
    // Print the time (millis and RTC)
    case send_time:

      digitalWrite(LED, HIGH); // Enable the LED

      // Get the RTC time
      am_hal_rtc_time_t rtc_time;
      am_hal_rtc_time_get(&rtc_time);
    
      // Start the console serial port again (zzz will have ended it)
      Serial.begin(115200);
      Serial.println();

      delay(123); // Let's delay a little just to prove that millis is running
      
      Serial.print(millis()); // Print millis
      Serial.print(F("\t"));

      // Print the RTC time
      if (rtc_time.ui32Hour < 10) Serial.print(F("0"));
      Serial.print(rtc_time.ui32Hour);
      Serial.print(F(":"));
      if (rtc_time.ui32Minute < 10) Serial.print(F("0"));
      Serial.print(rtc_time.ui32Minute);
      Serial.print(F(":"));
      if (rtc_time.ui32Second < 10) Serial.print(F("0"));
      Serial.print(rtc_time.ui32Second);
      Serial.println();

      delay(200); // Wait for serial data to be sent so the Serial.end doesn't cause problems

      digitalWrite(LED, LOW); // Disable the LED

      loop_step = zzz; // Move on, go to sleep
      
      break; // End of case send_time

    // ************************************************************************************************
    // Go to sleep
    case zzz:
    
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

  
      // This while loop keeps the processor asleep until INTERVAL seconds have passed
      while (!interval_alarm) // Wake up every INTERVAL seconds
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

      // Initialize for low power in the power control block.  (redundant?)
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

      // Restore the TX/RX connections between the Artemis module and the CH340S
      am_hal_gpio_pinconfig(48 /* TXO-0 */, g_AM_BSP_GPIO_COM_UART_TX);
      am_hal_gpio_pinconfig(49 /* RXI-0 */, g_AM_BSP_GPIO_COM_UART_RX);

      // Reenable the debugger GPIOs
      am_hal_gpio_pinconfig(20 /* SWDCLK */, g_AM_BSP_GPIO_SWDCK);
      am_hal_gpio_pinconfig(21 /* SWDIO */, g_AM_BSP_GPIO_SWDIO);

      // Turn on ADC
      ap3_adc_setup();

      // Do it all again!
      loop_step = send_time;
  
      break; // End of case wake

  } // End of switch (loop_step)
} // End of loop()
