// Definitions and functions to support storing the Artemis Global Tracker settings (message fields) in both RAM and EEPROM
// Also includes functions to parse settings received via serial or MT message

#ifndef MESSAGE_FIELDS
#define MESSAGE_FIELDS

#include "Arduino.h"

// Data type storage lengths when stored in EEPROM
#define LEN_BYTE    1
#define LEN_INT16   2
#define LEN_INT32   4
#define LEN_FLOAT   4

// Define the storage lengths for the message fields that will be stored in EEPROM
#define LEN_STX       LEN_BYTE    // STX is byte
#define LEN_SWVER     LEN_BYTE    // SWVER is byte
#define LEN_SOURCE    LEN_INT32   // SOURCE is uint32
#define LEN_MOFIELDS0 LEN_INT32   // MOFIELDS0 is uint32
#define LEN_MOFIELDS1 LEN_INT32   // MOFIELDS1 is uint32
#define LEN_MOFIELDS2 LEN_INT32   // MOFIELDS2 is uint32
#define LEN_FLAGS1    LEN_BYTE    // FLAGS1 is byte
#define LEN_FLAGS2    LEN_BYTE    // FLAGS2 is byte
#define LEN_DEST      LEN_INT32   // DEST is uint32
#define LEN_HIPRESS   LEN_INT16   // HIPRESS is unit16
#define LEN_LOPRESS   LEN_INT16   // LOPRESS is unit16
#define LEN_HITEMP    LEN_INT16   // HITEMP is int16
#define LEN_LOTEMP    LEN_INT16   // LOTEMP is int16
#define LEN_HIHUMID   LEN_INT16   // HIHUMID is uint16
#define LEN_LOHUMID   LEN_INT16   // LOHUMID is unit16
#define LEN_GEOFNUM   LEN_BYTE    // GEOFNUM is byte
#define LEN_GEOF1LAT  LEN_INT32   // GEOFLAT is int32
#define LEN_GEOF1LON  LEN_INT32   // GEOFLON is int32
#define LEN_GEOF1RAD  LEN_INT32   // GEOFRAD is uint32
#define LEN_GEOF2LAT  LEN_INT32   // GEOFLAT is int32
#define LEN_GEOF2LON  LEN_INT32   // GEOFLON is int32
#define LEN_GEOF2RAD  LEN_INT32   // GEOFRAD is unit32
#define LEN_GEOF3LAT  LEN_INT32   // GEOFLAT is int32
#define LEN_GEOF3LON  LEN_INT32   // GEOFLON is int32
#define LEN_GEOF3RAD  LEN_INT32   // GEOFRAD is unit32
#define LEN_GEOF4LAT  LEN_INT32   // GEOFLAT is int32
#define LEN_GEOF4LON  LEN_INT32   // GEOFLON is int32
#define LEN_GEOF4RAD  LEN_INT32   // GEOFRAD is unit32
#define LEN_WAKEINT   LEN_INT32   // WAKEINT is uint32
#define LEN_ALARMINT  LEN_INT16   // ALARMINT is uint16
#define LEN_TXINT     LEN_INT16   // TXINT is uint16
#define LEN_LOWBATT   LEN_INT16   // LOWBATT is uint16
#define LEN_DYNMODEL  LEN_BYTE    // DYNMODEL is byte
#define LEN_ETX       LEN_BYTE    // ETX is byte
#define LEN_CSUMA     LEN_BYTE    // ChecksumA is byte
#define LEN_CSUMB     LEN_BYTE    // ChecksumB is byte

// Define the storage locations for each message field that will be stored in EEPROM
#define LOC_STX       0 // Change this if you want the data to be stored higher up in the EEPROM
#define LOC_SWVER     LOC_STX + LEN_STX
#define LOC_SOURCE    LOC_SWVER + LEN_SWVER
#define LOC_MOFIELDS0 LOC_SOURCE+ LEN_SOURCE
#define LOC_MOFIELDS1 LOC_MOFIELDS0 + LEN_MOFIELDS0
#define LOC_MOFIELDS2 LOC_MOFIELDS1 + LEN_MOFIELDS1
#define LOC_FLAGS1    LOC_MOFIELDS2 + LEN_MOFIELDS2
#define LOC_FLAGS2    LOC_FLAGS1 + LEN_FLAGS1
#define LOC_DEST      LOC_FLAGS2 + LEN_FLAGS2
#define LOC_HIPRESS   LOC_DEST + LEN_DEST
#define LOC_LOPRESS   LOC_HIPRESS + LEN_HIPRESS
#define LOC_HITEMP    LOC_LOPRESS + LEN_LOPRESS
#define LOC_LOTEMP    LOC_HITEMP + LEN_HITEMP
#define LOC_HIHUMID   LOC_LOTEMP + LEN_LOTEMP
#define LOC_LOHUMID   LOC_HIHUMID + LEN_HIHUMID
#define LOC_GEOFNUM   LOC_LOHUMID + LEN_LOHUMID
#define LOC_GEOF1LAT  LOC_GEOFNUM + LEN_GEOFNUM
#define LOC_GEOF1LON  LOC_GEOF1LAT + LEN_GEOF1LAT
#define LOC_GEOF1RAD  LOC_GEOF1LON + LEN_GEOF1LON
#define LOC_GEOF2LAT  LOC_GEOF1RAD + LEN_GEOF1RAD
#define LOC_GEOF2LON  LOC_GEOF2LAT + LEN_GEOF2LAT
#define LOC_GEOF2RAD  LOC_GEOF2LON + LEN_GEOF2LON
#define LOC_GEOF3LAT  LOC_GEOF2RAD + LEN_GEOF2RAD
#define LOC_GEOF3LON  LOC_GEOF3LAT + LEN_GEOF3LAT
#define LOC_GEOF3RAD  LOC_GEOF3LON + LEN_GEOF3LON
#define LOC_GEOF4LAT  LOC_GEOF3RAD + LEN_GEOF3RAD
#define LOC_GEOF4LON  LOC_GEOF4LAT + LEN_GEOF4LAT
#define LOC_GEOF4RAD  LOC_GEOF4LON + LEN_GEOF4LON
#define LOC_WAKEINT   LOC_GEOF4RAD + LEN_GEOF4RAD
#define LOC_ALARMINT  LOC_WAKEINT + LEN_WAKEINT
#define LOC_TXINT     LOC_ALARMINT + LEN_ALARMINT
#define LOC_LOWBATT   LOC_TXINT + LEN_TXINT
#define LOC_DYNMODEL  LOC_LOWBATT + LEN_LOWBATT
#define LOC_ETX       LOC_DYNMODEL + LEN_DYNMODEL
#define LOC_CSUMA     LOC_ETX + LEN_ETX
#define LOC_CSUMB     LOC_CSUMA + LEN_CSUMA

// Define the default value for each message field
#define DEF_STX       0x02
#define DEF_SWVER     0x21 // Software version 2.1
#define DEF_SOURCE    0
#define DEF_BATTV     500 // 500 * 0.01V = 5V
#define DEF_PRESS     UINT16_MAX // fill value, 65535 hPa
#define DEF_TEMP      INT16_MIN  // fill value, -32768 * 0.01°C = -327.68°C
#define DEF_HUMID     UINT16_MAX // fill value, 65535 * 0.01%RH = 655.35%RH
#define DEF_YEAR      0 // fill value
#define DEF_MONTH     0 // fill value
#define DEF_DAY       0 // fill value
#define DEF_HOUR      UINT8_MAX // fill value, 255
#define DEF_MIN       UINT8_MAX // fill value, 255
#define DEF_SEC       UINT8_MAX // fill value, 255
#define DEF_MILLIS    UINT16_MAX // fill value, 65535
#define DEF_LAT       INT32_MIN // fill value, -2147483648 * 1e-7° = -214.7483648°
#define DEF_LON       INT32_MIN // fill value, -2147483648 * 1e-7° = -214.7483648°
#define DEF_ALT       INT32_MIN // fill value, -2147483648 mm = -2147483.648 m
#define DEF_SPEED     0
#define DEF_HEAD      0
#define DEF_SATS      0
#define DEF_PDOP      0
#define DEF_FIX       0
#define DEF_GEOFSTAT  0
#define DEF_USERVAL1  0
#define DEF_USERVAL2  0
#define DEF_USERVAL3  0
#define DEF_USERVAL4  0
#define DEF_USERVAL5  0
#define DEF_USERVAL6  0
#define DEF_USERVAL7  0.0
#define DEF_USERVAL8  0.0
#define DEF_MOFIELDS0 0x00000f00 // Default to sending DATETIME, LAT, LON and ALT in MO messages
#define DEF_MOFIELDS1 0x00000000
#define DEF_MOFIELDS2 0x00000000
#define DEF_FLAGS1    0 // Default to text messages
#define DEF_FLAGS2    0
#define DEF_DEST      0
#define DEF_HIPRESS   1084
#define DEF_LOPRESS   0
#define DEF_HITEMP    8500  // 8500 * 10^-2°C = 85.0°C
#define DEF_LOTEMP    -4000 // -4000 * 10^-2°C = -40.0°C
#define DEF_HIHUMID   10000 // 10000 * 10^-2%RH = 100.0%RH
#define DEF_LOHUMID   0
#define DEF_GEOFNUM   0
#define DEF_GEOF1LAT  0
#define DEF_GEOF1LON  0
#define DEF_GEOF1RAD  0
#define DEF_GEOF2LAT  0
#define DEF_GEOF2LON  0
#define DEF_GEOF2RAD  0
#define DEF_GEOF3LAT  0
#define DEF_GEOF3LON  0
#define DEF_GEOF3RAD  0
#define DEF_GEOF4LAT  0
#define DEF_GEOF4LON  0
#define DEF_GEOF4RAD  0
#define DEF_WAKEINT   60 // Seconds
#define DEF_ALARMINT  5 // Minutes
#define DEF_TXINT     5 // Minutes
#define DEF_LOWBATT   320 // 320 * 0.01V = 3.2V
#define DEF_DYNMODEL  DYN_MODEL_PORTABLE
#define DEF_ETX       0x03

#define CHECK_SERIAL_TIMEOUT 10UL // Make check_for_serial_data timeout after this many seconds
#define RX_IDLE_TIMEOUT 1UL // Make check_for_serial_data timeout after no further data is received for this many seconds

// Define the bits for FLAGS1
#define FLAGS1_BINARY   0x80
#define FLAGS1_DEST     0x40
#define FLAGS1_HIPRESS  0x20
#define FLAGS1_LOPRESS  0x10
#define FLAGS1_HITEMP   0x08
#define FLAGS1_LOTEMP   0x04
#define FLAGS1_HIHUMID  0x02
#define FLAGS1_LOHUMID  0x01

// Define the bits for FLAGS2
#define FLAGS2_GEOFENCE 0x80
#define FLAGS2_INSIDE   0x40
#define FLAGS2_LOWBATT  0x20
#define FLAGS2_RING     0x10

// Define the bits for MOFIELDS[0]
#define MOFIELDS0_SWVER     0x08000000
#define MOFIELDS0_SOURCE    0x00800000
#define MOFIELDS0_BATTV     0x00400000
#define MOFIELDS0_PRESS     0x00200000
#define MOFIELDS0_TEMP      0x00100000
#define MOFIELDS0_HUMID     0x00080000
#define MOFIELDS0_YEAR      0x00040000
#define MOFIELDS0_MONTH     0x00020000
#define MOFIELDS0_DAY       0x00010000
#define MOFIELDS0_HOUR      0x00008000
#define MOFIELDS0_MIN       0x00004000
#define MOFIELDS0_SEC       0x00002000
#define MOFIELDS0_MILLIS    0x00001000
#define MOFIELDS0_DATETIME  0x00000800
#define MOFIELDS0_LAT       0x00000400
#define MOFIELDS0_LON       0x00000200
#define MOFIELDS0_ALT       0x00000100
#define MOFIELDS0_SPEED     0x00000080
#define MOFIELDS0_HEAD      0x00000040
#define MOFIELDS0_SATS      0x00000020
#define MOFIELDS0_PDOP      0x00000010
#define MOFIELDS0_FIX       0x00000008
#define MOFIELDS0_GEOFSTAT  0x00000004

// Define the bits for MOFIELDS[1]
#define MOFIELDS1_USERVAL1  0x80000000
#define MOFIELDS1_USERVAL2  0x40000000
#define MOFIELDS1_USERVAL3  0x20000000
#define MOFIELDS1_USERVAL4  0x10000000
#define MOFIELDS1_USERVAL5  0x08000000
#define MOFIELDS1_USERVAL6  0x04000000
#define MOFIELDS1_USERVAL7  0x02000000
#define MOFIELDS1_USERVAL8  0x01000000
#define MOFIELDS1_MOFIELDS  0x00008000
#define MOFIELDS1_FLAGS1    0x00004000
#define MOFIELDS1_FLAGS2    0x00002000
#define MOFIELDS1_DEST      0x00001000
#define MOFIELDS1_HIPRESS   0x00000800
#define MOFIELDS1_LOPRESS   0x00000400
#define MOFIELDS1_HITEMP    0x00000200
#define MOFIELDS1_LOTEMP    0x00000100
#define MOFIELDS1_HIHUMID   0x00000080
#define MOFIELDS1_LOHUMID   0x00000040
#define MOFIELDS1_GEOFNUM   0x00000020
#define MOFIELDS1_GEOF1LAT  0x00000010
#define MOFIELDS1_GEOF1LON  0x00000008
#define MOFIELDS1_GEOF1RAD  0x00000004
#define MOFIELDS1_GEOF2LAT  0x00000002
#define MOFIELDS1_GEOF2LON  0x00000001

// Define the bits for MOFIELDS[2]
#define MOFIELDS2_GEOF2RAD  0x80000000
#define MOFIELDS2_GEOF3LAT  0x40000000
#define MOFIELDS2_GEOF3LON  0x20000000
#define MOFIELDS2_GEOF3RAD  0x10000000
#define MOFIELDS2_GEOF4LAT  0x08000000
#define MOFIELDS2_GEOF4LON  0x04000000
#define MOFIELDS2_GEOF4RAD  0x02000000
#define MOFIELDS2_WAKEINT   0x01000000
#define MOFIELDS2_ALARMINT  0x00800000
#define MOFIELDS2_TXINT     0x00400000
#define MOFIELDS2_LOWBATT   0x00200000
#define MOFIELDS2_DYNMODEL  0x00100000

// Define the bits for the alarm types
#define ALARM_HIPRESS 0x01
#define ALARM_LOPRESS 0x02
#define ALARM_HITEMP  0x04
#define ALARM_LOTEMP  0x08
#define ALARM_HIHUMID 0x10
#define ALARM_LOHUMID 0x20
#define ALARM_LOBATT  0x40

#define MOLIM 340 // Length limit for a Mobile Originated message
#define MTLIM 270 // Length limit for a Mobile Terminated message

// Define the status for serial data receive
enum tracker_serial_rx_status {
  DATA_NOT_SEEN = 0,
  DATA_SEEN,
  DATA_RECEIVED,
  DATA_TIMEOUT
};
  
// Define the result values for message parsing
enum tracker_parsing_result {
  DATA_VALID = 0,
  DATA_TOO_SHORT,
  NO_STX,
  INVALID_FIELD,
  NO_ETX,
  CHECKSUM_ERROR,
  DATA_WIDTH_INVALID
};

// Define the message field IDs (these are used in the binary format MO and MT messages)
enum tracker_message_fields
{
  STX       = 0x02,
  ETX       = 0x03,
  SWVER     = 0x04,
  SOURCE    = 0x08,
  BATTV     = 0x09,
  PRESS     = 0x0a,
  TEMP      = 0x0b,
  HUMID     = 0x0c,
  YEAR      = 0x0d,
  MONTH     = 0x0e,
  DAY       = 0x0f,
  HOUR      = 0x10,
  MIN       = 0x11,
  SEC       = 0x12,
  MILLIS    = 0x13,
  DATETIME  = 0x14,
  LAT       = 0x15,
  LON       = 0x16,
  ALT       = 0x17,
  SPEED     = 0x18,
  HEAD      = 0x19,
  SATS      = 0x1a,
  PDOP      = 0x1b,
  FIX       = 0x1c,
  GEOFSTAT  = 0x1d,
  USERVAL1  = 0x20,
  USERVAL2  = 0x21,
  USERVAL3  = 0x22,
  USERVAL4  = 0x23,
  USERVAL5  = 0x24,
  USERVAL6  = 0x25,
  USERVAL7  = 0x26,
  USERVAL8  = 0x27,
  MOFIELDS  = 0x30,
  FLAGS1    = 0x31,
  FLAGS2    = 0x32,
  DEST      = 0x33,
  HIPRESS   = 0x34,
  LOPRESS   = 0x35,
  HITEMP    = 0x36,
  LOTEMP    = 0x37,
  HIHUMID   = 0x38,
  LOHUMID   = 0x39,
  GEOFNUM   = 0x3a,
  GEOF1LAT  = 0x3b,
  GEOF1LON  = 0x3c,
  GEOF1RAD  = 0x3d,
  GEOF2LAT  = 0x3e,
  GEOF2LON  = 0x3f,
  GEOF2RAD  = 0x40,
  GEOF3LAT  = 0x41,
  GEOF3LON  = 0x42,
  GEOF3RAD  = 0x43,
  GEOF4LAT  = 0x44,
  GEOF4LON  = 0x45,
  GEOF4RAD  = 0x46,
  WAKEINT   = 0x47,
  ALARMINT  = 0x48,
  TXINT     = 0x49,
  LOWBATT   = 0x4a,
  DYNMODEL  = 0x4b,
  RBHEAD    = 0x52,
  USERFUNC1 = 0x58,
  USERFUNC2 = 0x59,
  USERFUNC3 = 0x5a,
  USERFUNC4 = 0x5b,
  USERFUNC5 = 0x5c,
  USERFUNC6 = 0x5d,
  USERFUNC7 = 0x5e,
  USERFUNC8 = 0x5f  
};

typedef struct {
  uint8_t ID;
  uint8_t data_width;
  } ID_width;

// Define unions for each data type so we can access individual bytes easily
typedef union {
  uint16_t the_data;
  byte the_bytes[2];
} union_uint16t;

typedef union {
  int16_t the_data;
  byte the_bytes[2];
} union_int16t;

typedef union {
  uint32_t the_data;
  byte the_bytes[4];
} union_uint32t;

typedef union {
  int32_t the_data;
  byte the_bytes[4];
} union_int32t;

typedef union {
  float the_data;
  byte the_bytes[4];
} union_float;

// Define the struct for _all_ of the message fields (stored in RAM)
typedef struct 
{
  byte STX;                   // 0x02 - when written to EEPROM, helps indicate if EEPROM contains valid data
  byte SWVER;                 // Software version: bits 7-4 = major version; bits 3-0 = minor version
  union_uint32t SOURCE;       // The tracker's RockBLOCK serial number
  union_uint16t BATTV;        // The battery (bus) voltage in V * 10^-2
  union_uint16t PRESS;        // The pressure in mbar
  union_int16t TEMP;          // The temperature in degrees C * 10^-2
  union_uint16t HUMID;        // The humidity in %RH * 10^-2
  union_uint16t YEAR;         // UTC year
  byte MONTH;                 // UTC month
  byte DAY;                   // UTC day
  byte HOUR;                  // UTC hour
  byte MIN;                   // UTC minute
  byte SEC;                   // UTC seconds
  union_uint16t MILLIS;       // UTC milliseconds
  union_int32t LAT;           // Latitude in degrees * 10^-7
  union_int32t LON;           // Latitude in degrees * 10^-7
  union_int32t ALT;           // Altitude above MSL in mm
  union_int32t SPEED;         // Ground speed in mm/s
  union_int32t HEAD;          // The heading in degrees * 10^-7
  byte SATS;                  // The number of satellites (space vehicles) used in the solution
  union_uint16t PDOP;         // The Positional Dilution of Precision in cm
  byte FIX;                   // The GNSS fix type as defined in the u-blox PVT message
  byte GEOFSTAT[3];           // The geofence status as defined in the u-blox UBX-NAV-GEOFENCE message
  byte USERVAL1;              // User value 1
  byte USERVAL2;              // User value 2
  union_uint16t USERVAL3;     // User value 3
  union_uint16t USERVAL4;     // User value 4
  union_uint32t USERVAL5;     // User value 5
  union_uint32t USERVAL6;     // User value 6
  union_float USERVAL7;       // User value 7
  union_float USERVAL8;       // User value 8
  union_uint32t MOFIELDS[3];  // Defines which fields are sent in MO messages
  byte FLAGS1;                // Defines various message options
  byte FLAGS2;                // Defines various message options
  union_uint32t DEST;         // The destination RockBLOCK serial number for message forwarding
  union_uint16t HIPRESS;      // The high pressure limit in mbar
  union_uint16t LOPRESS;      // The low pressure limit in mbar
  union_int16t HITEMP;        // The high temperature limit in degrees C * 10^-2
  union_int16t LOTEMP;        // The low temperature limit in degrees C * 10^-2
  union_uint16t HIHUMID;      // The high humidity limit in %RH * 10^-2
  union_uint16t LOHUMID;      // The low humidity limit in %RH * 10^-2
  byte GEOFNUM;               // Bits 7-4 = the number of geofences (0-4); bits 3-0 = confidence level (0-4)
  union_int32t GEOF1LAT;      // The latitude of the center of geofence circle 1 in degrees * 10^-7
  union_int32t GEOF1LON;      // The longitude of the center of geofence circle 1 in degrees * 10^-7
  union_uint32t GEOF1RAD;     // The radius of geofence circle 1 in cm
  union_int32t GEOF2LAT;      // The latitude of the center of geofence circle 2 in degrees * 10^-7
  union_int32t GEOF2LON;      // The longitude of the center of geofence circle 2 in degrees * 10^-7
  union_uint32t GEOF2RAD;     // The radius of geofence circle 2 in cm
  union_int32t GEOF3LAT;      // The latitude of the center of geofence circle 3 in degrees * 10^-7
  union_int32t GEOF3LON;      // The longitude of the center of geofence circle 3 in degrees * 10^-7
  union_uint32t GEOF3RAD;     // The radius of geofence circle 3 in cm
  union_int32t GEOF4LAT;      // The latitude of the center of geofence circle 4 in degrees * 10^-7
  union_int32t GEOF4LON;      // The longitude of the center of geofence circle 4 in degrees * 10^-7
  union_uint32t GEOF4RAD;     // The radius of geofence circle 4 in cm
  union_uint32t WAKEINT;      // The wake-up interval in seconds
  union_uint16t ALARMINT;     // The alarm transmit interval in minutes
  union_uint16t TXINT;        // The message transmit interval in minutes
  union_uint16t LOWBATT;      // The low battery limit in V * 10^-2
  dynModel DYNMODEL;          // The GNSS dynamic platform model
  byte ETX;                   // 0x03 - when written to EEPROM, helps indicate if EEPROM contains valid data
} trackerSettings;

// Define the binary data widths for _all_ IDs
#define NUM_ID_WIDTHS 70
ID_width ID_widths[NUM_ID_WIDTHS] = {
  {STX       , 0 },
  {ETX       , 0 },
  {SWVER     , 1 },
  {SOURCE    , 4 },
  {BATTV     , 2 },
  {PRESS     , 2 },
  {TEMP      , 2 },
  {HUMID     , 2 },
  {YEAR      , 2 },
  {MONTH     , 1 },
  {DAY       , 1 },
  {HOUR      , 1 },
  {MIN       , 1 },
  {SEC       , 1 },
  {MILLIS    , 2 },
  {DATETIME  , 7 },
  {LAT       , 4 },
  {LON       , 4 },
  {ALT       , 4 },
  {SPEED     , 4 },
  {HEAD      , 4 },
  {SATS      , 1 },
  {PDOP      , 2 },
  {FIX       , 1 },
  {GEOFSTAT  , 3 },
  {USERVAL1  , 1 },
  {USERVAL2  , 1 },
  {USERVAL3  , 2 },
  {USERVAL4  , 2 },
  {USERVAL5  , 4 },
  {USERVAL6  , 4 },
  {USERVAL7  , 4 },
  {USERVAL8  , 4 },
  {MOFIELDS  , 12 },
  {FLAGS1    , 1 },
  {FLAGS2    , 1 },
  {DEST      , 4 },
  {HIPRESS   , 2 },
  {LOPRESS   , 2 },
  {HITEMP    , 2 },
  {LOTEMP    , 2 },
  {HIHUMID   , 2 },
  {LOHUMID   , 2 },
  {GEOFNUM   , 1 },
  {GEOF1LAT  , 4 },
  {GEOF1LON  , 4 },
  {GEOF1RAD  , 4 },
  {GEOF2LAT  , 4 },
  {GEOF2LON  , 4 },
  {GEOF2RAD  , 4 },
  {GEOF3LAT  , 4 },
  {GEOF3LON  , 4 },
  {GEOF3RAD  , 4 },
  {GEOF4LAT  , 4 },
  {GEOF4LON  , 4 },
  {GEOF4RAD  , 4 },
  {WAKEINT   , 4 },
  {ALARMINT  , 2 },
  {TXINT     , 2 },
  {LOWBATT   , 2 },
  {DYNMODEL  , 1 },
  {RBHEAD    , 4 },
  {USERFUNC1 , 0 },
  {USERFUNC2 , 0 },
  {USERFUNC3 , 0 },
  {USERFUNC4 , 0 },
  {USERFUNC5 , 2 },
  {USERFUNC6 , 2 },
  {USERFUNC7 , 4 },
  {USERFUNC8 , 4 }
};

#endif
