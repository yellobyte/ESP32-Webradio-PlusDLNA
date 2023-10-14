# ESP32 Webradio++

**Remark:** This ESP32 internet radio implementation is partially based on Ed Smallenburgs (ed@smallenburg.nl) original ESP32 Radio project, version 10.06.2018. His still active & very popular project is documented at [ESP32-radio](https://github.com/Edzelf/ESP32-radio).  

My much loved 1991's FM Tuner **TECHNICS Tuner ST-G570** became obsolete and useless (at least for me) when I couldn't listen to my favourite FM radio stations anymore after moving abroad. Since I didn't want to part with the device, the decision was made to give it a second life: housing an internet radio with lots of additional features.  

Now, after putting countless hours into the project, the device not only plays **internet radio streams** and audio files from **SD cards** but also **audio content from DLNA media servers** in the same LAN. The Arduino library **SoapESP32** has been created especially for the latter feature and enables any ESP32 based device to connect to DLNA media servers in the local network, browse their content and download selected files. It is basically a byproduct of this project and meanwhile became part of the Arduino library [collection](https://www.arduino.cc/reference/en/libraries/category/communication/).  

The original encoder attached to the rotary knob of the Technics case has been replaced with a modern one (turn + push) and now the knob is used to browse through the list of pre-configured web radio stations, the content of SD-Cards or DLNA media servers storing thousands of audio files. Going up and down the directory levels and finally selecting an audio file for playing is done very fast with it.

The original display (rendered useless) has been removed and it's plexiglass cover now hides the infrared sensor for the remote control. A small 1.8" TFT color display (Aliexpress) has been added to the front and is used for interacting with the user and presenting essential info.  

Most of the original front buttons are still available, now for changing modes (Radio/SD/DLNA), skipping tracks, returning to the higher directory level and repeat mode selection. Two adjacent buttons just below the TFT display went into the bin and made room for a SD-Card reader. The existing 10 channel buttons (1...8,9,0) still serve their original purpose, only this time with web radio stations assigned. A special extender board sits between mainboard (connected via I2C) and original front PCB (connectors CP102, Pins 7-9 & CP103 Pins 1-8). Its many additional IO ports makes it possible to control LEDs and buttons via original control [matrix](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/ST-G570%20Key%20Matrix%20Original.JPG)).

A needed audio amplifier can be connected via the original audio output socket or even TOSLINK optical cable for better sound quality. Well, going digital at the output is probably not needed for web radio stations, as their stream bit rates usually range between 32...128kbps and very rarely top 192kbps.

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/ESP32-Radio%20Front2.jpg)

As for now, the device consists of several separate modules/PCBs. Three modules (ESP32, TOSLINK optical output, 10/100Mb Ethernet) were bought from Aliexpress and the other four modules (power supply, mainboard, VS1053B decoder, front extender) were especially designed with EAGLE Layout program. The PCBs were ordered unassembled from JLCPCB resp. PCBWay. All relevant EAGLE project files are available [here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/tree/main/EagleFiles).  

The original stereo audio output socket and the power input socket were harvested from the old original FM tuner PCBs and re-used.  The VS1053B encoder module provides an I2S output which is necessary to forward the digital audio data from the decoder to the TOSLINK optical output module. 

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/Open%20Case%202.jpg)

During the early stages of the project a lot of software updates were required and all were done via USB cable between ESP32-module and PC. However, at some stage the device joined the HiFi rack again and re-opening the case now and then for a quick software update became a real pain in the butt. Hence the possibility to perform an ESP32 firmware update via SD-Card was added.

Plenty of holes were drilled at the back of the case for better thermal management but they later proved unneeded as the temps inside the case always stayed below 35 degs even with the lid on and all holes covered.  

If Ethernet is not wanted/possible than a small WLAN antenna attached with a short cable to the ESP32 WLAN-socket (I-PEX) would suffice. In that case I recommend to place the antenna directly behind the plexiglass cover for better receiption. Early tests with a WLAN router in 3m distance and fully closed case showed no issues with that arrangement.

## :gift: Feature list

Starting from Ed Smallenburg's code (Version 10.06.2018) this **ESP32 Webradio++** project has seen a lot of additions and modifications over time. Here a summary:

 * Integration of [SoapESP32](https://github.com/yellobyte/SoapESP32) library for DLNA media server access
 * Digital audio output added (TOSLINK optical) using a WM8805 module from Aliexpress
 * Usage of special VS1053 decoder board (with I2S output and without 3.5mm audio socket)<br />
   -> You find the Eagle schematic & board files [here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/tree/main/EagleFiles).
 * VS1053 gets patched with new firmware v2.7 at each reboot<br />
   -> Latest firmware patches for the VLSI VS1053 are available from [here](http://www.vlsi.fi/en/support/software/vs10xxpatches.html).
 * VU meter added on TFT display (needs above mentioned firmware patch)
 * Usage of W5500 Ethernet module instead of ESP32 builtin WiFi
 * Option to use a Debug Server on port 23 for debug output (in this case the cmd server is disabled due to Ethernet socket shortage)
 * Handling of original TECHNICS Tuner ST-G570 front panel buttons & LEDs via I2C and extender module
 * SD card indexing code rewritten in large parts
 * Ability to update ESP32 Radio code via SD card (using OTA functionality during reboot)
 * Encoder debouncing done completely in hardware (RC + Schmitt-Trigger IC), a Stec Rotary Encoder STEC11B03 with 1 impulse per 2 clicks is used
 * MP3 progress bar while playing audio
 * Minor modifications to get an unspecified Ali 21-button remote control working
 * MQTT functionality & battery stuff removed completely
 * Handling of more special chars in webradio streams (some radio channels seem to have different utf8 conversion tables)
 * Quicker handling of firmware crashes (shorter recovery/reboot time)
 * Countless minor changes, some bugfixing
 * Regular modifications if required for building with new espressif32 framework releases   

If interested, have a look at [Revision history.txt](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/Revision%20history.txt) in the doc folder. 

## :zap: Application notes

All Coding was done with **VSCode/PlatformIO-IDE**. To install needed libraries (Ethernet, SoapESP32, etc.) click the **PlatformIO sidebar icon**, open the **libraries** view and search for libs. Once found, select the newest release and click on **Add to Project**.

Always make sure you have one of the latest versions of **Arduino espressif32 framework** installed. Older versions might produce build errors. Building the project was successfully done with latest espressif32 frameworks V4.2.0 and V5.0.0. Have a look at file [platformio.ini](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Software/platformio.ini) for required settings.

**Important:**  
If you a) use a Wiznet W5500 Ethernet card/shield **and** b) build with options `ENABLE_CMDSERVER` or `PORT23_ACTIVE` you will get a build error like this:  
```
src\main.cpp:394:19: error: cannot declare variable 'cmdserver' to be of abstract type 'EthernetServer'
 EthernetServer    cmdserver(80);                         // Instance of embedded webserver, port 80
In file included from C:\users\tj\.platformio\packages\framework-arduinoespressif32\cores\esp32/Arduino.h:152:0,
                 from src\main.cpp:108:
C:\users\tj\.platformio\packages\framework-arduinoespressif32\cores\esp32/Server.h:28:18: note:         virtual void Server::begin(uint16_t)
     virtual void begin(uint16_t port=0) =0;
```
Only in this case you need to modify by hand two files in the Ethernet lib:  
- In _Ethernet.h_ add one line as shown below:  
```
class EthernetServer : public Server {
private:
	uint16_t _port;
public:
	EthernetServer(uint16_t port) : _port(port) { }
	EthernetClient available();
	EthernetClient accept();
	virtual void begin();
	// added for ESP32-Webradio++
	virtual void begin(uint16_t port);
  ...
```
- In _EthernetServer.cpp_ add the following function definition:
```
void EthernetServer::begin(uint16_t port)
{
	_port = port;
	begin();
}
```

## :tada: Implementation of SoapESP32 library for DLNA server access

The lib makes playing audio files from media servers fairly easy: Function *browseServer()* is used to scan the server content and return a list of tracks (audio files). The device then selects an item and sends a read request to the media server using *readStart()*. If granted, it will repeatedly *read()* a chunk of data into the queue which feeds the audio codec VS1053B until end of file. Finally the data connection to the server is closed with *readStop()* and the next audio file from the list is requested, provided the 'repeat file/folder' mode is active.  

The following sample picture sequence shows the actual implementation into this webradio project:

![github](https://github.com/yellobyte/SoapESP32/raw/main/doc/ESP32-Radio-DLNA.jpg)

Alternatively have a look at the short clip _ESP32-Radio-DLNA.mp4_ in folder **Doc** to see the final implementation in action. To watch now, click [Here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/ESP32-Radio-DLNA.mp4).

## :hammer_and_wrench: Still to be done:

 * Enable the Webinterface to browse DLNA media servers and select audio files for playing.

## :relaxed: Postscript:

If you have questions or suggestions, feel free to contact me. However, my response times might be slow. 

