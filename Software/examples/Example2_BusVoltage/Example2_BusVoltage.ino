/*
 Artemis Global Tracker
 Example: Bus Voltage

 Written by Paul Clark (PaulZC)
 August 7th 2021

 ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
 ** Set the Board to "RedBoard Artemis ATP" **

 Based on the Artemis analogRead example
 By: Nathan Seidle
 SparkFun Electronics
 License: MIT. See license file for more information but you can
 basically do whatever you want with this code.

 Enables the bus voltage measurement circuit and reports the voltage

 If this example runs successfully, you can (hopefully) be confident that:
 The bus voltage measurement circuit is working

 Setting pin D34 high enables the bus voltage measurement circuit (via Q3 and Q4)
 The voltage on pin AD13 is the bus voltage divided by 3
 The Apollo3 internal ADC VADCREF is set to 2.0 Volts
 This example sets the analogReadResolution to 14-bits (16384)
 So, to convert the analogRead into Volts, we multiply by: 3.0 * 1.09 * 2.0 / 16384.0
 (The 1.09 is a correction factor for the divider source impedance)
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

  pinMode(busVoltageMonEN, OUTPUT); // Make the Bus Voltage Monitor Enable an output
  digitalWrite(busVoltageMonEN, HIGH); // Set it high to enable the measurement
  //digitalWrite(busVoltageMonEN, LOW); // Set it low to disable the measurement (busV should be ~zero)
  
  analogReadResolution(14); //Set resolution to 14 bit

  // Start the console serial port
  Serial.begin(115200);
  while (!Serial) // Wait for the user to open the serial monitor
    ;
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Example: Bus Voltage"));
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

}

void loop()
{
  digitalWrite(LED, LOW);

  int busV3 = analogRead(busVoltagePin); //Automatically sets pin to analog input
  Serial.print(F("busV/3: "));
  Serial.print(busV3);

  float busV = (float)busV3 * 3.0 * 1.09 * 2.0 / 16384.0; //Convert 1/3 busV to busV (correction factor 1.09)
  Serial.print(F(" busV: "));
  Serial.print(busV, 2);
  Serial.print(F("V"));

  //TODO enable battload
  int div3 = analogReadVCCDiv3(); //Read VCC across a 1/3 resistor divider
  Serial.print(F("   VCC/3: "));
  Serial.print(div3);

  float vcc = (float)div3 * 3.0 * 2.0 / 16384.0; //Convert 1/3 VCC to VCC
  Serial.print(F(" VCC: "));
  Serial.print(vcc, 2);
  Serial.print(F("V"));

  float internalTemp = getTempDegC(); // Read internal temperature sensor
  Serial.print(F("   internalTemp: "));
  Serial.print(internalTemp, 2);

  int vss = analogReadVSS(); //Read internal VSS (should be 0)
  Serial.print(F("   VSS: "));
  Serial.print(vss);

  Serial.println();

  digitalWrite(LED, HIGH);
  delay(1000);
}
