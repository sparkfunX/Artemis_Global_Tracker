/*
 Artemis Global Tracker
 Example: GNSS Module Info

 Written by Paul Clark (PaulZC)
 August 15th 2021

 ** Updated for v2.1.0 of the Apollo3 core / Artemis board package **
 ** (At the time of writing, v2.1.1 of the core conatins a feature which makes communication with the u-blox GNSS problematic. Be sure to use v2.1.0) **

 ** Set the Board to "RedBoard Artemis ATP" **
 ** (The Artemis Module does not have a Wire port defined, which prevents the GNSS library from compiling) **

 This example powers up the ZOE-M8Q and reads the module info (including the protocol version).
 
 You will need to install the SparkFun u-blox library before this example
 will run successfully:
 https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library

 The ZOE-M8Q shares I2C Port 1 with the MS8607: SCL = D8; SDA = D9

 Power for the ZOE is switched via Q2.
 D26 needs to be pulled low to enable the GNSS power.

 The ZOE's PIO14 pin (geofence signal) is conected to D10,
 so let's configure D10 as an input even though we aren't using it yet.

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

#define MAX_PAYLOAD_SIZE 384 // Override MAX_PAYLOAD_SIZE for getModuleInfo which can return up to 348 bytes
#include "SparkFun_u-blox_GNSS_Arduino_Library.h" //http://librarymanager/All#SparkFun_u-blox_GNSS

// Extend the class for getModuleInfo
class SFE_UBLOX_GNSS_ADD : public SFE_UBLOX_GNSS
{
public:
    boolean getModuleInfo(uint16_t maxWait = 1100); //Queries module, texts

    struct minfoStructure // Structure to hold the module info (uses 341 bytes of RAM)
    {
        char swVersion[30];
        char hwVersion[10];
        uint8_t extensionNo = 0;
        char extension[10][30];
    } minfo;
};

SFE_UBLOX_GNSS_ADD myGNSS;

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
  Serial.println(F("Example: GNSS Module Info"));
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

  // setPacketCfgPayloadSize tells the library how many bytes our customPayload can hold.
  // If we call it after the .begin, the library will attempt to resize the existing 256 byte payload buffer
  // by creating a new buffer, copying across the contents of the old buffer, and then delete the old buffer.
  // This uses a lot of RAM and causes the code to fail on the ATmega328P. (We are also allocating another 341 bytes for minfo.)
  // To keep the code ATmega328P compliant - don't call setPacketCfgPayloadSize after .begin. Call it here instead.    
  myGNSS.setPacketCfgPayloadSize(MAX_PAYLOAD_SIZE);

  if (myGNSS.begin(agtWire) == false) //Connect to the u-blox module using pads 8 & 9
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address. Please check wiring. Freezing."));
    while (1);
  }

  //myGNSS.factoryDefault(); delay(5000); // Uncomment this line to reset the ZOE-M8Q to the factory defaults

  //myGNSS.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial
  
  myGNSS.setI2COutput(COM_TYPE_UBX); // Limit I2C output to UBX (disable the NMEA noise)
  
  Serial.print(F("Polling module info"));
  if (myGNSS.getModuleInfo(1100) == false) // Try to get the module info
  {
      Serial.print(F("getModuleInfo failed! Freezing..."));
      while (1)
          ;
  }

  Serial.println();
  Serial.println();
  Serial.println(F("Module Info : "));
  Serial.print(F("Soft version: "));
  Serial.println(myGNSS.minfo.swVersion);
  Serial.print(F("Hard version: "));
  Serial.println(myGNSS.minfo.hwVersion);
  Serial.print(F("Extensions:"));
  Serial.println(myGNSS.minfo.extensionNo);
  for (int i = 0; i < myGNSS.minfo.extensionNo; i++)
  {
      Serial.print("  ");
      Serial.println(myGNSS.minfo.extension[i]);
  }
  Serial.println();
  Serial.println(F("Done!"));
}

void loop()
{
  // Nothing to do here...
}

boolean SFE_UBLOX_GNSS_ADD::getModuleInfo(uint16_t maxWait)
{
    myGNSS.minfo.hwVersion[0] = 0;
    myGNSS.minfo.swVersion[0] = 0;
    for (int i = 0; i < 10; i++)
        myGNSS.minfo.extension[i][0] = 0;
    myGNSS.minfo.extensionNo = 0;

    // Let's create our custom packet
    uint8_t customPayload[MAX_PAYLOAD_SIZE]; // This array holds the payload data bytes

    // setPacketCfgPayloadSize tells the library how many bytes our customPayload can hold.
    // If we call it here, after the .begin, the library will attempt to resize the existing 256 byte payload buffer
    // by creating a new buffer, copying across the contents of the old buffer, and then delete the old buffer.
    // This uses a lot of RAM and causes the code to fail on the ATmega328P. (We are also allocating another 341 bytes for minfo.)
    // To keep the code ATmega328P compliant - don't call setPacketCfgPayloadSize here. Call it before .begin instead.
    //myGNSS.setPacketCfgPayloadSize(MAX_PAYLOAD_SIZE);

    // The next line creates and initialises the packet information which wraps around the payload
    ubxPacket customCfg = {0, 0, 0, 0, 0, customPayload, 0, 0, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED};

    // The structure of ubxPacket is:
    // uint8_t cls           : The message Class
    // uint8_t id            : The message ID
    // uint16_t len          : Length of the payload. Does not include cls, id, or checksum bytes
    // uint16_t counter      : Keeps track of number of overall bytes received. Some responses are larger than 255 bytes.
    // uint16_t startingSpot : The counter value needed to go past before we begin recording into payload array
    // uint8_t *payload      : The payload
    // uint8_t checksumA     : Given to us by the module. Checked against the rolling calculated A/B checksums.
    // uint8_t checksumB
    // sfe_ublox_packet_validity_e valid            : Goes from NOT_DEFINED to VALID or NOT_VALID when checksum is checked
    // sfe_ublox_packet_validity_e classAndIDmatch  : Goes from NOT_DEFINED to VALID or NOT_VALID when the Class and ID match the requestedClass and requestedID

    // sendCommand will return:
    // SFE_UBLOX_STATUS_DATA_RECEIVED if the data we requested was read / polled successfully
    // SFE_UBLOX_STATUS_DATA_SENT     if the data we sent was writted successfully (ACK'd)
    // Other values indicate errors. Please see the sfe_ublox_status_e enum for further details.

    // Referring to the u-blox M8 Receiver Description and Protocol Specification we see that
    // the module information can be read using the UBX-MON-VER message. So let's load our
    // custom packet with the correct information so we can read (poll / get) the module information.

    customCfg.cls = UBX_CLASS_MON; // This is the message Class
    customCfg.id = UBX_MON_VER;    // This is the message ID
    customCfg.len = 0;             // Setting the len (length) to zero let's us poll the current settings
    customCfg.startingSpot = 0;    // Always set the startingSpot to zero (unless you really know what you are doing)

    // Now let's send the command. The module info is returned in customPayload

    if (sendCommand(&customCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
        return (false); //If command send fails then bail

    // Now let's extract the module info from customPayload

    uint16_t position = 0;
    for (int i = 0; i < 30; i++)
    {
        minfo.swVersion[i] = customPayload[position];
        position++;
    }
    for (int i = 0; i < 10; i++)
    {
        minfo.hwVersion[i] = customPayload[position];
        position++;
    }

    while (customCfg.len >= position + 30)
    {
        for (int i = 0; i < 30; i++)
        {
            minfo.extension[minfo.extensionNo][i] = customPayload[position];
            position++;
        }
        minfo.extensionNo++;
        if (minfo.extensionNo > 9)
            break;
    }

    return (true); //Success!
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
