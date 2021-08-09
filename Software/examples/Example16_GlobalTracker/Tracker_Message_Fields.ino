// Definitions and functions to support storing the Artemis Global Tracker settings (message fields) in both RAM and EEPROM
// Also includes functions to parse settings received via serial or MT message

#include "Tracker_Message_Fields.h" // Include the message field and storage definitions

// debug functions
void enableDebugging(Stream &debugPort)
{
  _debugSerial = &debugPort; //Grab which port the user wants us to use for debugging

  _printDebug = true; //Should we print the commands we send? Good for debugging
}
void disableDebugging(void)
{
  _printDebug = false; //Turn off extra print statements
}
#define debugPrint( var ) { if (_printDebug == true) _debugSerial->print( var ); }
#define debugPrintln( var ) { if (_printDebug == true) _debugSerial->println( var ); }


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
  debugPrint(F("checkEEPROM: "));
  if (result == true)
  {
    debugPrintln(F("valid"));
  }
  else
  {
    debugPrintln(F("invalid"));
  }
  return (result);
}

void updateEEPROMchecksum() // Update the two EEPROM checksum bytes
{
  byte csuma = calculateEEPROMchecksumA();
  byte csumb = calculateEEPROMchecksumB();
  EEPROM.write(LOC_CSUMA, csuma);
  EEPROM.write(LOC_CSUMB, csumb);
  if (_printDebug == true)
  {
    debugPrint(F("updateEEPROMchecksum: checksums updated: "));
    _debugSerial->print(csuma, HEX);
    debugPrint(F(" "));
    _debugSerial->println(csumb, HEX);
  }
}

void initTrackerSettings(trackerSettings *myTrackerSettings) // Initialises the trackerSettings in RAM with the default values
{
  myTrackerSettings->STX = DEF_STX;
  myTrackerSettings->SWVER = DEF_SWVER;
  myTrackerSettings->SOURCE.the_data = DEF_SOURCE;
  myTrackerSettings->BATTV.the_data = DEF_BATTV;
  myTrackerSettings->PRESS.the_data = DEF_PRESS;
  myTrackerSettings->TEMP.the_data = DEF_TEMP;
  myTrackerSettings->HUMID.the_data = DEF_HUMID;
  myTrackerSettings->YEAR.the_data = DEF_YEAR;
  myTrackerSettings->MONTH = DEF_MONTH;
  myTrackerSettings->DAY = DEF_DAY;
  myTrackerSettings->HOUR = DEF_HOUR;
  myTrackerSettings->MIN = DEF_MIN;
  myTrackerSettings->SEC = DEF_SEC;
  myTrackerSettings->MILLIS.the_data = DEF_MILLIS;
  myTrackerSettings->LAT.the_data = DEF_LAT;
  myTrackerSettings->LON.the_data = DEF_LON;
  myTrackerSettings->ALT.the_data = DEF_ALT;
  myTrackerSettings->SPEED.the_data = DEF_SPEED;
  myTrackerSettings->HEAD.the_data = DEF_HEAD;
  myTrackerSettings->SATS = DEF_SATS;
  myTrackerSettings->PDOP.the_data = DEF_PDOP;
  myTrackerSettings->FIX = DEF_FIX;
  myTrackerSettings->GEOFSTAT[0] = DEF_GEOFSTAT;
  myTrackerSettings->GEOFSTAT[1] = DEF_GEOFSTAT;
  myTrackerSettings->GEOFSTAT[2] = DEF_GEOFSTAT;
  myTrackerSettings->MOFIELDS[0].the_data = DEF_MOFIELDS0;
  myTrackerSettings->MOFIELDS[1].the_data = DEF_MOFIELDS1;
  myTrackerSettings->MOFIELDS[2].the_data = DEF_MOFIELDS2;
  myTrackerSettings->FLAGS1 = DEF_FLAGS1;
  myTrackerSettings->FLAGS2 = DEF_FLAGS2;
  myTrackerSettings->DEST.the_data = DEF_DEST;
  myTrackerSettings->HIPRESS.the_data = DEF_HIPRESS;
  myTrackerSettings->LOPRESS.the_data = DEF_LOPRESS;
  myTrackerSettings->HITEMP.the_data = DEF_HITEMP;
  myTrackerSettings->LOTEMP.the_data = DEF_LOTEMP;
  myTrackerSettings->HIHUMID.the_data = DEF_HIHUMID;
  myTrackerSettings->LOHUMID.the_data = DEF_LOHUMID;
  myTrackerSettings->GEOFNUM = DEF_GEOFNUM;
  myTrackerSettings->GEOF1LAT.the_data = DEF_GEOF1LAT;
  myTrackerSettings->GEOF1LON.the_data = DEF_GEOF1LON;
  myTrackerSettings->GEOF1RAD.the_data = DEF_GEOF1RAD;
  myTrackerSettings->GEOF2LAT.the_data = DEF_GEOF2LAT;
  myTrackerSettings->GEOF2LON.the_data = DEF_GEOF2LON;
  myTrackerSettings->GEOF2RAD.the_data = DEF_GEOF2RAD;
  myTrackerSettings->GEOF3LAT.the_data = DEF_GEOF3LAT;
  myTrackerSettings->GEOF3LON.the_data = DEF_GEOF3LON;
  myTrackerSettings->GEOF3RAD.the_data = DEF_GEOF3RAD;
  myTrackerSettings->GEOF4LAT.the_data = DEF_GEOF4LAT;
  myTrackerSettings->GEOF4LON.the_data = DEF_GEOF4LON;
  myTrackerSettings->GEOF4RAD.the_data = DEF_GEOF4RAD;
  myTrackerSettings->WAKEINT.the_data = DEF_WAKEINT;
  myTrackerSettings->ALARMINT.the_data = DEF_ALARMINT;
  myTrackerSettings->TXINT.the_data = DEF_TXINT;
  myTrackerSettings->LOWBATT.the_data = DEF_LOWBATT;
  myTrackerSettings->DYNMODEL = DEF_DYNMODEL;
  myTrackerSettings->ETX = DEF_ETX;
  debugPrintln(F("initTrackerSettings: RAM tracker settings initialised"));
}

void putTrackerSettings(trackerSettings *myTrackerSettings) // Write the trackerSettings from RAM into EEPROM
{
  EEPROM.erase(); // Erase any old data first
  EEPROM.put(LOC_STX, myTrackerSettings->STX);
  EEPROM.put(LOC_SWVER, myTrackerSettings->SWVER);
  EEPROM.put(LOC_SOURCE, myTrackerSettings->SOURCE.the_data);
  EEPROM.put(LOC_MOFIELDS0, myTrackerSettings->MOFIELDS[0].the_data);
  EEPROM.put(LOC_MOFIELDS1, myTrackerSettings->MOFIELDS[1].the_data);
  EEPROM.put(LOC_MOFIELDS2, myTrackerSettings->MOFIELDS[2].the_data);
  EEPROM.put(LOC_FLAGS1, myTrackerSettings->FLAGS1);
  EEPROM.put(LOC_FLAGS2, myTrackerSettings->FLAGS2);
  EEPROM.put(LOC_DEST, myTrackerSettings->DEST.the_data);
  EEPROM.put(LOC_HIPRESS, myTrackerSettings->HIPRESS.the_data);
  EEPROM.put(LOC_LOPRESS, myTrackerSettings->LOPRESS.the_data);
  EEPROM.put(LOC_HITEMP, myTrackerSettings->HITEMP.the_data);
  EEPROM.put(LOC_LOTEMP, myTrackerSettings->LOTEMP.the_data);
  EEPROM.put(LOC_HIHUMID, myTrackerSettings->HIHUMID.the_data);
  EEPROM.put(LOC_LOHUMID, myTrackerSettings->LOHUMID.the_data);
  EEPROM.put(LOC_GEOFNUM, myTrackerSettings->GEOFNUM);
  EEPROM.put(LOC_GEOF1LAT, myTrackerSettings->GEOF1LAT.the_data);
  EEPROM.put(LOC_GEOF1LON, myTrackerSettings->GEOF1LON.the_data);
  EEPROM.put(LOC_GEOF1RAD, myTrackerSettings->GEOF1RAD.the_data);
  EEPROM.put(LOC_GEOF2LAT, myTrackerSettings->GEOF2LAT.the_data);
  EEPROM.put(LOC_GEOF2LON, myTrackerSettings->GEOF2LON.the_data);
  EEPROM.put(LOC_GEOF2RAD, myTrackerSettings->GEOF2RAD.the_data);
  EEPROM.put(LOC_GEOF3LAT, myTrackerSettings->GEOF3LAT.the_data);
  EEPROM.put(LOC_GEOF3LON, myTrackerSettings->GEOF3LON.the_data);
  EEPROM.put(LOC_GEOF3RAD, myTrackerSettings->GEOF3RAD.the_data);
  EEPROM.put(LOC_GEOF4LAT, myTrackerSettings->GEOF4LAT.the_data);
  EEPROM.put(LOC_GEOF4LON, myTrackerSettings->GEOF4LON.the_data);
  EEPROM.put(LOC_GEOF4RAD, myTrackerSettings->GEOF4RAD.the_data);
  EEPROM.put(LOC_WAKEINT, myTrackerSettings->WAKEINT.the_data);
  EEPROM.put(LOC_ALARMINT, myTrackerSettings->ALARMINT.the_data);
  EEPROM.put(LOC_TXINT, myTrackerSettings->TXINT.the_data);
  EEPROM.put(LOC_LOWBATT, myTrackerSettings->LOWBATT.the_data);
  EEPROM.put(LOC_DYNMODEL, (byte)myTrackerSettings->DYNMODEL);
  EEPROM.put(LOC_ETX, myTrackerSettings->ETX);
  updateEEPROMchecksum();
  debugPrintln(F("putTrackerSettings: tracker settings stored in EEPROM"));
}

void updateTrackerSettings(trackerSettings *myTrackerSettings) // Update any changed trackerSettings in EEPROM
{
  EEPROM.update(LOC_STX, myTrackerSettings->STX);
  EEPROM.update(LOC_SWVER, myTrackerSettings->SWVER);
  EEPROM.update(LOC_SOURCE, myTrackerSettings->SOURCE.the_data);
  EEPROM.update(LOC_MOFIELDS0, myTrackerSettings->MOFIELDS[0].the_data);
  EEPROM.update(LOC_MOFIELDS1, myTrackerSettings->MOFIELDS[1].the_data);
  EEPROM.update(LOC_MOFIELDS2, myTrackerSettings->MOFIELDS[2].the_data);
  EEPROM.update(LOC_FLAGS1, myTrackerSettings->FLAGS1);
  EEPROM.update(LOC_FLAGS2, myTrackerSettings->FLAGS2);
  EEPROM.update(LOC_DEST, myTrackerSettings->DEST.the_data);
  EEPROM.update(LOC_HIPRESS, myTrackerSettings->HIPRESS.the_data);
  EEPROM.update(LOC_LOPRESS, myTrackerSettings->LOPRESS.the_data);
  EEPROM.update(LOC_HITEMP, myTrackerSettings->HITEMP.the_data);
  EEPROM.update(LOC_LOTEMP, myTrackerSettings->LOTEMP.the_data);
  EEPROM.update(LOC_HIHUMID, myTrackerSettings->HIHUMID.the_data);
  EEPROM.update(LOC_LOHUMID, myTrackerSettings->LOHUMID.the_data);
  EEPROM.update(LOC_GEOFNUM, myTrackerSettings->GEOFNUM);
  EEPROM.update(LOC_GEOF1LAT, myTrackerSettings->GEOF1LAT.the_data);
  EEPROM.update(LOC_GEOF1LON, myTrackerSettings->GEOF1LON.the_data);
  EEPROM.update(LOC_GEOF1RAD, myTrackerSettings->GEOF1RAD.the_data);
  EEPROM.update(LOC_GEOF2LAT, myTrackerSettings->GEOF2LAT.the_data);
  EEPROM.update(LOC_GEOF2LON, myTrackerSettings->GEOF2LON.the_data);
  EEPROM.update(LOC_GEOF2RAD, myTrackerSettings->GEOF2RAD.the_data);
  EEPROM.update(LOC_GEOF3LAT, myTrackerSettings->GEOF3LAT.the_data);
  EEPROM.update(LOC_GEOF3LON, myTrackerSettings->GEOF3LON.the_data);
  EEPROM.update(LOC_GEOF3RAD, myTrackerSettings->GEOF3RAD.the_data);
  EEPROM.update(LOC_GEOF4LAT, myTrackerSettings->GEOF4LAT.the_data);
  EEPROM.update(LOC_GEOF4LON, myTrackerSettings->GEOF4LON.the_data);
  EEPROM.update(LOC_GEOF4RAD, myTrackerSettings->GEOF4RAD.the_data);
  EEPROM.update(LOC_WAKEINT, myTrackerSettings->WAKEINT.the_data);
  EEPROM.update(LOC_ALARMINT, myTrackerSettings->ALARMINT.the_data);
  EEPROM.update(LOC_TXINT, myTrackerSettings->TXINT.the_data);
  EEPROM.update(LOC_LOWBATT, myTrackerSettings->LOWBATT.the_data);
  EEPROM.update(LOC_DYNMODEL, (byte)myTrackerSettings->DYNMODEL);
  EEPROM.update(LOC_ETX, myTrackerSettings->ETX);
  updateEEPROMchecksum();
  debugPrintln(F("updateTrackerSettings: EEPROM tracker settings updated"));
}

void getTrackerSettings(trackerSettings *myTrackerSettings) // Read the trackerSettings from EEPROM into RAM
{
  EEPROM.get(LOC_STX, myTrackerSettings->STX);
  EEPROM.get(LOC_SWVER, myTrackerSettings->SWVER);
  EEPROM.get(LOC_SOURCE, myTrackerSettings->SOURCE.the_data);
  EEPROM.get(LOC_MOFIELDS0, myTrackerSettings->MOFIELDS[0].the_data);
  EEPROM.get(LOC_MOFIELDS1, myTrackerSettings->MOFIELDS[1].the_data);
  EEPROM.get(LOC_MOFIELDS2, myTrackerSettings->MOFIELDS[2].the_data);
  EEPROM.get(LOC_FLAGS1, myTrackerSettings->FLAGS1);
  EEPROM.get(LOC_FLAGS2, myTrackerSettings->FLAGS2);
  EEPROM.get(LOC_DEST, myTrackerSettings->DEST.the_data);
  EEPROM.get(LOC_HIPRESS, myTrackerSettings->HIPRESS.the_data);
  EEPROM.get(LOC_LOPRESS, myTrackerSettings->LOPRESS.the_data);
  EEPROM.get(LOC_HITEMP, myTrackerSettings->HITEMP.the_data);
  EEPROM.get(LOC_LOTEMP, myTrackerSettings->LOTEMP.the_data);
  EEPROM.get(LOC_HIHUMID, myTrackerSettings->HIHUMID.the_data);
  EEPROM.get(LOC_LOHUMID, myTrackerSettings->LOHUMID.the_data);
  EEPROM.get(LOC_GEOFNUM, myTrackerSettings->GEOFNUM);
  EEPROM.get(LOC_GEOF1LAT, myTrackerSettings->GEOF1LAT.the_data);
  EEPROM.get(LOC_GEOF1LON, myTrackerSettings->GEOF1LON.the_data);
  EEPROM.get(LOC_GEOF1RAD, myTrackerSettings->GEOF1RAD.the_data);
  EEPROM.get(LOC_GEOF2LAT, myTrackerSettings->GEOF2LAT.the_data);
  EEPROM.get(LOC_GEOF2LON, myTrackerSettings->GEOF2LON.the_data);
  EEPROM.get(LOC_GEOF2RAD, myTrackerSettings->GEOF2RAD.the_data);
  EEPROM.get(LOC_GEOF3LAT, myTrackerSettings->GEOF3LAT.the_data);
  EEPROM.get(LOC_GEOF3LON, myTrackerSettings->GEOF3LON.the_data);
  EEPROM.get(LOC_GEOF3RAD, myTrackerSettings->GEOF3RAD.the_data);
  EEPROM.get(LOC_GEOF4LAT, myTrackerSettings->GEOF4LAT.the_data);
  EEPROM.get(LOC_GEOF4LON, myTrackerSettings->GEOF4LON.the_data);
  EEPROM.get(LOC_GEOF4RAD, myTrackerSettings->GEOF4RAD.the_data);
  EEPROM.get(LOC_WAKEINT, myTrackerSettings->WAKEINT.the_data);
  EEPROM.get(LOC_ALARMINT, myTrackerSettings->ALARMINT.the_data);
  EEPROM.get(LOC_TXINT, myTrackerSettings->TXINT.the_data);
  EEPROM.get(LOC_LOWBATT, myTrackerSettings->LOWBATT.the_data);
  EEPROM.get(LOC_DYNMODEL, myTrackerSettings->DYNMODEL);
  EEPROM.get(LOC_ETX, myTrackerSettings->ETX);
  debugPrintln(F("getTrackerSettings: tracker setings loaded from EEPROM to RAM"));
}

void displayEEPROMcontents() // Display the EEPROM data nicely
{
  // Define an array of the message field locations and use it to display the EEPROM contents nicely
  uint16_t field_locations[] = {
    LOC_STX,
    LOC_SWVER,
    LOC_SOURCE,
    LOC_MOFIELDS0,
    LOC_MOFIELDS1,
    LOC_MOFIELDS2,
    LOC_FLAGS1,
    LOC_FLAGS2,
    LOC_DEST,
    LOC_HIPRESS,
    LOC_LOPRESS,
    LOC_HITEMP,
    LOC_LOTEMP,
    LOC_HIHUMID,
    LOC_LOHUMID,
    LOC_GEOFNUM,
    LOC_GEOF1LAT,
    LOC_GEOF1LON,
    LOC_GEOF1RAD,
    LOC_GEOF2LAT,
    LOC_GEOF2LON,
    LOC_GEOF2RAD,
    LOC_GEOF3LAT,
    LOC_GEOF3LON,
    LOC_GEOF3RAD,
    LOC_GEOF4LAT,
    LOC_GEOF4LON,
    LOC_GEOF4RAD,
    LOC_WAKEINT,
    LOC_ALARMINT,
    LOC_TXINT,
    LOC_LOWBATT,
    LOC_DYNMODEL,
    LOC_ETX,
    LOC_CSUMA,
    LOC_CSUMB};
  uint16_t chars_printed = 0;
  for (uint16_t x = LOC_STX; x < (LOC_CSUMB + LEN_CSUMB); x += 1)
  {
    // Check if we have reached the end of a message field
    for (uint16_t i = 1; i < (sizeof(field_locations) / sizeof(field_locations[0])); i++)
    {
      if (x == field_locations[i]) // If we have, print a space plus a new line as required
      {
        Serial.print(F(" "));
        if (chars_printed >= 32)
        {
          Serial.println();
          chars_printed = 0;
        }
      }
    }
    uint8_t tempByte = EEPROM.read(x);
    if (tempByte < 16)
      Serial.printf("0%X", tempByte); // Print the EEPROM byte
    else
      Serial.printf("%X", tempByte); // Print the EEPROM byte
    chars_printed += 1;
  }
}

uint8_t ascii_hex_to_bin(uint8_t chr)
// Convert ASCII-encoded Hex to binary
{
  if ((chr >= '0') && (chr <= '9'))
  {
    return (chr - '0');
  }
  else if ((chr >= 'A') && (chr <= 'F'))
  {
    return (10 + chr - 'A');
  }
  else if ((chr >= 'a') && (chr <= 'f'))
  {
    return (10 + chr - 'a');
  }
  else return (0);
}

enum tracker_serial_rx_status check_for_serial_data(bool fresh)
// A non-blocking way to check for the arrival of serial data and time out when
// no data is received for CHECK_SERIAL_TIMEOUT seconds, or if there is a gap of
// RX_IDLE_TIMEOUT in receiving the data
{
  if (fresh == true) // If this is a fresh start
  {
    tracker_serial_rx_buffer_size = 0; // Reset the buffer size
    rx_start = millis(); // Record the start time
    last_rx = rx_start; // Fake the last rx time
    data_seen = false; // Clear the data seen flag
  }

  while (Serial.available()) // Has any serial data been received?
  {
    data_seen = true; // Set the flag
    last_rx = millis(); // Record the rx time
    tracker_serial_rx_buffer[tracker_serial_rx_buffer_size++] = Serial.read(); // Read and store one byte from the Serial rx buffer
  }

  if (millis() - rx_start >= 1000UL * CHECK_SERIAL_TIMEOUT) // Check if we have timed out
  {
    debugPrintln(F("check_for_serial_data: timed out"));
    return (DATA_TIMEOUT);
  }

  if (data_seen == false) // If we have not seen any data
  {
    return (DATA_NOT_SEEN);
  }

  // Data has been seen so check for a data idle timeout
  if (millis() - last_rx < 1000UL * RX_IDLE_TIMEOUT) // If the idle timeout has not expired
  {
    return (DATA_SEEN);
  }

  debugPrintln(F("check_for_serial_data: data received"));
  return (DATA_RECEIVED);
}

bool is_ID_valid(uint8_t ID, uint16_t &data_width)
// Checks that ID is a valid message field identifier. Sets data_width accordingly.
{
  for (size_t x = 0; x < NUM_ID_WIDTHS; x++)
  {
    if (ID_widths[x].ID == ID)
    {
      data_width = (uint16_t)(ID_widths[x].data_width);
      return (true);
    }
  }
  return (false);
}

enum tracker_parsing_result check_data(uint8_t *data_buffer, size_t &data_buffer_size)
// Checks that the data in data_buffer has the correct format and checksums
// We will want to use this function on both the tracker_serial_rx_buffer and the Iridium SBD buffer
// Serial data could be binary or ASCII-encoded Hex
{
  // Is there enough data in the buffer? At least STX, ETX, CSUMA, CSUMB
  if (data_buffer_size < 4)
  {
    debugPrintln(F("check_data: data too short"));
    return (DATA_TOO_SHORT);
  }

  // Is the data is ASCII Hex format? If so, convert it to binary.
  // Check if the first two bytes are "02"
  // (If the data is ASCII Hex, it can only have arrived via USB and so cannot have an RB header)
  // (I.e. we don't need to check if the first two bytes are "RB" here)
  if ((data_buffer[0] == '0') && (data_buffer[1] == '2'))
  {
    debugPrintln(F("check_data: data is ASCII Hex"));
    size_t pointer = 0;
    while (pointer < data_buffer_size)
    {
      // Convert each pair of chars to a single binary byte
      data_buffer[pointer >> 1] = (ascii_hex_to_bin(data_buffer[pointer]) << 4) | ascii_hex_to_bin(data_buffer[pointer + 1]);
      pointer += 2;
    }
    data_buffer_size >>= 1; // Divide buffer size by 2
  }
  
  // Data is now in binary format so check if it is valid
  
  // Check to see if the first two bytes are "RB" indicating this is a message forwarded via the RockBLOCK gateway
  size_t startAtByte = 0; // If this is a forwarded message, use startAtByte to hold the 5 byte offset
  if ((data_buffer[0] == 'R') && (data_buffer[1] == 'B'))
  {
    debugPrintln(F("check_data: data has a RockBLOCK header. Ignoring it..."));
    startAtByte = 5; // set startAtByte to 5 to skip the header
  }

  // First check for the STX
  if (data_buffer[startAtByte] != 2)
  {
    debugPrintln(F("check_data: no STX"));
    return (NO_STX);
  }
  
  // Next check there is enough data in the buffer
  if (data_buffer_size < 4)
  {
    debugPrintln(F("check_data: data too short"));
    return (DATA_TOO_SHORT);
  }
  
  // Next check for the ETX
  if (data_buffer[data_buffer_size - 3] != 3)
  {
    debugPrintln(F("check_data: no ETX"));
    return (NO_ETX);
  }
  
  // Now calculate the checksum bytes
  uint32_t csuma = 0;
  uint32_t csumb = 0;
  for (size_t x = startAtByte; x < (data_buffer_size -2); x += 1) // Calculate a sum of bytes and sum of sums for every byte from STX to ETX
  {
    csuma = csuma + data_buffer[x];
    csumb = csumb + csuma;
  }
  csuma = csuma & 0x000000ff;
  csumb = csumb & 0x000000ff;
  
  // Next check the checksum bytes
  if ((data_buffer[data_buffer_size -2] != (uint8_t)(csuma)) || (data_buffer[data_buffer_size -1] != (uint8_t)(csumb)))
  {
    if (_printDebug == true)
    {
      debugPrint(F("check_data: checksum error "));
      _debugSerial->print(csuma, HEX);
      debugPrint(F(" "));
      _debugSerial->print(data_buffer[data_buffer_size -2], HEX);
      debugPrint(F(" "));
      _debugSerial->print(csumb, HEX);
      debugPrint(F(" "));
      _debugSerial->println(data_buffer[data_buffer_size -1], HEX);
    }
    return (CHECKSUM_ERROR);
  }
  
  // Return now if the message is STX,ETX,CSUMA,CSUMB
  if (((startAtByte == 0) && (data_buffer_size == 4)) || ((startAtByte == 5) && (data_buffer_size == 9)))
  {
    debugPrintln(F("check_data: message is valid but contains no data"));
    return (DATA_VALID);
  }
  
  // Finally check that each field ID is recognized and that the data widths should be correct
  size_t x = startAtByte + 1; // Use x as an array pointer
  while (x < (data_buffer_size - 2)) // Check each byte except the checksum
  {
    uint16_t data_width;
    if (is_ID_valid(data_buffer[x], data_width)) // Check if the field ID is valid
    {
      x += data_width + 1; // If it is, add the ID + data width to the pointer
      if (x > (data_buffer_size - 2))
      {
        debugPrintln(F("check_data: invalid data width"));
        return (DATA_WIDTH_INVALID);
      }
    }
    else
    {
      if (_printDebug == true)
      {
        debugPrint(F("check_data: invalid field ID ( "));
        _debugSerial->print(data_buffer[x]);
        debugPrintln(F(" ) encountered"));
      }
      return (INVALID_FIELD);
    }
  }
  
  return (DATA_VALID);
}

enum tracker_parsing_result parse_data(uint8_t *data_buffer, size_t &data_buffer_size, trackerSettings *myTrackerSettings, bool over_serial)
// Parse the data, updating the tracker settings in RAM and EEPROM
// Some settings can only be updated over serial, not via SBD message.
{
  size_t x = 1; // Use x as an array pointer
  while (x < (data_buffer_size - 2)) // Check each byte except the checksum
  {
    union_uint32t uint32t;
    union_int32t int32t;
    union_uint16t uint16t;
    union_int16t int16t;
    uint8_t ID;
    
    ID = data_buffer[x];
    
    switch(ID) {
      case RBHEAD:
        x += 5;
        break;
      case STX:
      case ETX:
        x += 1;
        break;
      case SWVER: // Ignore attempt to change the software version
        x += 2;
        break;
      case SOURCE:
        if (over_serial) // Only allow SOURCE to be changed if the data came via serial (not via Iridium)
        {
          uint32t.the_bytes[0] = data_buffer[++x];
          uint32t.the_bytes[1] = data_buffer[++x];
          uint32t.the_bytes[2] = data_buffer[++x];
          uint32t.the_bytes[3] = data_buffer[++x];
          myTrackerSettings->SOURCE.the_data = uint32t.the_data;
          x += 1;
        }
        else
        {
          x += 5;
        }        
        break;
      case BATTV:
      case PRESS:
      case TEMP:
      case HUMID:
      case YEAR:
        x += 3;
        break;
      case MONTH:
      case DAY:
      case HOUR:
      case MIN:
      case SEC:
        x += 2;
        break;
      case MILLIS:
        x += 3;
        break;
      case DATETIME:
        x += 8;
        break;
      case LAT:
      case LON:
      case ALT:
      case SPEED:
      case HEAD:
        x += 5;
        break;
      case SATS:
        x += 2;
        break;
      case PDOP:
        x += 3;
        break;
      case FIX:
        x += 2;
        break;
      case GEOFSTAT:
        x += 4;
        break;
      case USERVAL1:
      case USERVAL2:
        x += 2;
        break;
      case USERVAL3:
      case USERVAL4:
        x += 3;
        break;
      case USERVAL5:
      case USERVAL6:
      case USERVAL7:
      case USERVAL8:
        x += 5;
        break;
      case MOFIELDS: // little endian!
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->MOFIELDS[0].the_data = uint32t.the_data;
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->MOFIELDS[1].the_data = uint32t.the_data;
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->MOFIELDS[2].the_data = uint32t.the_data;
        x += 1;
        break;
      case FLAGS1:
        myTrackerSettings->FLAGS1 = data_buffer[++x];
        x += 1;
        break;
      case FLAGS2:
        myTrackerSettings->FLAGS2 = data_buffer[++x];
        x += 1;
        break;
      case DEST:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->DEST.the_data = uint32t.the_data;
        x += 1;
        break;
      case HIPRESS:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->HIPRESS.the_data = uint16t.the_data;
        x += 1;
        break;
      case LOPRESS:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->LOPRESS.the_data = uint16t.the_data;
        x += 1;
        break;
      case HITEMP:
        int16t.the_bytes[0] = data_buffer[++x];
        int16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->HITEMP.the_data = int16t.the_data;
        x += 1;
        break;
      case LOTEMP:
        int16t.the_bytes[0] = data_buffer[++x];
        int16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->LOTEMP.the_data = int16t.the_data;
        x += 1;
        break;
      case HIHUMID:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->HIHUMID.the_data = uint16t.the_data;
        x += 1;
        break;
      case LOHUMID:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->LOHUMID.the_data = uint16t.the_data;
        x += 1;
        break;
      case GEOFNUM:
        myTrackerSettings->GEOFNUM = data_buffer[++x];
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF1LAT:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF1LAT.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF1LON:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF1LON.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF1RAD:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF1RAD.the_data = uint32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF2LAT:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF2LAT.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF2LON:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF2LON.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF2RAD:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF2RAD.the_data = uint32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF3LAT:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF3LAT.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF3LON:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF3LON.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF3RAD:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF3RAD.the_data = uint32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF4LAT:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF4LAT.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF4LON:
        int32t.the_bytes[0] = data_buffer[++x];
        int32t.the_bytes[1] = data_buffer[++x];
        int32t.the_bytes[2] = data_buffer[++x];
        int32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF4LON.the_data = int32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case GEOF4RAD:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->GEOF4RAD.the_data = uint32t.the_data;
        x += 1;
        geofencesSet = false; // Clear the flag so we update the geofences next time around the loop
        break;
      case WAKEINT:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        myTrackerSettings->WAKEINT.the_data = uint32t.the_data;
        x += 1;
        break;
      case ALARMINT:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->ALARMINT.the_data = uint16t.the_data;
        x += 1;
        break;
      case TXINT:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->TXINT.the_data = uint16t.the_data;
        x += 1;
        break;
      case LOWBATT:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        myTrackerSettings->LOWBATT.the_data = uint16t.the_data;
        x += 1;
        break;
      case DYNMODEL:
        myTrackerSettings->DYNMODEL = (dynModel)(data_buffer[++x]);
        x += 1;
        dynamicModelSet == false; // Clear the flag so the dynamic model will be updated next time around the loop
        break;
      case USERFUNC1:
        USER_FUNC_1(); // Call the user function
        x += 1;
        break;
      case USERFUNC2:
        USER_FUNC_2(); // Call the user function
        x += 1;
        break;
      case USERFUNC3:
        USER_FUNC_3(); // Call the user function
        x += 1;
        break;
      case USERFUNC4:
        USER_FUNC_4(); // Call the user function
        x += 1;
        break;
      case USERFUNC5:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        USER_FUNC_5(uint16t.the_data); // Call the user function
        x += 1;
        break;
      case USERFUNC6:
        uint16t.the_bytes[0] = data_buffer[++x];
        uint16t.the_bytes[1] = data_buffer[++x];
        USER_FUNC_6(uint16t.the_data); // Call the user function
        x += 1;
        break;
      case USERFUNC7:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        USER_FUNC_7(uint32t.the_data); // Call the user function
        x += 1;
        break;
      case USERFUNC8:
        uint32t.the_bytes[0] = data_buffer[++x];
        uint32t.the_bytes[1] = data_buffer[++x];
        uint32t.the_bytes[2] = data_buffer[++x];
        uint32t.the_bytes[3] = data_buffer[++x];
        USER_FUNC_8(uint32t.the_data); // Call the user function
        x += 1;
        break;
    }
  }
  return (DATA_VALID);
}

void printTrackerSettings(trackerSettings *myTrackerSettings)
// Print the tracker settings
{
  if (_printDebug == true)
  {
    _debugSerial->println("Tracker settings are:");
    _debugSerial->print("SWVER: ");
    _debugSerial->print((myTrackerSettings->SWVER & 0xf0) >> 4);
    _debugSerial->print(".");
    _debugSerial->println(myTrackerSettings->SWVER & 0x0f);
    _debugSerial->print("SOURCE: ");
    _debugSerial->println(myTrackerSettings->SOURCE.the_data);
    _debugSerial->print("BATTV: ");
    _debugSerial->println(((float)myTrackerSettings->BATTV.the_data / 100),2);
    _debugSerial->print("PRESS: ");
    _debugSerial->println(myTrackerSettings->PRESS.the_data);
    _debugSerial->print("TEMP: ");
    _debugSerial->println(((float)myTrackerSettings->TEMP.the_data / 100),2);
    _debugSerial->print("HUMID: ");
    _debugSerial->println(((float)myTrackerSettings->HUMID.the_data / 100),2);
    _debugSerial->print("YEAR: ");
    _debugSerial->println(myTrackerSettings->YEAR.the_data);
    _debugSerial->print("MONTH: ");
    _debugSerial->println(myTrackerSettings->MONTH);
    _debugSerial->print("DAY: ");
    _debugSerial->println(myTrackerSettings->DAY);
    _debugSerial->print("HOUR: ");
    _debugSerial->println(myTrackerSettings->HOUR);
    _debugSerial->print("MIN: ");
    _debugSerial->println(myTrackerSettings->MIN);
    _debugSerial->print("SEC: ");
    _debugSerial->println(myTrackerSettings->SEC);
    _debugSerial->print("MILLIS: ");
    _debugSerial->println(myTrackerSettings->MILLIS.the_data);
    _debugSerial->print("LAT: ");
    _debugSerial->println(((float)myTrackerSettings->LAT.the_data / 10000000),7);
    _debugSerial->print("LON: ");
    _debugSerial->println(((float)myTrackerSettings->LON.the_data / 10000000),7);
    _debugSerial->print("ALT: ");
    _debugSerial->println(((float)myTrackerSettings->ALT.the_data / 1000),3);
    _debugSerial->print("SPEED: ");
    _debugSerial->println(((float)myTrackerSettings->SPEED.the_data / 1000),3);
    _debugSerial->print("HEAD: ");
    _debugSerial->println(((float)myTrackerSettings->HEAD.the_data / 10000000),1);
    _debugSerial->print("SATS: ");
    _debugSerial->println(myTrackerSettings->SATS);
    _debugSerial->print("PDOP: ");
    _debugSerial->println(((float)myTrackerSettings->PDOP.the_data / 100),2);
    _debugSerial->print("FIX: ");
    _debugSerial->println(myTrackerSettings->FIX);
    _debugSerial->print("GEOFSTAT: ");
    _debugSerial->print((myTrackerSettings->GEOFSTAT[0] & 0xf0) >> 4);
    _debugSerial->print(myTrackerSettings->GEOFSTAT[0] & 0x0f);
    _debugSerial->print((myTrackerSettings->GEOFSTAT[1] & 0xf0) >> 4);
    _debugSerial->print(myTrackerSettings->GEOFSTAT[1] & 0x0f);
    _debugSerial->print((myTrackerSettings->GEOFSTAT[2] & 0xf0) >> 4);
    _debugSerial->println(myTrackerSettings->GEOFSTAT[2] & 0x0f);
    _debugSerial->print("USERVAL1: ");
    _debugSerial->println(myTrackerSettings->USERVAL1);
    _debugSerial->print("USERVAL2: ");
    _debugSerial->println(myTrackerSettings->USERVAL2);
    _debugSerial->print("USERVAL3: ");
    _debugSerial->println(myTrackerSettings->USERVAL3.the_data);
    _debugSerial->print("USERVAL4: ");
    _debugSerial->println(myTrackerSettings->USERVAL4.the_data);
    _debugSerial->print("USERVAL5: ");
    _debugSerial->println(myTrackerSettings->USERVAL5.the_data);
    _debugSerial->print("USERVAL6: ");
    _debugSerial->println(myTrackerSettings->USERVAL6.the_data);
    _debugSerial->print("USERVAL7: ");
    _debugSerial->println(myTrackerSettings->USERVAL7.the_data);
    _debugSerial->print("USERVAL8: ");
    _debugSerial->println(myTrackerSettings->USERVAL8.the_data);
    _debugSerial->print("MOFIELDS[0] (big endian): ");
    for (uint8_t x = 3; x < 4; x--) // 0-- = 255
    {
      printBinary(myTrackerSettings->MOFIELDS[0].the_bytes[x]);
    }
    _debugSerial->println();
    _debugSerial->print("MOFIELDS[1] (big endian): ");
    for (uint8_t x = 3; x < 4; x--) // 0-- = 255
    {
      printBinary(myTrackerSettings->MOFIELDS[1].the_bytes[x]);
    }
    _debugSerial->println();
    _debugSerial->print("MOFIELDS[2] (big endian): ");
    for (uint8_t x = 3; x < 4; x--) // 0-- = 255
    {
      printBinary(myTrackerSettings->MOFIELDS[2].the_bytes[x]);
    }
    _debugSerial->println();
    _debugSerial->print("FLAGS1: ");
    printBinary(myTrackerSettings->FLAGS1);
    _debugSerial->println();
    _debugSerial->print("FLAGS2: ");
    printBinary(myTrackerSettings->FLAGS2);
    _debugSerial->println();
    _debugSerial->print("DEST: ");
    _debugSerial->println(myTrackerSettings->DEST.the_data);
    _debugSerial->print("HIPRESS: ");
    _debugSerial->println(myTrackerSettings->HIPRESS.the_data);
    _debugSerial->print("LOPRESS: ");
    _debugSerial->println(myTrackerSettings->LOPRESS.the_data);
    _debugSerial->print("HITEMP: ");
    _debugSerial->println(((float)myTrackerSettings->HITEMP.the_data / 100), 2);
    _debugSerial->print("LOTEMP: ");
    _debugSerial->println(((float)myTrackerSettings->LOTEMP.the_data / 100), 2);
    _debugSerial->print("HIHUMID: ");
    _debugSerial->println(((float)myTrackerSettings->HIHUMID.the_data / 100), 2);
    _debugSerial->print("LOHUMID: ");
    _debugSerial->println(((float)myTrackerSettings->LOHUMID.the_data / 100), 2);
    _debugSerial->print("GEOFNUM: ");
    _debugSerial->println(myTrackerSettings->GEOFNUM);
    _debugSerial->print("GEOF1LAT: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF1LAT.the_data / 10000000),7);
    _debugSerial->print("GEOF1LON: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF1LON.the_data / 10000000),7);
    _debugSerial->print("GEOF1RAD: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF1RAD.the_data / 100),2);
    _debugSerial->print("GEOF2LAT: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF2LAT.the_data / 10000000),7);
    _debugSerial->print("GEOF2LON: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF2LON.the_data / 10000000),7);
    _debugSerial->print("GEOF2RAD: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF2RAD.the_data / 100),2);
    _debugSerial->print("GEOF3LAT: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF3LAT.the_data / 10000000),7);
    _debugSerial->print("GEOF3LON: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF3LON.the_data / 10000000),7);
    _debugSerial->print("GEOF3RAD: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF3RAD.the_data / 100),2);
    _debugSerial->print("GEOF4LAT: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF4LAT.the_data / 10000000),7);
    _debugSerial->print("GEOF4LON: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF4LON.the_data / 10000000),7);
    _debugSerial->print("GEOF4RAD: ");
    _debugSerial->println(((float)myTrackerSettings->GEOF4RAD.the_data / 100),2);
    _debugSerial->print("WAKEINT: ");
    _debugSerial->println(myTrackerSettings->WAKEINT.the_data);
    _debugSerial->print("ALARMINT: ");
    _debugSerial->println(myTrackerSettings->ALARMINT.the_data);
    _debugSerial->print("TXINT: ");
    _debugSerial->println(myTrackerSettings->TXINT.the_data);
    _debugSerial->print("LOWBATT: ");
    _debugSerial->println(((float)myTrackerSettings->LOWBATT.the_data / 100),2);
    _debugSerial->print("DYNMODEL: ");
    _debugSerial->println(myTrackerSettings->DYNMODEL);
  }
}

void printBinary(uint8_t the_byte)
// Print the_byte in binary format
{
  if (_printDebug == true)
  {
    for (uint8_t x = 0x80; x != 0x00; x >>= 1)
    {
      if (the_byte & x) _debugSerial->print("1");
      else _debugSerial->print("0");
    }
    _debugSerial->print(" ");
  }
}
