/*
 Artemis Global Tracker
 Example: Pressure, Humidity, Temperature

 Written by Paul Clark (PaulZC)
 August 7th 2021

 ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
 ** Set the Board to "RedBoard Artemis ATP" **

 ** (The Artemis Module does not have a Wire port defined, which prevents the MS8607 library from compiling) **

 This example reads the pressure, humidity and temperature from the on-board MS8607 sensor.
 
 You will also need to install the Qwiic_PHT_MS8607_Library:
 https://github.com/sparkfun/SparkFun_PHT_MS8607_Arduino_Library
 (Available through the Arduino Library Manager: search for SparkFun MS8607)

 Basic information on how to install an Arduino library is available here:
 https://learn.sparkfun.com/tutorials/installing-an-arduino-library

 The MS8607 is connected to I2C Port 1: SCL = D8; SDA = D9

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

#include <Wire.h>
TwoWire agtWire(9,8); //Create an I2C port using pads 8 (SCL) and 9 (SDA)

#include <SparkFun_PHT_MS8607_Arduino_Library.h> //http://librarymanager/All#SparkFun_MS8607

//Create an instance of the MS8607 object
MS8607 barometricSensor;

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
  // Configure the I/O pins
  pinMode(LED, OUTPUT);

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
  gnssOFF(); // Disable power for the GNSS

  // Set up the I2C pins
  agtWire.begin();

  // Start the console serial port
  Serial.begin(115200);
  while (!Serial) // Wait for the user to open the serial monitor
    ;
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Example: PHT"));
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

  if (barometricSensor.begin(agtWire) == false)
  {
    Serial.println("MS8607 sensor did not respond. Trying again...");
    if (barometricSensor.begin(agtWire) == false)
    {
      Serial.println("MS8607 sensor did not respond. Please check wiring.");
      while(1)
        ;
    }
  }
}

void loop()
{
  digitalWrite(LED, LOW);

  float temperature = barometricSensor.getTemperature();
  float pressure = barometricSensor.getPressure();
  float humidity = barometricSensor.getHumidity();

  Serial.print("Temperature=");
  Serial.print(temperature, 1);
  Serial.print("(C)");

  Serial.print(" Pressure=");
  Serial.print(pressure, 3);
  Serial.print("(hPa or mbar)");


  Serial.print(" Humidity=");
  Serial.print(humidity, 1);
  Serial.print("(%RH)");

  Serial.println();

  digitalWrite(LED, HIGH);
  delay(500);
}
