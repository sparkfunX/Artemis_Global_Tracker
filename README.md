# Artemis Global Tracker

An open source global satellite tracker utilising the [SparkFun Artemis module](https://www.sparkfun.com/products/15484),
[Iridium 9603N satellite transceiver](https://www.iridium.com/products/iridium-9603/) and [u-blox ZOE-M8Q GNSS](https://www.u-blox.com/en/product/zoe-m8-series).

![16469-Artemis_Global_Tracker-02.jpg](img/16469-Artemis_Global_Tracker-02.jpg)

![16469-Artemis_Global_Tracker-04.jpg](img/16469-Artemis_Global_Tracker-04.jpg)

## Repository Contents

- **/Documentation** - Documentation for the hardware and the full GlobalTracker example
- **/Hardware** - Eagle PCB, SCH and LBR design files
- **/Software** - Arduino examples
- **/Tools** - Tools to help: configure the full GlobalTracker example via USB or remotely via Iridium messaging; and track and map its position
- **LICENSE.md** contains the licence information

## Documentation

- [Hardware overview](Documentation/Hardware_Overview/README.md): an overview of the hardware
- The Artemis pin allocation is summarised [here](Documentation/Hardware_Overview/ARTEMIS_PINS.md)
- [Message Format](Documentation/Message_Format/README.md): a definition of the message format and fields (both binary and text) for the full GlobalTracker example
- [GlobalTracker FAQs](Documentation/GlobalTracker_FAQs/README.md): information to help you configure the full GlobalTracker example
