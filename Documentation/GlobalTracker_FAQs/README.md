# Artemis Global Tracker: GlobalTracker FAQs

These FAQs assume you are running the [full Global Tracker (Example16)](../../Software/examples/Example16_GlobalTracker)

## Contents:
- [Do I need the extra 10F supercapacitors?](#Do-I-need-the-extra-10F-supercapacitors)
- [What voltage does the AGT need?](#What-voltage-does-the-AGT-need)
- [How much current does the AGT draw?](#How-much-current-does-the-AGT-draw)
- [Which antenna should I use?](#Which-antenna-should-I-use)
- [Can I use an active antenna?](#Can-I-use-an-active-antenna)
- [How do I configure the messages sent by the tracker?](#How-do-I-configure-the-messages-sent-by-the-tracker)
- [How do I send binary messages?](#How-do-I-send-binary-messages)
- [How do I enable RockBLOCK message forwarding?](#How-do-I-enable-RockBLOCK-message-forwarding)
- [How do I enable Pressure, Humidity and Temperature alarms?](#How-do-I-enable-Pressure-Humidity-and-Temperature-alarms)
- [How do I enable Geofence alarms?](#How-do-I-enable-Geofence-alarms)
- [How do I monitor the ring channel continuously for new MT messages?](#How-do-I-monitor-the-ring-channel-continuously-for-new-MT-messages)
- [How do I define and trigger a user function?](#How-do-I-define-and-trigger-a-user-function)
- [How do I send a user value?](#How-do-I-send-a-user-value)
- [How can I see the location of my trackers on a map?](#How-can-I-see-the-location-of-my-trackers-on-a-map)

## Do I need the extra 10F supercapacitors?

The AGT comes with 1F supercapacitors installed. You only need to add the additional 10F capacitors on the bottom of the board if you decide to
change the supercapacitor charge current from 150mA to 60mA.

Background:

The LTC3225 supercapacitor charger charge current is adjustable up to a maximum of 150mA. When set to the full 150mA, the LTC3225 can match the 145mA average current
drawn by the 9603N during transmit. This means that 1 Farad supercapacitors are adequate as they only need to hold enough charge to meet the
9603N's 1.3A peak current draw during the very brief (8.3ms) transmit bursts.

If you want to power the Global Tracker from a low current source, e.g. solar panels, the charge current can be reduced to 60mA by
changing the **Charge Current** jumper link. The 60mA charge current is enough to offset the 9603N's 39mA average current draw during receive,
but bigger supercapacitors are needed to deliver the average current draw during a complete receive/transmit cycle.
So, if you do change the charge current to 60mA, you will also need to solder additional 10 Farad supercapacitors on to
the rear of the PCB using the solder pads provided.

## What voltage does the AGT need?

Please refer to the [Power Options](../Hardware_Overview#power-options) in the Hardware Overview.

**Important Note:** We recommend that you power the AGT using the USB, Lipo battery and/or "Solar" power connectors.
If you choose to power the AGT via the VBUS breakout pin, there will be no reverse-polarity protection and _bad things could happen_
when connecting another power source.

Via the VBUS breakout pin:
- Maximum: 6.0V
- Recommended Minimum: 3.4V
  - The AP2112K 3.3V regulator has a dropout voltage of 100mV at 250mA at 25°C
- Actual Minimum: 3.2V (Approx.)
  - The LTC3225 supercapacitor charger requires a minimum of 2.8V and is powered by the AP2112K
  - The Artemis and ZOE GNSS have lower minimum voltages

Via the USB, Lipo battery or "Solar" (external power) connectors:
- Maximum: 6.0V
- Minimum: add approximately 350mV to the above figures
  - The PMEG4005 reverse-protection diode has a forward voltage of approx. 350mV at 300mA at 25°C

## How much current does the AGT draw?

In deep sleep, with the Power LED disconnected, the AGT draws a current of approximately 220µA.

The maximum current draw is dictated by the LTC3225 supercapacitor charge current:
- The LTC3225 has an efficiency of approximately 80% at 3.3V V<sub>IN</sub> at 100mA charge current
- Using the maximum charge current of 150mA, the LTC3225 will draw approximately 200mA
- The typical total current draw during supercapacitor charging is 250mA

## Which antenna should I use?

We recommend [this one](https://www.sparkfun.com/products/16838). It is optimized for Iridium and GNSS (GPS and GLONASS).

## Can I use an active antenna?

No. Two reasons:
- The [antenna switch circuit](../Hardware_Overview#antenna-switch) includes a DC-blocking capacitor
- The Iridium transmit would almost certainly damage an active preamplifier

## How do I configure the messages sent by the tracker?

The messages sent by the Global Tracker contain one or more **MOFIELDS** (Mobile Originated message FIELDS).

By default, Example16_GlobalTracker sends a _text_ message, every 5 minutes, containing the following MOFIELDS:
- **DATETIME** : the GNSS date and time in _YYYYMMDDHHMMSS_ format
- **LAT** : the GNSS latitude in degrees
- **LON** : the GNSS longitude in degrees
- **ALT** : the GNSS altitude above mean sea level in meters

For a full definition of the message fields, please see the [message format documentation](../Message_Format).

Let's say you also want to include the **SPEED** (ground speed) and **HEAD** (heading) MOFIELDS:

You will find the Configuration Tool in the [Tools folder](../../Tools/Artemis_Global_Tracker_Configuration_Tool).
If you are familiar with Python and are able to install PyQt5 and all the other required modules, you can run _AGTCT.py_.
If you are less familiar with Python, you will find a 64-bit Windows executable (.exe) version in [this folder](../../Tools/Artemis_Global_Tracker_Configuration_Tool/Windows_64-bit).

When you run the configuration tool, the [PyQt5 user interface](#AGTCT1) will appear.
If you are using the 64-bit Windows executable (.exe) version: don't panic! It takes a few seconds for the user interface to appear.

It just so happens that **DATETIME** + **LAT** + **LON** + **ALT** + **SPEED** + **HEAD** are the MOFIELDS expected by the [Mapper](../../Tools/Artemis_Global_Tracker_Mapping_Tools)
and there is a pickle (.pkl) file containing those settings that you can load:
- Click on the _Browse_ button
- Click on _mapper.pkl_ and then click _Open_
- Click on the _Load Config_ button

The user interface should now look like [this](#AGTCT2). You will notice that:
- The **DATETIME**, **LAT**, **LON**, **ALT**, **SPEED** and **HEAD** MOFIELDS checkboxes are ticked. This tells the tracker to send (only) these MOFIELDS.
- The **MOFIELDS** _Include_ checkbox is ticked. This must be ticked otherwise the MOFIELDS will not be included in the configuration message. You will get a warning message if you forget.
- None of the _FLAGS1_ checkboxes are ticked. This ensures the message is sent in _text_ format, not _binary_.
- The **FLAGS1** _Include_ checkbox is ticked. This must be ticked otherwise FLAGS1 will not be included in the configuration message.
- The **TXINT** _Include_ checkbox is ticked and the value box contains _15_. This will change the message transmit interval to 15 minutes. The default is 5 minutes.

Now click on the _Calculate Config_ button and you should see the [configuration message](#AGTCT3)
_02310030c00f00000000000000000000490f00038d30_ in the _Configuration Message_ window.

You now have a choice to make. You can either update the tracker settings [via a USB cable](#Updating-the-configuration-via-USB),
or remotely [via an Iridium message](#Updating-the-configuration-via-Iridium).

If you want to restore the default settings, you can repeat the tracker configuration using the _default.pkl_ pickle file.

Loading the _empty.pkl_ is a quick way to clear all of the checkboxes and values.

### Updating the configuration via USB
- Connect the tracker to your computer using a standard USB-C cable. If you are using Windows, you should hear the usual bing-bong connection sound.
- Click on the _Refresh_ button and select the tracker's COM port from the drop-down box.
- Click on _Open Port_ and you should see a [welcome message from the tracker](#AGTCT4) in the _Serial Monitor_ window.
- Click on _Upload Config_ and you should see [these messages in the _Serial Monitor_](#AGTCT5):

```
*** Tracker Configuration ***
Waiting for data...
Data received! Checking if it is valid...
Data is valid! Parsing it...
Parsing complete. Updating values in EEPROM.
Done!
```

You can now disconnect the tracker and close the configuration tool. The tracker will use the new settings from now on.

### Updating the configuration via Iridium
- Right-click in the _Configuration Message_ window and click on _Select All_.
- Right-click in the _Configuration Message_ window again and click on _Copy_.
- Log in to [Rock7 Operations](https://rockblock.rock7.com/Operations).
- Click on the _Send a Message_ tab.
- Tick the RockBLOCK serial number of the tracker you want to update.
- Select _Hex_ from the Mode drop-down.
- Right-click in the _Hex String_ window and click _Paste_.
- Click _Send Message_ to send the configuration message.

![AGTCT6](../../img/AGTCT6.PNG)

The tracker settings will be updated the next time the tracker transmits.

## How do I send binary messages?

Sending messages in _binary_ format is a good way to save message credits. The message length will be approximately half compared to text, and you are charged one message
credit for every 50 bytes sent (or part thereof). The only downside is that binary messages are not human-readable.

- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **FLAGS1** _Send message in binary format_ checkbox.
- Tick the **FLAGS1** _Include_ checkbox.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT7). Now update the tracker settings
as shown above.

If you want to change the MOFIELDS, you can do that in the [same message](#AGTCT8).

## How do I enable RockBLOCK message forwarding?

The Rock7 RockBLOCK gateway has a powerful feature that allows you to automatically forward messages from one RockBLOCK device to another.
See [this link](https://docs.rockblock.rock7.com/docs/messaging-between-rockblocks) for further details.

E.g.
- if the RockBLOCK serial number of your tracker is 54321
- and you want to forward messages to RockBLOCK 12345

Do the following:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **FLAGS1** _Forward messages to DEST_ checkbox.
- Tick the **FLAGS1** _Include_ checkbox.
- Tick the **DEST** _Include_ checkbox.
- Enter _12345_ in the DEST value box.
- Tick the **SOURCE** _Include_ checkbox.
- Enter _54321_ in the SOURCE value box.
- Tick the MOFIELDS checkboxes for the message fields you want to send, including the **SOURCE** MOFIELD so the destination RockBLOCK knows which tracker sent the message
- Tick the **MOFIELDS** _Include_ checkbox.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT9). Now update the tracker settings
as shown above.

Notes:
- Including the **SOURCE** MOFIELD is optional, but is useful as it lets the destination RockBLOCK know which tracker sent the message.
- The tracker's SOURCE serial number can only be changed via USB, to avoid it being changed accidentally. Remember to set this before you deploy your tracker.
- You will be charged twice for each message: once when the source sends it and again when the destination receives it.

## How do I enable Pressure, Humidity and Temperature alarms?

The tracker can be configured to wake up every **WAKEINT** _seconds_. When it wakes, it will check the pressure, humidity and temperature readings from the on-board MS8607 PHT sensor.
If pressure, temperature or humidity alarms are enabled, a message will be sent every **ALARMINT** _minutes_ if any of the limits are exceeded.

E.g.
- If you want to use the tracker to monitor the temperature and humidity in your greenhouse once per minute;
- you want to send alarm messages every 15 minutes if the temperature goes above 40C;
- or if the humidity falls below 30% RH.
- If things are normal, you want to send a message once per day.

You would do the following:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **FLAGS1** _Enable HITEMP alarm messages_ and _Enable LOHUMID_ checkboxes.
- Tick the **FLAGS1** _Include_ checkbox.
- Tick the MOFIELDS checkboxes for the message fields you want to send, e.g. **PRESS**, **TEMP**, **HUMID** and **DATETIME**
- Tick the **MOFIELDS** _Include_ checkbox.
- Tick the **WAKEINT** _Include_ checkbox.
- Enter _60_ in the WAKEINT value box. This sets the wake interval to 60 seconds.
- Tick the **ALARMINT** _Include_ checkbox.
- Enter _15_ in the ALARMINT value box. This sets the alarm transmit interval to 15 minutes.
- Tick the **TXINT** _Include_ checkbox.
- Enter _1440_ in the TXINT value box. This sets the normal transmit interval to 1440 minutes (every 24 hours).
- Tick the **HITEMP** _Include_ checkbox.
- Enter _40.0_ in the HITEMP value box. This sets the high temperature alarm limit to 40.0C.
- Tick the **LOHUMID** _Include_ checkbox.
- Enter _30.0_ in the LOHUMID value box. This sets the low humidity alarm limit to 30.0% RH.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT10). Now update the tracker settings
as shown above.

## How do I enable Geofence alarms?

The tracker can be configured to leave the ZOE-M8Q GNSS powered on continuously so it can generate geofence alarms.

**Note: enabling geofence alerts will substantially increase the current draw and shorten the battery life as the ZOE will be powered continuously.**

E.g.
- If you want to set a geofence around latitude 55.0 degrees north, longitude 1 degree west, with a radius of 100m.
- You want to use a position confidence level of 95%.
- You want alarm messages to be generated when the tracker leaves the geofenced area.
- You want to send alarm messages every 5 minutes.
- If things are normal, you want to send a message once per day.

You would do the following:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **FLAGS2** _Enable GeoFence alarm messages_ checkbox.
- Tick the **FLAGS2** _Include_ checkbox.
- Tick the MOFIELDS checkboxes for the message fields you want to send, e.g. **LAT**, **LON**, **SPEED** and **HEAD**
- Tick the **MOFIELDS** _Include_ checkbox.
- Tick the **WAKEINT** _Include_ checkbox.
- Enter _300_ in the WAKEINT value box. This sets the wake interval to 300 seconds (5 minutes).
- Tick the **ALARMINT** _Include_ checkbox.
- Enter _5_ in the ALARMINT value box. This sets the alarm transmit interval to 5 minutes.
- Tick the **TXINT** _Include_ checkbox.
- Enter _1440_ in the TXINT value box. This sets the normal transmit interval to 1440 minutes (every 24 hours).
- Tick the **GEOFNUM** _Include_ checkbox.
- Enter _12_ in the GEOFNUM value box. This sets the number of geofences to 1 and the confidence level to 2 (95%). See the [GEOFNUM definition](../Message_Format#geofnum-0x3a) for more details.
- Tick the **GEOF1LAT** _Include_ checkbox.
- Enter _55.0_ in the GEOF1LAT value box. This sets the geofence latitude to 55 degrees north.
- Tick the **GEOF1LON** _Include_ checkbox.
- Enter _-1.0_ in the GEOF1LON value box. This sets the geofence longitude to 1 degree west (western longitudes are negative).
- Tick the **GEOF1RAD** _Include_ checkbox.
- Enter _100.0_ in the GEOF1RAD value box. This sets the geofence radius to 100m.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT11). Now update the tracker settings
as shown above.

Notes:
- Enabling geofence alerts will substantially increase the current draw and shorten battery life as the ZOE will be powered continuously.
- WAKEINT and ALARMINT need to be set to the same value.
- If you want the alarm messages to be generated when entering a geofenced area, tick the **FLAGS2** _Alarm when inside the GeoFence_ checkbox.

## How do I monitor the ring channel continuously for new MT messages?

The tracker can be configured to leave the Iridium 9603N powered on continuously so it can monitor the ring channel and respond to new Mobile Terminated messages immediately.

**Note: monitoring the ring channel will substantially increase the current draw as the Iridium 9603N will be powered continuously.**
**This is not recommended for battery-powered applications.**

E.g.
- You want to monitor the ring channel continuously so you can respond quickly to new MT messages.
- If things are normal, you want to send a message once per day.

You would do the following:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **FLAGS2** _Monitor Ring Channel continuously_ checkbox.
- Tick the **FLAGS2** _Include_ checkbox.
- Tick the **WAKEINT** _Include_ checkbox.
- Enter _86400_ in the WAKEINT value box. This sets the wake interval to 86400 seconds (24 hours).
- Tick the **ALARMINT** _Include_ checkbox.
- Enter _1440_ in the ALARMINT value box. This sets the alarm transmit interval to 1440 minutes (every 24 hours).
- Tick the **TXINT** _Include_ checkbox.
- Enter _1440_ in the TXINT value box. This sets the normal transmit interval to 1440 minutes (every 24 hours).
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT12). Now update the tracker settings
as shown above.

Notes:
- Monitoring the ring channel will substantially increase the current draw as the Iridium 9603N will be powered continuously. This is not recommended for battery-powered applications.
- WAKEINT should be set to the same value as ALARMINT and TXINT.
- Iridium ring alerts are only sent if the network knows your approximate location. You need to send at least one message per day in order to receive ring alerts.

## How do I define and trigger a user function?

The tracker supports eight different user functions. Each function is triggered when the corresponding field ID is received via a Mobile Terminated message.

The user functions are defined in [Tracker_User_Functions.ino](../../Software/examples/Example16_GlobalTracker/Tracker_User_Functions.ino).

E.g.
- You have a [Qwiic Relay](https://www.sparkfun.com/products/15093) connected to the tracker via the Qwiic connector.
- You want the relay to switch on when USERFUNC1 (field ID 0x58) is received.
- You want the relay to switch off again when USERFUNC2 (field ID 0x59) is received.

You will need to install the _SparkFun Qwiic Relay Arduino Library_ first, through the Arduino Library Manager. Search for _Qwiic Relay_.

Example code for the Qwiic (Single) Relay is available [here](https://github.com/sparkfun/SparkFun_Qwiic_Relay_Arduino_Library/blob/master/Examples/Example1_Single_Relay_Basics/Example1_Single_Relay_Basics.ino).

We need to add these lines to the start of Tracker_User_Functions.ino:

```
#include "SparkFun_Qwiic_Relay.h"
#define RELAY_ADDR 0x18 // Alternate address 0x19
Qwiic_Relay relay(RELAY_ADDR);
TwoWire myWire(4); //Will use Artemis pads 39/40
```

Then we need to add these lines to USER_FUNC_1:

```
  myWire.begin(); // Begin I2C communication to the relay
  if(relay.begin(myWire)) // Try to begin the relay
  {
    relay.turnRelayOn(); // If relay.begin was successful, turn the relay on
  }
  myWire.end(); // End I2C communication
```

And we need to add these lines to USER_FUNC_2:

```
  myWire.begin(); // Begin I2C communication to the relay
  if(relay.begin(myWire)) // Try to begin the relay
  {
    relay.turnRelayOff(); // If relay.begin was successful, turn the relay off
  }
  myWire.end(); // End I2C communication
```

The start of Tracker_User_Functions.ino should now look like this:

```
// Artemis Global Tracker: User Functions

// Add your own code to these functions to return USERVAL's or execute USERFUNC's

#include "SparkFun_Qwiic_Relay.h"
#define RELAY_ADDR 0x18 // Alternate address 0x19
Qwiic_Relay relay(RELAY_ADDR);
TwoWire myWire(4); //Will use Artemis pads 39/40

void USER_FUNC_1()
{
  debugPrintln("Executing USERFUNC1...");
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC1" (0x58) message field

  myWire.begin(); // Begin I2C communication to the relay
  if(relay.begin(myWire)) // Try to begin the relay
  {
    relay.turnRelayOn(); // If relay.begin was successful, turn the relay on
  }
  myWire.end(); // End I2C communication
}

void USER_FUNC_2()
{
  debugPrintln("Executing USERFUNC2...");
  // Add your own code here - it will be executed when the Tracker receives a "USERFUNC2" (0x59) message field

  myWire.begin(); // Begin I2C communication to the relay
  if(relay.begin(myWire)) // Try to begin the relay
  {
    relay.turnRelayOff(); // If relay.begin was successful, turn the relay off
  }
  myWire.end(); // End I2C communication
}
```

To trigger each user function, you need to send a configuration message to the tracker containing the appropriate field ID. E.g. to trigger USERFUNC1 you would do the following:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the **USERFUNCs** _Execute USERFUNC1_ checkbox.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT13).
Now send the configuration message _0258035db9_ to the tracker through Rock7 Operations as shown above.

The configuration message for USERFUNC2 is _0259035ebb_

Notes:
- The Qwiic Relay will draw extra current while it is energised. This will shorten your battery life.
- USERFUNCs 5 and 6 allow a 16-bit uint16_t to be passed to the user function.
- USERFUNCs 7 and 8 allow a 32-bit uint32_t to be passed to the user function.

## How do I send a user value?

[Tracker_User_Functions.ino](../../Software/examples/Example16_GlobalTracker/Tracker_User_Functions.ino#L53-L114) also contains
eight functions which can be used to include user-defined data in the tracker messages.
- USER_VAL_1 and USER_VAL_2 allow _byte_ values to be included.
- USER_VAL_3 and USER_VAL_4 allow _uint16_t_ values to be included.
- USER_VAL_5 and USER_VAL_6 allow _uint32_t_ values to be included.
- USER_VAL_7 and USER_VAL_8 allow _float_ values to be included.

Let's say you have connected a sensor to the tracker which returns a _float_ reading. You would include the code to read your sensor inside USER_VAL7() or USER_VAL_8(); you would load
your sensor reading into _retVal_.

To include USERVAL7 in the messages from the tracker, you would:
- Open the configuration tool as shown above.
- Make sure all of the checkboxes and values are clear. Loading _empty.pkl_ is a quick way to do this.
- Tick the MOFIELDS checkboxes for any standard message fields you want to send, e.g. **DATETIME**.
- Tick the **USERVAL7** MOFIELDS checkbox.
- Tick the **MOFIELDS** _Include_ checkbox.
- Click on _Calculate Config_.

The configuration tool should look like [this](#AGTCT14). Now update the tracker settings
as shown above.

## How can I see the location of my trackers on a map?

The [Tools Folder](../../Tools#artemis_global_tracker_mapping_tools) contains a set of mapping tools which will display
the location and routes of up to eight trackers on a Google Maps Static image in a PyQt5 user interface. The tools have all been tried and tested on both Linux and Windows.
The tools are straight forward to use, but you do need to be familiar with Python3.


## AGTCT Screenshots

### AGTCT1
![AGTCT1](../../img/AGTCT1.PNG)

[BACK](#How-do-I-configure-the-messages-sent-by-the-tracker)

### AGTCT2
![AGTCT2](../../img/AGTCT2.PNG)

[BACK](#How-do-I-configure-the-messages-sent-by-the-tracker)

### AGTCT3
![AGTCT3](../../img/AGTCT3.PNG)

[BACK](#How-do-I-configure-the-messages-sent-by-the-tracker)

### AGTCT4
![AGTCT4](../../img/AGTCT4.PNG)

[BACK](#Updating-the-configuration-via-USB)

### AGTCT5
![AGTCT5](../../img/AGTCT5.PNG)

[BACK](#Updating-the-configuration-via-USB)

### AGTCT6
![AGTCT6](../../img/AGTCT6.PNG)

[BACK](#Updating-the-configuration-via-Iridium)

### AGTCT7
![AGTCT7](../../img/AGTCT7.PNG)

[BACK](#How-do-I-send-binary-messages)

### AGTCT8
![AGTCT8](../../img/AGTCT8.PNG)

[BACK](#How-do-I-send-binary-messages)

### AGTCT9
![AGTCT9](../../img/AGTCT9.PNG)

[BACK](#How-do-I-enable-RockBLOCK-message-forwarding)

### AGTCT10
![AGTCT10](../../img/AGTCT10.PNG)

[BACK](#How-do-I-enable-Pressure-Humidity-and-Temperature-alarms)

### AGTCT11
![AGTCT11](../../img/AGTCT11.PNG)

[BACK](#How-do-I-enable-Geofence-alarms)

### AGTCT12
![AGTCT12](../../img/AGTCT12.PNG)

[BACK](#How-do-I-monitor-the-ring-channel-continuously-for-new-MT-messages)

### AGTCT13
![AGTCT13](../../img/AGTCT13.PNG)

[BACK](#How-do-I-define-and-trigger-a-user-function)

### AGTCT14
![AGTCT14](../../img/AGTCT14.PNG)

[BACK](#How-do-I-send-a-user-value)
