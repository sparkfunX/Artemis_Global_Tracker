# Artemis Global Tracker: Message Format and Fields

This document defines the message format used by the full GlobalTracker example.

The Iridium 9603N and the RockBLOCK Gateway support messaging in both text and binary format. Text is human-readable; binary is more compact and will use fewer message credits.

- Mobile Originated (MO) messages are messages sent _by_ the tracker.
- The maximum MO message length is **340** bytes.

- Mobile Terminated (MT) messages are messages sent _to_ the tracker.
- The maximum MT message length is **270** bytes.

Messages can be sent in both formats in both directions. However, for the Artemis Tracker we define that:

- MO messages can be sent in binary or text format.
- MT messages are only used to configure the tracker (remotely) and so should be sent in binary format only.

The [Tools](../../Tools) folder contains a tool which will generate a configuration message in ASCII-encoded Hex format,
which you can then send to the tracker via the _Send a Message_ function in Rock7 Operations. You can of course configure the tracker via USB-C too.

## The message formats:

### Text

This section defines the format of MO _text_ messages sent by the tracker.

The general format is:

| Header | , | Field 1 | , | Field 2 | , | ... | , | Field n |
|---|---|---|---|---|---|---|---|---|

- Gateway Header: **OPTIONAL** - 9 characters - [see below](#gateway-header) - followed by a comma (to make it easy to split the message string)
- Message Fields: sent as ASCII text in Comma Separated Value (CSV) format

MOFIELDS defines which message fields are to be or are being sent. The fields are sent in ascending ID order.

The length of each field will vary depending on the value it contains.

---
### Binary

This section defines the format of MO and MT _binary_ messages sent by or to the tracker.

The general format is:

| Header | STX | ID 1 | Data | ID 2 | Data | ... | ID n | Data | ETX | CS_A | CS_B
|---|---|---|---|---|---|---|---|---|---|---|---|

- Gateway Header: **OPTIONAL** - 5 bytes - [see below](#gateway-header)
- STX: 0x02
- Message fields: sent as a single unsigned byte (uint8_t) field ID followed by the correct number of data bytes for that field
- ETX: 0x03
- Checksum A: a single unsigned byte (uint8_t) containing the modulo-256 checksum of all bytes between the STX and ETX bytes (STX and ETX are included in the checksum but the gateway header is excluded)
- Checksum B: a single unsigned byte (uint8_t) modulo-256 cumulative sum of the checksum bytes

The checksum bytes are as defined in the 8-Bit Fletcher Algorithm, used by the TCP standard (RFC 1145), and are the same as those used by u-blox UBX format messages.

```
CS_A = 0, CS_B = 0
For(I=0;I<N;I++)
{
  CS_A = CS_A + Buffer[I]
  CS_B = CS_B + CS_A
}
```

---
### Gateway Header:

The Rock7 RockBLOCK Gateway allows you to automatically forward messages from one RockBLOCK (Iridium transceiver) to another, so long as they are
both registered to the same account. This is a powerful feature as it means you can send messages between trackers, anywhere, without needing an internet connection.

For text messages: the message is prefixed by the letters "RB" plus the RockBLOCK serial number of the destination transceiver padded out to seven digits. E.g. if
the RockBLOCK serial number of the transceiver you want to send the message to is _12345_ then the message would be prefixed with _RB0012345_

For binary messages: the message is still prefixed with ASCII "RB" (0x5242 in Hex) but the serial number is converted to unsigned 24-bit (3-byte) **big-endian** format.
Using the same example, 12345 is 0x003039 in Hex so the message would be prefixed by _0x5242003039_

Please follow [this link](https://docs.rockblock.rock7.com/docs/messaging-between-rockblocks) for further details.

---
### Message Fields

This table defines the Artemis Global Tracker message fields:
- ID: is the _uint8_t_ ID code for the message field
- Abv.: the abreviated field name
- Type: is the data type used to store the field in the tracker code
- Bin_Len: is the length of the data when sent as a binary message (excluding the preceding ID byte)
- Txt_Len: is the _maximum_ field length when sent as a text message (excluding the trailing comma)
- MO: can this field be sent in a Mobile Originated message?
- MT: can this field be set remotely by a Mobile Terminated message?
- USB: can this field be configured via the USB-C serial interface?
- EEPROM: is the data for this field stored in EEPROM?
- Desc.: a brief description of this field

Follow the ID links for a full definition and examples for each field.

If the number of IDs is expanded beyond 0x5f, MOFIELDS will need to be redefined accordingly.

| ID | Abv. | Type | Bin_Len | Txt_Len | MO | MT | USB | EEPROM | Desc. |
|---|---|---|---|---|---|---|---|---|---|
| 0x00 | RESV | | | | | | | | **Reserved - do not use** |
| 0x01 | RESV | | | | | | | | **Reserved - do not use** |
| [0x02](#stx-0x02) | STX | | | | | | | Yes | ASCII STX - used to indicate the start of a binary message |
| [0x03](#etx-0x03) | ETX | | | | | | | Yes | ASCII ETX - used to indicate the end of a binary message |
| [0x04](#swver-0x04) | SWVER | byte | 1 | 5 | Yes | No | No | Yes | The tracker software version |
| 0x05 | RESV | | | | | | | | **Reserved - do not use** |
| 0x06 | RESV | | | | | | | | **Reserved - do not use** |
| 0x07 | RESV | | | | | | | | **Reserved - do not use** |
| [0x08](#source-0x08) | SOURCE | uint32_t | 4 | 7 | Yes | No | Yes | Yes | The source address = the tracker's RockBLOCK serial number |
| [0x09](#battv-0x09) | BATTV | uint16_t | 2 | 4 | Yes | No | No | No | The battery voltage (actually the bus voltage) |
| [0x0a](#press-0x0a) | PRESS | uint16_t | 2 | 4 | Yes | No | No | No | The atmospheric pressure in mbar |
| [0x0b](#temp-0x0b) | TEMP | int16_t | 2 | 6 | Yes | No | No | No | The atmospheric temperature in Centigrade |
| [0x0c](#humid-0x0c) | HUMID | uint16_t | 2 | 6 | Yes | No | No | No | The atmospheric humidity in %RH |
| [0x0d](#year-0x0d) | YEAR | uint16_t | 2 | 4 | Yes | No | No | No | UTC year |
| [0x0e](#month-0x0e) | MONTH | byte | 1 | 2 | Yes | No | No | No | UTC month |
| [0x0f](#day-0x0f) | DAY | byte | 1 | 2 | Yes | No | No | No | UTC day |
| [0x10](#hour-0x10) | HOUR | byte | 1 | 2 | Yes | No | No | No | UTC hour |
| [0x11](#min-0x11) | MIN | byte | 1 | 2 | Yes | No | No | No | UTC minute |
| [0x12](#sec-0x12) | SEC | byte | 1 | 2 | Yes | No | No | No | UTC second |
| [0x13](#millis-0x13) | MILLIS | uint16_t | 2 | 3 | Yes | No | No | No | UTC milliseconds |
| [0x14](#datetime-0x14) | DATETIME | | 7 | 14 | Yes | No | No | No | Concatenated UTC Date & Time |
| [0x15](#lat-0x15) | LAT | int32_t | 4 | 11 | Yes | No | No | No | The latitude in degrees |
| [0x16](#lon-0x16) | LON | int32_t | 4 | 12 | Yes | No | No | No | The longitude in degrees |
| [0x17](#alt-0x17) | ALT | int32_t | 4 | 9 | Yes | No | No | No | The altitude above MSL |
| [0x18](#speed-0x18) | SPEED | int32_t | 4 | 7 | Yes | No | No | No | The ground speed |
| [0x19](#head-0x19) | HEAD | int32_t | 4 | 6 | Yes | No | No | No | The course (heading) in degrees |
| [0x1a](#sats-0x1a) | SATS | byte | 1 | 2 | Yes | No | No | No | The number of satellites (space vehicles) used in the solution |
| [0x1b](#pdop-0x1b) | PDOP | uint16_t | 2 | 6 | Yes | No | No | No | The positional dilution of precision |
| [0x1c](#fix-0x1c) | FIX | byte | 1 | 1 | Yes | No | No | No | The GNSS fix type |
| [0x1d](#geofstat-0x1d) | GEOFSTAT | 3 x byte | 3 | 6 | Yes | No | No | No | The geofence status |
| 0x1e - 0x1f | | | | | | | | | **Currently undefined - do not use** |
| [0x20](#userval1-0x20) | USERVAL1 | byte | 1 | 3 | Yes | No | No | No | User value 1 (e.g. from an external sensor) |
| [0x21](#userval2-0x21) | USERVAL2 | byte | 1 | 3 |Yes | No | No | No | User value 2 |
| [0x22](#userval3-0x22) | USERVAL3 | uint16_t | 2 | 5 |Yes | No | No | No | User value 3 |
| [0x23](#userval4-0x23) | USERVAL4 | uint16_t | 2 | 5 |Yes | No | No | No | User value 4 |
| [0x24](#userval5-0x24) | USERVAL5 | uint32_t | 4 | 10 |Yes | No | No | No | User value 5 |
| [0x25](#userval6-0x25) | USERVAL6 | uint32_t | 4 | 10 |Yes | No | No | No | User value 6 |
| [0x26](#userval7-0x26) | USERVAL7 | float | 4 | 14 |Yes | No | No | No | User value 7 |
| [0x27](#userval8-0x27) | USERVAL8 | float | 4 | 14 |Yes | No | No | No | User value 8 |
| 0x28 - 0x2f | | | | | | | | | **Currently undefined - do not use** |
| [0x30](#mofields-0x30) | MOFIELDS | 3 x uint32_t | 12 | 24 | Yes | Yes | Yes | Yes | Defines which fields are included in MO messages |
| [0x31](#flags1-0x31) | FLAGS1 | byte | 1 | 2 | Yes | Yes | Yes | Yes | Defines various message options - see below for the full definition |
| [0x32](#flags2-0x32) | FLAGS2 | byte | 1 | 2 | Yes | Yes | Yes | Yes | Defines various message options - see below for the full definition |
| [0x33](#dest-0x33) | DEST | uint32_t | 4 | 7 | Yes | Yes | Yes | Yes | The destination RockBLOCK serial number for message forwarding |
| [0x34](#hipress-0x34) | HIPRESS | uint16_t | 2 | 4 | Yes | Yes | Yes | Yes | The high pressure alarm limit |
| [0x35](#lopress-0x35) | LOPRESS | uint16_t | 2 | 4 | Yes | Yes | Yes | Yes | The low pressure alarm limit |
| [0x36](#hitemp-0x36) | HITEMP | int16_t | 2 | 6 | Yes | Yes | Yes | Yes | The high temperature alarm limit |
| [0x37](#lotemp-0x37) | LOTEMP | int16_t | 2 | 6 | Yes | Yes | Yes | Yes | The low temperature alarm limit |
| [0x38](#hihumid-0x38) | HIHUMID | uint16_t | 2 | 6 | Yes | Yes | Yes | Yes | The high humidity alarm limit |
| [0x39](#lohumid-0x39) | LOHUMID | uint16_t | 2 | 6 | Yes | Yes | Yes | Yes | The low humidity alarm limit |
| [0x3a](#geofnum-0x3a) | GEOFNUM | byte | 1 | 2 | Yes | Yes | Yes | Yes | The number of geofences (0-4) and confidence level (0-4) |
| [0x3b](#geof1lat-0x3b) | GEOF1LAT | int32_t | 4 | 11 | Yes | Yes | Yes | Yes | The latitude of the center of geofence circle 1 |
| [0x3c](#geof1lon-0x3c) | GEOF1LON | int32_t | 4 | 12 | Yes | Yes | Yes | Yes | The longitude of the center of geofence circle 1 |
| [0x3d](#geof1rad-0x3d) | GEOF1RAD | uint32_t | 4 | 9 | Yes | Yes | Yes | Yes | The radius of geofence circle 1 |
| [0x3e](#geof2lat-0x3e) | GEOF2LAT | int32_t | 4 | 11 | Yes | Yes | Yes | Yes | The latitude of the center of geofence circle 2 |
| [0x3f](#geof2lon-0x3f) | GEOF2LON | int32_t | 4 | 12 | Yes | Yes | Yes | Yes | The longitude of the center of geofence circle 2 |
| [0x40](#geof2rad-0x40) | GEOF2RAD | uint32_t | 4 | 9 | Yes | Yes | Yes | Yes | The radius of geofence circle 2 |
| [0x41](#geof3lat-0x41) | GEOF3LAT | int32_t | 4 | 11 | Yes | Yes | Yes | Yes | The latitude of the center of geofence circle 3 |
| [0x42](#geof3lon-0x42) | GEOF3LON | int32_t | 4 | 12 | Yes | Yes | Yes | Yes | The longitude of the center of geofence circle 3 |
| [0x43](#geof3rad-0x43) | GEOF3RAD | uint32_t | 4 | 9 | Yes | Yes | Yes | Yes | The radius of geofence circle 3 |
| [0x44](#geof4lat-0x44) | GEOF4LAT | int32_t | 4 | 11 | Yes | Yes | Yes | Yes | The latitude of the center of geofence circle 4 |
| [0x45](#geof4lon-0x45) | GEOF4LON | int32_t | 4 | 12 | Yes | Yes | Yes | Yes | The longitude of the center of geofence circle 4 |
| [0x46](#geof4rad-0x46) | GEOF4RAD | uint32_t | 4 | 9 | Yes | Yes | Yes | Yes | The radius of geofence circle 4 |
| [0x47](#wakeint-0x47) | WAKEINT | uint32_t | 4 | 5 | Yes | Yes | Yes | Yes | Defines the tracker's wake-up interval (seconds)  |
| [0x48](#alarmint-0x48) | ALARMINT | uint16_t | 2 | 4 | Yes | Yes | Yes | Yes | Defines the tracker's transmission interval during an alarm (minutes)  |
| [0x49](#txint-0x49) | TXINT | uint16_t | 2 | 4 | Yes | Yes | Yes | Yes | Defines the tracker's normal transmission interval (minutes)  |
| [0x4a](#lowbatt-0x4a) | LOWBATT | uint16_t | 2 | 4 | Yes | Yes | Yes | Yes | The low battery limit |
| [0x4b](#dynmodel-0x4b) | DYNMODEL | byte | 1 | 2 | Yes | Yes | Yes | Yes | The GNSS dynamic platform model |
| 0x4c - 0x51 | | | | | | | | | **Currently undefined - do not use** |
| 0x52 | RBHEAD | | 4 | | | | | | **ASCII "R" - Reserved for the RockBLOCK gateway header (message forwarding)** |
| 0x53 - 0x57 | | | | | | | | | **Currently undefined - do not use** |
| [0x58](#userfunc1-0x58) | USERFUNC1 | N/A | 0 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 1 |
| [0x59](#userfunc2-0x59) | USERFUNC2 | N/A | 0 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 2 |
| [0x5a](#userfunc3-0x5a) | USERFUNC3 | N/A | 0 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 3 |
| [0x5b](#userfunc4-0x5b) | USERFUNC4 | N/A | 0 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 4 |
| [0x5c](#userfunc5-0x5c) | USERFUNC5 | uint16_t | 2 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 5 |
| [0x5d](#userfunc6-0x5d) | USERFUNC6 | uint16_t | 2 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 6 |
| [0x5e](#userfunc7-0x5e) | USERFUNC7 | uint32_t | 4 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 7 |
| [0x5f](#userfunc8-0x5f) | USERFUNC8 | uint32_t | 4 | N/A | No | Yes | Yes | N/A | Instructs the tracker to execute user function 8 |

---
### Field Definition

The uint16_t, uint32_t and float examples are calculated using:
- https://www.binaryconvert.com/convert_signed_short.html
- https://www.binaryconvert.com/convert_signed_int.html
- https://www.binaryconvert.com/convert_float.html

---
STX (0x02)
---

| Description: | ASCII STX - used to indicate the start of a binary message (not used in text messages) |
|---|---|

---
ETX (0x03)
---

| Description: | ASCII ETX - used to indicate the end of a binary message (not used in text messages) |
|---|---|

---
SWVER (0x04)
---

| []() | |
|---|---|
| Description: | The tracker software version. |
| Binary: | byte, the 4 most significant bits indicate the major software version, the 4 least significant bits indicate the minor software version. |
| Text: | Sent in the format m.m in the range 0.0 to 15.15 |
| Example value: | Major version 1, minor version 3 |
| Binary example: | 0x0413 |
| Text example: | 1.3 |

---
SOURCE (0x08)
---

| []() | |
|---|---|
| Description: | The source address = the tracker's RockBLOCK serial number. |
| Binary: | uint32_t, 4 bytes, little endian. |
| Text: | Sent in the format nnnnn in the range 0 to 9999999 |
| Example value: | 12345 |
| Binary example: | 0x0839300000 (12345 is 0x00003039) |
| Text example: | 12345 |

---
BATTV (0x09)
---

| []() | |
|---|---|
| Description: | The battery voltage (actually the bus voltage; the battery voltage minus a small diode voltage drop). |
| Binary: | uint16_t, 2 bytes, little endian, in Volts * 10^-2. |
| Text: | Sent in the format v.v with 1 or 2 decimal places in the range 0.0 to 9.99 |
| Example value: | 3.60V |
| Binary example: | 0x096801 (360 is 0x0168) |
| Text example: | 3.6 |

---
PRESS (0x0a)
---

| []() | |
|---|---|
| Description: | The atmospheric pressure in mbar. |
| Binary: | uint16_t, 2 bytes, little endian, in mbar. |
| Text: | Sent in the format nnnn in the range 0 to 1084 (approx.) |
| Example value: | 998 |
| Binary example: | 0x0ae603 (998 is 0x03e6) |
| Text example: | 998 |

---
TEMP (0x0b)
---

| []() | |
|---|---|
| Description: | The atmospheric temperature in Centigrade. |
| Binary: | int16_t (signed), 2 bytes, little endian, in Centigrade * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places, signed, in the range -40.0 to 85.0 (approx.) |
| Example value: | -12.34C |
| Binary example: | 0x0b2efb (-1234 is 0xfb2e) |
| Text example: | -12.34 |

---
HUMID (0x0c)
---

| []() | |
|---|---|
| Description: | The atmospheric humidity in %RH. |
| Binary: | uint16_t, 2 bytes, little endian, in %RH * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places in the range 0.0 to 100.0 (approx.) |
| Example value: | 12.34%RH |
| Binary example: | 0x0cd204 (1234 is 0x04d2) |
| Text example: | 12.34 |

---
YEAR (0x0d)
---

| []() | |
|---|---|
| Description: | UTC year. |
| Binary: | uint16_t, 2 bytes, little endian. |
| Text: | Sent in the format YYYY |
| Example value: | 2019 |
| Binary example: | 0x0de307 (2019 is 0x07e3) |
| Text example: | 2019 |

---
MONTH (0x0e)
---

| []() | |
|---|---|
| Description: | UTC month. |
| Binary: | byte. |
| Text: | Sent in the format MM _without a preceding zero_ |
| Example value: | 7 |
| Binary example: | 0x0e07 (7 is 0x07) |
| Text example: | 7 |

---
DAY (0x0f)
---

| []() | |
|---|---|
| Description: | UTC day. |
| Binary: | byte. |
| Text: | Sent in the format DD _without a preceding zero_ |
| Example value: | 16 |
| Binary example: | 0x0f10 (16 is 0x10) |
| Text example: | 16 |

---
HOUR (0x10)
---

| []() | |
|---|---|
| Description: | UTC hour. |
| Binary: | byte. |
| Text: | Sent in 24 hour format HH _without a preceding zero_ |
| Example value: | 23 |
| Binary example: | 0x1017 (23 is 0x17) |
| Text example: | 23 |

---
MIN (0x11)
---

| []() | |
|---|---|
| Description: | UTC minute. |
| Binary: | byte. |
| Text: | Sent in the format MM _without a preceding zero_ |
| Example value: | 7 |
| Binary example: | 0x1107 (7 is 0x07) |
| Text example: | 7 |

---
SEC (0x12)
---

| []() | |
|---|---|
| Description: | UTC seconds. |
| Binary: | byte. |
| Text: | Sent in the format SS _without a preceding zero_ |
| Example value: | 23 |
| Binary example: | 0x1217 (23 is 0x17) |
| Text example: | 23 |

---
MILLIS (0x13)
---

| []() | |
|---|---|
| Description: | UTC milliseconds. |
| Binary: | uint16_t, 2 bytes, little endian. |
| Text: | Sent in the format MMM _without preceding zeroes_ |
| Example value: | 470 |
| Binary example: | 0x13d601 (470 is 0x01d6) |
| Text example: | 470 |

---
DATETIME (0x14)
---

| []() | |
|---|---|
| Description: | Concatenated UTC date and time. |
| Binary: | 7 bytes, YYMDHMS, year is little endian. |
| Text: | Sent in the format YYYYMMDDHHMMSS **with preceding zeroes as required** |
| Example value: | 2019/7/16 23:07:23 |
| Binary example: | 0x14e3070710170717 |
| Text example: | 20190716230723 |

---
LAT (0x15)
---

| []() | |
|---|---|
| Description: | The latitude in degrees. |
| Binary: | int32_t (signed), 4 bytes, little endian, in degrees * 10^-7. |
| Text: | Sent in the format -d.d with up to 7 decimal places in the range -90.0 to 90.0 _without preceding or trailing zeroes_ |
| Example value: | 40 degrees South |
| Binary example: | 0x15007c28e8 (-400000000 is 0xe8287c00) |
| Text example: | -40.0 |

---
LON (0x16)
---

| []() | |
|---|---|
| Description: | The longitude in degrees. |
| Binary: | int32_t (signed), 4 bytes, little endian, in degrees * 10^-7. |
| Text: | Sent in the format -d.d with up to 7 decimal places in the range -180.0 to 179.9999999 _without preceding or trailing zeroes_ |
| Example value: | 170 degrees West |
| Binary example: | 0x16000fac9a (-1700000000 is 0x9aac0f00) |
| Text example: | -170.0 |

---
ALT (0x17)
---

| []() | |
|---|---|
| Description: | The altitude above MSL. |
| Binary: | int32_t (signed), 4 bytes, little endian, in **mm** above MSL. |
| Text: | Sent as **m** above MSL, with mm resolution, in the format -m.m with up to 3 decimal places in the range -420.0 to 50000.0 _without preceding or trailing zeroes_ |
| Example value: | 123m above MSL |
| Binary example: | 0x1778e00100 (123000 is 0x0001e078) |
| Text example: | 123.0 |

---
SPEED (0x18)
---

| []() | |
|---|---|
| Description: | The ground speed. |
| Binary: | int32_t (signed), 4 bytes, little endian, in **mm/s**. |
| Text: | Sent as **m/s**, with mm/s resolution, in the format s.s with up to 3 decimal places in the range 0.0 to 500.0 _without preceding or trailing zeroes_ |
| Example value: | 10 m/s |
| Binary example: | 0x1810270000 (10000 is 0x00002710) |
| Text example: | 10.0 |

---
HEAD (0x19)
---

| []() | |
|---|---|
| Description: | The heading in degrees. |
| Binary: | int32_t (signed), 4 bytes, little endian, in degrees * 10^-7. |
| Text: | Sent in the format -d.d with **1** decimal place _without preceding zeroes_ |
| Example value: | 45 degrees |
| Binary example: | 0x198074d21a (450000000 is 0x1ad27480) |
| Text example: | 45.0 |

---
SATS (0x1a)
---

| []() | |
|---|---|
| Description: | The number of satellites (space vehicles) used in the position solution. |
| Binary: | byte. |
| Text: | Sent in the format nn _without a preceding zero_ |
| Example value: | 14 |
| Binary example: | 0x1a0e (14 is 0x0e) |
| Text example: | 14 |

---
PDOP (0x1b)
---

| []() | |
|---|---|
| Description: | The positional dilution of precision. |
| Binary: | uint16_t, 2 bytes, little endian, in **cm**. |
| Text: | Sent as **m**, with cm resolution, in the format m.m with 1 or 2 decimal places _without preceding or trailing zeroes_ |
| Example value: | 1.02m |
| Binary example: | 0x1b6600 (102 is 0x0066) |
| Text example: | 1.02 |

---
FIX (0x1c)
---

| []() | |
|---|---|
| Description: | The GNSS fix type as defined in the u-blox PVT message: 0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix |
| Binary: | byte. |
| Text: | Sent in the format n in the range 0 to 5 |
| Example value: | 3 |
| Binary example: | 0x1c03 (3 is 0x03) |
| Text example: | 3 |

---
GEOFSTAT (0x1d)
---

| []() | |
|---|---|
| Description: | The geofence status as defined in the u-blox UBX-NAV-GEOFENCE message |
| Binary: | 3 x byte: |
| | Byte 0 Bits 7-4: status: 0x0 - Geofencing not available or not reliable; 0x1 - Geofencing active |
| | Byte 0 Bits 3-0: combState: combined (logical OR) state of all geofences: 0x0 - Unknown; 0x1 - Inside; 0x2 - Outside |
| | Byte 1 Bits 7-4: geofence 1 state: 0x0 - Unknown; 0x1 - Inside; 0x2 - Outside |
| | Byte 1 Bits 3-0: geofence 2 state: 0x0 - Unknown; 0x1 - Inside; 0x2 - Outside |
| | Byte 2 Bits 7-4: geofence 3 state: 0x0 - Unknown; 0x1 - Inside; 0x2 - Outside |
| | Byte 2 Bits 3-0: geofence 4 state: 0x0 - Unknown; 0x1 - Inside; 0x2 - Outside |
| Text: | Sent as six ASCII digits: 000000 to 122222 |
| Example value: | Geofencing is active and the tracker is inside all four geofences |
| Binary example: | 0x1d111111 |
| Text example: | 111111 |

---
USERVAL1 (0x20)
---

| []() | |
|---|---|
| Description: | A value defined by the user (e.g. the reading from an external sensor) |
| Binary: | byte. |
| Text: | Sent in the format n in the range 0 to 255 |
| Example value: | 3 |
| Binary example: | 0x2003 (3 is 0x03) |
| Text example: | 3 |

---
USERVAL2 (0x21)
---

See USERVAL1

---
USERVAL3 (0x22)
---

| []() | |
|---|---|
| Description: | A value defined by the user (e.g. the reading from an external sensor) |
| Binary: | uint16_t, 2 bytes, little endian. |
| Text: | Sent in the format n in the range 0 to 65535 |
| Example value: | 3 |
| Binary example: | 0x220300 (3 is 0x0003) |
| Text example: | 3 |

---
USERVAL4 (0x23)
---

See USERVAL3

---
USERVAL5 (0x24)
---

| []() | |
|---|---|
| Description: | A value defined by the user (e.g. the reading from an external sensor) |
| Binary: | uint32_t, 4 bytes, little endian. |
| Text: | Sent in the format n in the range 0 to 4294967295 |
| Example value: | 3 |
| Binary example: | 0x2403000000 (3 is 0x00000003) |
| Text example: | 3 |

---
USERVAL6 (0x25)
---

See USERVAL5

---
USERVAL7 (0x26)
---

| []() | |
|---|---|
| Description: | A value defined by the user (e.g. the reading from an external sensor) |
| Binary: | float, 4 bytes, little endian. |
| Text: | Sent in the format n in the range 3.4028235E+38 to -3.4028235E+38 |
| Example value: | 3.0 |
| Binary example: | 0x2600004040 (3.0 is 0x40400000 (3.0E0)) |
| Text example: | 3.0 |

---
USERVAL8 (0x27)
---

See USERVAL7

---
MOFIELDS (0x30)
---

| []() | |
|---|---|
| Description: | Defines or shows which fields are included in MO messages. |
| Binary: | 3 x uint32_t, **little endian**. The most significant bit of the first uint32_t defines if the field 0x00 will be sent. The least significant bit of the third uint32_t defines if field 0x5f will be sent. |
| Text: | Sent as ASCII-encoded Hex (**little endian**) in the range 000000000000000000000000 to ffffffffffffffffffffffff |
| Example value: | To send (only) DATETIME (0x14), LAT (0x15), LON (0x16) and ALT (0x17) |
| Binary example: | 0x30000f00000000000000000000 |
| Text example: | 000f00000000000000000000 |
| Default value: | 0x000f00000000000000000000 |

**Notes:**

Although MOFIELDS has a bit for every field ID, some are of course reserved / undefined or invalid for MO messages. Each field will only be included if valid for MO messages.

---
FLAGS1 (0x31)
---

| []() | |
|---|---|
| Description: | Defines various message options. |
| Binary: | byte. |
| []() | |
| Bit 7 (MSB): | Set to 1 if binary messages are to be sent / are being sent. Set to 0 for text messages (default). |
| Bit 6: | When set to 1, message forwarding via the RockBLOCK gateway is enabled. Message will be forwarded automatically to RB DEST. |
| Bit 5: | When set to 1, a message will be sent when PRESS > HIPRESS. PRESS is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until PRESS is < HIPRESS. |
| Bit 4: | When set to 1, a message will be sent when PRESS < LOPRESS. PRESS is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until PRESS is > LOPRESS. |
| Bit 3: | When set to 1, a message will be sent when TEMP > HITEMP. TEMP is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until TEMP is < HITEMP. |
| Bit 2: | When set to 1, a message will be sent when TEMP < LOTEMP. TEMP is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until TEMP is > LOTEMP. |
| Bit 1: | When set to 1, a message will be sent when HUMID > HIHUMID. HUMID is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until HUMID is < HIHUMID. |
| Bit 0 (LSB): | When set to 1, a message will be sent when HUMID < LOHUMID. HUMID is checked every WAKEINT seconds. Messages will be sent every ALARMINT minutes until HUMID is > LOHUMID. |
| []() | |
| Text: | Sent as ASCII-encoded Hex in the range 00 to ff |
| Example value: | To send binary messages, without forwarding, and alarm when TEMP is > HITEMP |
| Binary example: | 0x3188 (B10001000 is 0x88) |
| Text example: | 88 |
| Default value: | 0x00 |

---
FLAGS2 (0x32)
---

| []() | |
|---|---|
| Description: | Defines various message options. |
| Binary: | byte. |
| []() | |
| Bit 7 (MSB): | When set to 1, geofence alerts are enabled. |
| Bit 6: | When set to 1, geofence alerts will be sent every ALARMINT minutes when the tracker _is inside_ a geofenced area. When clear, geofence alerts will be sent when the tracker _is outside_ all geofenced areas (default). |
| Bit 5: | When set to 1, messages will be sent every ALARMINT minutes when the battery (bus) voltage falls below LOWBATT. |
| Bit 4: | When set to 1, the tracker will stay powered up and will monitor the Iridium ring channel continuously for new MT messages. |
| Bit 3: | Undefined. |
| Bit 2: | Undefined. |
| Bit 1: | Undefined. |
| Bit 0 (LSB): | Undefined. |
| []() | |
| Text: | Sent as ASCII-encoded Hex in the range 00 to ff |
| Example value: | To enable geofence alerts when the tracker leaves the geofenced area(s) |
| Binary example: | 0x3280 (B10000000 is 0x80) |
| Text example: | 80 |
| Default value: | 0x00 |

**Notes:**

The geofence alert is generated by the ZOE-M8Q. It will automatically wake the tracker so an alert message can be sent.
Battery consumption will increase when geofence alerts are enabled as the ZOE-M8Q will be powered continuously.
WAKEINT should be set to the same interval as ALARMINT if geofence alerts are enabled.

Power consumption will increase dramatically if the tracker is configured to monitor the Iridium ring channel. This is not recommended for battery-powered applications.
WAKEINT should be set to the same interval as ALARMINT and TXINT if the ring channel is being monitored.

---
DEST (0x33)
---

| []() | |
|---|---|
| Description: | The destination RockBLOCK serial number for message forwarding. |
| Binary: | uint32_t, 4 bytes, little endian. |
| Text: | Sent in the format nnnnn in the range 0 to 9999999 |
| Example value: | 12345 |
| Binary example: | 0x3339300000 (12345 is 0x00003039) |
| Text example: | 12345 |
| Default value: | 0 |

---
HIPRESS (0x34)
---

| []() | |
|---|---|
| Description: | The high atmospheric pressure alarm limit. |
| Binary: | uint16_t, 2 bytes, little endian, in mbar. |
| Text: | Sent in the format nnnn in the range 0 to 1084 (approx.) |
| Example value: | 998 |
| Binary example: | 0x34e603 (998 is 0x03e6) |
| Text example: | 998 |
| Default value: | 1084 |

---
LOPRESS (0x35)
---

| []() | |
|---|---|
| Description: | The low atmospheric pressure alarm limit. |
| Binary: | uint16_t, 2 bytes, little endian, in mbar. |
| Text: | Sent in the format nnnn in the range 0 to 1084 (approx.) |
| Example value: | 998 |
| Binary example: | 0x34e603 (998 is 0x03e6) |
| Text example: | 998 |
| Default value: | 0 |

---
HITEMP (0x36)
---

| []() | |
|---|---|
| Description: | The high atmospheric temperature alarm limit. |
| Binary: | int16_t (signed), 2 bytes, little endian, in Centigrade * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places, signed, in the range -40.0 to 85.0 (approx.) |
| Example value: | -12.34C |
| Binary example: | 0x362efb (-1234 is 0xfb2e) |
| Text example: | -12.34 |
| Default value: | 85 |

---
LOTEMP (0x37)
---

| []() | |
|---|---|
| Description: | The low atmospheric temperature alarm limit. |
| Binary: | int16_t (signed), 2 bytes, little endian, in Centigrade * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places, signed, in the range -40.0 to 85.0 (approx.) |
| Example value: | -12.34C |
| Binary example: | 0x372efb (-1234 is 0xfb2e) |
| Text example: | -12.34 |
| Default value: | -40 |

---
HIHUMID (0x38)
---

| []() | |
|---|---|
| Description: | The high atmospheric humidity alarm limit. |
| Binary: | uint16_t, 2 bytes, little endian, in %RH * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places in the range 0.0 to 100.0 (approx.) |
| Example value: | 12.34%RH |
| Binary example: | 0x38d204 (1234 is 0x04d2) |
| Text example: | 12.34 |
| Default value: | 100 |

---
LOHUMID (0x39)
---

| []() | |
|---|---|
| Description: | The low atmospheric humidity alarm limit. |
| Binary: | uint16_t, 2 bytes, little endian, in %RH * 10^-2. |
| Text: | Sent in the format n.n with 1 or 2 decimal places in the range 0.0 to 100.0 (approx.) |
| Example value: | 12.34%RH |
| Binary example: | 0x39d204 (1234 is 0x04d2) |
| Text example: | 12.34 |
| Default value: | 0 |

---
GEOFNUM (0x3a)
---

| []() | |
|---|---|
| Description: | The number of geofences in use (0-4) and the confidence level: 0 = no confidence, 1 = 68%, 2 = 95%, 3 = 99.7%, 4 = 99.99%. |
| Binary: | byte, the 4 most significant bits indicate the number of geofences in use, the 4 least significant bits indicate the confidence level. |
| Text: | Sent in the format n.c in the range 0.0 to 4.4 |
| Example value: | 1 geofence, 99.7% confidence (level 3) |
| Binary example: | 0x3a13 |
| Text example: | 1.3 |
| Default value: | 00 |

---
GEOF1LAT (0x3b)
---

| []() | |
|---|---|
| Description: | The latitude of the center of geofence circle 1. |
| Binary: | int32_t (signed), 4 bytes, little endian, in degrees * 10^-7. |
| Text: | Sent in the format -d.d with up to 7 decimal places in the range -90.0 to 90.0 _without preceding or trailing zeroes_ |
| Example value: | 40 degrees South |
| Binary example: | 0x3b007c28e8 (-400000000 is 0xe8287c00) |
| Text example: | -40.0 |
| Default value: | 0.0 |

---
GEOF1LON (0x3c)
---

| []() | |
|---|---|
| Description: | The longitude of the center of geofence circle 1. |
| Binary: | int32_t (signed), 4 bytes, little endian, in degrees * 10^-7. |
| Text: | Sent in the format -d.d with up to 7 decimal places in the range -180.0 to 179.9999999 _without preceding or trailing zeroes_ |
| Example value: | 170 degrees West |
| Binary example: | 0x3c000fac9a (-1700000000 is 0x9aac0f00) |
| Text example: | -170.0 |
| Default value: | 0.0 |

---
GEOF1RAD (0x3d)
---

| []() | |
|---|---|
| Description: | The longitude of the center of geofence circle 1. |
| Binary: | uint32_t, 4 bytes, little endian, in **cm**. |
| Text: | Sent as **m**, with cm resolution, in the format m.m with 1 or 2 decimal places _without preceding or trailing zeroes_ |
| Example value: | 100m |
| Binary example: | 0x3d10270000 (10000 is 0x00002710) |
| Text example: | 100.0 |
| Default value: | 0.0 |

---
GEOF2LAT (0x3e)
---

See GEOF1LAT

---
GEOF2LON (0x3f)
---

See GEOF1LON

---
GEOF2RAD (0x40)
---

See GEOF1RAD

---
GEOF3LAT (0x41)
---

See GEOF1LAT

---
GEOF3LON (0x42)
---

See GEOF1LON

---
GEOF3RAD (0x43)
---

See GEOF1RAD

---
GEOF4LAT (0x44)
---

See GEOF1LAT

---
GEOF4LON (0x45)
---

See GEOF1LON

---
GEOF4RAD (0x46)
---

See GEOF1RAD

---
WAKEINT (0x47)
---

| []() | |
|---|---|
| Description: | The tracker's wake-up interval (seconds). The tracker will wake up every WAKEINT seconds and check the PHT values. |
| Binary: | uint32_t, 4 bytes, little endian, in seconds. |
| Text: | Sent in the format nnnnn _without preceding zeroes_ |
| Example value: | 10 seconds |
| Binary example: | 0x470a000000 (10 is 0x0000000a) |
| Text example: | 10 |
| Default value: | 60 |

**Notes:**

The maximum value is 86400 seconds (= 24 hours)

---
ALARMINT (0x48)
---

| []() | |
|---|---|
| Description: | The tracker's alarm interval (minutes). The tracker will send a message every ALARMINT minutes while an alarm is present. |
| Binary: | uint16_t, 2 bytes, little endian, in minutes. |
| Text: | Sent in the format nnnn _without preceding zeroes_ |
| Example value: | 10 minutes |
| Binary example: | 0x480a00 (10 is 0x000a) |
| Text example: | 10 |
| Default value: | 5 |

**Notes:**

The maximum value is 1440 minutes (= 24 hours)

---
TXINT (0x49)
---

| []() | |
|---|---|
| Description: | The tracker's transmission interval (minutes). The tracker will send a routine message every TXINT minutes. |
| Binary: | uint16_t, 2 bytes, little endian, in minutes. |
| Text: | Sent in the format nnnn _without preceding zeroes_ |
| Example value: | 10 minutes |
| Binary example: | 0x490a00 (10 is 0x000a) |
| Text example: | 10 |
| Default value: | 5 |

**Notes:**

The maximum value is 1440 minutes (= 24 hours)

---
LOWBATT (0x4a)
---

| []() | |
|---|---|
| Description: | The low battery limit. |
| Binary: | uint16_t, 2 bytes, little endian, in Volts * 10^-2. |
| Text: | Sent in the format v.v with 1 or 2 decimal places in the range 0.0 to 9.99 |
| Example value: | 3.60V |
| Binary example: | 0x096801 (360 is 0x0168) |
| Text example: | 3.6 |
| Default value: | 3.2 |

---
DYNMODEL (0x4b)
---

| []() | |
|---|---|
| Description: | The GNSS dynamic platform model: |
| |	0 = PORTABLE : Applications with low acceleration, e.g. portable devices. Suitable for most situations. |
| |	2 = STATIONARY : Used in timing applications (antenna must be stationary) or other stationary applications. Velocity restricted to 0 m/s. Zero dynamics assumed. |
| | 3 = PEDESTRIAN : Applications with low acceleration and speed, e.g. how a pedestrian would move. Low acceleration assumed. |
| |	4 = AUTOMOTIVE : Used for applications with equivalent dynamics to those of a passenger car. Low vertical acceleration assumed. |
| |	5 = SEA : Recommended for applications at sea, with zero vertical velocity. Zero vertical velocity assumed. Sea level assumed. |
| |	6 = AIRBORNE1g : Airborne <1g acceleration. Used for applications with a higher dynamic range and greater vertical acceleration than a passenger car. No 2D position fixes supported. |
| |	7 = AIRBORNE2g : Airborne <2g acceleration. Recommended for typical airborne environments. No 2D position fixes supported. |
| |	8 = AIRBORNE4g: Airborne <4g acceleration. Only recommended for extremely dynamic environments. No 2D position fixes supported. |
| |	9 = WRIST : Not supported in protocol versions less than 18. Only recommended for wrist worn applications. Receiver will filter out arm motion. |
| |	10 = BIKE : Supported in protocol versions 19.2 |
| Binary: | byte. |
| Text: | Sent in the format n in the range 0 to 10 |
| Example value: | 3 |
| Binary example: | 0x4b03 (3 is 0x03) |
| Text example: | 3 |
| Default value: | 0 |

---
USERFUNC1 (0x58)
---

Upon receiving this field in an MT message, the tracker will execute user function 1 at the end of the SBD message cycle.

This could trigger a Qwiic-connected relay for example.

The field ID is followed by _zero_ data bytes.

---
USERFUNC2 (0x59)
---

See USERFUNC1

---
USERFUNC3 (0x5a)
---

See USERFUNC1

---
USERFUNC4 (0x5b)
---

See USERFUNC1

---
USERFUNC5 (0x5c)
---

Upon receiving this field in an MT message, the tracker will execute user function 5 at the end of the SBD message cycle.

The field ID is followed by _two_ data bytes (uint16_t, little endian). The uint16_t is passed as a parameter to the user function.

---
USERFUNC6 (0x5d)
---

See USERFUNC5

---
USERFUNC7 (0x5e)
---

Upon receiving this field in an MT message, the tracker will execute user function 7 at the end of the SBD message cycle.

The field ID is followed by _four_ data bytes (uint32_t, little endian). The uint32_t is passed as a parameter to the user function.

---
USERFUNC8 (0x5f)
---

See USERFUNC7
