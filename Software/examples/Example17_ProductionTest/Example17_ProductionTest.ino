/*
 Artemis Global Tracker
 Production Test Code
 
 ** Requires the Artemis Global Tracker Test Header **

 Written by Paul Clark (PaulZC)
 20th March 2020

 ** Set the Board to "SparkFun Artemis Module" **

 This code tests each component on the AGT.
 The Iridium 9603N connections are tested using the AGT Test Header.
 
 _Before_ connecting the USB-C cable:
 carefully insert the test header into the Samtec socket;
 secure the test header with at least one screw;
 connect the Qwiic socket on the test header to the Qwiic socket
 on the AGT using a short Qwiic cable.

 Helpful messages will appear in the Serial Monitor (115200 Baud) as the test progresses.

 You will need to install the Qwiic_PHT_MS8607_Library before this example
 will run successfully:
 https://github.com/PaulZC/Qwiic_PHT_MS8607_Library
 (Search for MS8607 in the Library Manager)

 You will need to install the SparkFun u-blox library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library

*/

//#define noFail // Uncomment this line to allow the test to continue even though it has failed

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
#define iridiumTx           25 // The Iridium 9603N Tx pin (RX_OUT) (Serial1 Rx)
#define iridiumRx           24 // The Iridium 9603N Rx pin (TX_IN) (Serial1 Tx)
// Make sure you do not have gnssEN and iridiumPwrEN enabled at the same time!
// If you do, bad things might happen to the AS179 RF switch!

// Define the bit masks for the IO pin states (for command 0x55)
#define ON_OFF_BIT_MASK 0x1000
#define RX_OUT_BIT_MASK 0x0800
#define DCD_BIT_MASK 0x0400
#define CTS_BIT_MASK 0x0200
#define RTS_BIT_MASK 0x0100
#define RES_17_BIT_MASK 0x0080
#define NA_BIT_MASK 0x0040
#define TX_IN_BIT_MASK 0x0020
#define DSR_BIT_MASK 0x0010
#define RI_BIT_MASK 0x0008
#define DTR_BIT_MASK 0x0004
#define RES_16_BIT_MASK 0x0002
#define SUPPLY_BIT_MASK 0x0001

#include <Wire.h>
TwoWire testWire(4); //Will use Artemis pads 39/40 to talk to the AGT Test Header

#include <MS8607_Library.h>

//Create an instance of the MS8607 object
//The on-board MS8607 is connected to I2C Port 1 (Wire1): SCL = D8; SDA = D9
MS8607 barometricSensor;

#include "SparkFun_Ublox_Arduino_Library.h" //http://librarymanager/All#SparkFun_Ublox_GPS
//The ZOE-M8Q shares I2C Port 1 (Wire1) with the MS8607: SCL = D8; SDA = D9
SFE_UBLOX_GPS myGPS;

//Globals
bool testFailed = false; // Flag to show if the test failed (but might have continued anyway if noFail is defined)
volatile bool geofenceFlag = false; // Flag to show if the geofence pin has changed state

// geofencePin Interrupt Service Routine
// (Always keep ISRs as short as possible, don't do anything clever in them,
//  and always use volatile variables if the main loop needs to access them too.)
void geofenceISR(void)
{
  geofenceFlag = true; // Update the flag
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
  // Configure the I/O pins
  pinMode(LED, OUTPUT);

  pinMode(iridiumPwrEN, OUTPUT); // Configure the Iridium Power Pin (connected to the ADM4210 ON pin)
  digitalWrite(iridiumPwrEN, LOW); // Disable Iridium Power
  pinMode(superCapChgEN, OUTPUT); // Configure the super capacitor charger enable pin (connected to LTC3225 !SHDN)
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
  
  gnssOFF(); // Disable power for the GNSS
  pinMode(geofencePin, INPUT); // Configure the geofence pin as an input
  attachInterrupt(digitalPinToInterrupt(geofencePin), geofenceISR, CHANGE); // Call geofenceISR whenever geofencePin changes state

  pinMode(iridiumSleep, INPUT_PULLUP); // Iridium 9603N On/Off (Sleep) pin
  pinMode(iridiumRI, INPUT_PULLUP); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT_PULLUP); // Configure the Iridium Network Available as an input
  pinMode(iridiumTx, INPUT_PULLUP); // Configure the pin connected to the Iridium 9603N Tx (RX_OUT)
  pinMode(iridiumRx, INPUT_PULLUP); // Configure the pin connected to the Iridium 9603N Rx (TX_IN)
  pinMode(superCapPGOOD, INPUT); // Configure the super capacitor charger PGOOD input

  pinMode(busVoltageMonEN, OUTPUT); // Make the Bus Voltage Monitor Enable an output
  digitalWrite(busVoltageMonEN, HIGH); // Enable the bus voltage monitor
  analogReadResolution(14); //Set resolution to 14 bit

  // Set up the I2C pins for the test header
  testWire.begin();

  // Set up the I2C pins for the MS8607
  Wire1.begin();

  // Start the console serial port
  Serial.begin(115200);

  // Comment the next two lines if you want the code to start automatically
  // - without needing to open the Serial Monitor.
  // The OK LED on the test header will indicate if the test was successful.
  while (!Serial) // Wait for the user to open the serial monitor
    ;

  delay(100);
  Serial.println();
  Serial.println();
  Serial.println(F("Artemis Global Tracker"));
  Serial.println(F("Production Test"));
  Serial.println();
  Serial.println(F("Please make sure the AGT Test Header is connected before running this test"));
  Serial.println();

  //empty the serial buffer
  while(Serial.available() > 0)
    Serial.read();

  //wait for the user to press any key before beginning
  Serial.println(F("Please check that the Serial Monitor is set to 115200 Baud"));
  Serial.println(F("and that the line ending is set to Newline."));
  Serial.println(F("Then click Send to start the test."));
  Serial.println();
  while(Serial.available() == 0)
    ;

  //turn the green LED on the test header off
  testWire.beginTransmission(0x63);
  testWire.write(0x22); // 0x22 turns the LED off
  testWire.endTransmission();

// -----------------------------------------------------------------------
// Test 1 - check the bus voltage and VCC
// -----------------------------------------------------------------------

  float busV = ((float)analogRead(busVoltagePin)) * 3.0 * 2.0 / 16384.0;
  Serial.print(F("Test 1 Step 1: The bus voltage is: "));
  Serial.print(busV, 2);
  Serial.println(F("V"));

  if (busV < 4.25)
  {
    Serial.println(F("Test 1 Step 1: The bus voltage is too low!"));
    fail();
  }
  if (busV > 5.25)
  {
    Serial.println(F("Test 1 Step 1: The bus voltage is too high!"));
    fail();
  }

  int div3 = analogRead(ADC_INTERNAL_VCC_DIV3); //Read VCC across a 1/3 resistor divider
  float vcc = (float)div3 * 3.0 * 2.0 / 16384.0; //Convert 1/3 VCC to VCC
  Serial.print(F("Test 1 Step 2: VCC is: "));
  Serial.print(vcc, 2);
  Serial.println(F("V"));

  if (vcc < 3.1)
  {
    Serial.println(F("Test 1 Step 2: VCC is too low!"));
    fail();
  }
  if (vcc > 3.5)
  {
    Serial.println(F("Test 1 Step 2: VCC is too high!"));
    fail();
  }

  int vss = analogRead(ADC_INTERNAL_VSS); //Read internal VSS (should be 0)
  Serial.print(F("Test 1 Step 3: VSS: "));
  Serial.print(vss);
  Serial.println();

  if (vss > 100)
  {
    Serial.println(F("Test 1 Step 3: VSS is too high!"));
    fail();
  }

  if (testFailed == false)
  {
    Serial.println(F("Test 1: Passed"));
  }

// -----------------------------------------------------------------------
// Test 2 - check the on-board MS8607
// -----------------------------------------------------------------------

  if (barometricSensor.begin(Wire1) == false)
  {
    Serial.println(F("Test 2 Step 1: MS8607 sensor did not respond. Trying again..."));
    if (barometricSensor.begin(Wire1) == false)
    {
      Serial.println(F("Test 2 Step 1: MS8607 sensor did not respond (2nd attempt)!"));
      fail();
    }
  }

  float temperature = barometricSensor.getTemperature();
  float pressure = barometricSensor.getPressure();
  float humidity = barometricSensor.getHumidity();

  Serial.print(F("Test 2 Step 2: Temperature="));
  Serial.print(temperature, 1);
  Serial.println(F("(C)"));

  if (temperature < 10.0)
  {
    Serial.println(F("Test 2 Step 2: Temperature is too low!"));
    fail();
  }
  if (temperature > 30.0)
  {
    Serial.println(F("Test 2 Step 2: Temperature is too high!"));
    fail();
  }

  Serial.print(F("Test 2 Step 3: Pressure="));
  Serial.print(pressure, 3);
  Serial.println(F("(hPa or mbar)"));

  if (pressure < 750.0)
  {
    Serial.println(F("Test 2 Step 3: Pressure is too low!"));
    fail();
  }
  if (pressure > 1080.0)
  {
    Serial.println(F("Test 2 Step 3: Pressure is too high!"));
    fail();
  }

  Serial.print(F("Test 2 Step 4: Humidity="));
  Serial.print(humidity, 1);
  Serial.println(F("(%RH)"));

  if (humidity < 10.0)
  {
    Serial.println(F("Test 2 Step 4: Humidity is too low!"));
    fail();
  }
  if (humidity > 90.0)
  {
    Serial.println(F("Test 2 Step 4: Humidity is too high!"));
    fail();
  }

  if (testFailed == false)
  {
    Serial.println(F("Test 2: Passed"));
  }

// -----------------------------------------------------------------------
// Test 3 - check the ZOE-M8Q GNSS
// -----------------------------------------------------------------------

  Serial.println(F("Test 3 Step 1: Enabling the ZOE GNSS (takes 4 seconds)"));
  Serial.println(F("Test 3 Step 1: The blue GNSS LED on the Tracker should be lit up."));

  gnssON(); // Enable power for the ZOE
  delay(2000); // Let the ZOE power up

  if (myGPS.begin(Wire1) == false) //Connect to the Ublox module using Wire port
  {
    Serial.println(F("Test 3 Step 1: ZOE not detected at default I2C address!"));
    gnssOFF();
    fail();
  }
  if (myGPS.isConnected() == false) //Check the connection
  {
    Serial.println(F("Test 3 Step 1: ZOE not connected!"));
    gnssOFF();
    fail();
  }

  //myGPS.enableDebugging(); // Enable debug messages
  myGPS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)

  delay(2000); // Wait for NMEA data to finish

  // Now then... During the production test, the ZOE won't have an antenna connection so we need
  // to be creative about how we test it.
  // The ZOE's clock will be running (but not sync'd to GNSS time). So let's read the
  // time of week twice and make sure it is incrementing like we'd expect.
  uint32_t millis1 = myGPS.getTimeOfWeek(); // Read the time of week (this increments without an antenna)
  delay(1100); // Wait for > 1 sec
  uint32_t millis2 = myGPS.getTimeOfWeek(); // Read the time of week again
  if ((millis2 - millis1) < 1000) // If the two TOWs are not at least 1 sec apart then we have a problem
  {
    Serial.println(F("Test 3 Step 2: The GPS Time Of Week did not increment! Trying again..."));
    millis1 = myGPS.getTimeOfWeek(); // Read the time of week (this increments without an antenna)
    delay(1100); // Wait for > 1 sec
    millis2 = myGPS.getTimeOfWeek(); // Read the time of week again
    if ((millis2 - millis1) < 1000) // If the two TOWs are not at least 1 sec apart then we have a problem
    {
      Serial.println(F("Test 3 Step 2: The GPS Time Of Week did not increment!"));
      gnssOFF();
      fail();    
    }
  }

  // OK. The GNSS is talking and its clock is updating. So far so good.
  // Let's see if the geofence pin (PIO14) is connected.
  // We'll do that by:
  //  leaving the standard NMEA messages enabled on UART1;
  //  ensuring PIO14 isn't allocated for antenna control with clearAntPIO();
  //  making PIO14 the txReady pin for UART1;
  //  request a PVT on I2C - when we get it we can be confident that UART1 will still be transmitting its NMEA messages;
  //  sample the txReady pin then (while UART1 is transmitting);
  //  changing the txReady polarity from high-active to low-active (with zero threshold).

  myGPS.clearAntPIO(); // Ensure PIO14 is not allocated to the ANT

  // Let's create the custom packetCfg to let us change the UBX_CFG_PRT
  uint8_t customPayloadCfg[MAX_PAYLOAD_SIZE];
  ubxPacket customPacketCfg = {0, 0, 0, 0, 0, customPayloadCfg, 0, 0, false};

  // The UART1 timing is a bit tricky, we need to sample txReady while UART1 is transmitting,
  // so we'll attempt this test up to three times if required.

  byte attempts = 3; // Limit the number of attempts
  bool txReadyPassed = true; // Use this to flag if the test was successful

  while (attempts > 0) // Keep repeating the test while attempts is > zero
  {
    txReadyPassed = true; // Set the test passed flag (it will be cleared by a fail)
  
    customPacketCfg.cls = UBX_CLASS_CFG;
    customPacketCfg.id = UBX_CFG_PRT;
    customPacketCfg.len = 20;
    customPacketCfg.startingSpot = 0;
    customPayloadCfg[0] = 0x01; // portID = 1 (UART1)
    customPayloadCfg[1] = 0; // reserved1
    // thres=0x000 | pin=14 | pol=0 (high active) | en=1 : 000000000 01110 0 1 = 0x0039
    // (Don't forget the u-blox X2 is little endian!)
    customPayloadCfg[2] = 0x39; // txReady
    customPayloadCfg[3] = 0x00; // txReady
    // nStopBits=0 (1 stop bit) | parity=4 (none) | charLen=3 (8-bit) : 000000000000000000 00 100 0 11 000000 = 0x000008C0
    customPayloadCfg[4] = 0xC0; // mode
    customPayloadCfg[5] = 0x08;
    customPayloadCfg[6] = 0x00;
    customPayloadCfg[7] = 0x00;
    // baud rate = 9600 = 0x00002580
    customPayloadCfg[8] = 0x80; // baudRate
    customPayloadCfg[9] = 0x25;
    customPayloadCfg[10] = 0x00;
    customPayloadCfg[11] = 0x00;
    customPayloadCfg[12] = 0x03; // inProtoMask = 3 (ubx + NMEA)
    customPayloadCfg[13] = 0x00;
    customPayloadCfg[14] = 0x03; // outProtoMask = 3 (ubx + NMEA)
    customPayloadCfg[15] = 0x00;
    customPayloadCfg[16] = 0x00; // flags (no extendedTxTimeout)
    customPayloadCfg[17] = 0x00;
    customPayloadCfg[18] = 0x00; // reserved2
    customPayloadCfg[19] = 0x00;  
  
    Serial.println(F("Test 3 Step 3: Updating the ZOE geofence/txReady pin"));
  
    // Send the custom packetCfg.
    // This will always timeout due to a 'feature' in the u-blox library
    // but we don't care about that...
    myGPS.sendCommand(customPacketCfg, 0);
  
    // Now let's ask for the PVT message via I2C
    // We can be confident that NMEA will be being transmitted on UART1 at the same time (but slower)
    myGPS.getPVT();

    // Now let's check the geofence/txReady pin (PIO14). It should be high indicating there is data waiting.
    if (digitalRead(geofencePin) != HIGH)
    {
      Serial.println(F("Test 3 Step 3: Possible short on the ZOE-M8Q PIO14 (geofence) pin?"));
      txReadyPassed = false; // Flag that this attempt has failed
    }
  
    geofenceFlag = false; // Clear the geofence interrupt flag - just before we change the txReady polarity
  
    customPacketCfg.cls = UBX_CLASS_CFG;
    customPacketCfg.id = UBX_CFG_PRT;
    customPacketCfg.len = 20;
    customPacketCfg.startingSpot = 0;
    customPayloadCfg[0] = 0x01; // portID = 1 (UART1)
    customPayloadCfg[1] = 0; // reserved1
    // thres=0x000 | pin=14 | pol=1 (low active) | en=1 : 000000000 01110 1 1 = 0x003B
    // (Don't forget the u-blox X2 is little endian!)
    customPayloadCfg[2] = 0x3B; // txReady
    customPayloadCfg[3] = 0x00; // txReady
    // nStopBits=0 (1 stop bit) | parity=4 (none) | charLen=3 (8-bit) : 000000000000000000 00 100 0 11 000000 = 0x000008C0
    customPayloadCfg[4] = 0xC0; // mode
    customPayloadCfg[5] = 0x08;
    customPayloadCfg[6] = 0x00;
    customPayloadCfg[7] = 0x00;
    // baud rate = 9600 = 0x00002580
    customPayloadCfg[8] = 0x80; // baudRate
    customPayloadCfg[9] = 0x25;
    customPayloadCfg[10] = 0x00;
    customPayloadCfg[11] = 0x00;
    customPayloadCfg[12] = 0x03; // inProtoMask = 3 (ubx + NMEA)
    customPayloadCfg[13] = 0x00;
    customPayloadCfg[14] = 0x03; // outProtoMask = 3 (ubx + NMEA)
    customPayloadCfg[15] = 0x00;
    customPayloadCfg[16] = 0x00; // flags (no extendedTxTimeout)
    customPayloadCfg[17] = 0x00;
    customPayloadCfg[18] = 0x00; // reserved2
    customPayloadCfg[19] = 0x00;  
  
    Serial.println(F("Test 3 Step 4: Updating the ZOE geofence/txReady pin"));
  
    // Send the custom packetCfg.
    // This will always timeout due to a 'feature' in the u-blox library
    // but we don't care about that...
    myGPS.sendCommand(customPacketCfg, 0);
  
    // Now let's ask for the PVT message via I2C
    // We can be confident that NMEA will be being transmitted on UART1 at the same time
    myGPS.getPVT();

    // Now let's check the geofence/txReady pin (PIO14). It should be low indicating there is data waiting.
    if (digitalRead(geofencePin) != LOW)
    {
      Serial.println(F("Test 3 Step 4: Possible open circuit on the ZOE-M8Q PIO14 (geofence) pin?"));
      txReadyPassed = false; // Flag that this attempt has failed
      // Keep going anyway... (Just in case the UART1 timing wasn't right)
    }
  
    // Finally, let's check that the geofence ISR was triggered
    if (geofenceFlag != true)
    {
      Serial.println(F("Test 3 Step 5: No change seen on the ZOE-M8Q PIO14 (geofence) pin. Possible open circuit!"));
      txReadyPassed = false; // Flag that this attempt has failed
      // Failing this part of the test almost certainly indicates that the geofence pin is open circuit
      // but we'll give it another chance just in case...
    }

    // Let's check if the test was successful
    if (txReadyPassed == true)
    {
      attempts = 0; // The test passed so let's set attempts to zero to avoid repeating the test
    }
    else
    {
      Serial.println(F("Repeating Test 3 from Step 3..."));
      attempts = attempts - 1; // Decrement attempts and repeat
    }
  
  } // End of while (attempts > 0)

  // Test is complete so let's turn the ZOE off again
  gnssOFF();

  // Check if the test failed
  if (txReadyPassed == false)
  {
    fail();
  }

  if (testFailed == false)
  {
    Serial.println(F("Test 3: Passed"));
  }

// -----------------------------------------------------------------------
// Test 4 - check the Iridium 9603N pins via the test header
// -----------------------------------------------------------------------

  // All of the 9603N IO pins should be being pulled high by the pull-ups
  // on both the test header and the Artemis. So, we can check for shorts
  // and open circuits by checking that each pin is high.

  uint16_t ioPins = readTestHeaderPins();
  // Bit 12      : ON_OFF
  // Bit 11      : RX_OUT
  // Bit 10      : DCD
  // Bit 9       : CTS
  // Bit 8       : RTS
  // Bit 7       : RES_17
  // Bit 6       : NA
  // Bit 5       : TX_IN
  // Bit 4       : DSR
  // Bit 3       : RI
  // Bit 2       : DTR
  // Bit 1       : RES_16
  // Bit 0 (LSB) : SUPPLY

  Serial.print(F("Test 4 Step 1: IO Pins are: 0x"));
  Serial.println(ioPins, HEX);

  // Check if the ON_OFF pin is high
  if ((ioPins & ON_OFF_BIT_MASK) != ON_OFF_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 1: Iridium 9603N ON_OFF (Pin 5) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the RX_OUT pin is high
  if ((ioPins & RX_OUT_BIT_MASK) != RX_OUT_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 2: Iridium 9603N RX_OUT (Pin 7) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the DCD pin is high
  if ((ioPins & DCD_BIT_MASK) != DCD_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 3: Iridium 9603N DCD (Pin 9) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the CTS pin is high
  if ((ioPins & CTS_BIT_MASK) != CTS_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 4: Iridium 9603N CTS (Pin 11) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the RTS pin is low
  if ((ioPins & RTS_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 5: Iridium 9603N RTS (Pin 13) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the RES_17 pin is high
  if ((ioPins & RES_17_BIT_MASK) != RES_17_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 6: Iridium 9603N RES_17 (Pin 17) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the NA pin is high
  if ((ioPins & NA_BIT_MASK) != NA_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 7: Iridium 9603N NA (Pin 19) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the TX_IN pin is high
  if ((ioPins & TX_IN_BIT_MASK) != TX_IN_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 8: Iridium 9603N TX_IN (Pin 6) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the DSR pin is high
  if ((ioPins & DSR_BIT_MASK) != DSR_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 9: Iridium 9603N DSR (Pin 10) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the RI pin is high
  if ((ioPins & RI_BIT_MASK) != RI_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 10: Iridium 9603N RI (Pin 12) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the DTR pin is low
  if ((ioPins & DTR_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 11: Iridium 9603N DTR (Pin 14) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the RES_16 pin is high
  if ((ioPins & RES_16_BIT_MASK) != RES_16_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 12: Iridium 9603N RES_16 (Pin 16) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the SUPPLY pin is high
  if ((ioPins & SUPPLY_BIT_MASK) != SUPPLY_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 13: Iridium 9603N SUPPLY (Pin 20) is LOW. Possible short circuit!"));
    fail();
  }

  // Now pull the Artemis pins low and check each pin again
  pinMode(iridiumSleep, OUTPUT); // Configure the Iridium 9603N On/Off (Sleep) pin
  digitalWrite(iridiumSleep, LOW);
  pinMode(iridiumRI, OUTPUT); // Configure the Iridium Ring Indicator pin
  digitalWrite(iridiumRI, LOW);
  pinMode(iridiumNA, OUTPUT); // Configure the Iridium Network Available pin
  digitalWrite(iridiumNA, LOW);
  pinMode(iridiumTx, OUTPUT); // Configure the pin connected to the Iridium 9603N Tx
  digitalWrite(iridiumTx, LOW);
  pinMode(iridiumRx, OUTPUT); // Configure the pin connected to the Iridium 9603N Rx
  digitalWrite(iridiumRx, LOW);

  delay(100); // Let the pins settle

  ioPins = readTestHeaderPins(); // Update ioPins

  Serial.print(F("Test 4 Step 14: IO Pins are: 0x"));
  Serial.println(ioPins, HEX);

  // Check if the ON_OFF pin is low
  if ((ioPins & ON_OFF_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 14: Iridium 9603N ON_OFF (Pin 5) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the RX_OUT pin is low
  if ((ioPins & RX_OUT_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 15: Iridium 9603N RX_OUT (Pin 7) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the DCD pin is high
  if ((ioPins & DCD_BIT_MASK) != DCD_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 16: Iridium 9603N DCD (Pin 9) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the CTS pin is high
  if ((ioPins & CTS_BIT_MASK) != CTS_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 17: Iridium 9603N CTS (Pin 11) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the RTS pin is low
  if ((ioPins & RTS_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 18: Iridium 9603N RTS (Pin 13) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the RES_17 pin is high
  if ((ioPins & RES_17_BIT_MASK) != RES_17_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 19: Iridium 9603N RES_17 (Pin 17) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the NA pin is low
  if ((ioPins & NA_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 20: Iridium 9603N NA (Pin 19) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the TX_IN pin is low
  if ((ioPins & TX_IN_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 21: Iridium 9603N TX_IN (Pin 6) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the DSR pin is high
  if ((ioPins & DSR_BIT_MASK) != DSR_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 22: Iridium 9603N DSR (Pin 10) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the RI pin is low
  if ((ioPins & RI_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 23: Iridium 9603N RI (Pin 12) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the DTR pin is low
  if ((ioPins & DTR_BIT_MASK) != 0x0000)
  {
    Serial.println(F("Test 4 Step 24: Iridium 9603N DTR (Pin 14) is HIGH. Possible open circuit!"));
    fail();
  }
  // Check if the RES_16 pin is high
  if ((ioPins & RES_16_BIT_MASK) != RES_16_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 25: Iridium 9603N RES_16 (Pin 16) is LOW. Possible short circuit!"));
    fail();
  }
  // Check if the SUPPLY pin is high
  if ((ioPins & SUPPLY_BIT_MASK) != SUPPLY_BIT_MASK)
  {
    Serial.println(F("Test 4 Step 26: Iridium 9603N SUPPLY (Pin 20) is LOW. Possible short circuit!"));
    fail();
  }

  // The IO pins all check out OK.
  // Tidy up the Artemis pins.
  pinMode(iridiumSleep, INPUT_PULLUP); // Iridium 9603N On/Off (Sleep) pin
  pinMode(iridiumRI, INPUT_PULLUP); // Configure the Iridium Ring Indicator as an input
  pinMode(iridiumNA, INPUT_PULLUP); // Configure the Iridium Network Available as an input
  pinMode(iridiumTx, INPUT_PULLUP); // Configure the pin connected to the Iridium 9603N Tx (RX_OUT)
  pinMode(iridiumRx, INPUT_PULLUP); // Configure the pin connected to the Iridium 9603N Rx (TX_IN)

  if (testFailed == false)
  {
    Serial.println(F("Test 4: Passed"));
  }

// -----------------------------------------------------------------------
// Test5 - test the supercapacitor charger
// -----------------------------------------------------------------------

  // Enable the supercapacitor charger
  Serial.println(F("Test 5 Step 1: Enabling the supercapacitor charger..."));
  digitalWrite(superCapChgEN, HIGH); // Enable the super capacitor charger
  Serial.println(F("Test 5 Step 1: The yellow CHARGING LED on the Tracker should be lit up."));
  delay(2000); // Let the PGOOD signal settle

  // Wait for up to 30 seconds for the supercapacitor charger PGOOD signal to go high
  for (unsigned long tnow = millis(); (digitalRead(superCapPGOOD) == false) && ((millis() - tnow) < (30UL * 1000UL));)
  {
    Serial.println(F("Test 5 Step 2: Waiting for supercapacitors to charge..."));
    delay(1000);
  }
  
  if (digitalRead(superCapPGOOD) == false)
  {
    Serial.println(F("Test 5 Step 2: Supercapacitors failed to charge! Possible fault on the LTC3225. Please look for shorts / dry joints."));
    digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
    fail();
  }
  else
  {
    Serial.println(F("Test 5 Step 2: Supercapacitors charged!"));
  }

  // Enable power for the 9603N
  Serial.println(F("Test 5 Step 3: Enabling 9603N power..."));
  digitalWrite(iridiumPwrEN, HIGH); // Enable power for the 9603N
  Serial.println(F("Test 5 Step 3: 9603N power is enabled. The red 9603N POWER LED on the Tracker should be lit up."));
  delay(2000); // Let the voltage settle

  // Now check the voltage on the two PWR pins
  
  float pwrVolts = readTestHeaderPWR1(); // Read the voltage on PWR_1
  
  Serial.print(F("Test 5 Step 4: The PWR_1 voltage is: "));
  Serial.print(pwrVolts, 2);
  Serial.println(F("V"));

  if (pwrVolts < 5.1)
  {
    Serial.println(F("Test 5 Step 4: The Iridium PWR_1 (Pin 1) voltage is too low!"));
    digitalWrite(iridiumPwrEN, LOW); // Disable power for the 9603N
    digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
    fail();
  }
  if (pwrVolts > 5.4)
  {
    Serial.println(F("Test 5 Step 4: The Iridium PWR_1 (Pin 1) voltage is too high!"));
    digitalWrite(iridiumPwrEN, LOW); // Disable power for the 9603N
    digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
    fail();
  }

  pwrVolts = readTestHeaderPWR2(); // Read the voltage on PWR_2
  
  Serial.print(F("Test 5 Step 5: The PWR_2 voltage is: "));
  Serial.print(pwrVolts, 2);
  Serial.println(F("V"));

  if (pwrVolts < 5.1)
  {
    Serial.println(F("Test 5 Step 5: The Iridium PWR_2 (Pin 2) voltage is too low!"));
    digitalWrite(iridiumPwrEN, LOW); // Disable power for the 9603N
    digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
    fail();
  }
  if (pwrVolts > 5.4)
  {
    Serial.println(F("Test 5 Step 5: The Iridium PWR_2 (Pin 2) voltage is too high!"));
    digitalWrite(iridiumPwrEN, LOW); // Disable power for the 9603N
    digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger
    fail();
  }

  // Test is complete - power down the 9603N
  digitalWrite(iridiumPwrEN, LOW); // Disable power for the 9603N
  digitalWrite(superCapChgEN, LOW); // Disable the super capacitor charger

  if (testFailed == false)
  {
    Serial.println(F("Test 5: Passed"));
  }

// -----------------------------------------------------------------------
// Tests complete!
// -----------------------------------------------------------------------

  Serial.println();
  Serial.println(F("*** Tests complete ***"));
  Serial.println();

  digitalWrite(LED, HIGH); // Turn on the white LED on the Tracker
  Serial.println(F("Please check that the white LED on the Tracker is on"));
  Serial.println();
    
  if (testFailed == false) // If all tests passed, turn the green "OK" LED on the test header on
  {
    // Turn on the green OK LED on the test header
    testWire.beginTransmission(0x63);
    testWire.write(0x11); // 0x11 turns the LED on
    testWire.endTransmission();
  }
  else
  {
    Serial.println(F("!!! At least one test failed... Please check the test results! !!!"));
    Serial.println();
  }

  Serial.println(F("Please check the LiPo charger:"));
  Serial.println(F(" plug a LiPo cell into the LiPo socket"));
  Serial.println(F(" and check that the yellow LiPo LED lights up"));
  Serial.println();
}

void loop()
{
}

// Read the state of the IO pins from the AGT Test Header.
// Returns a uint16_t:
// Bit 12      : ON_OFF
// Bit 11      : RX_OUT
// Bit 10      : DCD
// Bit 9       : CTS
// Bit 8       : RTS
// Bit 7       : RES_17
// Bit 6       : NA
// Bit 5       : TX_IN
// Bit 4       : DSR
// Bit 3       : RI
// Bit 2       : DTR
// Bit 1       : RES_16
// Bit 0 (LSB) : SUPPLY
uint16_t readTestHeaderPins()
{
  uint16_t retVal = 0; // Use this to store the return value
  
  testWire.beginTransmission(0x63);
  testWire.write(0x55); // 0x55 reads the IO pins
  testWire.endTransmission(false);
  testWire.requestFrom(0x63, 2); // Read two bytes
  
  if (testWire.available() >= 2) // Read the I2C bytes
  {
    uint8_t lsb = testWire.read(); // IO pins data is little endian
    uint8_t msb = testWire.read();
    retVal = (((uint16_t)msb) << 8) | ((uint16_t)lsb);
  }
  
  while (testWire.available())
  {
    testWire.read(); // Mop up any unexpected bytes (hopefully redundant!)
  }

  return (retVal);
}

float readTestHeaderPWR1()
{
  float retVal = 0.0; // Use this to store the return value
  uint16_t deciVolts = 0; // Use this to store the voltage in Volts * 10^-2
  
  testWire.beginTransmission(0x63);
  testWire.write(0x33); // 0x33 reads the PWR_1 voltage
  testWire.endTransmission(false);
  testWire.requestFrom(0x63, 2); // Read two bytes
  
  if (testWire.available() >= 2) // Read the I2C bytes
  {
    uint8_t lsb = testWire.read(); // Voltage data is little endian
    uint8_t msb = testWire.read();
    deciVolts = (((uint16_t)msb) << 8) | ((uint16_t)lsb);
  }
  
  while (testWire.available())
  {
    testWire.read(); // Mop up any unexpected bytes (hopefully redundant!)
  }

  retVal = ((float)deciVolts) / 100.0; // Convert to Volts
  
  return (retVal);
}

float readTestHeaderPWR2()
{
  float retVal = 0.0; // Use this to store the return value
  uint16_t deciVolts = 0; // Use this to store the voltage in Volts * 10^-2
  
  testWire.beginTransmission(0x63);
  testWire.write(0x44); // 0x44 reads the PWR_2 voltage
  testWire.endTransmission(false);
  testWire.requestFrom(0x63, 2); // Read two bytes
  
  if (testWire.available() >= 2) // Read the I2C bytes
  {
    uint8_t lsb = testWire.read(); // Voltage data is little endian
    uint8_t msb = testWire.read();
    deciVolts = (((uint16_t)msb) << 8) | ((uint16_t)lsb);
  }
  
  while (testWire.available())
  {
    testWire.read(); // Mop up any unexpected bytes (hopefully redundant!)
  }

  retVal = ((float)deciVolts) / 100.0; // Convert to Volts
  
  return (retVal);
}

void fail() // Stop the test
{
  Serial.println(F("!!! TEST FAILED! !!!"));
  testFailed = true;
#ifdef noFail
  return;
#else  
  while(1)
    ; // Do nothing more
#endif
}
