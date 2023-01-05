# LoRa APRS iGate

![Build check and build](https://github.com/lora-aprs/LoRa_APRS_iGate/workflows/Build%20check%20and%20build/badge.svg)

The LoRa APRS iGate will work with very cheap hardware which you can buy from amazon, ebay or aliexpress.
Try it out and be part of the APRS network.

![TTGO LoRa32](pics/iGate.png)

## Blog posts and Youtube videos from other Hams
* [ON4MOD - LoRa APRS : le guide pratique](https://on5vl.org/lora-aprs-le-guide-pratique/) (blog post - french) 06.12.2022
* [Manuel Lausmann - iGate & Tracker](https://www.youtube.com/watch?v=-KdFQEaNC1k) (youtube - german) 04.06.2021
* [Manuel Lausmann - Installationsanleitung als PDF](http://mala-one.de/Lora-APRS/) (PDF - german) 04.06.2021
* [Thierry F4EWI - LoRa et LoRa APRS](http://www.f5kmy.fr/spip.php?article509) (blog post - french) 06.12.2020
* [OE1ROT](https://www.aronaut.at/2020/11/lora-aprs-gateway-mit-esp32-boards/) (blog post - german) 14.11.2020
* [DL7AG](http://online.dl7ag.de/lora-aprs-dl7ag-10/) (blog post - german) 08.11.2020
* [Manuel Lausmann - iGate](https://www.youtube.com/watch?v=C7hfVe32pXs) (youtube - german - OLD) 06.11.2020
* [Manuel Lausmann - Tracker](https://www.youtube.com/watch?v=clIlTEFbWLk) (youtube - german - OLD) 02.11.2020
* [OE1ROT](https://www.aronaut.at/2019/12/lora-aprs-tracker-mit-ttgo-t-beam-433mhz/) (blog post - german) 09.12.2019

Feel free to add yours or create a ticket if you want to be added.

## Supported boards

You can use one of the Lora32 boards without changes:

* Heltec WiFi LoRa 32 V1 (433MHz SX1278)
* Heltec WiFi LoRa 32 V2 (433MHz SX1278)
* TTGO LoRa32 V1 (433MHz SX1278)
* TTGO LoRa32 V2 (433MHz SX1278)
* TTGO LoRa32 V2.1 (433MHz SX1278)
* TTGO T-Beam V0.7 (433MHz SX1278)
* TTGO T-Beam V1 (433MHz SX1278)
* Tracker D from [OE1ACM and OE1CGC](https://www.lora-aprs.at/)
* and sure many more...

Here are some amazon-de links for some example boards:
* [LoRa32 V1](https://www.amazon.de/dp/B07VPHYYJD)
* [LoRa32 V1](https://www.amazon.de/dp/B07QRG89ZV)
* [LoRa32 V2](https://www.amazon.de/dp/B07VL97VNH)
* [LoRa32 V2.1](https://www.amazon.de/dp/B07RXSKPBX)
* [T-Beam V1.0](https://www.amazon.de/dp/B07RT9FKPL)

This boards cost around 20 Euros, they are very cheap and perfect for an LoRa iGate.
Keep in mind: you need a 433MHz version!

## Compiling and configuration

**There is a german [quick start](https://www.lora-aprs.info/docs/LoRa_APRS_iGate/quick-start-guide/) page! Take a look ;)**

**There is a french [quick start](https://on5vl.org/lora-aprs-le-guide-pratique/) page! Take a look ;)**


### How to compile

The best success is to use PlatformIO (and it is the only platform where I can support you). 

* Go to [PlatformIO](https://platformio.org/) download and install the IDE.
* If installed open the IDE, go to the left side and click on 'extensions' then search for 'PlatformIO' and install.
* When installed click 'the ant head' on the left and choose import the project on the right.
* Just open the folder and you can compile the Firmware.

### Configuration

* The settings to change for your configuration are located in **data/is-cfg.json**. [Here](./doc/is-cfg_json.md) is a detailed list of the parameters and their meaning.
* To upload it to your board you have to do this via **Upload File System image** in PlatformIO!
* To find the 'Upload File System image' click the PlatformIO symbol (the little alien) on the left side, choose your configuration, click on 'Platform' and search for 'Upload File System image'.

## Branches in this repository and version system

This firmware is developed in a rolling release system: everyday a new release could be created. But there are still rules where new pull requests has to go and and how the version system looks like.

### Version system

If the *develop* branch is stable enough for a new release it will be merged with a pull request to the *master* branch and a new version will be generated.

The versions are based on this settings:
* major: the current year (2 digits)
* minor: the current week of the year
* patch: if there is a important fix for an release, just increment the number, otherwise 0

*example*: a new release will be created on the 11/14/2020, this version numbers will be used: 
* major: 20
* minor: 46
* patch: 0

so the version will be: 20.46.0

## Internal Web Server

The internal webserver, if enabled in the configuration, allows you to update the module remotely and check the state of the various services.

### Updating the igate via the web server

The "Firmware Update" section of the web server allows you update the firmware and the SPIFFS partition of the module.

To update the firmware, click on "Upload Firmware", select the `firmware.bin` file that PlatformIO generated (in the project folder, under `./.pio/build/lora_board/firmware.bin`). Then click "Upload".

To update the SPIFFS filesystem, click on "Upload SPIFFS", select the `spiffs.bin` file that PlatformIO generated (in the project folder, under `./.pio/build/lora_board/spiffs.bin`). Then click "Upload".

**Warning** As of now, trying to upload both files at the same time WILL NOT work. When updating the device, upload the SPIFFS file before the firmware file.

## Packet logger

A packet logger can be enabled in the configuration. It will store the latest packets received along with some more data (RSSI, SNR and frequency error of the reception). If a packet is recevived with incorrect data (an incorrect CRC check), a log line will be added with the RSSI, SNR and frequency_error of that transmission.

## Future plans

* [x] show time until next beaconing
* [x] show login issues from IS server
* [ ] add better OLED library to support multiple different OLEDs
* [x] add support to turn OLED on, off and dimming
* [ ] add support for temperature chips (BMExxx)
* [x] add FTP server support to upload configuration
* [x] add web server for configuration and other things
* [x] add statistics for received packages
* [ ] show received packages on a map
* [ ] etc.

## LoRa Tracker

Look at my other project: a [LoRa Tracker](https://github.com/peterus/LoRa_APRS_Tracker)

## Hints

### Here are some peculiarities of the different boards

* TTGO T-Beam V1.0 and V1.1 and SSD1306 OLED display

When adding an SSD1306 0,96" OLED display direct to the board you have to be careful, there are two different pinout
versions on the market. 
For direct mount you need a display with this Pinout -> [VCC - GND - SCL - SDA](pics/display-right.jpg).
A direct mount of the [other display](pics/display-wrong.jpg) is not possible without damage the display!
The 'wrong' display works too but you have to change VCC and GND by wire !

The [LoRa APRS WiKi Displays](https://github.com/lora-aprs/LoRa_APRS_Tracker/wiki/Displays) page has more details.

Feel free to add hints!


