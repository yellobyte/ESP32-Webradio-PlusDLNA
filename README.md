# ESP32 Webradio++

This ESP32 internet radio implementation is partially based on Ed Smallenburgs (ed@smallenburg.nl) original ESP32 Radio project, version 10.06.2018. His active & very popular web radio project is documented [here](https://github.com/Edzelf/ESP32-radio).  

It all started when my much loved 1991's FM Tuner device **TECHNICS Tuner ST-G570** became obsolete and useless (at least for me). I simply couldn't listen to my favourite FM radio stations anymore after having moved abroad. Since I didn't want to part with the old longtime pal, the decision was made to give it a second life: housing a modern internet radio with lots of additional features.  

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/ESP32-Radio%20Front2.jpg)

Now, after putting countless hours into the project, the device not only plays **internet radio streams** and audio files from **SD cards** but also **audio content from DLNA media servers** in the same LAN.  

The Arduino library **SoapESP32** has been created especially for the latter feature and enables any ESP32 based device to connect to DLNA media servers in the local network, browse their content and download selected files. It is basically a byproduct of this project and is now listed in the official Arduino library [collection](https://www.arduino.cc/reference/en/libraries/category/communication/).  

## :gift: Feature list:

Starting from Ed Smallenburg's code (Version 10.06.2018) this project has seen a lot of additions and modifications over time. Here a summary:

 * Integration of [SoapESP32](https://github.com/yellobyte/SoapESP32) library for DLNA media server access
 * Digital audio output added (TOSLINK optical) using a WM8805 module from Aliexpress
 * Usage of special VS1053 decoder board (with I2S output and without 3.5mm audio socket)<br />
   -> You find the Eagle schematic & board files [here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/tree/main/EagleFiles).
 * VS1053 gets patched with new firmware v2.7 at each reboot<br />
   -> Latest firmware patches for the VLSI VS1053 are available from [here](http://www.vlsi.fi/en/support/software/vs10xxpatches.html).
 * VU meter added to TFT display (needs above mentioned firmware patch)
 * Usage of W5500 Ethernet module instead of ESP32 builtin WiFi
 * Option to use a Debug Server on port 23 for debug output (in this case the cmd server is disabled due to Ethernet socket shortage)
 * Handling of the original front panel buttons & LEDs with the aid of an extender module (connected to ESP32 via I2C)
 * SD card indexing code rewritten in large parts
 * Ability to update ESP32 Radio code via SD card (using OTA functionality during reboot)
 * New rotary encoder fitted to available rotary knob, debouncing done in hardware
 * Track progress bar while playing audio files from SD card or media server
 * Minor modifications to get an unspecified Ali 21-button remote control working
 * MQTT functionality & battery stuff removed completely
 * Handling of more special chars in webradio streams (some radio channels seem to have different utf8 conversion tables)
 * Quicker handling of (very rare) firmware crashes resulting in shorter recovery/reboot time
 * Countless minor changes, adjustments, bugfixing
 * Regular modifications if required for building with new espressif32 framework releases   

If interested, have a look at [Revision history.txt](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/Revision%20history.txt) in the doc folder. 

## Some technical details:
The original display (rendered useless) has been removed and it's plexiglass cover now hides the infrared sensor VS1838B which receives signals from a remote control. A small 1.8" TFT color display (160x128 pixel, ST7735 driver IC, PCI bus) has been added to the front and is used for interacting with the user and presenting essential info. The necessary cutout in the front has been made with a small and affordable Proxxon mill/drill unit used by many hobbyists. A thin plexiglass is glued into the cutout. The TFT display sits right behind it, mounted on a frame made of PCB material to keep it in place. 

After milling/drilling the front was thoroughly cleaned to get a smooth surface and then the upper flat part of it was painted black twice with a bottle of color spray. It rested 3 days in the sun to dry completely before being reattached to the case. A silver permanent marker was used to label the buttons later on.  

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/Front%20-%20Just%20Painted%20All%20Black.jpg)

The original encoder attached to the rotary knob has been replaced with a modern one (Stec Rotary Encoder STEC11B03, 1 impulse per 2 clicks, turn + push). The knob is now used to browse through the list of pre-configured web radio stations, the content of SD cards or DLNA media servers storing thousands of audio files. Going up and down the directory levels and finally selecting an audio file for playing is done very fast with the knob in combination with the small 'return' button next to it.  

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/RotaryEncoder%2BLED-PCB%2BInfraredSensor.jpg)

Most of the original front buttons are still available and used for changing modes (Radio, SD card or DLNA), skipping tracks and selecting a repeat mode (None, Track, List or Random). Two adjacent buttons just below the TFT display went into the bin and made room for a SD card module (PCI bus with 4MHz clock speed only for stability reasons).   

A special extender board connects the mainboard with the original front PCB. This board makes it possible to control LEDs and buttons via the original control matrix. The existing 10 channel buttons (1...8,9,0) can continue to serve their original purpose, only this time with web radio stations assigned. The infrared sensor is attached to the extender board as well. 

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/Front%20PCB%20%2B%20Extender%20Board.jpg)

The extender board contains two 'Remote 8-Bit I/O Expander for I2C Bus ICs' PCF8574 and is connected to the mainboard via I2C bus. Its only task is to provide additional IO ports to the ESP32 on the mainboard. Jumper JP1 on the board is open as the interrupt signal INT from both ICs is unused. Every ~60ms the ICs get polled from the ESP32 instead.

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/EagleFiles/Extender-PCB/Schematic.JPG)

The original front panel 7x4 control matrix has all buttons connected between 7 rows and 4 columns as shown below. Quite a few single header pins had to be soldered onto the front panel PCB to get electrical access to the matrix. 

<p align="center"><img src="https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/ST-G570%20Key%20Matrix%20Original.JPG" width="500"/></p>

Rows 1...7 of the matrix (from top to bottom) are wired to connectors SV1 and SV2 on the extender board as follows:  
 * Row 1 (CP103/5) --> SV1/5 (PCF8574-1/P4, output)
 * Row 2 (CP103/4) is unused (buttons 'Preset up/down' have been removed)
 * Row 3 (CP102/8) --> SV2/2 (PCF8574-2/P1, output)
 * Row 4 (CP102/9) --> SV2/1 (PCF8574-2/P0, output)
 * Row 5 (CP103/6) --> SV1/6 (PCF8574-1/P5, output)
 * Row 6 (CP103/7) --> SV1/7 (PCF8574-1/P6, output)
 * Row 7 (CP103/8) --> SV1/8 (PCF8574-1/P7, output)

And colums 1...4 (from left to right) are wired to the extender board as follows:  
 * Column 1 (CP103/1) --> SV1/1 (PCF8574-1/P0, input, with 8.2k pull-up resistor to 3V3)
 * Column 2 (CP103/2) --> SV1/2 (PCF8574-1/P1, input, with 8.2k pull-up resistor to 3V3)
 * Column 3 (CP103/3) --> SV1/3 (PCF8574-1/P2, input, with 8.2k pull-up resistor to 3V3)
 * Column 4 (CP102/7) --> SV1/4 (PCF8574-1/P3, input, with 8.2k pull-up resistor to 3V3)

In order to detect pressed buttons the extender board successively pulls the rows LOW and checks it's column input lines for a HIGH to LOW transition because any button pressed will then subsequently pull its corresponding column to LOW as well.

The extender board additionally drives the 3 front panel LEDs via IC PCF8574-2/P5-P7 (connector pins SV2/6...8) by setting the lines to LOW. Those 3 lines plus 3V3 are wired directly to a separate LED PCB next to the rotary encoder.  

As for now, the device consists of several separate modules/PCBs. Three modules (ESP32, TOSLINK optical output, 10/100Mb Ethernet) were bought from various internet shops and the other four modules (power supply, mainboard, VS1053B decoder and front extender) were especially designed for this project with EAGLE PCB Design & Layout tool. The PCBs were ordered unassembled from various PCB prototype manufacturers in CN for little money. I did all the soldering myself, the decoder chip VS1053B in a tiny LQFP-48 package was a challenge though. All relevant EAGLE project files, schematics, board layouts etc. are available [here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/tree/main/EagleFiles).  

The analog audio output socket, the power input socket, the common mode choke and the many PCB distance holders were harvested from the original tuner PCBs and reused.  The mounting holes on the new PCBs were placed in a way to match the existing mounting holes at the bottom of the case.  

A necessary audio amplifier can be connected via the analog audio output or even the TOSLINK optical output for better sound quality. Well, going digital at the output is probably useless for web radio stations only, as their stream bit rates mostly range between 32...128kbps and very rarely top 192kbps.  

The special VS1053B decoder module (the red one in the middle) provides an I2S output which connects the decoder with the TOSLINK optical output module (the green one on the left). Since all available VS1053B decoder modules on Ali etc. lacked an I2S port I unavoidably had to build my own one. 

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/W5500Eth%2BWM8805Opt%2BVS1053Dec%20Boards.jpg)

If a digital audio output is not needed than any of the available cheap VS1053B decoder modules with 3.5mm jack socket (and without I2S socket) would do.  

The WM8805 I2S to TOSLINK module (China) was the cheapest module version I could get as it lacks the COAX output socket, the crystal oscillator, some Cs, Rs and the power input socket. However, all this is not needed in this project. Important was the ability to put the WM8805 into HW mode by shifting around some Rs on the module. 

The WM8805 can be operated in either software or hardware control mode. The mode is simply determined by sampling the state of the SDIN pin. If SDIN is LOW
during power up or hardware reset then the WM8805 switches into hardware control mode. In this mode most features will assume default values but some can be configured using external pins (SDOUT, SCLK, CSB, SWIFMODE). 

Having the Rs configured as shown below gives the following configuration of the chip: hardware control mode, slave mode, S/PDIF transmitter data source is I2S and the I2S interface expects 16-bit data.  

To make things work the decoder chip VS1053B is programmed to feed the WM8805 with 12.288MHz MCLK, 48kHz LRCK (MCLK divided by 256) and 16-bit audio data as configured.

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/I2S%20to%20OPT%20Board%20Schematic.jpg)

The controller board used on the mainboard is an inexpensive ESP32-T dev board equipped with an ESP32-Bit module (4MB Flash, 512kB RAM, no PSRAM, ultra small SMD WLAN antenna, I-PEX WLAN socket). Compared with other popular ESP32 dev boards the ESP32-T is slimmer and therefore even more breadboard compatible. I used it in the first flying test set and since it worked without issues I decided to keep it in the final project.

For my next similar project (this time an old Denon TU-550 tuner case from the early 90's) I will use the [YB-ESP32-S3-ETH dev board](https://github.com/yellobyte/ESP32-DevBoards-Getting-Started/tree/main/boards/YB-ESP32-S3-ETH(YelloByte)) as it combines ESP32 + Ethernet + Wifi + debug port on a single small dev board. Both versions -N4 and -N8R8 will do but the latter provides 8MB PSRAM which allows buffering audio streams if that was needed. This board will allow to make the main board much smaller as well.

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/Open%20Case%202.jpg)

During the early stages of the project a lot of software updates were required and all were done via USB cable between ESP32-module and PC. However, at some stage the device rejoined the HiFi rack and opening the case now and then for a quick software update became a real pain in the butt. Hence the possibility to perform an ESP32 firmware update simply via SD card was added.  

<p align="center"><img src="https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/Update%20per%20SD%20Card.JPG" height="250"/></p>  

Plenty of holes were drilled at the back of the case for better thermal management but they later proved unnecessary as the temps inside the case always stayed below 35 degs even with the lid on and all holes covered.  

If Ethernet is not wanted/possible than a small WLAN antenna attached with a short cable to the ESP32 WLAN-socket (I-PEX) would suffice. In that case I recommend to place the antenna directly behind the plexiglass cover for better receiption. Early tests with a WLAN router in 3m distance and fully closed case showed no issues with that arrangement.  

## Hardware troubleshooting:

In the early stages of the project the device was sometimes not able to establish an Ethernet connection after being switched on and it took me quite a while to figure out the problem. As it turned out, the W5500 Ethernet chip needs to be resetted substantially longer after power on than e.g the ESP32 chip.  Hence I modified the mainboard and replaced the original RC combination holding the reset line low with a Microchip MCP130T-315. This is a voltage supervisory IC designed to keep a microcontroller or any other chip in reset for a determined time (typically 350ms) after the system voltage has reached the desired level (here >= 3.15V) and stabilized. With that chip the connection issue vanished for good. 

Another troubling issue were the infrequent system resets after inserting a SD card into the SD card socket while the system was running. I figured it might be the MISO signal from the SD card module being directly attached to the ESP32. After adding driver gates IC2B (HC4050) and IC3D (74HC125) into the path between ESP32 and SD module this problem was gone as well. This solution disconnects the SD modules's MISO signal from the system and therefor prevents any interference from the SD card module when its assigned chip select signal SDCS is inactive (high). 

Since mechanical rotary encoders (independent of their build quality) need to be debounced I decided very early in the project to perform it fully in hardware by using RC circuits (10k/100n) in combination with Schmitt-Trigger ICs (74HC14).  As expected, debouncing issues never surfaced and no line of code had to be wasted on this topic.  

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/EagleFiles/Main-PCB/Schematic.JPG)  

As visible in the main board schematic above, the inputs of all unused 74HC gates are kept grounded and not floating in order to avoid possible gate oscillation as their inputs are all high impedance (in contrast to e.g. old 7400's). Furthermore I added plenty of capacitors near the ICs and close to each socket, connector etc. in order to keep the power supply rail as clean as possible.

## :zap: Software application notes:

All coding was done with **VSCode/PlatformIO-IDE**. To install needed libraries (Ethernet, SoapESP32, etc.) click the **PlatformIO sidebar icon**, open the **libraries** view and search for libs. Once found, select the newest release and click on **Add to Project**.

Always make sure you have one of the latest versions of **Arduino espressif32 framework** installed. Older versions might produce build errors. Building the project was successfully done with latest espressif32 frameworks V4.2.0, V5.0.0 and V6.0.0. Have a look at file [platformio.ini](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Software/platformio.ini) for required settings.

**Important:**  
Building with the official Arduino Ethernet library **and** with build options `ENABLE_CMDSERVER` or `PORT23_ACTIVE` will produce a build error like this:  
```
src\main.cpp:394:19: error: cannot declare variable 'cmdserver' to be of abstract type 'EthernetServer'
 EthernetServer    cmdserver(80);                         // Instance of embedded webserver, port 80
In file included from C:\users\tj\.platformio\packages\framework-arduinoespressif32\cores\esp32/Arduino.h:152:0,
                 from src\main.cpp:108:
C:\users\tj\.platformio\packages\framework-arduinoespressif32\cores\esp32/Server.h:28:18: note:         virtual void Server::begin(uint16_t)
     virtual void begin(uint16_t port=0) =0;
```
In this case file *Ethernet.h* in the Ethernet lib needs to be modified. Just change line  
```
class EthernetServer : public Server {
```
to
```
class EthernetServer : public Print {
```

## :tada: Implementation of SoapESP32 library for DLNA server access

The lib makes retrieving audio files from a DLNA media server a breeze: Function *browseServer()* is used to scan the server content and return a list of tracks (audio files). The device then selects an item from the list and sends a read request to the media server using *readStart()*. If granted, it will repeatedly *read()* a chunk of data into the queue which feeds the audio codec VS1053B until end of file. Finally the data connection to the server is closed with *readStop()* and the next item from the list is requested, provided the 'repeat file/folder' mode is active.  

The following sample picture sequence shows the actual implementation into this webradio project:

![github](https://github.com/yellobyte/SoapESP32/raw/main/doc/ESP32-Radio-DLNA.jpg)

Alternatively have a look at the short clip _ESP32-Radio-DLNA.mp4_ in folder **Doc** to see the final implementation in action. To watch now, click [Here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/ESP32-Radio-DLNA.mp4).

## :hammer_and_wrench: Still to be done:

 * Enable the Webinterface to browse DLNA media servers and select audio files for playing.

## :relaxed: Postscript:

Maybe this project can inspire one or two owners of old HiFi equipment to give their devices a second chance instead of tossing them into the scrap metal container. I spent a lot of time and efforts doing this project and wouldn't want to miss a second of it for I had plenty of fun and learned many things as well.  

If you have questions, miss some information or have suggestions, feel free to create an issue or contact me. However, my response time might be slow at times. 

