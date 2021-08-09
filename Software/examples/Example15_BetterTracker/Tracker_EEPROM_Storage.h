// Artemis Global Tracker: BetterTracker EEPROM storage

#ifndef TrackerEEPROM
#define TrackerEEPROM

// Data type storage lengths when stored in EEPROM
#define LEN_BYTE    1
#define LEN_INT16   2
#define LEN_INT32   4
#define LEN_FLOAT   4

// Define the storage lengths for the message fields that will be stored in EEPROM
#define LEN_STX       LEN_BYTE    // STX is byte
#define LEN_SOURCE    LEN_INT32   // SOURCE is uint32
#define LEN_DEST      LEN_INT32   // DEST is uint32
#define LEN_TXINT     LEN_INT16   // TXINT is uint16
#define LEN_ETX       LEN_BYTE    // ETX is byte
#define LEN_CSUMA     LEN_BYTE    // ChecksumA is byte
#define LEN_CSUMB     LEN_BYTE    // ChecksumB is byte

// Define the storage locations for each message field that will be stored in EEPROM
#define LOC_STX       0 // Change this if you want the data to be stored higher up in the EEPROM
#define LOC_SOURCE    LOC_STX + LEN_STX
#define LOC_DEST      LOC_SOURCE + LEN_SOURCE
#define LOC_TXINT     LOC_DEST + LEN_DEST
#define LOC_ETX       LOC_TXINT + LEN_TXINT
#define LOC_CSUMA     LOC_ETX + LEN_ETX
#define LOC_CSUMB     LOC_CSUMA + LEN_CSUMA

// Define the default value for each message field
#define DEF_STX       0x02
#define DEF_ETX       0x03

// Define the struct for the tracker settings (stored in RAM and copied to or loaded from EEPROM)
// myTrackerSettings.TXINT is accessed by the rtc ISR and so needs to be volatile
typedef struct 
{
  byte STX;                 // 0x02 - when written to EEPROM, helps indicate if EEPROM contains valid data
  uint32_t SOURCE;          // The tracker's RockBLOCK serial number
  uint32_t DEST;            // The destination RockBLOCK serial number for message forwarding
  volatile uint16_t TXINT;  // The message transmit interval in minutes
  byte ETX;                 // 0x03 - when written to EEPROM, helps indicate if EEPROM contains valid data
} trackerSettings;

#endif
