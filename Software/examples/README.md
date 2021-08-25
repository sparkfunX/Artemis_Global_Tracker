# Artemis Global Tracker Examples

These examples demonstrate how to use the Artemis Global Tracker. There are examples showing: how to communicate with the MS8607 PHT sensor;
how to get a fix from the ZOE-M8Q GNSS; how to set up a geofence and how to wake up the Artemis when the geofence status changes;
how to send Iridium Short Burst Data (SBD) messages and monitor the ring channel for new messages. Example16 contains the code
for the full GlobalTracker.

If you don't want to use the Arduino IDE, you can upload the examples using the [Artemis Firmware Upload GUI](https://github.com/sparkfun/Artemis-Firmware-Upload-GUI) instead.
You will find the binary files in the [binaries folder](../../binaries).

**Example1 - Blink** powers up the tracker and blinks the white LED (connected to D19).

**Example2 - Bus Voltage** demonstrates how to read: the battery (bus) voltage; the internal VCC voltage; the Artemis internal temperature; the VSS (ground) voltage.

**Example3 - PHT** demonstrates how to read the pressure, humidity and temperature from the on-board MS8607 sensor.

**Example4 - External PHT** demonstrates how to read the pressure, humidity and temperature from an [external MS8607 sensor](https://www.sparkfun.com/products/16298) connected to the Qwiic port.

**Example5 - GNSS** demonstrates how to read the tracker's position from the ZOE-M8Q GNSS.

**Example6 - Geofence** demonstrates how to set four geofences around the tracker's current position. The white LED will go out if the tracker is moved outside the largest geofenced area.

**Example7 - Get IMEI** demonstrates how to read the IMEI identifier from the Iridium 9603N modem. You do not need message credits to run this example.

**Example8 - Check CSQ** demonstrates how to read the signal quality from the Iridium 9603N modem. You do not need message credits to run this example.

**Example9 - Get Time** demonstrates how to read the time from the Iridium network. You do not need message credits to run this example.

**Example10 - Basic Send** demonstrates how to send a simple "Hello, world!" message via Iridium. This example will use one message credit each time you run it.

**Example11 - Ring** demonstrates how to monitor the Iridium ring channel and download new messages sent to the tracker. This example will use one or more credits each time
a message is downloaded (depending on the length of the message).

**Example12 - Test Low Power** demonstrates how to put the Artemis into deep sleep and wake it up again every INTERVAL seconds.

**Example13 - Geofence Alert** demonstrates how to set a geofence around the tracker's current position. The Artemis will then go into deep sleep and will be woken up again by the ZOE-M8Q
if the geofence status changes. The ZOE is put into power save mode to help reduce the current draw (to approximately 9mA).

**Example14 - Simple Tracker** is a simple tracker which: wakes up every INTERVAL minutes; gets a GNSS fix; reads the PHT sensor; and sends all the data in a SBD message.
The message is transmitted as text and has the format:
- DateTime,Latitude,Longitude,Altitude,Speed,Course,PDOP,Satellites,Pressure,Temperature,Battery,Count

This example will use two message credits each time a message is sent due to the length of the message.

**Example15 - Better Tracker** is a better tracker where the INTERVAL, RBDESTINATION and RBSOURCE are stored in 'EEPROM' (Flash) and can be configured via Iridium SBD messages.

The message transmit interval can be configured by sending a plain text message to the tracker via the RockBLOCK Gateway using the format _[INTERVAL=nnn]_
where _nnn_ is the new message interval in _minutes_. The interval will be updated the next time the beacon wakes up for a transmit cycle.

If you want to enable message forwarding via the RockBLOCK Gateway, you can do this by including the text _[RBDESTINATION=nnnnn]_ in the message
where _nnnnn_ is the RockBLOCK serial number of the tracker or RockBLOCK you want the messages to be forwarded to. You can disable message forwarding
again by sending a message containing _[RBDESTINATION=0]_. You can change the source serial number which is included in the messages by including the text
_[RBSOURCE=nnnnn]_ in the RockBLOCK message where _nnnnn_ is the serial number of the RockBLOCK 9603N on the tracker. You can concatenate the configuration messages.
Send the following to set all three settings in one go: _[INTERVAL=5][RBDESTINATION=12345][RBSOURCE=54321]_

If message forwarding is enabled, the message format will be (using the above example):
- RB0012345,DateTime,Latitude,Longitude,Altitude,Speed,Course,PDOP,Satellites,Pressure,Temperature,Battery,Count,RB0054321

If message forwarding is enabled, you will be charged twice for each message: once to send it, and once to receive it.

**Example16 - Global Tracker** is the full Global Tracker which has many settings that can be configured and stored in EEPROM. The GlobalTracker can be configured to transmit:
on a GeoFence alert; or when Pressure, Temperature or Humidity limits are exceeded; or if the battery voltage is low. All of the settings can be configured via USB-C
or remotely via a binary format Iridium SBD message.

Messages can be sent in text format (human-readable) or binary format (to save messages credits). You can configure which message fields are included in the message so you only send the data you need.
The [Artemis Global Tracker Configuration Tool (AGTCT)](../../Tools) will generate the configuration messages for you.

You can trigger user-defined functions e.g. to operate an [external relay](https://www.sparkfun.com/products/15093).

You can also include readings from additional external sensors.

You can have the Iridium 9603N monitor the ring channel continuously for new Mobile Terminated messages so it can respond to them immediately,
but this will increase the current draw considerably. This is not recommended for battery-powered applications.

Please refer to the [GlobalTracker FAQs](../../Documentation/GlobalTracker_FAQs/README.md) for more information.

**Example17 - Production Test** is code used to test the Artemis Global Tracker during production. You will need an AGT Test Header to allow this code to run.

**Example18 - Production Test 2** is code used to test the Artemis Global Tracker during production. You will need an AGT Test Header to allow this code to run.

**Example19 - Serial Terminal** allows the AGT to be operated via a simple Serial Terminal interface. Open the Serial Monitor or a Terminal Emulator at 115200 baud to see the menu. You can:
- Read the temperature and pressure from the MS8607
- Read position, velocity and time data from the ZOE-M8Q GNSS
- Check for new Iridium messages
- Send and receive text and binary Iridium messages
- Flush the Iridium Mobile Terminated message queue

**Example20 - GNSS Module Info** reads the module information from the ZOE-M8Q GNSS, allowing you to see what software version it is running

**Example21 - Iridium Serial and Power Test** tests the work-around for v2.1 of the Apollo3 core (where the Iridium modem pulling the Serial1 RX pin low can cause the Artemis to hang)

<br/>

To run the examples, you will need to install **v2.1.0** of the SparkFun Apollo3 core and then set the board to the "**RedBoard Artemis ATP**":
- At the time of writing, v2.1.1 of the core contains a feature which makes communication with the u-blox GNSS problematic. Be sure to use **v2.1.0**
- https://learn.sparkfun.com/tutorials/artemis-development-with-the-arduino-ide
 
You will need to install [this version of the Iridium SBD library](https://github.com/sparkfun/SparkFun_IridiumSBD_I2C_Arduino_Library)
- (Available through the Arduino Library Manager: search for IridiumSBDi2c)

You will also need to install the [Qwiic_PHT_MS8607_Library](https://github.com/sparkfun/SparkFun_PHT_MS8607_Arduino_Library)
- (Available through the Arduino Library Manager: search for SparkFun MS8607)

You also need the [SparkFun u-blox GNSS library](https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library)
- (Available through the Arduino Library Manager: search for SparkFun u-blox GNSS)

Basic information on how to install an Arduino library is available [here](https://learn.sparkfun.com/tutorials/installing-an-arduino-library)
