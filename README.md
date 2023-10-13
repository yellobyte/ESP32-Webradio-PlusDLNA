# ESP32 Webradio++

This ESP32 internet radio implementation is partially based on Ed Smallenburgs (ed@smallenburg.nl) original ESP32 Radio project, version 10.06.2018. His still active project is documented at https://github.com/Edzelf/ESP32-radio.

The device not only plays internet radio streams and audio files from SD cards but also **audio content from DLNA media servers** in the same LAN. The Arduino library [**SoapESP32**](https://github.com/yellobyte/SoapESP32) enables it to connect to DLNA media servers in the local network, browse their content and download selected files. The lib makes implementing this feature fairly easy: After selecting a track (audio file) from the list returned by *browseServer()*, the device sends a read request to the media server using *readStart()* and if granted, will repeatedly *read()* a chunk of data into the queue which feeds the audio codec VS1053B until end of file. Then the data connection to the server is closed with *readStop()* and the next audio file from the list is selected automatically, provided the 'repeat file/folder' mode is active.  

Using a rotary switch encoder is all it needs to browse through the list of pre-configured web radio stations, the content of the SD-Card or a DLNA media server. Going up and down the directory levels and finally selecting an audio file for playing is done very fast with it. And since the whole project now resides in a 43cm wide, pretty old but still stylish **TECHNICS Tuner ST-G570** case, the original rotary knob could luckily get utilized for exact this feature.

The original display (rendered useless) has been removed and it's cover now hides the infrared sensor for the remote control. A small TFT display has been added to the front and is used for interacting with the user and presenting some essential data.  

Most of the original front buttons are still usable, now for changing modes (Radio/SD/DLNA), skipping tracks etc. Two adjacent buttons just below the TFT display went into the bin and made room for a SD-Card reader. The existing 10 channel buttons (1...8,9,0) still serve their original purpose, only this time with web radio stations assigned.

Audio amplifiers can be connected via the original audio output socket or even TOSLINK optical cable for better sound quality. Well, going digital is probably not needed for web radio stations, as their stream bit rates usually top 128kbps and very rarely 192kbps.

![github](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/raw/main/Doc/ESP32-Radio%20Front2.jpg)

## :gift: Feature list ##

Starting from Ed Smallenburg's code (Version 10.06.2018) this **ESP32 Webradio++** project has seen a lot of additions and modifications over time. Here a summary:

 * integration of [SoapESP32](https://github.com/yellobyte/SoapESP32) library for DLNA media server access
 * Digital audio output added (TOSLINK optical) using a WM8805 module from Aliexpress
 * Usage of own VS1053 decoder board (with I2S output and w/o 3.5mm audio sockets)<br />
   -> You find the Eagle schematic & board files [here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/tree/main/EagleFiles).
 * VS1053 gets patched with new firmware v2.7 at each reboot<br />
   -> Latest firmware patches for the VLSI VS1053 are available from [here](http://www.vlsi.fi/en/support/software/vs10xxpatches.html).
 * VU meter added on TFT display (needs above mentioned firmware patch)
 * Usage of W5500 Ethernet module instead of ESP32 builtin WiFi
 * option to use a Debug Server on port 23 for debug output (in this case the cmd server is disabled due to Ethernet socket shortage)
 * Handling of original TECHNICS Tuner ST-G570 front panel buttons & LEDs via I2C and extender module
 * SD card indexing code rewritten in large parts
 * Ability to update ESP32 Radio code via SD card (using OTA functionality during reboot)
 * Encoder debouncing done completely in hardware (RC + Schmitt-Trigger IC), a Stec Rotary Encoder STEC11B03 with 1 impulse per 2 clicks is used
 * MP3 progress bar while playing audio
 * minor modification to get an unspecified Ali 21-button remote control working
 * MQTT functionality & battery stuff removed completely
 * handling of more special chars in webradio streams (some radio channels seem to have different utf8 conversion tables)
 * countless minor changes, some bugfixing
 * regular modifications if required for building with new espressif32 framework releases   

If interested, have a look at "Revision history.txt" in the doc folder. 

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

## Still to be done (just an idea):

 * Enable the Webinterface to browse DLNA media servers and select files

## :tada: Implementation of SoapESP32 library ##

The following sample picture sequence shows the actual implementation into this webradio project:

![github](https://github.com/yellobyte/SoapESP32/raw/main/doc/ESP32-Radio-DLNA.jpg)

Alternatively have a look at the short clip _ESP32-Radio-DLNA.mp4_ in folder **Doc** to see the final implementation in action. To watch now, click [Here](https://github.com/yellobyte/ESP32-Webradio-PlusDLNA/blob/main/Doc/ESP32-Radio-DLNA.mp4).

