# Artemis Global Tracker: Binaries

This folder contains the binary files for some of the AGT examples.

You can upload these to the AGT using the [Artemis Firmware Upload GUI](https://github.com/sparkfun/Artemis-Firmware-Upload-GUI) instead of the Arduino IDE.
Which is handy if you want to quickly update the firmware in the field, or are not familiar with the IDE.

To use:

* Download and extract the [AGT repo ZIP](https://github.com/sparkfun/Artemis_Global_Tracker/archive/main.zip)
* Download and extract the [AFU repo ZIP](https://github.com/sparkfun/Artemis-Firmware-Upload-GUI/archive/master.zip)
* Run the AFU artemis_firmware_uploader_gui executable for your platform
  * **/Windows** contains the Windows .exe
  * **/OSX** contains an executable for macOS X
  * **/Linux** contains an executable built on Ubuntu
  * **/Raspberry_Pi__Debian** contains an executable for Raspberry Pi 4 (Debian Buster)
* Select the AGT firmware file you'd like to upload (should end in *.bin*)
  * **Example1 - Blink** powers up the tracker and blinks the white LED (connected to D19).
  * **Example14 - Simple Tracker** is a simple tracker which: wakes up every INTERVAL minutes; gets a GNSS fix; reads the PHT sensor; and sends all the data in a SBD message.
  * **Example15 - Better Tracker** is a better tracker where the INTERVAL, RBDESTINATION and RBSOURCE are stored in 'EEPROM' (Flash) and can be configured via Iridium SBD messages.
  * **Example16 - Global Tracker** is the full Global Tracker. All of the settings can be configured via USB-C or remotely via an Iridium SBD message using the [Artemis Global Tracker Configuration Tool (AGTCT)](../Tools).
  * **Example19 - Serial Terminal** allows the AGT to be operated via a simple Serial Terminal interface. Open the Serial Monitor or a Terminal Emulator at 115200 baud to see the menu.
* Attach the AGT target board using USB
* Select the COM port (hit Refresh to refresh the list of USB devices)
* Press Upload

The GUI does take a few seconds to load and run. _**Don't Panic**_ if the GUI does not start right away.
