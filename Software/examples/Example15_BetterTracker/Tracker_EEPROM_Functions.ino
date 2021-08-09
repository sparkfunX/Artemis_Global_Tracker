// Artemis Global Tracker: BetterTracker EEPROM storage

// These functions provide a robust way of storing the SOURCE, DEST and TXINT variables in non-volatile "EEPROM" (Flash)

#include "Tracker_EEPROM_Storage.h"

byte calculateEEPROMchecksumA() // Calculate the RFC 1145 Checksum A for the EEPROM data
{
  uint32_t csuma = 0;
  for (uint16_t x = LOC_STX; x < (LOC_ETX + LEN_ETX); x += 1) // Calculate a sum of every byte from STX to ETX
  {
    csuma = csuma + EEPROM.read(x);
  }
  return ((byte)(csuma & 0x000000ff));
}

byte calculateEEPROMchecksumB() // Calculate the RFC 1145 Checksum B for the EEPROM data
{
  uint32_t csuma = 0;
  uint32_t csumb = 0;
  for (uint16_t x = LOC_STX; x < (LOC_ETX + LEN_ETX); x += 1) // Calculate a sum of sums for every byte from STX to ETX
  {
    csuma = csuma + EEPROM.read(x);
    csumb = csumb + csuma;
  }
  return ((byte)(csumb & 0x000000ff));
}

bool checkEEPROM(trackerSettings *myTrackerSettings)
// Checks if EEPROM data is valid (i.e. has been initialised) by checking that the STX, ETX and the two checksum bytes are valid
{
  byte stx;
  byte etx;
  EEPROM.get(LOC_STX, stx);
  EEPROM.get(LOC_ETX, etx);
  byte eeprom_csuma;
  byte eeprom_csumb;
  EEPROM.get(LOC_CSUMA, eeprom_csuma);
  EEPROM.get(LOC_CSUMB, eeprom_csumb);
  byte csuma = calculateEEPROMchecksumA();
  byte csumb = calculateEEPROMchecksumB();
  bool result = true;
  result = result && ((stx == myTrackerSettings->STX) && (etx == myTrackerSettings->ETX)); // Check that EEPROM STX and ETX match the values in RAM
  result = result && ((csuma == eeprom_csuma) && (csumb == eeprom_csumb)); // Check that the EEPROM checksums are valid
  result = result && ((stx == DEF_STX) && (etx == DEF_ETX)); // Check that EEPROM STX and ETX are actually STX and ETX (not zero!)
  return (result);
}

void updateEEPROMchecksum() // Update the two EEPROM checksum bytes
{
  byte csuma = calculateEEPROMchecksumA();
  byte csumb = calculateEEPROMchecksumB();
  EEPROM.write(LOC_CSUMA, csuma);
  EEPROM.write(LOC_CSUMB, csumb);
}

void initTrackerSettings(trackerSettings *myTrackerSettings) // Initialises the trackerSettings in RAM with the default values
{
  myTrackerSettings->STX =    DEF_STX;
  myTrackerSettings->SOURCE = DEF_SOURCE;
  myTrackerSettings->DEST =   DEF_DEST;
  myTrackerSettings->TXINT =  (volatile uint16_t)DEF_TXINT;
  myTrackerSettings->ETX =    DEF_ETX;
}

void putTrackerSettings(trackerSettings *myTrackerSettings) // Write the trackerSettings from RAM into EEPROM
{
  EEPROM.erase(); // Erase any old data first
  EEPROM.put(LOC_STX,     myTrackerSettings->STX);
  EEPROM.put(LOC_SOURCE,  myTrackerSettings->SOURCE);
  EEPROM.put(LOC_DEST,    myTrackerSettings->DEST);
  EEPROM.put(LOC_TXINT,   (uint16_t)myTrackerSettings->TXINT);
  EEPROM.put(LOC_ETX,     myTrackerSettings->ETX);
  updateEEPROMchecksum();
}

void updateTrackerSettings(trackerSettings *myTrackerSettings) // Update any changed trackerSettings in EEPROM
{
  EEPROM.update(LOC_STX,     myTrackerSettings->STX);
  EEPROM.update(LOC_SOURCE,  myTrackerSettings->SOURCE);
  EEPROM.update(LOC_DEST,    myTrackerSettings->DEST);
  EEPROM.update(LOC_TXINT,   (uint16_t)myTrackerSettings->TXINT);
  EEPROM.update(LOC_ETX,     myTrackerSettings->ETX);
  updateEEPROMchecksum();
}

void getTrackerSettings(trackerSettings *myTrackerSettings) // Read the trackerSettings from EEPROM into RAM
{
  EEPROM.get(LOC_STX, myTrackerSettings->STX);
  EEPROM.get(LOC_SOURCE, myTrackerSettings->SOURCE);
  EEPROM.get(LOC_DEST, myTrackerSettings->DEST);

  uint16_t tempUint16;
  EEPROM.get(LOC_TXINT, tempUint16);
  myTrackerSettings->TXINT = (volatile uint16_t)tempUint16;
  
  EEPROM.get(LOC_ETX, myTrackerSettings->ETX);
}
