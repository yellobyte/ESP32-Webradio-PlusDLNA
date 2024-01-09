//*******************************************************************************************************
//*  ESP32 Webradio++                                                                                   *
//*                                                                                                     *
//*  For ESP32, 1.8 color display, VS1053 decoder module, Ethernet module and Optical digital audio     *
//*  output in an original Technics ST-G570 Radio Tuner case.                                           *
//*                                                                                                     *
//*  Based on Ed Smallenburgs (ed@smallenburg.nl) original ESP32 Radio project, version 10.06.2018.     * 
//*  His popular project is documented at https://github.com/Edzelf/ESP32-radio.                        *
//*                                                                                                     *
//*  Starting from Ed's code (Version 10.06.2018) this project has seen a lot of additions and          *
//*  modifications over time. Here a summary:                                                           *
//*   - DLNA/SOAP functionality added (utilizing SoapESP32 library)                                     *
//*   - Digital audio output added (TOSLINK optical) using a WM8805 module (China)                      *
//*   - Usage of own VS1053 decoder board (similar to China board but with I2S output and w/o sockets)  *
//*   - VS1053 gets patched with new firmware v2.7 at each reboot                                       *
//*   - VU meter added on TFT display (needs above mentioned firmware patch)                            *                           
//*   - Debug Server on port 23 for debug output (when compiled in than the cmd server is omitted)      *
//*   - Handling of original TECHNICS ST-G570 front panel buttons & LEDs via I2C and extender module    *
//*   - SD card indexing code rewritten in large parts                                                  *
//*   - Usage of W5500 Ethernet card instead of builtin WiFi                                            *
//*   - Ability to update ESP32 Radio code via SD card (using OTA functionality during reboot)          *
//*   - Encoder debouncing done completely in hardware (RC + Schmitt-Trigger IC), we use a Stec Rotary  *
//*     Encoder STEC11B03 with 1 impulse per 2 clicks                                                   *
//*   - MP3 progress bar while playing audio                                                            *
//*   - MQTT functionality & battery stuff removed completely                                           *
//*   - handling of more special chars in webradio streams (each channels seems to have a different     *
//*     utf8 conversion table or philosophy)                                                            *
//*   - countless minor changes & improvements, bugfixing                                               *
//*   - regular modifications for new espressif32 frameworks releases                                   *
//*  Have a look at "Revision history.txt" in the doc folder.                                           *
//*                                                                                                     *
//*  July 2022, TJ                                                                                      *
//*                                                                                                     *
//*******************************************************************************************************
//
// The standard Arduino Ethernet Library V2.0.0 causes errors when compiled for ESP32-Radio.
// Two files Ethernet.h and EthernetServer.cpp had to be modified.
// In EthernetServer.cpp, a new function has to be defined:
//    void EthernetServer::begin(uint16_t port)
//    {
//      _port = port;
//      begin();
//    }
//
// In Ethernet.h (Section EthernetServer), above function is to be declared:
//    virtual void begin(uint16_t port);
//
// In Ethernet.h: #define MAX_SOCK_NUM 4 & #define ETHERNET_LARGE_BUFFERS for better connection stability.
//
// SPI speed for the Ethernet Card is default 14MHz in W5100.h (Arduino Ethernet Library V2.0.0).
//
// Class 2 SD cards support at least 2MB/s (16Mbit/s) speed. Only 4MHz SPI clock speed is used here
// for stability reasons (long wires to SD-module).
//
// The VSPI interface is used for VS1053, TFT and SD. The I2C Arduino "Wire" Library was initially used for the
// extender card but proofed to be very unstable. Using low level I2C functions instead works a treat.
//
// Wiring. 
// Most pins (except 18,19,23 of the SPI interface) can be configured in the config page of the web interface.
// ESP32dev Signal  Wired to LCD    Wired to VS1053  SDCARD  Ethernet   Wired to the rest
// -------- ------  -------------   ---------------  ------  ---------  ------
// GPIO16           -               XDCS             -       -          -
// GPIO5            -               XCS              -       -          -
// GPIO4            -               DREQ             -       -          -
// GPIO2            D/C, RS or A0   -                -       -          -
// GPIO17           -               -                CS      -          -
// GPIO18   SCK     CLK or SCK      SCK              SCK     SCLK       -
// GPIO19   MISO    -               MISO             MISO    MISO       -
// GPIO23   MOSI    DIN or SDA      MOSI             MOSI    MOSI       -
// GPIO15   -       CS              -                -       -          -
// GPIO22   -       -               -                -       SCS        -
// GPI03    RXD0    -               -                -       -          Reserved
// GPIO1    TXD0    -               -                -       -          Reserved
// GPIO34   -       -               -                -       -          Reserved
// GPIO35   -       -               -                -       -          Infrared receiver VS1838B
// GPIO25   -       -               -                -       -          Rotary encoder CLK
// GPIO26   -       -               -                -       -          Rotary encoder DT
// GPIO27   -       -               -                -       -          Rotary encoder SW
// GPIO32   SDA     -               -                -       -          I2C-SDA Extenderboard
// GPIO33   SCL     -               -                -       -          I2C-SCL Extenderboard
// -------  ------  -------------   ---------------  ------  ---------  ----------------
// GND      -       GND             GND              GND     GND        Power supply GND
// VCC 5 V  -       BL              -                +5      5V         Power supply
// VCC 5 V  -       VCC             5V               -       -          Power supply
// EN       -       RST             XRST             -       RST        -
// 3.3 V    -       -               -                -       -          Rotary Encoder, 74HC4050, 74HC125
//
//

// Define the version number, also used for webserver as Last-Modified header:
#define VERSION "15 July 2022 13:55"
//
// Defined in platform.ini as it affects soapESP32 too !
//#define USE_ETHERNET                   // Use Ethernet/LAN instead of WiFi builtin
// 

#define ENABLE_CMDSERVER               // Enable port 80 web server (Control Website)
#define ENABLE_SOAP                    // Enable SOAP/DLNA access to media server
//#define ENABLE_INFRARED                // Enable remote control functionality per IR
//#define ENABLE_DIGITAL_INPUTS          // If we want to use digital inputs to start actions/commands
//#define PORT23_ACTIVE                  // Configure Server on port 23 for sending debug messages
//#define CHECK_LOOP_TIME                // Checking loop round time for debugging purposes
#define ENABLE_I2S                     // Activates I2S on VS1053B for communication with WM8805 module (I2S/OPT)
#define LOAD_VS1053_PATCH              // Loads a patch into the VS1053 which fixes the SS_REFERENCE_SEL bug
#define VU_METER                       // Displays VU-Meter levels provided by VS1053B
#define SD_UPDATES                     // SW-Updates via SD-Card during power-up
#define ENABLE_ESP32_HW_WDT            // Enable ESP32 Hardware Watchdog

#include <Arduino.h>
//#include <FS.h>
//#include <SPI.h>
#include <SD.h>
#include <nvs.h>
#include <stdio.h>
#include <string.h>
#ifdef SD_UPDATES
#include <Update.h>
#define UPDATE_FILE_NAME "/firmware.bin"
#define TEST_FILE_NAME "/testfile.txt"
#endif
#ifndef USE_ETHERNET
#include <WiFiMulti.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#else
#include <WiFi.h>
#include <vector>
#endif
#include <time.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#ifdef ENABLE_ESP32_HW_WDT
#include <esp_task_wdt.h>
#endif
#include <esp_partition.h>
#include "driver/i2c.h"

#ifdef USE_ETHERNET
#include <Ethernet.h>
#include <EthernetUdp.h>
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
#define _claimSPI(p)  claimSPI(p)    // mp3client.read() sometimes returns garbage without it
#define _releaseSPI() releaseSPI()
#else
#define _claimSPI(p)
#define _releaseSPI()
#endif
#ifdef ENABLE_SOAP
#include "SoapESP32.h"
#endif

// comment out if ESP_EARLY_LOGx not needed
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
//#include "esp_log.h"
//DRAM_ATTR static const char* TAG = "ESP32Radio";

// ESP32 hardware watchdog timeout
#define WDT_TIMEOUT 60
// Number of entries in the queue
#define QSIZ 1000
// Debug buffer size
#define DEBUG_BUFFER_SIZE 250
// Access point name if connection to WiFi network fails.  Also the hostname for WiFi and OTA.
// Not that the password of an AP must be at least as long as 8 characters.
// Also used for other naming.
#define NAME "ESP32Radio"
// Adjust size of buffer to the longest expected string for nvsgetstr
#define NVSBUFSIZE 150
// Position (column) of time in topline relative to end
#define TIMEPOS -30
// SPI speed for SD card, (8 MHz still working fine on breadboard)
#define SDSPEED 4000000
// Size of metaline buffer
#define METASIZ 1024
// Max. number of NVS keys in table
#define MAXKEYS 200
// Max. node depth on SD card we will recognize
#define SD_MAXDEPTH 4
// Max mp3-files to recognize on SD card (we skip all the others)
#define SD_MAXFILES 500
//length of longest debug command string plus two spaces for CR + LF (from client on port 23)
#define MAXSIZE_TELNET_CMD 10 
// defaults, can be overridden by preferences
IPAddress local_IP(192, 168, 1, 99);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
IPAddress secondaryDNS(195, 186, 4, 162);     //optional
#define NTP_SERVER_DEFAULT "ptbtime1.ptb.de"  // time-a.timefreq.bldrdoc.gov, pool.ntp.org, etc.....
#ifdef USE_ETHERNET
#define NTP_TZ_STRING_DEFAULT "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"
#endif
// Position (column) in topline relative to end
#define MUTEPOS     -42
#define SDSTATUSPOS -49
#if defined VU_METER && defined LOAD_VS1053_PATCH
#define VUMETERPOS  -61
#endif
// I2C stuff: PCF8574 Addresses & assigned Pins
#define PCF8574_1_ADDR (0x20)
#define PCF8574_2_ADDR (0x21)
#define I2C_SDA_PIN  32
#define I2C_SCL_PIN  33
#ifdef ENABLE_SOAP
// Default SOAP/DLNA media server settings
// parameters for Twonky media server on QNAP TS-253D
#define MEDIASERVER_DEFAULT_MAC "24:5E:BE:5D:2F:66"
#define MEDIASERVER_DEFAULT_IP 192,168,1,11
#define MEDIASERVER_DEFAULT_PORT 9000
#define MEDIASERVER_DEFAULT_CONTROL_URL "dev0/srv1/control"
#endif
//
//
//**************************************************************************************************
// Forward declaration and prototypes of various functions.                                        *
//**************************************************************************************************
void        displayTime(const char* str, uint16_t color = 0xFFFF);
void        showStreamTitle(const char* ml, bool full = false);
void        handlebyte_ch(uint8_t b);
void        handleFSf(const String& pagename);
void        handleCmd();
char*       dbgprint(const char* format, ...);
const char* analyzeCmd(const char* str);
const char* analyzeCmd(const char* par, const char* val);
void        chomp(String &str);
String      limitString(String& strg, int maxPixel);
String      httpHeader(String contentstype);
bool        nvsSearch(const char* key);
void        mp3loop();
//void        tftlog(const char *str, uint16_t textColor = (WHITE));
void        playTask(void * parameter);       // Task to play the stream
void        spfTask(void * parameter);        // Task for special functions
void        getTime();
void        extenderTask(void * parameter);   // Task for communication with PCF8574 ICs
void        vumeterTask(void * parameter);    // Task for showing vu meter readings
bool        handlePCF8574();
#ifdef USE_ETHERNET
void        tzset(void);
int         setenv(const char *name, const char *value, int overwrite);
#endif

//**************************************************************************************************
// Several structs.                                                                                *
//**************************************************************************************************
//

struct scrseg_struct                                 // For screen segments
{
  bool     update_req;                               // Request update of screen
  uint16_t color;                                    // Textcolor
  uint16_t y;                                        // Begin of segment row
  uint16_t height;                                   // Height of segment
  String   str;                                      // String to be displayed (Name)
};

enum qdata_type { QDATA, QSTARTSONG, QSTOPSONG };    // datatyp in qdata_struct
struct qdata_struct
{
  int datatyp;                                       // Identifier
  __attribute__((aligned(4))) uint8_t buf[32];       // Buffer for chunk
};

struct ini_struct
{
  uint8_t        reqvol;                             // Requested volume
  uint8_t        rtone[4];                           // Requested bass/treble settings
  int8_t         newpreset;                          // Requested preset
#ifdef USE_ETHERNET
  String         clk_tzstring;                       // Posix timezone string
#else
  int8_t         clk_offset;                         // Offset in hours with respect to UTC
  int8_t         clk_dst;                            // Number of hours shift during DST
#endif
  String         clk_server;                         // Server to be used for time of day clock
#ifdef ENABLE_SOAP
  IPAddress      srv_ip;                             // media server ip
  uint16_t       srv_port;                           // media server port
  String         srv_controlUrl;                     // media server control URL
  String         srv_macWOL;                         // mac needed to send WOL packets to media server
#endif
#ifdef ENABLE_INFRARED
  int8_t         ir_pin;                             // GPIO connected to output of IR decoder
#endif  
  int8_t         enc_clk_pin;                        // GPIO connected to CLK of rotary encoder
  int8_t         enc_dt_pin;                         // GPIO connected to DT of rotary encoder
  int8_t         enc_sw_pin;                         // GPIO connected to SW of rotary encoder
  int8_t         tft_cs_pin;                         // GPIO connected to CS of TFT screen
  int8_t         tft_dc_pin;                         // GPIO connected to D/C or A0 of TFT screen
  int8_t         sd_cs_pin;                          // GPIO connected to CS of SD card
  int8_t         vs_cs_pin;                          // GPIO connected to CS of VS1053
  int8_t         vs_dcs_pin;                         // GPIO connected to DCS of VS1053
  int8_t         vs_dreq_pin;                        // GPIO connected to DREQ of VS1053
  int8_t         vs_shutdown_pin;                    // GPIO to shut down the amplifier
  int8_t         spi_sck_pin;                        // GPIO connected to SPI SCK pin
  int8_t         spi_miso_pin;                       // GPIO connected to SPI MISO pin
  int8_t         spi_mosi_pin;                       // GPIO connected to SPI MOSI pin
};

#ifndef USE_ETHERNET
struct WifiInfo_t                                     // For list with WiFi info
{
  uint8_t inx;                                       // Index as in "wifi_00"
  char * ssid;                                       // SSID for an entry
  char * passphrase;                                 // Passphrase for an entry
};
#endif

struct nvs_entry
{
  uint8_t  Ns;                                       // Namespace ID
  uint8_t  Type;                                     // Type of value
  uint8_t  Span;                                     // Number of entries used for this item
  uint8_t  Rvs;                                      // Reserved, should be 0xFF
  uint32_t CRC;                                      // CRC
  char     Key[16];                                  // Key in Ascii
  uint64_t Data;                                     // Data in entry
};

struct nvs_page                                       // For nvs entries
{ // 1 page is 4096 bytes
  uint32_t  State;
  uint32_t  Seqnr;
  uint32_t  Unused[5];
  uint32_t  CRC;
  uint8_t   Bitmap[32];
  nvs_entry Entry[126];
};

struct keyname_t                                      // For keys in NVS
{
  char Key[16];                                       // Mac length is 15 plus delimeter
};

struct mp3node_t                                      // for directories/files on SD card
{
  bool    isDirectory;                                // node is file or directory
  int16_t parentDir;                                  // which directory is it in
  String  name;                                       // file/directory name
};

#ifdef ENABLE_SOAP
struct soapChain_t
{
  String id;
  String name;
  size_t size;
  size_t startingIndex;
};
#endif

//**************************************************************************************************
// Global data section.                                                                            *
//**************************************************************************************************
// There is a block ini-data that contains some configuration.  Configuration data is              *
// saved in the preferences by the webinterface.  On restart the new data will                     *
// de read from these preferences.                                                                 *
// Items in ini_block can be changed by commands from webserver or Serial.                         *
//**************************************************************************************************

enum enum_datamode { INIT = 1, HEADER = 2, DATA = 4,     // State for datastream
                     METADATA = 8, PLAYLISTINIT = 16,
                     PLAYLISTHEADER = 32, PLAYLISTDATA = 64,
                     STOPREQD = 128, STOPPED = 256, CONNECTERROR = 512 };
enum enum_enc_menu { IDLING, SELECT };                   // State for rotary encoder menu
enum enum_selection { NONE, STATION, SDCARD, MEDIASERVER }; // play mode or selected source
enum enum_repeat_mode { NOREPEAT, SONG, DIRECTORY, RANDOM }; // repeat mode

// Global variables
int               DEBUG = 1;                             // Debug on/off
int               numSsid;                               // Number of available WiFi networks
ini_struct        ini_block;                             // Holds configurable data
#ifndef USE_ETHERNET
WiFiMulti         wifiMulti;                             // Possible WiFi networks
#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
WiFiServer        cmdserver(80);                         // Instance of embedded webserver, port 80
WiFiClient        cmdclient;                             // An instance of the client for commands
#endif
#ifdef PORT23_ACTIVE
WiFiServer        dbgserver(23);                         // Instance of server, telnet port 23
WiFiClient        dbgclient;                             // An instance of the debug/telnet client
bool              dbgConnectFlag = false;                // true if client connected on port 23
#endif
WiFiClient        mp3client;                             // An instance of the mp3 client
#else 
// we use Ethernet/LAN
#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
EthernetServer    cmdserver(80);                         // Instance of embedded webserver, port 80
EthernetClient    cmdclient;                             // An instance of the client for commands
#endif
#ifdef PORT23_ACTIVE
EthernetServer    dbgserver(23);                         // Instance of server, telnet port 23
EthernetClient    dbgclient;                             // An instance of the debug/telnet client
bool              dbgConnectFlag = false;                // true if client connected on port 23
#endif
EthernetClient    mp3client;                             // An instance of the mp3 client
EthernetUDP       udpclient;                             // A UDP instance used for ntp time retrieval
EthernetLinkStatus lstat;                                // Ethernet link status
bool              reInitEthernet = false;                // W5500 board re-initialization needed
#endif
TaskHandle_t      mainTask;                              // Taskhandle for main task
TaskHandle_t      xplayTask;                             // Task handle for play task
TaskHandle_t      xspfTask;                              // Task handle for special functions
TaskHandle_t      xextenderTask;                         // Task handle for communication with PCF8574 ICs
#if defined VU_METER && defined LOAD_VS1053_PATCH
TaskHandle_t      xvumeterTask;                          // Task handle for displaying vu-meter value
#endif
SemaphoreHandle_t SPIsem = NULL;                         // For exclusive SPI usage
hw_timer_t*       timer = NULL;                          // For timer
char              timetxt[6];                            // Converted timeinfo
QueueHandle_t     dataQueue;                             // Queue for mp3 datastream
qdata_struct      outchunk;                              // Data to queue
qdata_struct      inchunk;                               // Data from queue
uint8_t*          outqp = outchunk.buf;                  // Pointer to buffer in outchunk
uint32_t          totalCount = 0;                        // Counter mp3 data
enum_datamode     dataMode = STOPPED;                    // State of datastream
int               metacount;                             // Number of bytes in metadata
int               datacount;                             // Counter databytes before metadata
char              metalinebf[METASIZ + 1];               // Buffer for metaline/ID3 tags
int16_t           metalinebfx;                           // Index for metalinebf
String            icystreamtitle;                        // Streamtitle from metadata
String            icyname;                               // Icecast station name
String            ipaddress;                             // Own IP-address
int               bitrate;                               // Bitrate in kb/sec
int               mbitrate;                              // Measured bitrate
int               metaint = 0;                           // Number of databytes between metadata
int8_t            currentPreset = -1;                    // Preset station playing
int8_t            highestPreset = 0;                     // highest preset
String            host;                                  // The URL to connect to or file to play
String            playlist;                              // The URL of the specified playlist
bool              hostreq = false;                       // Request for new host
bool              reqtone = false;                       // New tone setting requested
int8_t            muteFlag = 0;                          // Mute output (0=unmuted, -1=permanently, >0 temp. in 1/10 sec)
bool              resetreq = false;                      // Request to reset the ESP32
bool              NetworkFound = false;                  // True if WiFi network connected
String            networks;                              // Found networks in the surrounding
int8_t            playlist_num = 0;                      // Nonzero for selection from playlist
File              mp3file;                               // File containing mp3 on SD card
size_t            mp3fileLength;                         // File length
size_t            mp3fileBytesLeft;                      // Still to read
enum_selection    currentSource = NONE;                  // momentary source we play from at this moment
bool              chunked = false;                       // Station provides chunked transfer
int               chunkcount = 0;                        // Counter for chunked transfer
String            http_getcmd;                           // Contents of last GET command
String            http_rqfile;                           // Requested file
bool              http_reponse_flag = false;             // Response required
#ifdef ENABLE_INFRARED
uint16_t          ir_value = 0;                          // IR code
uint32_t          ir_0 = 550;                            // Average duration of an IR short pulse
uint32_t          ir_1 = 1650;                           // Average duration of an IR long pulse
#endif
struct tm         timeinfo;                              // Will be filled by NTP server
bool              time_req = false;                      // Set time requested
bool              SD_okay = false;                       // True if SD card in place and readable
int16_t           SD_mp3fileCount = 0;                   // Number of mp3 files found
int16_t           currentIndex = -1;                     // current index in mp3List when SD (0 means random) and in soapList when Mediaserver
uint16_t          clength;                               // Content length found in http header
enum_repeat_mode  mp3fileRepeatFlag = NOREPEAT;
bool              mp3fileJumpForward = false;            // jump forward in mp3 file
bool              mp3fileJumpBack = false;               // jump backwards in mp3 file
bool              mp3filePause = false;                  // pause playing mp3 file
bool              staticIPs = false;
bool              dhcpRequested = false;
String            sdOutbuf;                              // Output buffer for cmdclient
String            lastArtistSong;                        // for restoring text after timeout
String            lastAlbumStation;                      // for restoring text after timeout
bool              tryToMountSD = false;                  // request to mount SD when system is already up and running
bool              forceProgressBar = false;              // request progress bar to be painted
std::vector<mp3node_t> mp3nodeList;
#ifndef USE_ETHERNET
std::vector<WifiInfo_t> wifilist;                        // List with wifi_xx info
#else
unsigned int      localPortNtp = 8888;                   // local port to listen for UDP packets
const int         NTP_PACKET_SIZE = 48;                  // NTP time stamp is in the first 48 bytes of the message
#endif
// nvs stuff
nvs_page               nvsbuf;                           // Space for 1 page of NVS info
const esp_partition_t* nvs;                              // Pointer to partition struct
esp_err_t              nvserr;                           // Error code from nvs functions
uint32_t               nvshandle = 0;                    // Handle for nvs access
uint8_t                namespaceID;                      // Namespace ID found
char                   nvskeys[MAXKEYS][16];             // Space for NVS keys
std::vector<keyname_t> keynames;                         // Keynames in NVS
// Rotary encoder & front panel stuff
#define sv DRAM_ATTR static volatile
sv uint16_t       clickcount = 0;                        // Incremented per encoder click
sv int16_t        rotationcount = 0;                     // Current position of rotary switch
sv uint16_t       enc_inactivity = 0;                    // Time inactive
sv int16_t        locrotcount = 0;                       // Local rotation count
sv bool           singleClick = false;                   // True if single click detected
//sv bool           tripleclick = false;                  // True if triple click detected
sv bool           doubleClick = false;                   // True if double click detected
sv bool           longClick = false;                     // True if longClick detected
bool              buttonReturn = false;                  // True if return button pressed
bool              buttonRepeatMode = false;              // True if repeat button pressed
int8_t            buttonPreset = -1;                     // 0...9 if station button pressed
bool              buttonSD = false;                      // True if "SD" button presed
bool              buttonStation = false;                 // True if "Station" button pressed
//
#ifdef ENABLE_SOAP
#ifdef USE_ETHERNET
SoapESP32         soap(&mp3client, &udpclient, &SPIsem); // UDP/SSDP used only for WOL, global SPI lock 
#else
SoapESP32         soap(&mp3client, NULL);                // UDP/SSDP not used, no global SPI lock 
#endif
bool              buttonMediaserver = false;             // True if "SD" & "Station" buttons were pressed at once
std::vector<soapChain_t> soapChain;                      // For finding our way back up to root ("0")
soapObjectVect_t  soapList;                              // SOAP browse results for a given object (container/directory)
soapObject_t      hostObject;                            // more needed than just a string as for radio or SD card
#endif
bool              buttonSkipBack = false;                // True if "Skip Back" button is pressed
bool              buttonSkipForward = false;             // True if "Skip Forward" button is pressed
bool              skipStationAllowed = false;            // allow skip buttons in station mode
enum_enc_menu     encoderMode = IDLING;                  // Default is IDLING mode
enum_selection    playMode = STATION;                    // Default is STATION mode
#ifdef CHECK_LOOP_TIME
uint32_t          maxLoopTime = 0;                       // just for debugging purposes
#endif

// Needed for the ST7735 display
#include "bluetft.h"                                     // For ILI9163C or ST7735S 128x160 display
void     tftlog(const char *str, uint16_t textColor = (WHITE));

#ifdef ENABLE_DIGITAL_INPUTS
struct progpin_struct                                    // For programmable input pins
{
  int8_t         gpio;                                  // Pin number
  bool           reserved;                              // Reserved for connected devices
  bool           avail;                                 // Pin is available for a command
  String         command;                               // Command to execute when activated
  // Example: "uppreset=1"
  bool           cur;                                   // Current state, true = HIGH, false = LOW
};

progpin_struct   progpin[] =                             // Input pins and programmed function
{
  {  0, false, false,  "", false },
  //{  1, true,  false,  "", false },                    // Reserved for TX Serial output
  {  2, false, false,  "", false },
  //{  3, true,  false,  "", false },                    // Reserved for RX Serial input
  {  4, false, false,  "", false },
  {  5, false, false,  "", false },
  //{  6, true,  false,  "", false },                    // Reserved for FLASH SCK
  //{  7, true,  false,  "", false },                    // Reserved for FLASH D0
  //{  8, true,  false,  "", false },                    // Reserved for FLASH D1
  //{  9, true,  false,  "", false },                    // Reserved for FLASH D2
  //{ 10, true,  false,  "", false },                    // Reserved for FLASH D3
  //{ 11, true,  false,  "", false },                    // Reserved for FLASH CMD
  { 12, false, false,  "", false },
  { 13, false, false,  "", false },
  { 14, false, false,  "", false },
  { 15, false, false,  "", false },
  { 16, false, false,  "", false },
  { 17, false, false,  "", false },
  { 18, false, false,  "", false },                      // Default for SPI CLK
  { 19, false, false,  "", false },                      // Default for SPI MISO
  //{ 20, true,  false,  "", false },                    // Not exposed on DEV board
  { 21, false, false,  "", false },                      // Also Wire SDA
  { 22, false, false,  "", false },                      // Also Wire SCL
  { 23, false, false,  "", false },                      // Default for SPI MOSI
  //{ 24, true,  false,  "", false },                    // Not exposed on DEV board
  { 25, false, false,  "", false },
  { 26, false, false,  "", false },
  { 27, false, false,  "", false },
  //{ 28, true,  false,  "", false },                    // Not exposed on DEV board
  //{ 29, true,  false,  "", false },                    // Not exposed on DEV board
  //{ 30, true,  false,  "", false },                    // Not exposed on DEV board
  //{ 31, true,  false,  "", false },                    // Not exposed on DEV board
  { 32, false, false,  "", false },
  { 33, false, false,  "", false },
  { 34, false, false,  "", false },                      // Note, no internal pull-up
  { 35, false, false,  "", false },                      // Note, no internal pull-up
  { -1, false, false,  "", false }                       // End of list
};

struct touchpin_struct                                   // For programmable input pins
{
  int8_t         gpio;                                   // Pin number GPIO
  bool           reserved;                               // Reserved for connected devices
  bool           avail;                                  // Pin is available for a command
  String         command;                                // Command to execute when activated
  // Example: "uppreset=1"
  bool           cur;                                    // Current state, true = HIGH, false = LOW
  int16_t        count;                                  // Counter number of times low level
};
touchpin_struct   touchpin[] =                           // Touch pins and programmed function
{
  {   4, false, false, "", false, 0 },                   // TOUCH0
  //{ 0, true,  false, "", false, 0 },                   // TOUCH1, reserved for BOOT button
  {   2, false, false, "", false, 0 },                   // TOUCH2
  {  15, false, false, "", false, 0 },                   // TOUCH3
  {  13, false, false, "", false, 0 },                   // TOUCH4
  {  12, false, false, "", false, 0 },                   // TOUCH5
  {  14, false, false, "", false, 0 },                   // TOUCH6
  {  27, false, false, "", false, 0 },                   // TOUCH7
  {  33, false, false, "", false, 0 },                   // TOUCH8
  {  32, false, false, "", false, 0 },                   // TOUCH9
  {  -1, false, false, "", false, 0 }
  // End of table
};
#endif

//**************************************************************************************************
// Pages, CSS and data for the webinterface.                                                       *
//**************************************************************************************************
#include "about_html.h"
#include "config_html.h"
#include "index_html.h"
#include "mp3play_html.h"
#include "radio_css.h"
#include "favicon_ico.h"
#include "defaultprefs.h"

//**************************************************************************************************
// End of global data section.                                                                     *
//**************************************************************************************************

//
//**************************************************************************************************
// VS1053 stuff.  Based on maniacbug library.                                                      *
//**************************************************************************************************
// VS1053 class definition.                                                                        *
// Remark: The VS1053's external clock is 12.288MHz (XTALI set by quarz) and the internal clock    *
//         multiplier will be set to 3.5 which results in an internal clock 43.008 MHz.            *
//         So one internal clock cycle takes 0.02325us (43 clock cycles per us) which is important *
//         for calculating the time it takes to perform a SCI/SDI operation in worst case.         *
//         See chapter 9.6 in the VS1053B datasheet for explanation and more details.              *
//**************************************************************************************************
class VS1053
{
  private:
    int8_t        cs_pin;                        // Pin where CS line is connected
    int8_t        dcs_pin;                       // Pin where DCS line is connected
    int8_t        dreq_pin;                      // Pin where DREQ line is connected
    int8_t        shutdown_pin;                  // Pin where the shutdown line is connected
    uint8_t       curvol;                        // Current volume setting 0..100%
    const uint8_t vs1053_chunk_size = 32;
    // SCI Register                              // max cycles to perform this operation (worst case)
    const uint8_t SCI_MODE          = 0x0;       // 80 CLKI -> 1.86us
    const uint8_t SCI_STATUS        = 0x1;       // 80 CLKI
    const uint8_t SCI_BASS          = 0x2;       // 80 CLKI
    const uint8_t SCI_CLOCKF        = 0x3;       // 1200 XTALI -> 98us
    const uint8_t SCI_AUDATA        = 0x5;       // 450 CLKI -> 10.46us
    const uint8_t SCI_WRAM          = 0x6;       // 100 CLKI -> 2.32us
    const uint8_t SCI_WRAMADDR      = 0x7;       // 100 CLKI
    const uint8_t SCI_AIADDR        = 0xA;       // 210 CLKI -> 4.88us
    const uint8_t SCI_VOL           = 0xB;       // 80 CLKI
    const uint8_t SCI_AICTRL0       = 0xC;       // 80 CLKI
    const uint8_t SCI_AICTRL1       = 0xD;       // 80 CLKI
    const uint8_t SCI_AICTRL2       = 0xE;       // 80 CLKI
    const uint8_t SCI_AICTRL3       = 0xF;       // 80 CLKI
    const uint8_t SCI_num_registers = 0xF;
    // SCI_MODE bits
    const uint8_t SM_SDINEW         = 11;        // Bitnumber in SCI_MODE always on
    const uint8_t SM_RESET          = 2;         // Bitnumber in SCI_MODE soft reset
    const uint8_t SM_CANCEL         = 3;         // Bitnumber in SCI_MODE cancel song
    const uint8_t SM_TESTS          = 5;         // Bitnumber in SCI_MODE for tests
    const uint8_t SM_LINE1          = 14;        // Bitnumber in SCI_MODE for Line input
    // SCI_STATUS bits
    const uint16_t SS_VU_ENABLE     = 0x0200;    // Enables VU-Meter (needs newest patches)
    const uint16_t SS_REFERENCE_SEL = 0x0101;    // Sets higher reference voltage 1.65V instead of 1.3V
    SPISettings   VS1053_SPI;                    // SPI settings for this slave
    uint8_t       endFillByte;                   // Byte to send when stopping song
    bool          okay              = true;      // VS1053 is working

  protected:
    inline void await_data_request(unsigned long maxDelay_us = 0) const
    {
      unsigned long previousTime = micros();

      if (dreq_pin < 0) return;
      while (1) {
        if (digitalRead(dreq_pin) ||             // break when VS1053 is ready for data/next SCI command
            (maxDelay_us > 0 && ((micros() - previousTime) > maxDelay_us))) { // or timeout
          break;
        }  
        NOP();                                   // Very short delay
      }
    }

    inline void control_mode_on() const
    {
      SPI.beginTransaction(VS1053_SPI);          // Prevent other SPI users
      digitalWrite(cs_pin, LOW);
    }

    inline void control_mode_off() const
    {
      digitalWrite(cs_pin, HIGH);                // End control mode
      SPI.endTransaction();                      // Allow other SPI users
    }

    inline void data_mode_on() const
    {
      SPI.beginTransaction(VS1053_SPI);          // Prevent other SPI users
      //digitalWrite(cs_pin, HIGH);              // Bring slave in data mode
      digitalWrite(dcs_pin, LOW);
    }

    inline void data_mode_off() const
    {
      digitalWrite(dcs_pin, HIGH);               // End data mode
      SPI.endTransaction();                      // Allow other SPI users
    }

    // if max delay is 0 then below routines will only return when DREQ is HIGH again !
    uint16_t    read_register(uint8_t _reg, unsigned long maxDelay_us = 0) const;
    void        write_register(uint8_t _reg, uint16_t _value, unsigned long maxDelay_us = 0) const;
    //
    inline bool sdi_send_buffer(uint8_t* data, size_t len);
    void        sdi_send_fillers(size_t length);
    void        wram_write(uint16_t address, uint16_t data);
    uint16_t    wram_read(uint16_t address);

  public:
    // Constructor.  Only sets pin values.  Doesn't touch the chip.  Be sure to call begin()!
    VS1053 (int8_t _cs_pin, int8_t _dcs_pin, int8_t _dreq_pin, int8_t _shutdown_pin);
    void     begin();                                   // Begin operation.  Sets pins correctly,
    // and prepares SPI bus.
    void     startSong();                               // Prepare to start playing. Call this each
    // time a new song starts.
    inline bool playChunk(uint8_t* data,                // Play a chunk of data.  Copies the data to
                          size_t len);                  // the chip.  Blocks until complete.
    // Returns true if more data can be added
    // to fifo
    void     stopSong();                                // Finish playing a song. Call this after
    // the last playChunk call.
    void     setVolume(uint8_t vol);                    // Set the player volume.Level from 0-100,
    // higher is louder.
    void     setTone (uint8_t* rtone);                  // Set the player baas/treble, 4 nibbles for
    // treble gain/freq and bass gain/freq
    inline uint8_t getVolume() const                    // Get the current volume setting.
    { // higher is louder.
      return curvol;
    }
    void     printDetails(const char *header);          // Print config details to serial output
    void     softReset();                               // Do a soft reset
    bool     testComm(const char *header);              // Test communication with module
    inline bool data_request() const
    {
      return (digitalRead(dreq_pin) == HIGH);
    }
#ifdef LOAD_VS1053_PATCH
    void     loadVS1053Patch(void);
#endif
#if defined VU_METER && defined LOAD_VS1053_PATCH
    uint16_t readVuMeter();
#endif
};

//**************************************************************************************************
// VS1053 class implementation.                                                                    *
//**************************************************************************************************

VS1053::VS1053(int8_t _cs_pin, int8_t _dcs_pin, int8_t _dreq_pin, int8_t _shutdown_pin) :
  cs_pin(_cs_pin), dcs_pin(_dcs_pin), dreq_pin(_dreq_pin), shutdown_pin(_shutdown_pin)
{
}

uint16_t VS1053::read_register(uint8_t _reg, unsigned long maxDelay_us) const
{
  uint16_t result;

  control_mode_on();
  SPI.write(3);                                   // Read operation
  SPI.write(_reg);                                // Register to write (0..0xF)
  // Note: transfer16 does not seem to work
  result = (SPI.transfer(0xFF) << 8) |            // Read 16 bits data
           (SPI.transfer(0xFF));
  await_data_request(maxDelay_us);                // Wait for DREQ to be HIGH again or maxDelay_us timeout
  control_mode_off();
  return result;
}

void VS1053::write_register(uint8_t _reg, uint16_t _value, unsigned long maxDelay_us) const
{
  control_mode_on();
  SPI.write(2);                                   // Write operation
  SPI.write(_reg);                                // Register to write (0..0xF)
  SPI.write16(_value);                            // Send 16 bits data
  await_data_request(maxDelay_us);                // Wait for DREQ to be HIGH again or maxDelay_us timeout
  control_mode_off();
}

bool VS1053::sdi_send_buffer(uint8_t* data, size_t len)
{
  size_t chunk_length;                            // Length of chunk 32 byte or shorter

  data_mode_on();
  while (len) {                                   // More to do?
    chunk_length = len;
    if (len > vs1053_chunk_size) {
      chunk_length = vs1053_chunk_size;
    }
    len -= chunk_length;
    await_data_request();                         // Wait for space available
    SPI.writeBytes(data, chunk_length);
    data += chunk_length;
  }
  data_mode_off();
  return data_request();                          // True if more data can de stored in fifo
}

void VS1053::sdi_send_fillers(size_t len)
{
  size_t chunk_length;                            // Length of chunk 32 byte or shorter

  data_mode_on();
  while (len) {                                   // More to do?
    await_data_request();                         // Wait for space available
    chunk_length = len;
    if (len > vs1053_chunk_size) {
      chunk_length = vs1053_chunk_size;
    }
    len -= chunk_length;
    while (chunk_length--) {
      SPI.write(endFillByte);
    }
  }
  data_mode_off();
}

void VS1053::wram_write(uint16_t address, uint16_t data)
{
  write_register(SCI_WRAMADDR, address);
  write_register(SCI_WRAM, data);
}

uint16_t VS1053::wram_read(uint16_t address)
{
  write_register(SCI_WRAMADDR, address);             // Start reading from WRAM
  return read_register(SCI_WRAM);                    // Read back result
}

// write_register() is already defined at this point
#ifdef LOAD_VS1053_PATCH
#include "VS1053_patch.h"
#endif

bool VS1053::testComm(const char *header)
{
  // Test the communication with the VS1053 module.  The result wille be returned.
  // If DREQ is low, there is problably no VS1053 connected.  Pull the line HIGH
  // in order to prevent an endless loop waiting for this signal.  The rest of the
  // software will still work, but readbacks from VS1053 will fail.
  int       i;                                         // Loop control
  uint16_t  r1, r2, cnt = 0;
  uint16_t  delta = 300;                               // 3 for fast SPI

  dbgprint(header);                                    // Show a header
  if (!digitalRead (dreq_pin)) {
    dbgprint("VS1053 not properly installed!(?) DREQ=LOW!");
    // Allow testing without the VS1053 module
    pinMode(dreq_pin,  INPUT_PULLUP);                  // DREQ is now input with pull-up
    return false;                                      // Return bad result
  }
  // Further TESTING.  Check if SCI bus can write and read without errors.
  // We will use the volume setting for this.
  // Will give warnings on serial output if DEBUG is active.
  // A maximum of 20 errors will be reported.
  if (strstr(header, "Fast")) {
    delta = 3;                                         // Fast SPI, more loops
  }
  for (i = 0; (i < 0xFFFF) && (cnt < 20); i += delta) {
    write_register(SCI_VOL, i);                        // Write data to SCI_VOL
    r1 = read_register(SCI_VOL);                       // Read back for the first time
    r2 = read_register(SCI_VOL);                       // Read back a second time
    if  (r1 != r2 || i != r1 || i != r2) {             // Check for 2 equal reads
      dbgprint("VS1053 error retry SB:%04X R1:%04X R2:%04X", i, r1, r2);
      cnt++;
      delay (10);
    }
  }
  okay = (cnt == 0);                                   // True if working correctly
  return (okay);                                       // Return the result
}

void VS1053::begin()
{
  pinMode(dreq_pin, INPUT);                            // DREQ is an input
  pinMode(cs_pin, OUTPUT);                             // the SCI and SDI signals
  pinMode(dcs_pin,OUTPUT);
  digitalWrite(dcs_pin, HIGH);                         // start HIGH for SCI en SDI
  digitalWrite(cs_pin, HIGH);
  if (shutdown_pin >= 0) {                             // shutdown in use?
    pinMode(shutdown_pin, OUTPUT);
    digitalWrite(shutdown_pin, HIGH);                  // shut down audio output
  }
  delay(100);
  // Init SPI in slow mode (200kHz)
  VS1053_SPI = SPISettings(200000, MSBFIRST, SPI_MODE0);
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  delay(20);
  if (testComm("Slow SPI, Testing VS1053 read/write registers...")) {
    // GPIO0 & GPIO1 are pulled low with resistors on VS1053 boards from China.
    // Other VS1053 modules may start up in midi mode.  The result is that there is no audio
    // when playing MP3.  You can modify the board, but there is another way:
    //wram_write(0xC017, 3);                           // GPIO DDR = 3
    //wram_write(0xC019, 0);                           // GPIO ODATA = 0
    //
    delay(100);
    softReset();                                       // do a soft reset
#ifdef LOAD_VS1053_PATCH
    dbgprint("VS1053B loading patch %s", PATCH_VERSION); // patch must be loaded >after< reset/sw reset
    delay(20);
    loadVS1053Patch();
    delay(10);
    await_data_request(2000000);                       // wait for DREQ to rise (max 2sec)
#endif    
    // Switch on the analog parts
    write_register(SCI_AUDATA, 44100 + 1);             // 44.1kHz + stereo
    // The next clocksetting allows higher SPI clocking
#if defined VU_METER && defined LOAD_VS1053_PATCH
    // CLKI = XTALI * 3.5, No Multiplier modification allowed, XTALI = 12.288 MHz
    write_register(SCI_CLOCKF, 8 << 12);
#else
    // CLKI = XTALI * 3.0, No Multiplier modification allowed, XTALI = 12.288 MHz
    write_register(SCI_CLOCKF, 6 << 12);
#endif
    delay(10);
    await_data_request(1000000);                       // wait for DREQ to rise (max 1 sec)
    // Now we can set high speed SPI clock.
    VS1053_SPI = SPISettings (5000000, MSBFIRST, SPI_MODE0);   // Speed up SPI
    write_register(SCI_MODE, _BV (SM_SDINEW) | _BV (SM_LINE1));
    testComm("Fast SPI, Testing VS1053 read/write registers again...");
    delay(10);
    await_data_request(1000000);                       // wait for DREQ to rise (max 1sec)
    endFillByte = wram_read(0x1E06) & 0xFF;
    dbgprint("VS1053B endFillByte is %X", endFillByte);
    uint16_t stat = read_register(SCI_STATUS);
    dbgprint("VS1053B Status Register = 0x%04X", stat);
#ifdef LOAD_VS1053_PATCH
    // patch V2.7 is needed to fix SS_REFERENCE_SEL bug 
    if (!(stat & 0x000C)) {
      // Analog power down has been turned off
      dbgprint("VS1053B setting SS_REFERENCE_SEL bit(s) [see VS1053-patches.pdf, Chapter 1.1]");
      write_register(SCI_STATUS, stat | SS_REFERENCE_SEL);
      stat = read_register(SCI_STATUS);
      dbgprint("VS1053B Status Register now = 0x%04X", stat);
    }  
#endif    
#if defined VU_METER && defined LOAD_VS1053_PATCH
    // patch V2.7 is needed to get VU meter feature
    dbgprint("VS1053B setting VU-Meter bit [see VS1053-patches.pdf, Chapter 1.2]");
    write_register(SCI_STATUS, stat | SS_VU_ENABLE);
    stat = read_register(SCI_STATUS);
    dbgprint("VS1053B Status Register now = 0x%04X", stat);
#endif
#ifdef ENABLE_I2S
    // Enabling I2S with MCLK output and sample rate 48kHz 
    wram_write(0xC017, 0x00F0);
    wram_write(0xC040, 0x000C);
    dbgprint("VS1053B I2S interface enabled: MCLK active, Sample Rate = 48kHz");
		const char *p = dbgprint("TOSLINK optical output ok.");
    tftlog(p);
#endif    
    delay(100);
  }
}

void VS1053::setVolume(uint8_t vol)
{
  // Set volume.  Both left and right.
  // Input value is 0..100.  100 is the loudest.
  // Clicking reduced by using 0xf8 to 0x00 as limits.
  uint16_t value;                                      // Value to send to SCI_VOL

  if (vol != curvol) {
    curvol = vol;                                      // Save for later use
    value = map(vol, 0, 100, 0xF8, 0x00);              // 0..100% to one channel
    value = (value << 8) | value;
    write_register(SCI_VOL, value, 3);                 // Volume left and right
    //uint16_t stat = read_register(SCI_STATUS, 3);
    //dbgprint("VS1053B Status Register = 0x%04X", stat);
    //write_register(SCI_STATUS, stat | 0x0001, 3);    // shows no effect! (Chapter 9.6.2 in datasheet)
  }
}

void VS1053::setTone(uint8_t *rtone)                   // Set bass/treble (4 nibbles)
{
  // Set tone characteristics.  See documentation for the 4 nibbles.
  uint16_t value = 0;                                  // Value to send to SCI_BASS
  int      i;                                          // Loop control

  for (i = 0; i < 4; i++) {
    value = (value << 4) | rtone[i];                   // Shift next nibble in
  }
  write_register(SCI_BASS, value, 3);
}

void VS1053::startSong()
{
  sdi_send_fillers (10);
  if (shutdown_pin >= 0) {                             // Shutdown in use?
    digitalWrite(shutdown_pin, LOW);                   // Enable audio output
  }

}

bool VS1053::playChunk(uint8_t* data, size_t len)
{
  return okay && sdi_send_buffer(data, len);           // True if more data can be added to fifo
}

void VS1053::stopSong()
{
  uint16_t modereg;                                    // Read from mode register
  int      i;                                          // Loop control

  sdi_send_fillers(2052);
  if (shutdown_pin >= 0) {                             // Shutdown in use?
    digitalWrite(shutdown_pin, HIGH);                  // Disable audio output
  }
  delay(10);
  write_register(SCI_MODE, _BV (SM_SDINEW) | _BV (SM_CANCEL), 3);
  for (i = 0; i < 200; i++) {
    sdi_send_fillers(32);
    modereg = read_register(SCI_MODE, 3);              // Read status
    if ((modereg & _BV (SM_CANCEL)) == 0) {
      sdi_send_fillers(2052);
      //dbgprint("Song stopped correctly after %d msec", i * 10);
      return;
    }
    delay(10);
  }
  printDetails("Song stopped incorrectly!");
}

void VS1053::softReset()
{
  write_register(SCI_MODE, _BV (SM_SDINEW) | _BV (SM_RESET), 3);
  delay(10);
  await_data_request();
}

void VS1053::printDetails(const char *header)
{
  uint16_t regbuf[16];
  uint8_t  i;

  dbgprint(header);
  dbgprint("REG   Contents");
  dbgprint("---   -----");
  for (i = 0; i <= SCI_num_registers; i++) {
    regbuf[i] = read_register(i);
  }
  for (i = 0; i <= SCI_num_registers; i++) {
    delay(5);
    dbgprint("%3X - %5X", i, regbuf[i]);
  }
}

#if defined VU_METER && defined LOAD_VS1053_PATCH
uint16_t VS1053::readVuMeter()
{
  return okay ? read_register(SCI_AICTRL3, 3) : 0;         // Read back VU-Meter result
}
#endif

// The object for the MP3 player
VS1053* vs1053player;

//**************************************************************************************************
// End of VS1053 stuff.                                                                            *
//**************************************************************************************************


//**************************************************************************************************
//                                      N V S O P E N                                              *
//**************************************************************************************************
// Open Preferences with my-app namespace. Each application module, library, etc.                  *
// has to use namespace name to prevent key name collisions. We will open storage in               *
// RW-mode (second parameter has to be false).                                                     *
//**************************************************************************************************
void nvsopen()
{
  if (!nvshandle) {                                       // Opened already?
    nvserr = nvs_open(NAME, NVS_READWRITE, &nvshandle);   // No, open nvs
    if (nvserr) {
      dbgprint("nvs_open failed!");
    }
  }
}

//**************************************************************************************************
//                                      N V S C L E A R                                            *
//**************************************************************************************************
// Clear all preferences.                                                                          *
//**************************************************************************************************
esp_err_t nvsclear()
{
  nvsopen();                                         // Be sure to open nvs
  return nvs_erase_all(nvshandle);                   // Clear all keys
}

//**************************************************************************************************
//                                      N V S G E T S T R                                          *
//**************************************************************************************************
// Read a string from nvs.                                                                         *
//**************************************************************************************************
String nvsgetstr(const char* key)
{
  static char nvs_buf[NVSBUFSIZE];         // Buffer for contents
  size_t      len = NVSBUFSIZE;            // Max length of the string, later real length

  nvsopen();                               // Be sure to open nvs
  nvs_buf[0] = '\0';                       // Return empty string on error
  nvserr = nvs_get_str(nvshandle, key, nvs_buf, &len);
  if (nvserr) {
    dbgprint("nvs_get_str failed %X for key %s, keylen is %d, len is %d!",
               nvserr, key, strlen (key), len);
    dbgprint("Contents: %s", nvs_buf);
  }
  return String(nvs_buf);
}

//**************************************************************************************************
//                                      N V S S E T S T R                                          *
//**************************************************************************************************
// Put a key/value pair in nvs.  Length is limited to allow easy read-back.                        *
// Max key lenght is 15 chars (defined in nvs.h) !!!!!! No writing if no change.                   *
//**************************************************************************************************
esp_err_t nvssetstr(const char* key, String val)
{
  String curcont;                                          // Current contents
  bool   wflag = true;                                     // Assume update or new key

  //dbgprint("Setstring for %s: %s", key, val.c_str());
  if (val.length() >= NVSBUFSIZE) {                        // Limit length of string to store
    dbgprint("nvssetstr length failed!");
    return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
  }
  if (nvsSearch(key)) {                                    // Already in nvs?
    curcont = nvsgetstr(key);                              // Read current value
    wflag = (curcont != val);                              // Value change?
  }
  if (wflag) {                                             // Update or new?
    //dbgprint("nvssetstr update value");
    nvserr = nvs_set_str(nvshandle, key, val.c_str());     // Store key and value
    if (nvserr) {                                          // Check error
      dbgprint("nvssetstr failed!");
    }
    else {
      dbgprint("nvssetstr: %s = %s saved to NVS flash", key, val.c_str());
    }
  }
  return nvserr;
}

//**************************************************************************************************
//                                      N V S C H K E Y                                            *
//**************************************************************************************************
// Change a keyname in in nvs.                                                                     *
//**************************************************************************************************
void nvschkey (const char* oldk, const char* newk)
{
  String curcont;                                     // Current contents

  if (nvsSearch(oldk)) {                              // Old key in nvs?
    curcont = nvsgetstr(oldk);                        // Read current value
    nvs_erase_key(nvshandle, oldk);                   // Remove key
    nvssetstr(newk, curcont);                         // Insert new
  }
}

//**************************************************************************************************
//                                      C L A I M S P I                                            *
//**************************************************************************************************
// Claim the SPI bus.  Uses FreeRTOS semaphores.                                                   *
//**************************************************************************************************
void claimSPI(const char* p)
{
  const TickType_t ctry = 10;                         // Time to wait for semaphore (10ms)

  while (xSemaphoreTake(SPIsem, ctry) != pdTRUE) {    // claim SPI bus
    //...
  }
}

//**************************************************************************************************
//                                   R E L E A S E S P I                                           *
//**************************************************************************************************
// Free the SPI bus.  Uses FreeRTOS semaphores.                                                    *
//**************************************************************************************************
void releaseSPI()
{
  xSemaphoreGive(SPIsem);                               // release SPI bus
}

//**************************************************************************************************
//                                      Q U E U E F U N C                                          *
//**************************************************************************************************
// Queue a special function for the play task.                                                     *
//**************************************************************************************************
void queuefunc(int func)
{
  qdata_struct specchunk;                              // Special function to queue

  specchunk.datatyp = func;                            // Put function in datatyp
  xQueueSend(dataQueue, &specchunk, 200);              // Send to queue
}

//**************************************************************************************************
//                                      N V S S E A R C H                                          *
//**************************************************************************************************
// Check if key exists in nvs.                                                                     *
//**************************************************************************************************
bool nvsSearch(const char* key)
{
  size_t len = NVSBUFSIZE;                              // Length of the string

  nvsopen();                                            // Be sure to open nvs
  nvserr = nvs_get_str(nvshandle, key, NULL, &len);     // Get length of contents
  return (nvserr == ESP_OK);                            // Return true if found
}

//**************************************************************************************************
//                                      T F T S E T                                                *
//**************************************************************************************************
// Request to display a segment on TFT.  Version for char* and String parameter.                   *
//**************************************************************************************************
void tftset(uint16_t inx, const char *str)
{
  if (inx < TFTSECS) {                                 // segment available on display ?
    if (str) {                                         // string specified?
      tftdata[inx].str = String(str);                  // set string
    }
    tftdata[inx].update_req = true;                    // and request update
  }
}

void tftset(uint16_t inx, String& str)
{
  tftdata[inx].str = str;                              // set string
  tftdata[inx].update_req = true;                      // and request update
}

//**************************************************************************************************
//                                      U T F 8 A S C I I                                          *
//**************************************************************************************************
// UTF8-Decoder: convert UTF8-string to extended ASCII.                                            *
// Convert a single Character from UTF8 to Extended ASCII.                                         *
// Return "0" if a byte has to be ignored.                                                         *
//**************************************************************************************************
byte utf8ascii(byte ascii)
{
  static const byte lut_C3[] = { "AAAAAAACEEEEIIIIDNOOOOO#0UUUU##Baaaaaaaceeeeiiiidnooooo##uuuuyyy" };
  static byte       c1;              // Last character buffer
  byte              res = 0;         // Result, default 0

  if (ascii <= 0x7F) {               // Standard ASCII-set 0..0x7F handling
    c1 = 0;  res = ascii;            // Return unmodified
  }
  else if ((c1 != 0xC3) && (ascii == 0xC9)) {
    c1 = 0;  res = 'E';              // É
  }
  else if ((c1 != 0xC3) && (ascii == 0x84 || ascii == 0x86 || ascii == 0xE0 || ascii == 0xE1 || ascii == 0xE4 || ascii == 0xE5)) {
    c1 = 0;  res = 'a';              // à, á, å oder ä
  }
  else if ((c1 != 0xC3) && (ascii == 0xE7)) {
    c1 = 0;  res = 'c';              // ç
  }
  else if ((c1 != 0xC3) && (ascii == 0x82 || ascii == 0xD9 || ascii == 0xE9 || ascii == 0xEB)) {
    c1 = 0;  res = 'e';              // é, ë
  }
  else if ((c1 != 0xC3) && (ascii == 0xED)) {
    c1 = 0;  res = 'i';              // í
  }
  else if ((c1 != 0xC3) && (ascii == 0xDF)) {
    c1 = 0;  res = 'B';              // ß
  }
  else if ((c1 != 0xC3) && (ascii == 0x94 || ascii == 0xF6)) {
    c1 = 0;  res = 'o';              // ö
  }
  else if ((c1 != 0xC3) && (ascii == 0x81 || ascii == 0xFC)) {
    c1 = 0;  res = 'u';              // ü
  }
  else if ((c1 != 0xC3) && (ascii == 0x8E || ascii == 0xC4)) {
    c1 = 0;  res = 'A';              // Ä
  }
  else if ((c1 != 0xC3) && (ascii == 0x99 || ascii == 0xD6)) {
    c1 = 0;  res = 'O';              // Ö
  }
  else if ((c1 != 0xC3) && (ascii == 0x9A || ascii == 0xDC)) {
    c1 = 0;  res = 'U';              // Ü
  }
  else if ((c1 != 0xC3) && (ascii == 0xB4 || ascii == 0xEF)) {
    c1 = 0;  res = '\'';             // ´
  }
  else {
    switch (c1) {                    // conversion depending on first UTF8-character
      case 0x00: c1 = ascii;         // remember first special character
                 break;              // return zero, character has to be ignored
      case 0xC2: if (ascii == 0xB7) res = '-';
                 else res = '~';
                 c1 = 0;
                 break;
      case 0xC3: res = lut_C3[ascii - 128];
                 c1 = 0;
                 break;
      case 0x82: if (ascii == 0xAC) {
                   res = 'E';        // special case Euro-symbol
                 }
                 c1 = 0;
                 break;
    }
  }
  if (ascii > 0x7F) dbgprint("utf8ascii() ---> Special Char: 0x%02X, return: 0x%02X", ascii, res);

  return res;                        
}

//**************************************************************************************************
//                                      U T F 8 A S C I I                                          *
//**************************************************************************************************
// In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!).                          *
//**************************************************************************************************
void utf8ascii(char *s)
{
  int  i, k = 0;                     // Indexes for in en out string
  char c;

  for (i = 0; s[i]; i++) {           // For every input character
    c = utf8ascii(s[i]);             // Translate if necessary
    if (c) {                         // Good translation?
      s[k++] = c;                    // Yes, put in output string
    }
  }
  s[k] = 0;                          // Take care of delimeter
}

//**************************************************************************************************
//                                      U T F 8 A S C I I                                          *
//**************************************************************************************************
// Conversion UTF8-String to Extended ASCII String.                                                *
//**************************************************************************************************
String utf8ascii(const char *s)
{
  int  len = strlen(s);
  char buf[len + 1];
  
  strncpy(buf, s, sizeof(buf));
  utf8ascii(buf);

  return String(buf);
}

//**************************************************************************************************
//                                      U T F 16 A S C I I                                         *
//**************************************************************************************************
// For encoding ID3-Tags                                                                           *
//**************************************************************************************************
void utf16ascii(char *s, int length)
{
  int  i, k = 0;

  if (s[0] == 0xFF && s[1] == 0xFE) {       // Little Endian
    i = 2;
  }
  else if (s[0] == 0xFE && s[1] == 0xFF) {  // Big Endian
    i = 3;
  }
  else {
    //dbgprint("utf16ascii: ID3-Tag UTF16-Format not recognized 0x%02x 0x%02x", s[0], s[1]);
    return;
  }
  for (; i < length;) {
    s[k++] = s[i];                          // put in output string
    i += 2;
  }
  s[k] = 0;                                 // Take care of delimeter
}

//**************************************************************************************************
//                                          D B G P R I N T                                        *
//**************************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the DEBUG flag.        *
// Print only if DEBUG flag is true.  Always returns the formatted string.                         *
//**************************************************************************************************
char* dbgprint(const char* format, ...)
{
  static char sbuf[DEBUG_BUFFER_SIZE];                 // For debug lines
  va_list varArgs;                                     // For variable number of params

  va_start(varArgs, format);                           // Prepare parameters
  vsnprintf(sbuf, sizeof(sbuf), format, varArgs);      // Format the message
  va_end(varArgs);                                     // End of using parameters
  if (DEBUG) {                                         // DEBUG on?
    Serial.print("D: ");                               // Yes, print prefix
    Serial.println(sbuf);                              // and the info
#ifdef PORT23_ACTIVE
    if (dbgConnectFlag) {
      _claimSPI("dbg1");                               // claim SPI bus
      uint8_t erg = dbgclient.connected();
      _releaseSPI();                                   // release SPI bus
      if (erg) {                                       // just to make sure the client is still connected
        char sbuf1[DEBUG_BUFFER_SIZE + 5];
        snprintf(sbuf1, sizeof(sbuf1), "D: %s\r\n", sbuf); 
        size_t len = strlen(sbuf1);
        _claimSPI("dbg2");                             // claim SPI bus
        dbgclient.write(sbuf1, len);
        _releaseSPI();                                 // release SPI bus
      }
      else {
        dbgConnectFlag = false;                        // clear the flag
      }
    }
#endif
  }
  return sbuf;                                         // Return stored string
}

#ifdef ENABLE_SOAP
//**************************************************************************************************
//                           N E X T S O A P F I L E I N D E X                                     *
//**************************************************************************************************
// Select the next or previous mp3 file from SOAP browse list. We wrap around if needed. If        *
// parameter delta is 0 we select the first mp3 file on the list. Delta is >0 or <0 for next or    *
// previous track. Return value is the new nodeIndex or -1 for error.                              *
//**************************************************************************************************
int16_t nextSoapFileIndex(int16_t inx, int16_t delta)
{
  int i, ret = -1;
  String uriLc((char *)0);                          // no allocation yet

  if (hostreq) return 0;                            // no action when host request already set

  if (inx >= soapList.size()) {
    dbgprint("nextSoapFileIndex: error, inx=%d out ouf bounds (0...%d)",
               inx, soapList.size() - 1);
    return -1;
  }

  if (delta == 0) {
    // find first mp3 file in list
    for (i = 0; i < soapList.size(); i++) {
      if (soapList[i].isDirectory) continue;
      if (soapList[i].fileType != fileTypeAudio) continue;
        uriLc = soapList[i].uri;
        uriLc.toLowerCase();
      if (!uriLc.endsWith(".mp3")) continue;
      ret = i;
      break;
    }
  }
  else {
    for (i = inx; ;) {
      if (delta > 0) {
        if (++i >= soapList.size()) i = 0;
      }
      else {
        if (--i < 0) i = soapList.size() - 1;
      }
      if (i == inx) {
        // back to where we started from
        ret = inx;
        break;
      }
      if (soapList[i].isDirectory) continue;
      if (soapList[i].fileType != fileTypeAudio) continue;
        uriLc = soapList[i].uri;
        uriLc.toLowerCase();
      if (!uriLc.endsWith(".mp3")) continue;
      ret = i;
      break;
    }
  }
  dbgprint("nextSoapFileIndex: soapList.size=%d inx=%d delta=%d return=%d", 
           soapList.size(), inx, delta, ret);
  return ret;                                      // Return new index
}
#endif

//**************************************************************************************************
//                           N O S U B D I R I N S A M E D I R                                     *
//**************************************************************************************************
// Parameter must be an index that points to a file. If a sub-directory is in the same directory   *
// as the file is than this function returns false.                                                *
//**************************************************************************************************
bool noSubDirInSameDir(int16_t inx)
{
  int16_t parentDir;
  
  if (mp3nodeList[inx].isDirectory) return false;

  parentDir = mp3nodeList[inx].parentDir;
  for (int16_t i = 1; i < mp3nodeList.size(); i++) {
    if (parentDir == mp3nodeList[i].parentDir && mp3nodeList[i].isDirectory) {
      return false;
    }
  }
  return true;
}  

//**************************************************************************************************
//                           F I R S T S D I N D E X I N D I R                                     *
//**************************************************************************************************
// Find the first entry of a given directory. Parameter is the directory represented by an index.  *
// The found index will be returned to the caller. The returned index can be a file or directory.  *
// If the directory is empty we return the input parameter unchanged.                              *
//**************************************************************************************************
int firstSDindexInDir(int16_t inx)
{
  bool    found = false;
  int16_t i;

  if (inx >= mp3nodeList.size()) {
    dbgprint("firstSDindexInDir: error, inx=%d out ouf bounds (0...%d)",
             inx, mp3nodeList.size() - 1);
    return 0;
  }
  if (!mp3nodeList[inx].isDirectory) {
    dbgprint("firstSDindexInDir: error, inx=%d is not a directory", inx);
    return 0;
  }
  for (i = 1; i < mp3nodeList.size(); i++) {
    if (inx == mp3nodeList[i].parentDir) {
      found = true;
      break;
    }
  }

  return found ? i : inx;                           // return new index
}

//**************************************************************************************************
//                           N E X T S D I N D E X I N S A M E D I R                               *
//**************************************************************************************************
// Select the next or previous index from SD which belongs to the same directory level.            *
// Rotationcount is positive or negative for next or previous index.                               *
// The found index will be returned to the caller. Indexes can be files or directories !           *
//**************************************************************************************************
int nextSDindexInSameDir(int16_t inx, int16_t rotationcount)
{
  bool    found = false;
  int16_t x, dir;

  if (inx >= mp3nodeList.size()) {
    dbgprint("nextSDindexInSameDir: error, inx=%d out ouf bounds (0...%d)",
               inx, mp3nodeList.size() - 1);
    return 0;
  }
  if (inx != 0) {                                    // Random playing?
    x = inx;
    dir = mp3nodeList[x].parentDir;
    rotationcount = (rotationcount > 0) ? 1 : -1;
    x += rotationcount;
    for (; x > 0 && x < mp3nodeList.size(); x += rotationcount) {
      if (dir == mp3nodeList[x].parentDir) {
        found = true;
        break;
      }
    }
  }

  return found ? x : inx;                           // return new index
}

//**************************************************************************************************
//                   N E X T S D F I L E I N D E X I N S A M E D I R                               *
//**************************************************************************************************
// Parameter is the index of a mp3 file.                                                           *
// Select the next or previous mp3 file from the same directory on SD. If last file than select    *
// the first. If first then select last. The new node index will be returned to the caller.        *
//**************************************************************************************************
int nextSDfileIndexInSameDir(int16_t inx, int16_t delta)
{
  int16_t currentDir;

  dbgprint("nextSDfileIndexInSameDir: current index is %d", inx);
  if (inx >= mp3nodeList.size()) {
    dbgprint("nextSDfileIndexInSameDir: error, inx=%d out ouf bounds (0...%d)",
             inx, mp3nodeList.size() - 1);
    return 0;
  }
  if (inx != 0) {                                   // Random playing?
    currentDir = mp3nodeList[inx].parentDir;        // current directory we are playing from
    if (delta > 0) {
      while (true) {
        inx++;
        if (inx >= mp3nodeList.size())
          inx = 0;
        if (!mp3nodeList[inx].isDirectory &&
             currentDir == mp3nodeList[inx].parentDir)
          break;                                    // we found next file index
      }
    }
    else {
      while (true) {
        inx--;
        if (inx == 0)
          inx = mp3nodeList.size() - 1;
        if (!mp3nodeList[inx].isDirectory &&
            currentDir == mp3nodeList[inx].parentDir)
          break;                                    // we found previous file index
      }
    }
  }
  dbgprint("nextSDfileIndexInSameDir: return=%d", inx);
  return inx;
}

//**************************************************************************************************
//                                 N E X T S D F I L E I N D E X                                   *
//**************************************************************************************************
// Select the next or previous mp3 file from SD. We wrap around if needed. If parameter inx is 0   *
// we select the first mp3 file on SD card. Delta is +1 or -1 for next or previous track. The new  *
// nodeIndex will be returned.                                                                     *
//**************************************************************************************************
int nextSDfileIndex(int16_t inx, int16_t delta)
{
  if (hostreq) return 0;                            // no action when host request already set

  if (inx >= mp3nodeList.size()) {
    dbgprint("nextSDfileIndex: error, inx=%d out ouf bounds (0...%d)",
               inx, mp3nodeList.size() - 1);
    return 0;
  }
  dbgprint("nextSDfileIndex: inx=%d delta=%d", inx, delta);
  if (inx != 0) {                                   // random playing?
    while (true) {
      if (delta > 0) {
        inx++;
        if (inx >= mp3nodeList.size()) inx = 0;
      }
      else {
        inx--;
        if (inx < 0) inx = mp3nodeList.size() - 1;
      }
      if (!mp3nodeList[inx].isDirectory) break;
    }
  }
  else { // searching the first mp3 file on SD card
    for (int x = 1; x < mp3nodeList.size(); x += 1) {
      if (!mp3nodeList[x].isDirectory) {
        inx = x;
        break;
      }
    }
  }
  dbgprint("nextSDfileIndex: return=%d", inx);
  return inx;                                       // return new index
}

//**************************************************************************************************
//                                      G E T S D F I L E N A M E                                  *
//**************************************************************************************************
// Translate the mp3fileIndex of a track to the full filename that can be used as a station.       *
// If index is 0 choose a random track.                                                            *
//**************************************************************************************************
String getSDfilename(int inx)
{
  String        res;                                    // function result
  int           x, rnd;
  //const char*   p = "/";                              // points to directory/file
  mp3node_t     mp3node;

  if (inx == 0) {                                       // random playing ?
    dbgprint("getSDfilename(0) -> random choice");
    rnd = random(SD_mp3fileCount);                      // choose a random index
    int i = 0, ii = 0;
    while (true) {
      if (mp3nodeList[ii].isDirectory == false) i++;
      if (i > rnd) break;
      ii++;
    }
    inx = ii;
  }
  dbgprint("getSDfilename requested index is %d", inx);  // show requeste node ID
  currentIndex = inx;                                    // save current node

  if (inx < 0 || inx > mp3nodeList.size()) {
    dbgprint("getSDfilename returns error: inx=%d > mp3nodeList.size()=%d or inx=-1", inx, mp3nodeList.size());
    return "error";
  }
  mp3node = mp3nodeList[inx];
  if (mp3node.isDirectory == true) {
    dbgprint("getSDfilename error: requested file index %d is a directory", inx);
    return "error";
  }
  // Building file name (including path): we start with the end and work our way upwards
  res = String("/") + mp3node.name;
  while ((x = mp3node.parentDir) != 0) {
    mp3node = mp3nodeList[x];
    res = String("/") + mp3node.name + res;
  }

  res = String("sdcard") + res;
  dbgprint("getSDfilename returns: %s", res.c_str());
  return res;                                             // return full station spec
}

//**************************************************************************************************
//                                      L I S T S D T R A C K S                                    *
//**************************************************************************************************
// Search all MP3 files on directory of SD card.  Return the number of files found.                *
// A "node" of max. SD_MAXDEPTH levels (the subdirectory level) will be generated for every file.  *
// The numbers within the node-array is the sequence number of the file/directory in that          *
// subdirectory.                                                                                   *
// The list will be stored in sdOutbuf if parameter "send" is true. sdOutbuf can later be send     *
// to the webinterface if requested.                                                               *
//**************************************************************************************************
int listsdtracks(const char *dirname, int level = 0, bool send = true, int parentDirNodeId = 0)
{
  static int16_t  fcount, oldfcount;                   // total number of files
  static int16_t  mp3nodeIndex;
  File            root, file;                          // handle to root and directory entry
  String          filename;                            // copy of filename
  String          tmpstr, tmpstr1;
  String          lastDir;
  bool            lastEntryWasDir;
  static bool     delimiterJustAdded = true;
  int16_t         x, mp3nodeIndexThis;
  int             inx;
  mp3node_t       mp3node;

  if (strstr(dirname, "System Volume Information")) {
    // we skip unwanted system directories
    return fcount;
  }

  if (strcmp(dirname, "/") == 0) {                     // are we at root ?
    mp3nodeIndex = 0;
    mp3nodeList.clear();                               // reset node list
    fcount = 0;                                        // reset count
    sdOutbuf = String();                               // reset output buffer
    if (!SD_okay) {                                    // SD card detected & ok ?
      return 0;
    }
  }
  oldfcount = fcount;
  dbgprint("current SD directory is now %s", dirname);
  claimSPI("sdopen2");
  root = SD.open(dirname);                             // open current directory level
  releaseSPI();

  claimSPI("listsdtr");
  if (!root || !root.isDirectory()) {
    dbgprint("%s is not a directory (error: %s)", dirname, !root ? "!root" : "!root.isDirectory()");
    if (root) {
      root.close();
    }
    releaseSPI();
    return fcount;
  }
  releaseSPI();
  mp3node.isDirectory = true;
  mp3node.parentDir = parentDirNodeId;
  mp3node.name = root.name();
  mp3nodeList.push_back(mp3node);
  //dbgprint("mp3node added: inx=%d, isDir=%d, parId=%d, name=%s",
  //           mp3nodeIndex, mp3node.isDirectory, mp3node.parentDir, mp3node.name.c_str());
  mp3nodeIndexThis = mp3nodeIndex++;

  if (send) {
    if (strcmp (dirname, "/") == 0) {                  // root
      //lastDir = String("-1/Rootdirectory ---->\n");
    }
    else {                                             // not root
      lastDir = String("-1") +
                String(dirname) +
                String(" ---->\n");
      for (int i = 5; i < lastDir.length();) {
        if ((inx = lastDir.indexOf("/", i)) >= i) {    // web interface doesn't like "/"
          lastDir.setCharAt (inx, '-');
          i = inx + 1;
        }
        else {
          break;
        }
      }
    }
    lastEntryWasDir = true;
  }
  while (true) {                                       // find all mp3 files
    claimSPI("opennextf");
    file = root.openNextFile();                        // get next file (if any)
    releaseSPI();
    if (!file) {
      break;                                           // end of list
    }
    const char *p = file.name();
    if ((p[0] == '.') ||                               // skip hidden directories
        (p[1] == 'S' && p[2] == 'y' && p[3] == 's')) { // and System Volume Directories 
      claimSPI("close3");
      file.close();
      releaseSPI();
      continue;
    }
    if (file.isDirectory()) {                          // item is directory ?
      if (level < SD_MAXDEPTH - 1 && fcount < SD_MAXFILES) {  // digging deeper ?
        //listsdtracks(file.name(), level + 1, send, mp3nodeIndexThis); // recurse
        // required modification for espressif32 platform >= 4.2.x
        listsdtracks(file.path(), level + 1, send, mp3nodeIndexThis); // recurse
      }
    }
    else {
      if (fcount >= SD_MAXFILES) {
        claimSPI("close4");
        file.close();
        releaseSPI();
        break;
      }
      filename = String(file.name());
      if ((inx = filename.indexOf(".mp3")) > 0 ||
          (inx = filename.indexOf(".MP3")) > 0) {      // neglect non-MP3 files
        mp3node.isDirectory = false;
        mp3node.parentDir = mp3nodeIndexThis;
        mp3node.name = filename;
        mp3nodeList.push_back(mp3node);
        //dbgprint("mp3node added: inx=%d, isDir=%d, parId=%d, name=%s",
        //           mp3nodeIndex, mp3node.isDirectory, mp3node.parentDir, mp3node.name.c_str());
        mp3nodeIndex++;
        fcount++;                                      // count total number of MP3 files
        tmpstr1 = "";

        if (send) {
          if (lastEntryWasDir) {
            if (!delimiterJustAdded) {
              sdOutbuf += String("-1/ \n");            // add spacing in list
              delimiterJustAdded = true;
            }
            sdOutbuf += lastDir;
            lastEntryWasDir = false;
          }
          tmpstr = filename.substring(0, inx);         // extract file name without extension
          tmpstr1 += String(mp3nodeIndex - 1);
          tmpstr1 += "/";
          if (strcmp(dirname, "/") == 0) {
            tmpstr1 += String("Root-");
          }
          tmpstr1 += utf8ascii(tmpstr.c_str());
          tmpstr1 += "\n";
          sdOutbuf += tmpstr1;                         // update file list
                                                       // will fail silently when max alloc heap gets too small !
                                                       // (~17kB, ESP.getMaxAllocHeap() will tell)
          delimiterJustAdded = false;
        }
        dbgprint("Index+File: %d \"%s\"", mp3nodeIndex - 1, file.name());
      }
    }
    if (send) {
      //mp3loop();                                     // commented out, stop playing while indexing SD card
    }
    claimSPI("close5");
    file.close();
    releaseSPI();
  }
  if (send) {
    if (fcount != oldfcount && !delimiterJustAdded) {  // files in this directory?
      sdOutbuf += String("-1/ \n");                    // add spacing in list
      delimiterJustAdded = true;
    }
  }
  claimSPI("close6");
  root.close();
  releaseSPI();
  if (strcmp(dirname, "/") == 0) {                     // are we back at root directory?
    dbgprint("mp3nodeList contains now %d entries", mp3nodeList.size());
    dbgprint("sdOutbuf.length() = %d", sdOutbuf.length());
  }
  //esp_task_wdt_reset();                              // does not help against "task_wdt:  - IDLE0 (CPU 0)"
  delay(1);                                            // this does: allows task switching

  return fcount;                                       // return number of registered MP3 files (so far)
}

//**************************************************************************************************
//                                     G E T E N C R Y P T I O N T Y P E                           *
//**************************************************************************************************
// Read the encryption type of the network and return as a 4 byte name                             *
//**************************************************************************************************
#ifndef USE_ETHERNET
const char* getEncryptionType (wifi_auth_mode_t thisType)
{
  switch (thisType) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA_WPA2_PSK";
    case WIFI_AUTH_MAX:
      return "MAX";
    default:
      break;
  }
  return "????";
}

//**************************************************************************************************
//                                        L I S T N E T W O R K S                                  *
//**************************************************************************************************
// List the available networks.                                                                    *
// Acceptable networks are those who have an entry in the preferences.                             *
// SSIDs of available networks will be saved for use in webinterface.                              *
//**************************************************************************************************
void listNetworks()
{
  WifiInfo_t        winfo;            // Entry from wifilist
  wifi_auth_mode_t  encryption;       // TKIP(WPA), WEP, etc.
  const char       *p;                // Netwerk is acceptable for connection
  int               i, j;             // Loop control

  p = dbgprint("Scan for WiFi Networks");            // Scan for nearby networks
  tftlog(p);
  numSsid = WiFi.scanNetworks();
  dbgprint("Scan completed");
  if (numSsid <= 0) {
    dbgprint("Couldn't get a wifi connection");
    return;
  }
  // print the list of networks seen:
  dbgprint("Number of available networks: %d", numSsid);
  // Print the network number and name for each network found and
  for (i = 0; i < numSsid; i++) {
    p = "";                                          // Assume not acceptable
    for (j = 0; j < wifilist.size(); j++) {          // Search in wifilist
      winfo = wifilist[j];                           // Get one entry
      if (WiFi.SSID(i).indexOf(winfo.ssid) == 0) {   // Is this SSID acceptable?
        p = "Acceptable";
        break;
      }
    }
    encryption = WiFi.encryptionType(i);
    dbgprint("%2d - %-25s Signal: %3d dBm, Channel: %d, Encryption %4s, %s",
               i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
               getEncryptionType(encryption), p);
    // Remember this network for later use
    networks += WiFi.SSID(i) + String("|");
  }
  dbgprint("End of list");
}
#endif  // USE_ETHERNET

//**************************************************************************************************
//                                          T I M E R 5 S E C                                      *
//**************************************************************************************************
// Extra watchdog.  Called every 5 seconds.                                                        *
// If totalCount has not been changed, there is a problem and playing will stop.                   *
// Note that calling timely procedures within this routine or in called functions will             *
// cause a crash!                                                                                  *
//**************************************************************************************************
void IRAM_ATTR timer5sec()
{
  static uint32_t oldtotalCount = 7321;          // needed for change detection
  static uint16_t morethanonce = 0;              // counter for successive fails
  uint32_t bytesplayed;                          // bytes send to MP3 converter

  if (dataMode & (INIT | HEADER | DATA |         // test if playing
                  METADATA | PLAYLISTINIT |
                  PLAYLISTHEADER |
                  PLAYLISTDATA)) {
    if (currentSource == SDCARD && mp3filePause) {
      mbitrate = 0;
      return;
    }
    bytesplayed = totalCount - oldtotalCount;    // number of bytes played in the last 5 seconds
    oldtotalCount = totalCount;                  // save for comparison in next cycle
    if (bytesplayed < 5000) {                    // still properly playing?
      //if (morethanonce > 10) {                 // happened too many times?
      //  ESP.restart();                           // reset the CPU
      //}
      if (dataMode & (PLAYLISTDATA |             // in playlist mode?
                      PLAYLISTINIT |
                      PLAYLISTHEADER)) {
        playlist_num = 0;                        // yes, end of playlist
      }
      goto error_handling;
    }
    else {
      //                                         // data has been sent to MP3 decoder
      // Bitrate in kbits/s is bytesplayed / sec / 1000 * 8
      mbitrate = (bytesplayed + 312) / 625;      // measured bitrate (5s)
      morethanonce = 0;                          // data seen, reset failcounter
#ifdef USE_ETHERNET
      reInitEthernet = false;
#endif
    }
  }
  else if (dataMode & CONNECTERROR) {
error_handling:    
    if (((morethanonce > 5) && (morethanonce < 10)) ||  // happened more than 5x?
          (playlist_num > 0)) {                 // or playlist active?
      dataMode = STOPREQD;                      // stop player
      ini_block.newpreset++;                    // yes, try next channel
      if (ini_block.newpreset > highestPreset)
        ini_block.newpreset = 0;
    }
    else if (morethanonce <= 5 && currentSource == STATION) {
      dataMode = STOPREQD;                      // stop player
      currentPreset = -1;                       // attempt to reconnect to same station
#ifdef USE_ETHERNET
      if (morethanonce == 0 || morethanonce == 2) // re-init ethernet chip might help 
        reInitEthernet = true;
#endif
    }
    morethanonce++;                            // count the fails
  }
}

//**************************************************************************************************
//                                          T I M E R 1 0 0                                        *
//**************************************************************************************************
// Called every 100 msec on interrupt level, so must be in IRAM and no lengthy operations          *
// allowed.                                                                                        *
//**************************************************************************************************
void IRAM_ATTR timer100()
{
  sv int16_t count5sec = 0;                      // counter for activating 5 seconds process
  sv int16_t eqcount = 0;
  sv int16_t oldclickcount = 0;                  // for detecting difference

  if (++count5sec == 50 ) {                      // 5 seconds passed?
    timer5sec();                                 // do 5 second procedure
    count5sec = 0;                               // reset count
  }
  if ((count5sec % 10) == 0) {                   // one second over?
    if (++timeinfo.tm_sec >= 60) {               // update number of seconds
      timeinfo.tm_sec = 0;                       // wrap after 60 seconds
      time_req = true;                           // show current time request
      if (++timeinfo.tm_min >= 60) {
        timeinfo.tm_min = 0;                     // wrap after 60 minutes
        if (++timeinfo.tm_hour >= 24) {
          timeinfo.tm_hour = 0;                  // wrap after 24 hours
        }
      }
    }
  }
  // handle rotary encoder, inactivity counter will be reset by encoder interrupt
  if (++enc_inactivity == 36000) {               // count inactivity time
    enc_inactivity = 1000;                       // prevent wrap
  }
  // clear rotation counter after some inactivity
  if (enc_inactivity > 15 && locrotcount) {
    locrotcount = 0;
  }
  // now detection of single/double click of rotary encoder switch
  if (clickcount) {                              // any click?
    if (oldclickcount == clickcount) {           // stable situation?
      if (++eqcount == 5) {                      // long time stable?
        eqcount = 0;
        if (clickcount > 2) {                    // triple click?
          //tripleclick = true;
        }
        else if (clickcount > 1) {               // double click?
          doubleClick = true;
        }
        else {
          singleClick = true;
        }
        clickcount = 0;
      }
    }
    else {
      oldclickcount = clickcount;                // for detecting change
      eqcount = 0;                               // not stable yet, reset count
    }
  }
}

#ifdef ENABLE_INFRARED
//**************************************************************************************************
//                                          I S R _ I R                                            *
//**************************************************************************************************
// Interrupts received from VS1838B on every change of the signal.                                 *
// Intervals are 640 or 1640 microseconds for data,  sync pulses are 3400 micros or longer.        *
// Input is complete after 65 level changes.                                                       *
// Only the last 32 level changes are significant and will be handed over to common data.          *
// Important: This is only valid for a certain China 21-button remote control model from Ali.      *
//**************************************************************************************************
void IRAM_ATTR isr_IR()
{
  sv uint32_t t0 = 0;                               // To get the interval
  sv uint32_t ir_locvalue = 0;                      // IR code
  sv int      ir_loccount = 0;                      // Length of code
  uint32_t    t1, intval;                           // Current time and interval since last change
  uint32_t    mask_in = 2;                          // Mask input for conversion
  uint16_t    mask_out = 1;                         // Mask output for conversion

  t1 = micros();                                    // Get current time
  intval = t1 - t0;                                 // Compute interval
  t0 = t1;                                          // Save for next compare
  if ((intval > 300) && (intval < 800)) {           // Short pulse?
    ir_locvalue = ir_locvalue << 1;                 // Shift in a "zero" bit
    ir_loccount++;                                  // Count number of received bits
    ir_0 = (ir_0 * 3 + intval) / 4;                 // Compute average durartion of a short pulse
  }
  else if ((intval > 1400) && (intval < 1900)) {    // Long pulse?
    ir_locvalue = (ir_locvalue << 1) + 1;           // Shift in a "one" bit
    ir_loccount++;                                  // Count number of received bits
    ir_1 = (ir_1 * 3 + intval) / 4;                 // Compute average durartion of a short pulse
  }
  else if (ir_loccount == 65) {                     // Value is correct after 65 level changes
    while (mask_in) {                               // Convert 32 bits to 16 bits
      if (ir_locvalue & mask_in) {                  // Bit set in pattern?
        ir_value |= mask_out;                       // Set bit in result
      }
      mask_in <<= 2;                                // Shift input mask 2 positions
      mask_out <<= 1;                               // Shift output mask 1 position
    }
    ir_loccount = 0;                                // Ready for next input
  }
  else {
    ir_locvalue = 0;                                // Reset decoding
    ir_loccount = 0;
  }
}
#endif

//**************************************************************************************************
//                                          I S R _ E N C _ S W I T C H                            *
//**************************************************************************************************
// Interrupts received from rotary encoder switch.                                                 *
//**************************************************************************************************
void IRAM_ATTR isr_enc_switch()
{
  sv uint32_t     oldtime = 0;                            // time in millis previous interrupt
  sv bool         sw_state;                               // true is pushed (LOW)
  bool            newstate;                               // current state of input signal
  uint32_t        newtime;                                // current timestamp

  // Read current state of SW pin
  newstate = (digitalRead (ini_block.enc_sw_pin) == LOW);
  newtime = millis();
  if (newtime < (oldtime + 10)) {                         // debounce
    return;
  }
  if (newstate != sw_state) {                             // state changed ?
    sw_state = newstate;                                  // yes, set current (new) state
    if (!sw_state) {                                      // switch released ?
      if ((newtime - oldtime) > 3000) {                   // more than 1 second ?
        longClick = true;                                 // yes, register longClick
      }
      else {
        clickcount++;                                     // yes, click detected
      }
      enc_inactivity = 0;                                 // not inactive anymore
    }
  }
  oldtime = newtime;                                      // nor next compare
}

//**************************************************************************************************
//                                          I S R _ E N C _ T U R N                                *
//**************************************************************************************************
// Interrupts received from rotary encoder (clk signal), debouncing is all done in hardware!       *
//**************************************************************************************************
void IRAM_ATTR isr_enc_turn()
{
  if (digitalRead(ini_block.enc_clk_pin) == digitalRead(ini_block.enc_dt_pin)) {
    rotationcount++;
  }
  else {
    rotationcount--;
  }
  enc_inactivity = 0;
}


//**************************************************************************************************
//                                S H O W S T R E A M T I T L E                                    *
//**************************************************************************************************
// Show artist and songtitle if present in metadata.                                               *
// Show always if full=true.                                                                       *
//**************************************************************************************************
void showStreamTitle(const char *ml, bool full)
{
  char* p1;
  char* p2;
  char  streamtitle[150];                      // streamtitle from metadata

  if (strstr(ml, "StreamTitle=")) {
    dbgprint(ml);
    p1 = (char*)ml + 12;                       // begin of artist and title
    if ((p2 = strstr(ml, "';"))) {             // search for end of title
      if (*p1 == '\'') {                       // surrounded by quotes?
        p1++;
        //p2--;
      }
      *p2 = '\0';                              // strip the rest of the line
    }
    // save last part of string as streamtitle.  Protect against buffer overflow
    strncpy(streamtitle, p1, sizeof (streamtitle));
    streamtitle[sizeof (streamtitle) - 1] = '\0';
  }
  else if (full) {
    // info probably from playlist or mp3 file
    strncpy(streamtitle, ml, sizeof (streamtitle));
    streamtitle[sizeof (streamtitle) - 1] = '\0';
  }
  else {
    icystreamtitle = "";                       // Unknown type
    return;                                    // Do not show
  }
  utf8ascii(streamtitle);
  if (currentSource == STATION && *streamtitle == '\0')
    strcpy(streamtitle, "...waiting for text...");
  // save for status request from browser
  icystreamtitle = streamtitle;
  if (!full) {
    if ((p1 = strstr(streamtitle, " - ")) ||
         (p1 = strstr(streamtitle, " / "))) {  // look for artist/title separator
      *p1++ = '\n';                            // found: replace 3 characters by newline
      p2 = p1 + 2;
      if (*p2 == ' ') {                        // leading space in title?
        p2++;
      }                                        // shift 2nd part of title 2 or 3 places
      strcpy (p1, p2);                         // before: ".. - ..", after: "..\n.."
    }
    // skip [xxx] if exists (comes only with some radio stations, e.g. 1FM)
    if ((p1 = strstr(streamtitle, "[")) &&
         (p2 = strstr(streamtitle, "]")) &&
         p1 < p2) {
      *p1 = '\0';
      if (*(p1 - 1) == ' ') *(p1 - 1) = '\0';
    }
  }
  if (encoderMode != SELECT) {                 // don't disturb selecting SD tracks or stations
    tftset(1, streamtitle);                    // set screen segment text middle part
  }
  lastArtistSong = streamtitle;
}

//**************************************************************************************************
//                                    S T O P _ M P 3 C L I E N T                                  *
//**************************************************************************************************
// Disconnect from radio station.                                                                  *
//**************************************************************************************************
void stopMp3client()
{
P1SMP3:  
  _claimSPI("stpmp3cl1");                               // claim SPI bus
  uint8_t erg = mp3client.connected();
  _releaseSPI();                                        // release SPI bus
  while (erg) {
    dbgprint("stopMp3client(): mp3client still connected. Try to disconnect.");  // Stop connection to host
    _claimSPI("stpmp3cl2");                             // claim SPI bus
    //mp3client.flush();                                // hangs quite often !
    mp3client.stop();
    _releaseSPI();                                      // release SPI bus
    delay(300);
    goto P1SMP3;
  }
  delay(50);
  _claimSPI("stpmp3cl3");                               // claim SPI bus
  //mp3client.flush();                                  // hangs quite often !
  mp3client.stop();                                     // Stop stream client
  _releaseSPI();                                        // release SPI bus
  delay(100);
}

//**************************************************************************************************
//                                    C O N N E C T T O H O S T                                    *
//**************************************************************************************************
// Connect to the Internet radio server specified by newpreset.                                    *
//**************************************************************************************************
bool connectToHost()
{
  int      inx;                                    // Position of ":" in hostname
  uint16_t port = 80;                              // Port number for host
  char     buf[100];                               // temp buffer
  String   extension = "/";                        // May be like "/mp3" in "skonto.ls.lv:8002/mp3"
  String   hostwoext = host;                       // Host without extension and portnumber
  String   tmp;

  stopMp3client();                                 // Disconnect if still connected
  dbgprint("Connect to host %s", host.c_str());
  tftset(0, "ESP32 Radio");                        // Set screen segment text top line
  displayTime("");                                 // Clear time on TFT screen
  dataMode = INIT;                                 // can be changed further down
  chunked = false;                                 // Assume not chunked
  if (host.endsWith(".m3u")) {                     // Is it an m3u playlist?
    playlist = host;                               // Save copy of playlist URL
    dataMode = PLAYLISTINIT;                       // Yes, start in PLAYLIST mode
    if (playlist_num == 0) {                       // First entry to play?
      playlist_num = 1;                            // Yes, set index
    }
    dbgprint("Playlist request, entry %d", playlist_num);
  }
  // In the URL there may be an extension, like noisefm.ru:8000/play.m3u&t=.m3u
  inx = host.indexOf("/");                         // Search for begin of extension
  if (inx > 0) {                                   // Is there an extension?
    extension = host.substring(inx);               // Yes, change the default
    hostwoext = host.substring(0, inx);            // Host without extension
  }
  // In the host there may be a portnumber
  inx = hostwoext.indexOf(":");                    // Search for separator
  if (inx >= 0) {                                  // Portnumber available?
    port = host.substring(inx + 1).toInt();        // Get portnumber as integer
    hostwoext = host.substring(0, inx);            // Host without portnumber
  }
  dbgprint("Server %s, port %d, extension %s",
             hostwoext.c_str(), port, extension.c_str());
  _claimSPI("connecttohost1");                     // claim SPI bus
  int16_t erg = mp3client.connect(hostwoext.c_str(), port);
  _releaseSPI();                                   // release SPI bus

  if (erg == 1) {
    dbgprint("Successfully connected to server");
    // send request to server and request metadata.
    _claimSPI("connecttohost2");                   // claim SPI bus
    mp3client.print(String("GET ") +
                    extension +
                    String(" HTTP/1.1\r\n") +
                    String("Host: ") +
                    hostwoext +
                    String("\r\n") +
                    String("Icy-MetaData:1\r\n") +
                    String("Connection: close\r\n\r\n"));
    _releaseSPI();                                // release SPI bus
    return true;
  }

  // error connecting to host
  dbgprint("Request %s failed!", host.c_str());
#ifdef USE_ETHERNET
  _claimSPI("connecttohost3");                    // claim SPI bus
  lstat = Ethernet.linkStatus();                  // Get physical ethernet link status
  _releaseSPI();                                  // release SPI bus
  if (lstat == LinkOFF) dbgprint("Ethernet link status: down !!");
#endif
  tmp = host;
  if (tmp.length() > 65) {                        // line limiting
    tmp.remove(63);
    tmp += "...";
  }
#ifdef USE_ETHERNET
  if (lstat == LinkOFF) {                         // if ethernet link is down than show it
    if (tmp.length() > 55) {
      tmp.remove(53);
      tmp += "...";
    }
    tmp += " [Eth Link down!]";
  }
#endif
  sprintf(buf, "Can't connect to: %s", tmp.c_str());
  tftset(1, buf);
  tftset(3, " ");
  icyname = "Couldn't connect to host";
  icystreamtitle = host;
  dataMode = CONNECTERROR;
  return false;
}

//**************************************************************************************************
//                                      S S C O N V                                                *
//**************************************************************************************************
// Convert an array with 4 "synchsafe integers" to a number.                                       *
// There are 7 bits used per byte.                                                                 *
//**************************************************************************************************
uint32_t ssconv(const uint8_t* bytes)
{
  uint32_t res = 0;                                      // Result of conversion
  uint8_t  i;                                            // Counter number of bytes to convert

  for (i = 0; i < 4; i++) {                              // Handle 4 bytes
    res = res * 128 + bytes[i];                          // Convert next 7 bits
  }
  return res;                                            // Return the result
}

//**************************************************************************************************
//                                  Q U I C K S D C H E C K                                        *
//**************************************************************************************************
// If we have files in our list try to open the first one. If not successful clear SD_okay.        *
// Don't check if mp3-file is currently open.                                                      *
//**************************************************************************************************
void quickSdCheck()
{
  uint8_t c;
  int index = 0;
  
  if (SD_okay && SD_mp3fileCount > 0 && !mp3file) {
    for (int i=1; i < mp3nodeList.size(); i++) {        // 0 is root index, so we start with 1
      if (!mp3nodeList[i].isDirectory) {
        index = i;
        break;
      }
    }
    if (!index) return;                                 // no file found...weird...
    String path = getSDfilename(index);                 // returns path with "sdcard" in front
    path = path.substring(6);                           // we need path, so skip the "sdcard" part
    claimSPI("qusdchk1");                               // claim SPI bus
    mp3file = SD.open(path);                            // Open the file
    releaseSPI();
    if (!mp3file) {
      SD_okay = false;
      dbgprint("quickSdCheck: error SD.open(%s) -> SD_okay = false", path.c_str());
      return;
    }
    claimSPI("qusdchk2");                               // claim SPI bus
    int i = mp3file.read(&c, 1);                        // read only 1 byte
    mp3file.close();
    releaseSPI();
    if (i != 1) {
      SD_okay = false;
      dbgprint("quickSdCheck: error mp3file.read(1) -> SD_okay = false");
      return;
    }
    dbgprint("quickSdCheck: All ok.");
  }
}

//**************************************************************************************************
//                                  H A N D L E I D 3                                              *
//**************************************************************************************************
// Check file on SD card for ID3 tags and use them to display some info.                           *
// Extended headers are not parsed. Parameter must be "soap/....." or "sdcard/......."             *
//**************************************************************************************************
bool handleID3 (String& path)
{
  const char*  p;                                          // Pointer to filename
  struct ID3head_t                                         // First part of ID3 info
  {
    char    fid[3];                                        // Should be filled with "ID3"
    uint8_t majV, minV;                                    // Major and minor version
    uint8_t hflags;                                        // Headerflags
    uint8_t ttagsize[4];                                   // Total tag size
  } ID3head;
  uint8_t  exthsiz[4];                                     // Extended header size
  uint32_t stx;                                            // Ext header size converted
  uint32_t sttg;                                           // Total tagsize converted
  uint32_t stg;                                            // Size of a single tag
  struct ID3tag_t                                           // Tag in ID3 info
  {
    char    tagid[4];                                      // Things like "TCON", "TYER", ...
    uint8_t tagsize[4];                                    // Size of the tag
    uint8_t tagflags[2];                                   // Tag flags
  } ID3tag;
  uint8_t  tmpbuf[4];                                      // Scratch buffer
  uint8_t  tenc;                                           // Text encoding
  String   artttl;                                         // Artist and title
  enum_selection source = (path.indexOf("sdcard") == 0) ? SDCARD : MEDIASERVER;

  if (source == SDCARD && !SD_okay) {
    dbgprint("handleID3: SD_okay is false");
    return false;
  }
  if (encoderMode != SELECT) {                           // Don't disturb selecting SD tracks
    // just for the case we won't find an ID3 tag
    if (source == SDCARD) {
      tftset(3, "Playing from local file");
      lastAlbumStation = "Playing from local file";
    }
    else {
      lastAlbumStation = "Playing from media server";      // album info can be missing
    }  
  }  
  dbgprint("handleID3: Looking for ID3 tags in %s", 
    (char*)path/*substring((source==SDCARD)?6:4)*/.c_str());
  p = path.substring(path.lastIndexOf("/") + 1).c_str();   // Point just to filename
  showStreamTitle(p, true);                                // filename as title (lastArtistSong), but will 
                                                           // be overridden if mp3 tags found 
  if (source == SDCARD) {                                                           
    claimSPI("id30");
    mp3file = SD.open(path.substring(6));                  // Open the file
    releaseSPI();
    if (!mp3file) {
      SD_okay = false;
      currentIndex = -1;
      dbgprint("handleID3: error SD.open()");
      return false;
    }
    // scan ID3 tag if found
    claimSPI("id31");
    mp3file.read((uint8_t*)&ID3head, sizeof(ID3head));     // Read first part of ID3 info
    releaseSPI();
    if (strncmp(ID3head.fid, "ID3", 3) == 0) {
      // file contains ID3 tag
      sttg = ssconv(ID3head.ttagsize);                       // Convert tagsize
      dbgprint("Found ID3 info, total tagsize = %d", sttg);
      if (ID3head.hflags & 0x40) {                           // Extended header?
        claimSPI("id32");
        mp3file.read((uint8_t*)exthsiz, 4);
        releaseSPI();
        stx = ssconv(exthsiz);                               // Yes, get size of extended header
        stx -= 4;
        while (stx--) {
          claimSPI("id32");
          mp3file.read();                                    // Skip next byte of extended header
          releaseSPI();
        }
      }
      while (sttg > 10) {                                    // Now handle the tags
        claimSPI("id33");
        sttg -= mp3file.read((uint8_t*)&ID3tag,
                            sizeof(ID3tag));                // Read first part of a tag
        releaseSPI();
        if (ID3tag.tagid[0] == 0) {                          // Reached the end of the list?
          break;                                             // Yes, quit the loop
        }
        stg = ssconv(ID3tag.tagsize);                        // Convert size of tag
        claimSPI("id34");
        if (ID3tag.tagflags[1] & 0x08) {                     // Compressed?
          sttg -= mp3file.read(tmpbuf, 4);                   // Yes, ignore 4 bytes
          stg -= 4;                                          // Reduce tag size
        }
        if (ID3tag.tagflags[1] & 0x044) {                    // Encrypted or grouped?
          sttg -= mp3file.read(tmpbuf, 1);                   // Yes, ignore 1 byte
          stg--;                                             // Reduce tagsize by 1
        }
        releaseSPI();
        if (stg > (sizeof(metalinebf) + 2)) {                // Room for tag?
          break;                                             // No, skip this and further tags
        }
        claimSPI("id36");                                    // claim SPI bus
        sttg -= mp3file.read((uint8_t*)metalinebf, stg);     // Read tag contents
        releaseSPI();
        metalinebf[stg] = '\0';                              // Add delimeter
        tenc = metalinebf[0];                                // First byte is encoding type
        if (tenc == 0x01 || tenc == 0x02) {                  // UTF-16 string (Little Endian or Big Endian)
          utf16ascii(metalinebf + 1, stg - 1);
        }
        if ((strncmp(ID3tag.tagid, "TPE1", 4) == 0) ||       // artist
            (strncmp(ID3tag.tagid, "TIT2", 4) == 0)) {       // song title
          dbgprint("ID3 %s = %s", ID3tag.tagid, metalinebf + 1);
          artttl += String(metalinebf + 1);                  // add to string
          artttl += String("\n");                            // add newline
        }
        if (strncmp(ID3tag.tagid, "TALB", 4) == 0) {         // album
          dbgprint("ID3 %s = %s", ID3tag.tagid, metalinebf + 1);
          lastAlbumStation = metalinebf + 1;
        }
      }
    }
  }
#ifdef ENABLE_SOAP  
  else {
    // no need to scan for ID3 tags, we get it from mediaserver
    if (soapList[currentIndex].artist.length()) {            // artist can be missing
      artttl = soapList[currentIndex].artist;
      artttl += String("\n");                                // add newline
      dbgprint("SOAP TPE1 = %s", soapList[currentIndex].artist.c_str());
    }
    artttl += soapList[currentIndex].name;
    dbgprint("SOAP TIT2 = %s", soapList[currentIndex].name.c_str());
    if (soapList[currentIndex].album.length()) {             // album can be missing
      lastAlbumStation = soapList[currentIndex].album;
      dbgprint("SOAP TALB = %s", soapList[currentIndex].album.c_str());
    }
  }
#endif  

  if (artttl.length()) lastArtistSong = artttl;
  if (encoderMode != SELECT) {                               // don't disturb selecting SD tracks
    tftset(1, lastArtistSong);                               // Show artist and title
    tftset(3, lastAlbumStation  );                           // show album
  }    
  return true;
}

//**************************************************************************************************
//                                       C O N N E C T T O F I L E                                 *
//**************************************************************************************************
// Open the local mp3-file.                                                                        *
//**************************************************************************************************
bool connecttofile()
{
  tftset(0, "ESP32 MP3Player");                          // set screen segment top line
  displayTime("");                                       // Clear time on TFT screen
  if (mp3file) {                                         // close old mp3 file if still open
    dbgprint("connecttofile: close mp3file");
    claimSPI("close7");                                  // claim SPI bus
    mp3file.close();                                     // Close file
    releaseSPI();                                        // release SPI bus
  }
  // See if there are ID3 tags in this file
  if (!handleID3(host)) {
    dbgprint("connecttofile: Error reading file %s", host.substring(6).c_str());  // No luck
    return false;
  }
  claimSPI("sdavail5");                                  // claim SPI bus
  mp3file.seek(0);
  mp3fileLength = mp3fileBytesLeft = mp3file.available();  // Get file length
  releaseSPI();                                          // release SPI bus
  icyname = "";                                          // No icy name yet
  chunked = false;                                       // File not chunked
  metaint = 0;                                           // No metadata

  return true;
}

//**************************************************************************************************
//                                       C O N N E C T W I F I                                     *
//**************************************************************************************************
// Connect to WiFi using the SSID's available in wifiMulti.                                        *
// If only one AP if found in preferences (i.e. wifi_00) the connection is made without            *
// using wifiMulti.                                                                                *
// If connection fails the function returns false.                                                 *
//**************************************************************************************************
#ifndef USE_ETHERNET
bool connectwifi()
{
  char*      pfs;                                        // Pointer to formatted string
  bool       res = false;                                // Result

  WifiInfo_t winfo;                                      // Entry from wifilist

  for (int ii = 0; ii < 2; ii++) {
    WiFi.disconnect();                                   // After restart the router could
    WiFi.softAPdisconnect(true);                         // still keep the old connection
    if (wifilist.size()) {                               // Any AP defined?
      if (wifilist.size() == 1) {                        // Just one AP defined in preferences?
        winfo = wifilist[0];                             // Get this entry
        WiFi.begin(winfo.ssid, winfo.passphrase);        // Connect to single SSID found in wifi_xx
        dbgprint("Try WiFi %s", winfo.ssid);             // Message to show during WiFi connect
      }
      else {                                             // More AP to try
        wifiMulti.run();                                 // Connect to best network
      }
      if (WiFi.waitForConnectResult() != WL_CONNECTED) { // Try to connect
        dbgprint("WiFi Failed! Can't connect. Attempt %d.", ii + 1);
      }
      else {
        pfs = dbgprint("Connected to %s", WiFi.SSID().c_str());
        tftlog(pfs);
        res = true;                                      // Return result of connection
        break;
      }
    }
    else {
      dbgprint("WiFi Failed! No SSID/Password defined.");
      break;
    }
  }
  return res;
}

//**************************************************************************************************
//                                           O T A S T A R T                                       *
//**************************************************************************************************
// Update via WiFi has been started by Arduino IDE.                                                *
//**************************************************************************************************
void otastart()
{
  char* p;

  p = dbgprint("OTA update Started");
  tftset(3, p);                                   // Set screen segment bottom part
}
#endif  // USE_ETHERNET

//**************************************************************************************************
//                                  R E A D H O S T F R O M P R E F                                *
//**************************************************************************************************
// Read the mp3 host from the preferences specified by the parameter.                              *
// The host will be returned.                                                                      *
//**************************************************************************************************
String readhostfrompref(int8_t preset)
{
  char tkey[12];                                   // Key as an array of chars

  sprintf(tkey, "preset_%02d", preset);            // Form the search key
  if (nvsSearch(tkey)) {                           // Does it exists?
    // Get the contents
    return nvsgetstr(tkey);                        // Get the station (or empty string)
  }
  else {
    return String("");                             // Not found
  }
}

//**************************************************************************************************
//                                  R E A D H O S T F R O M P R E F                                *
//**************************************************************************************************
// Search for the next mp3 host in preferences specified newpreset.                                *
// The host will be returned.  newpreset will be updated                                           *
//**************************************************************************************************
String readhostfrompref()
{
  String contents = "";                                // Result of search
  int    maxtry = 0;                                   // Limit number of tries

  while ((contents = readhostfrompref (ini_block.newpreset)) == "") {
    if (++maxtry > 99) {
      return "";
    }
    if (++ini_block.newpreset > 99) {                  // Next or wrap to 0
      ini_block.newpreset = 0;
    }
  }
  // Get the contents
  return contents;                                     // return the station
}

//**************************************************************************************************
//                             F I N D H I G H E S T P R E S E T                                   *
//**************************************************************************************************
// Search for the highest preset_xx number                                                         *
//**************************************************************************************************
uint8_t findHighestPreset()
{
  uint8_t erg = 0, i = 0;

  while (true) {
    if (readhostfrompref (i) != "") {
      erg = i++;
    }
    else {
      break;
    }
  }
  dbgprint("findHighestPreset(): erg = %d", erg);
  return erg;
}

//**************************************************************************************************
//                                       R E A D P R O G B U T T O N S                             *
//**************************************************************************************************
// Read the preferences for the programmable input pins and the touch pins.                        *
//**************************************************************************************************
#ifdef ENABLE_DIGITAL_INPUTS
void readProgButtons()
{
  char   mykey[20];                                      // For numerated key
  int8_t pinnr;                                          // GPIO pinnumber to fill
  int    i;                                              // Loop control
  String val;                                            // Contents of preference entry

  for (i = 0; (pinnr = progpin[i].gpio) >= 0; i++) {     // Scan for all programmable pins
    sprintf(mykey, "gpio_%02d", pinnr);                 // Form key in preferences
    if (nvsSearch(mykey)) {
      val = nvsgetstr(mykey);                            // Get the contents
      if (val.length()) {                                // Does it exists?
        if (!progpin[i].reserved) {                      // Do not use reserved pins
          progpin[i].avail = true;                       // This one is active now
          progpin[i].command = val;                      // Set command
          dbgprint("gpio_%02d will execute %s",          // Show result
                     pinnr, val.c_str());
        }
      }
    }
  }
  // Now for the touch pins 0..9, identified by their GPIO pin number
  for (i = 0; (pinnr = touchpin[i].gpio) >= 0; i++) {    // Scan for all programmable pins
    sprintf(mykey, "touch_%02d", pinnr);                 // Form key in preferences
    if (nvsSearch(mykey)) {
      val = nvsgetstr(mykey);                            // Get the contents
      if (val.length()) {                                // Does it exists?
        if (!touchpin[i].reserved) {                     // Do not use reserved pins
          touchpin[i].avail = true;                      // This one is active now
          touchpin[i].command = val;                     // Set command
          //pinMode (touchpin[i].gpio,  INPUT);          // Free floating input
          dbgprint("touch_%02d will execute %s",         // Show result
                   pinnr, val.c_str());
        }
        else {
          dbgprint("touch_%02d pin (GPIO%02d) is reserved for I/O!", pinnr, pinnr);
        }
      }
    }
  }
}
#endif

//**************************************************************************************************
//                                       R E S E R V E P I N                                       *
//**************************************************************************************************
// Set I/O pin to "reserved".                                                                      *
// The pin is than not available for a programmable function.                                      *
//**************************************************************************************************
#ifdef ENABLE_DIGITAL_INPUTS
void reservepin(int8_t rpinnr)
{
  uint8_t i = 0;                                           // Index in progpin/touchpin array
  int8_t  pin;                                             // Pin number in progpin array

  while ((pin = progpin[i].gpio) >= 0) {                   // Find entry for requested pin
    if (pin == rpinnr) {                                   // Entry found?
      //dbgprint("GPIO%02d unavailabe for 'gpio_'-command", pin);
      progpin[i].reserved = true;                          // Yes, pin is reserved now
      break;                                               // No need to continue
    }
    i++;                                                   // Next entry
  }
  // Also reserve touchpin numbers
  i = 0;
  while ((pin = touchpin[i].gpio) >= 0) {                  // Find entry for requested pin
    if (pin == rpinnr) {                                   // Entry found?
      //dbgprint("GPIO%02d unavailabe for 'touch'-command", pin);
      touchpin[i].reserved = true;                         // Yes, pin is reserved now
      break;                                               // No need to continue
    }
    i++;                                                   // Next entry
  }
}
#endif

//**************************************************************************************************
//                                       R E A D I O P R E F S                                     *
//**************************************************************************************************
// Scan the preferences for IO-pin definitions.                                                    *
//**************************************************************************************************
void readIOprefs()
{
  struct iosetting
  {
    const char* gname;                                   // Name in preferences
    int8_t*     gnr;                                     // GPIO pin number
    int8_t      pdefault;                                // Default pin
  };
  struct iosetting klist[] = {                           // List of I/O related keys
#ifdef ENABLE_INFRARED
    { "pin_ir",       &ini_block.ir_pin,          35 },  // use 35 if needed
#endif
    { "pin_enc_clk",  &ini_block.enc_clk_pin,     25 },
    { "pin_enc_dt",   &ini_block.enc_dt_pin,      26 },
    { "pin_enc_sw",   &ini_block.enc_sw_pin,      27 },
    { "pin_tft_cs",   &ini_block.tft_cs_pin,      15 },  // Display SPI version
    { "pin_tft_dc",   &ini_block.tft_dc_pin,       2 },  // Display SPI version
    //{ "pin_tft_scl",  &ini_block.tft_scl_pin,     -1 },  // Display I2C version
    //{ "pin_tft_sda",  &ini_block.tft_sda_pin,     -1 },  // Display I2C version
    { "pin_sd_cs",    &ini_block.sd_cs_pin,       17 },
    { "pin_vs_cs",    &ini_block.vs_cs_pin,        5 },
    { "pin_vs_dcs",   &ini_block.vs_dcs_pin,      16 },
    { "pin_vs_dreq",  &ini_block.vs_dreq_pin,      4 },
    { "pin_shutdown", &ini_block.vs_shutdown_pin, -1 },
    { "pin_spi_sck",  &ini_block.spi_sck_pin,     18 },
    { "pin_spi_miso", &ini_block.spi_miso_pin,    19 },
    { "pin_spi_mosi", &ini_block.spi_mosi_pin,    23 },
    { NULL,           NULL,                        0 }   // End of list
  };
  int         i;                                         // Loop control
  int         count = 0;                                 // Number of keys found
  String      val;                                       // Contents of preference entry
  int8_t      ival;                                      // Value converted to integer
  int8_t*     p;                                         // Points to variable

  for (i = 0; klist[i].gname; i++) {                     // Loop trough all I/O related keys
    p = klist[i].gnr;                                    // Point to target variable
    ival = klist[i].pdefault;                            // Assume pin number to be the default
    if (nvsSearch (klist[i].gname)) {                    // Does it exist?
      val = nvsgetstr(klist[i].gname);                   // Read value of key
      if (val.length()) {                                // Parameter in preference?
        count++;                                         // Yes, count number of filled keys
        ival = val.toInt();                              // Convert value to integer pinnumber
      }
    }
#ifdef ENABLE_DIGITAL_INPUTS        
    reservepin(ival);                                    // Set pin to "reserved"
#endif        
    *p = ival;                                           // Set pinnumber in ini_block
    dbgprint("%s set to %d", klist[i].gname, ival);      // Show result
  }
}

//**************************************************************************************************
//                                       R E A D P R E F S                                         *
//**************************************************************************************************
// Read the preferences and interpret the commands.                                                *
// If output == true, the key / value pairs are returned to the caller as a String.                *
//**************************************************************************************************
String readPrefs(bool output)
{
  uint16_t    i;                                           // Loop control
  String      val;                                         // Contents of preference entry
  String      cmd;                                         // Command for analyzCmd
  String      outstr = "";                                 // Outputstring
  char*       key;                                         // Point to nvskeys[i]
  uint8_t     winx;                                        // Index in wifilist
  uint16_t    last2char = 0;                               // To detect paragraphs

  i = 0;
  while (*(key = nvskeys[i])) {                            // Loop trough all available keys
    val = nvsgetstr(key);                                  // Read value of this key
    cmd = String(key) +                                    // Yes, form command
          String(" = ") +
          val;
    if (strstr(key, "wifi_")) {                            // Is it a wifi ssid/password?
      if ((winx = val.indexOf("/")) > 0) {
        val.remove(winx + 1);
        val +=  String("*******");                         // Yes, hide password
      }
      cmd = String("");                                    // Do not analyze this
    }
    if (output) {
      if ((i > 0) &&
          (*(uint16_t*)key != last2char)) {                // New paragraph?
        outstr += String("#\n");                           // Yes, add separator
      }
      last2char = *(uint16_t*)key;                         // Save 2 chars for next compare
      outstr += String(key) +                              // Add to outstr
                String(" = ") +
                val +
                String("\n");                              // Add newline
    }
    else {
      analyzeCmd(cmd.c_str());                             // Analyze it
    }
    i++;                                                   // Next key
  }
  if (i == 0) {
    outstr = String("No preferences found.\n"
                    "Use defaults or run Esp32_radio_init first.\n");
  }
  return outstr;
}

//**************************************************************************************************
//                                     S C A N S E R I A L                                         *
//**************************************************************************************************
// Listen to commands on the Serial inputline.                                                     *
//**************************************************************************************************
void scanserial()
{
  static String serialcmd;                        // Command from Serial input
  //char          c;                              // Input character
  //const char*   reply;                          // Reply string froma analyzeCmd
  //uint16_t      len;                            // Length of input string

  while (Serial.available()) {                    // Any input seen?
    /*c =  (char)*/Serial.read();                 // Yes, read the next input character
    // TEST
    //Serial.write(c);                            // Echo
    /* 
      len = serialcmd.length();                   // Get the length of the current string
      if ((c == '\n') || (c == '\r')) {
        if (len) {
          strncpy(cmd, serialcmd.c_str(), sizeof(cmd));
          reply = analyzeCmd(cmd);                // Analyze command and handle it
          dbgprint(reply);                        // Result for debugging
          serialcmd = "";                         // Prepare for new command
        }
      }
      if (c >= ' ') {                             // Only accept useful characters
        serialcmd += c;                           // Add to the command
      }
      if (len >= (sizeof(cmd) - 2)) {             // Check for excessive length
        serialcmd = "";                           // Too long, reset
      }
    */
  }
}

//**************************************************************************************************
//                                     S C A N D I G I T A L                                       *
//**************************************************************************************************
// Scan digital inputs.                                                                            *
//**************************************************************************************************
#ifdef ENABLE_DIGITAL_INPUTS
void  scandigital()
{
  static uint32_t oldmillis = 5000;                        // To compare with current time
  int             i;                                       // Loop control
  int8_t          pinnr;                                   // Pin number to check
  bool            level;                                   // Input level
  const char*     reply;                                   // Result of analyzeCmd
  int16_t         tlevel;                                  // Level found by touch pin
  const int16_t   THRESHOLD = 30;                          // Threshold or touch pins

  if ((millis() - oldmillis) < 100) {                      // Debounce
    return;
  }
  oldmillis = millis();                                    // 100 msec over
  for (i = 0; (pinnr = progpin[i].gpio) >= 0; i++) {       // Scan all inputs
    if (!progpin[i].avail || progpin[i].reserved) {        // Skip unused and reserved pins
      continue;
    }
    level = (digitalRead(pinnr) == HIGH);                  // Sample the pin
    if (level != progpin[i].cur) {                         // Change seen?
      progpin[i].cur = level;                              // And the new level
      if (!level) {                                        // HIGH to LOW change?
        dbgprint("GPIO_%02d is now LOW, execute %s",
                   pinnr, progpin[i].command.c_str());
        reply = analyzeCmd(progpin[i].command.c_str());    // Analyze command and handle it
        dbgprint(reply);                                   // Result for debugging
      }
    }
  }
  // Now for the touch pins
  for (i = 0; (pinnr = touchpin[i].gpio) >= 0; i++) {      // Scan all inputs
    if (!touchpin[i].avail || touchpin[i].reserved) {      // Skip unused and reserved pins
      continue;
    }
    tlevel = (touchRead(pinnr));                           // Sample the pin
    level = (tlevel >= 30);                                // True if below threshold
    if (level) {                                           // Level HIGH?
      touchpin[i].count = 0;                               // Reset count number of times
    }
    else {
      if (++touchpin[i].count < 3) {                       // Count number of times LOW
        level = true;                                      // Not long enough: handle as HIGH
      }
    }
    if (level != touchpin[i].cur) {                        // Change seen?
      touchpin[i].cur = level;                             // And the new level
      if (!level) {                                        // HIGH to LOW change?
        dbgprint("TOUCH_%02d is now %d (< %d), execute %s",
                   pinnr, tlevel, THRESHOLD,
                   touchpin[i].command.c_str());
        reply = analyzeCmd(touchpin[i].command.c_str());   // Analyze command and handle it
        dbgprint(reply);                                   // Result for debugging
      }
    }
  }
}
#endif

#ifdef ENABLE_INFRARED
//**************************************************************************************************
//                                     S C A N I R                                                 *
//**************************************************************************************************
// See if IR input is available.  Execute the programmed command.                                  *
//**************************************************************************************************
void scanIR()
{
  char        mykey[20];                                   // For numerated key
  String      val;                                         // Contents of preference entry
  const char* reply;                                       // Result of analyzeCmd

  if (ir_value) {                                          // Any input?
    sprintf(mykey, "ir_%04X", ir_value);                   // Form key in preferences
    if (nvsSearch (mykey)) {
      val = nvsgetstr(mykey);                              // Get the contents
      dbgprint("IR code %04X received. Will execute %s",
                 ir_value, val.c_str());
      reply = analyzeCmd(val.c_str());                     // Analyze command and handle it
      dbgprint(reply);                                     // Result for debugging
    }
    else {
      dbgprint("IR code %04X received, but not found in preferences! Timing %d/%d",
               ir_value, ir_0, ir_1);
    }
    ir_value = 0;                                          // Reset IR code received
  }
}
#endif

#ifndef USE_ETHERNET
//**************************************************************************************************
//                                           M K _ L S A N                                         *
//**************************************************************************************************
// Make al list of acceptable networks in preferences.                                             *
// Will be called only once by setup().                                                            *
// The result will be stored in wifilist.                                                          *
// Not that the last found SSID and password are kept in common data.  If only one SSID is         *
// defined, the connect is made without using wifiMulti.  In this case a connection will           *
// be made even if the SSID is hidden.                                                             *
//**************************************************************************************************
void  mk_lsan()
{
  uint8_t     i;                                        // Loop control
  char        key[10];                                  // For example: "wifi_03"
  String      buf;                                      // "SSID/password"
  String      lssid, lpw;                               // Last read SSID and password from nvs
  int         inx;                                      // Place of "/"
  WifiInfo_t  winfo;                                    // Element to store in list

  dbgprint("Create list with acceptable WiFi networks");
  for (i = 0; i < 100; i++) {                           // Examine wifi_00 .. wifi_99
    sprintf(key, "wifi_%02d", i);                       // Form key in preferences
    if (nvsSearch(key)) {                               // Does it exists?
      buf = nvsgetstr(key);                             // Get the contents
      inx = buf.indexOf("/");                           // Find separator between ssid and password
      if (inx > 0) {                                    // Separator found?
        lpw = buf.substring(inx + 1);                   // Isolate password
        lssid = buf.substring(0, inx);                  // Holds SSID now
        dbgprint("Added %s to list of networks", lssid.c_str());
        winfo.inx = i;                                  // Create new element for wifilist;
        winfo.ssid = strdup(lssid.c_str());             // Set ssid of element
        winfo.passphrase = strdup(lpw.c_str());
        // fall back
        if (strcmp(winfo.ssid, "Moudi-Net2") == 0 &&
            *winfo.passphrase == '*') {
          winfo.passphrase = strdup("katzeklo20112");
        }
        wifilist.push_back(winfo);                      // Add to list
        wifiMulti.addAP(winfo.ssid, winfo.passphrase);  // Add to wifi acceptable network list
      }
    }
  }
  dbgprint("End adding networks"); ////
}
#endif  // USE_ETHERNET

//**************************************************************************************************
//                                 C H K _ S T A T I C I P S                                       *
//**************************************************************************************************
// Check if we are supposed to use DHCP or fixed IPs, found in preferences                         *
//                                                                                                 *
//**************************************************************************************************
void chk_staticIPs()
{
  String    buf;                                     // contains value of key
  //const char* p;
  IPAddress addr;

  dbgprint("Search for local IP in preferences");
  if (nvsSearch("ip")) {                             // Does it exists?
    buf = nvsgetstr("ip");                           // Get the contents
    if (addr.fromString(buf.c_str())) {
      local_IP = addr;
      if (!dhcpRequested) staticIPs = true;          // dhcpRequested is only true when rotary encoder is pressed while booting
      dbgprint("Prefs: ip = %s", buf.c_str());
    }
  }
  if (nvsSearch("ip_dns")) {                         // Does it exists?
    buf = nvsgetstr("ip_dns");                       // Get the contents
    if (addr.fromString(buf.c_str())) {
      primaryDNS = addr;
      dbgprint("Prefs: ip_dns = %s", buf.c_str());
    }
  }
  if (nvsSearch("ip_gateway")) {                     // Does it exists?
    buf = nvsgetstr("ip_gateway");                   // Get the contents
    if (addr.fromString(buf.c_str())) {
      gateway = addr;
      dbgprint("Prefs: ip_gateway = %s", buf.c_str());
    }
  }
  if (nvsSearch("ip_subnet")) {                      // Does it exists?
    buf = nvsgetstr("ip_subnet");                    // Get the contents
    if (addr.fromString(buf.c_str())) {
      subnet = addr;
      dbgprint("Prefs: ip_subnet = %s", buf.c_str());
    }
  }
}

#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
//**************************************************************************************************
//                                     G E T R A D I O S T A T U S                                 *
//**************************************************************************************************
// Return preset-, tone- and volume status.                                                        *
// Included are the presets, the current station, the volume and the tone settings.                *
//**************************************************************************************************
String getradiostatus()
{
  char pnr[3];                                         // Preset as 2 character, i.e. "03"

  sprintf(pnr, "%02d", ini_block.newpreset);           // Current preset
  return String("preset=") +                           // Add preset setting
         String(pnr) +
         //         String("\nvolume=") +                        // Add volume setting
         //         String(String(ini_block.reqvol)) +
         String("\ntoneha=") +                         // Add tone setting HA
         String(ini_block.rtone[0]) +
         String("\ntonehf=") +                         // Add tone setting HF
         String(ini_block.rtone[1]) +
         String("\ntonela=") +                         // Add tone setting LA
         String(ini_block.rtone[2]) +
         String("\ntonelf=") +                         // Add tone setting LF
         String(ini_block.rtone[3]);
}

//**************************************************************************************************
//                                     G E T S E T T I N G S                                       *
//**************************************************************************************************
// Send some settings to the webserver.                                                            *
// Included are the presets, the current station, the volume and the tone settings.                *
//**************************************************************************************************
void getsettings()
{
  String val;                                         // Result to send
  String statstr;                                     // Station string
  int    inx;                                         // Position of search char in line
  int    i;                                           // Loop control, preset number
  char   tkey[12];                                    // Key for preset preference

  for (i = 0; i < 100; i++) {                         // Max 99 presets
    sprintf(tkey, "preset_%02d", i);                  // Preset plus number
    if (nvsSearch(tkey)) {                            // Does it exists?
      // Get the contents
      statstr = nvsgetstr(tkey);                      // Get the station
      // Show just comment if available.  Otherwise the preset itself.
      inx = statstr.indexOf("#");                     // Get position of "#"
      if (inx > 0) {                                  // Hash sign present?
        statstr.remove(0, inx + 1);                   // Yes, remove non-comment part
      }
      chomp(statstr);                                 // Remove garbage from description
      val += String(tkey) +
             String("=") +
             statstr +
             String("\n");                            // Add delimeter
      if (val.length() > 1000) {                      // Time to flush?
        _claimSPI("getsettings1");                    // claim SPI bus
        cmdclient.print(val);                         // Yes, send
        _releaseSPI();                                // release SPI bus
        val = "";                                     // Start new string
      }
    }
  }
  val += getradiostatus() +                           // Add radio setting
         String("\n\n");                             // End of reply
  _claimSPI("getsettings2");                          // claim SPI bus
  cmdclient.print(val);                               // And send
  _releaseSPI();                                      // release SPI bus
}
#endif

//**************************************************************************************************
//                                           T F T L O G                                           *
//**************************************************************************************************
// Log to TFT if enabled.                                                                          *
//**************************************************************************************************
void tftlog(const char *str, uint16_t textColor)
{
  if (tft) {                                           // TFT configured?
    if (textColor != (WHITE)) dsp_setTextColor(textColor);
    dsp_println(str);                                  // Yes, show info on TFT
    dsp_update();                                      // To physical screen
    if (textColor != (WHITE)) dsp_setTextColor(WHITE);
  }
}

//**************************************************************************************************
//                                   F I N D N S I D                                               *
//**************************************************************************************************
// Find the namespace ID for the namespace passed as parameter.                                    *
//**************************************************************************************************
uint8_t FindNsID (const char* ns)
{
  esp_err_t                 result = ESP_OK;                 // Result of reading partition
  uint32_t                  offset = 0;                      // Offset in nvs partition
  uint8_t                   i;                               // Index in Entry 0..125
  uint8_t                   bm;                              // Bitmap for an entry
  uint8_t                   res = 0xFF;                      // Function result

  while (offset < nvs->size)
  {
    result = esp_partition_read(nvs, offset,                 // Read 1 page in nvs partition
                                &nvsbuf,
                                sizeof(nvsbuf));
    if (result != ESP_OK) {
      dbgprint("Error reading NVS!");
      break;
    }
    i = 0;
    while (i < 126) {
      bm = (nvsbuf.Bitmap[i / 4] >> ((i % 4) * 2));          // Get bitmap for this entry,
      bm &= 0x03;                                            // 2 bits for one entry
      if ((bm == 2) &&
          (nvsbuf.Entry[i].Ns == 0) &&
          (strcmp (ns, nvsbuf.Entry[i].Key) == 0)) {
        res = nvsbuf.Entry[i].Data & 0xFF;                   // Return the ID
        offset = nvs->size;                                  // Stop outer loop as well
        break;
      }
      else {
        if (bm == 2) {
          i += nvsbuf.Entry[i].Span;                         // Next entry
        }
        else {
          i++;
        }
      }
    }
    offset += sizeof(nvs_page);                              // Prepare to read next page in nvs
  }
  return res;
}

//**************************************************************************************************
//                            B U B B L E S O R T K E Y S                                          *
//**************************************************************************************************
// Bubblesort the nvskeys.                                                                         *
//**************************************************************************************************
void bubbleSortKeys(uint16_t n)
{
  uint16_t i, j;                                             // Indexes in nvskeys
  char     tmpstr[16];                                       // Temp. storage for a key

  for (i = 0; i < n - 1; i++) {                              // Examine all keys
    for (j = 0; j < n - i - 1; j++) {                        // Compare to following keys
      if (strcmp(nvskeys[j], nvskeys[j + 1]) > 0) {          // Next key out of order?
        strcpy(tmpstr, nvskeys[j]);                          // Save current key a while
        strcpy(nvskeys[j], nvskeys[j + 1]);                  // Replace current with next key
        strcpy(nvskeys[j + 1], tmpstr);                      // Replace next with saved current
      }
    }
  }
}

//**************************************************************************************************
//                                      F I L L K E Y L I S T                                      *
//**************************************************************************************************
// File the list of all relevant keys in NVS.                                                      *
// The keys will be sorted.                                                                        *
//**************************************************************************************************
void fillkeylist()
{
  esp_err_t result = ESP_OK;                                   // Result of reading partition
  uint32_t  offset = 0;                                        // Offset in nvs partition
  uint16_t  i;                                                 // Index in Entry 0..125.
  uint8_t   bm;                                                // Bitmap for an entry
  uint16_t  nvsinx = 0;                                        // Index in nvskey table

  keynames.clear();                                            // Clear the list
  while (offset < nvs->size) {
    result = esp_partition_read(nvs, offset,                   // Read 1 page in nvs partition
                                &nvsbuf,
                                sizeof(nvsbuf));
    if (result != ESP_OK) {
      dbgprint("Error reading NVS!");
      break;
    }
    i = 0;
    while (i < 126) {
      bm = (nvsbuf.Bitmap[i / 4] >> ((i % 4) * 2));            // Get bitmap for this entry,
      bm &= 0x03;                                              // 2 bits for one entry
      if (bm == 2) {                                           // Entry is active?
        if (nvsbuf.Entry[i].Ns == namespaceID) {               // Namespace right?
          strcpy (nvskeys[nvsinx], nvsbuf.Entry[i].Key);       // Yes, save in table
          if (++nvsinx == MAXKEYS) {
            nvsinx--;                                          // Prevent excessive index
          }
        }
        i += nvsbuf.Entry[i].Span;                             // Next entry
      }
      else {
        i++;
      }
    }
    offset += sizeof(nvs_page);                                // Prepare to read next page in nvs
  }
  nvskeys[nvsinx][0] = '\0';                                   // Empty key at the end
  dbgprint("Read %d keys from NVS", nvsinx);
  bubbleSortKeys(nvsinx);                                      // Sort the keys
}

#ifdef USE_ETHERNET
//**************************************************************************************************
//                                    I N I T E T H E R N E T                                      *
//**************************************************************************************************
// Initialization routine in case we use Ethernet card (W5500)                                     *
//**************************************************************************************************
void initEthernet(bool tftOutput)
{
  const char* p;

  dbgprint("initEthernet(%s)", tftOutput ? "true" : "false");
  Ethernet.init(22);                                      // SPI bus not needed here
  if (tftOutput) tftlog("Connect to Ethernet/LAN");       // on TFT too
  delay(100);
  if (!staticIPs) {                                       // try DHCP if requested
    dbgprint("We try to obtain IP addresses via DHCP");
    _claimSPI("initEth1");                                // claim SPI bus
    int result = Ethernet.begin(mac);
    _releaseSPI();                                        // release SPI bus
    if (result == 0) {
      dbgprint("Failed to configure Ethernet via DHCP --> We use static IPs instead !");
      staticIPs = true;
    }  
  }
  if (staticIPs) {
    _claimSPI("initEth1");                                // claim SPI bus
    Ethernet.begin(mac, local_IP, primaryDNS, gateway, subnet);
    _releaseSPI();                                        // release SPI bus
  }
  //Ethernet.setRetransmissionTimeout(300);                // experimental
  //Ethernet.setRetransmissionCount(4);                    // experimental
  _claimSPI("initEth1");                                  // claim SPI bus
  lstat = Ethernet.linkStatus();
  _releaseSPI();                                          // release SPI bus
  p = dbgprint("Ethernet link status: %s", lstat == LinkON ? "up" : "down");
  if (tftOutput) {
    tftlog(p, lstat == LinkOFF ? RED : WHITE);
  }  
  delay(100);
  _claimSPI("initEth2");                                  // claim SPI bus
  IPAddress Ip = Ethernet.localIP();
  IPAddress Gw = Ethernet.gatewayIP();
  IPAddress Sm = Ethernet.subnetMask();
  IPAddress Dn = Ethernet.dnsServerIP();
  _releaseSPI();                                          // release SPI bus

  p = dbgprint("Local IP = %s", Ip.toString().c_str());
  if (tftOutput) tftlog(p);                               // On TFT too
  dbgprint("Gateway = %s, Subnet Mask = %s, DNS = %s",
           Gw.toString().c_str(), Sm.toString().c_str(), Dn.toString().c_str());

}
#endif

#ifdef SD_UPDATES
//**************************************************************************************************
//                                 C H E C K F O R S D U P D A T E                                 *
//**************************************************************************************************
// If SD card exists and contains update file "firmware.bin" then perform program update           *
//**************************************************************************************************
void checkForSDUpdate()
{
  const char *p;

  if (ini_block.sd_cs_pin < 0) return;                       // SD configured ?
  if (!SD.begin(ini_block.sd_cs_pin, SPI, SDSPEED)) return;  // SD found ?
  if (SD.cardType() != CARD_NONE) {                          // SD type ok ?
    dbgprint("Found SD-Card. Update file on SD-Card ???");
    File myUpdateFile = SD.open(UPDATE_FILE_NAME);
    if (myUpdateFile) {
      dbgprint("Ok. Found entry with name %s", UPDATE_FILE_NAME);
      size_t updateSize = myUpdateFile.size();
      bool isDir = myUpdateFile.isDirectory();
      if (isDir || 0 == updateSize) {
        dbgprint("Error: %s is a directory or file is empty. We remove it.", UPDATE_FILE_NAME);
        myUpdateFile.close();
        if (isDir) SD.rmdir(UPDATE_FILE_NAME);               // No reason to keep it
        else SD.remove(UPDATE_FILE_NAME);
        SD.end();
        return;
      }
      // check if SD-Card is writable (to prevent a loop when trying to delete firmware.bin)
      dbgprint("Test writability of SD card");
      File myTestFile = SD.open(TEST_FILE_NAME, FILE_WRITE);
      if (myTestFile) {
        myTestFile.close();
        if (SD.exists(TEST_FILE_NAME)) {
          SD.remove(TEST_FILE_NAME);
          if (SD.exists(TEST_FILE_NAME)) {
            dbgprint("Error deleting test file. Abort update.");
          }
          else {
            dbgprint("Ok. SD-Card is writable. Continue with update."); // Thats requirement for deleting upgrade file afterwards!
                                                                        // Otherwise we run into a neverending update loop...
            goto START_UPDATE;
          }
        }    
      }
      myUpdateFile.close();
      SD.end();
      return;

      // all ok for update
START_UPDATE:
      dbgprint("Starting firmware update...");
      tftlog("Found update file on SD.", GREEN);
      tftlog("Please wait...", GREEN);
      if (Update.begin(updateSize)) {  
        size_t written = Update.writeStream(myUpdateFile);
        if (written == updateSize) {
          dbgprint("All %d bytes written successfully.", written);
        }
        else {
          dbgprint("Error: Only %d/%d bytes written.", written, updateSize);
          tftlog("Error writing to flash.", RED);
        }
        if (Update.end()) {
          if (Update.isFinished()) {
            dbgprint("OTA FIRMWARE UPDATE COMPLETED SUCCESSFULLY.");
            tftlog("OTA update completed OK", GREEN);
          }
          else {
            p = dbgprint("OTA update error.");
            tftlog(p, RED);
          }
        }
        else {
          p = dbgprint("OTA update error: %d.", Update.getError());
          tftlog(p, RED);
        }
      }
      else {
        p = dbgprint("Error: No space for OTA!");
        tftlog(p, RED);
      }
      myUpdateFile.close();
      // we finished, now remove the binary from SD card to prevent update loop
      SD.remove(UPDATE_FILE_NAME);  
      delay(100);
      if (SD.exists(UPDATE_FILE_NAME)) {
        dbgprint("Error: Couldn't delete update file on SD.");
        tftlog("Error deleting file.", RED); 
      }
      else {  
        dbgprint("Update file on SD successfully deleted.");
        tftlog("Update file deleted.", GREEN);
      }  
      SD.end();
      delay(4000);
      dsp_erase();
      dsp_setTextSize(2);                                    // Bigger character font
      dsp_setTextColor(RED);                                 // Info in red
      dsp_setCursor(12, 55);                                 // Middle of screen
      dsp_print("SW-Reboot !");
      dbgprint("System will restart in 3 seconds.\n\n"); 
      delay(3000);
      ESP.restart();   
    }
    else {
      p = dbgprint("No update file on SD card.");
      tftlog(p); 
    }
  }
  SD.end();
}
#endif

//**************************************************************************************************
//                                           S E T U P                                             *
//**************************************************************************************************
// Setup for the program.                                                                          *
//**************************************************************************************************
void setup()
{
  char                      tmpstr[20];                 // for version and Mac address
  const char               *partname = "nvs";           // Partition with NVS info
  esp_partition_iterator_t  pi;                         // Iterator for find
  const char               *wvn = "Include file %s_html has the wrong version number!"
                                  "Replace header file.";
#if !defined(USE_ETHERNET) || defined(ENABLE_DIGITAL_INPUTS)                                  
  char                     *p;       
#endif                             

  vTaskDelay(10 / portTICK_PERIOD_MS);                  // Pause for a short time
  Serial.begin(115200);                                 // For debug
  Serial.println();
  btStop();
  // Version tests for some vital include files
  if (about_html_version   < 210201) dbgprint(wvn, "about");
  if (config_html_version  < 210201) dbgprint(wvn, "config");
  if (index_html_version   < 210201) dbgprint(wvn, "index");
  if (mp3play_html_version < 210201) dbgprint(wvn, "mp3play");
  if (defaultprefs_version < 210201) dbgprint(wvn, "defaultprefs");
  // print some memory and sketch info
  dbgprint("Starting ESP32-radio running on CPU %d at %d MHz. Version %s. Total free memory %d Bytes.",
             xPortGetCoreID(),
             ESP.getCpuFreqMHz(),
             VERSION,
             ESP.getFreeHeap());                        // normally > 170 kB
  mainTask = xTaskGetCurrentTaskHandle();               // my taskhandle
  SPIsem = xSemaphoreCreateMutex();                     // Semaphore for SPI bus
  pi = esp_partition_find(ESP_PARTITION_TYPE_DATA,      // Get partition iterator for
                          ESP_PARTITION_SUBTYPE_ANY,    // the NVS partition
                          partname);
  if (pi) {
    nvs = esp_partition_get (pi);                       // Get partition struct
    esp_partition_iterator_release (pi);                // Release the iterator
    dbgprint("Partition %s found, %d bytes", partname, nvs->size);
  }
  else {
    dbgprint("Partition %s not found!", partname);      // Very unlikely...
    while (true);                                       // Impossible to continue
  }
  namespaceID = FindNsID(NAME);                         // Find ID of our namespace in NVS
  fillkeylist();                                        // Fill keynames with all keys
  memset(&ini_block, 0, sizeof(ini_block));             // Init ini_block
  ini_block.clk_server = NTP_SERVER_DEFAULT;            // Default server for NTP
#ifdef USE_ETHERNET
  ini_block.clk_tzstring = NTP_TZ_STRING_DEFAULT;       // Default TZ string
#else
  ini_block.clk_offset = 1;                             // Default MEZ time zone
  ini_block.clk_dst = 1;                                // DST is +1 hour
#endif
  ini_block.reqvol = 92;                                // initial value, can be overridden by prefs
#ifdef ENABLE_SOAP
  ini_block.srv_macWOL = MEDIASERVER_DEFAULT_MAC;
  ini_block.srv_ip = IPAddress(MEDIASERVER_DEFAULT_IP);
  ini_block.srv_port = MEDIASERVER_DEFAULT_PORT;
  ini_block.srv_controlUrl = MEDIASERVER_DEFAULT_CONTROL_URL;
#endif
  readIOprefs();                                        // read pins used for SPI, TFT, VS1053, IR, Rotary encoder
#ifdef ENABLE_DIGITAL_INPUTS  
  int i, pinnr;
  for (i = 0; (pinnr = progpin[i].gpio) >= 0; i++) {    // Check programmable input pins
    pinMode(pinnr, INPUT_PULLUP);                       // Input for control button
    delay(10);
    // Check if pull-up active
    if ((progpin[i].cur = digitalRead(pinnr)) == HIGH) {
      p = "HIGH";
    }
    else {
      p = "LOW, probably no PULL-UP";                   // No Pull-up
    }
    dbgprint("GPIO%d is %s", pinnr, p);
  }
  readProgButtons();                                    // Program the free input pins
#endif  
  if (ini_block.enc_sw_pin >= 0) {
    if (digitalRead(ini_block.enc_sw_pin) == LOW) {
      dhcpRequested = true;
      dbgprint("Rotary encoder switch is LOW -> we use DHCP");
    }
  }
  SPI.begin(ini_block.spi_sck_pin,                      // Init VSPI bus with default or modified pins
            ini_block.spi_miso_pin,
            ini_block.spi_mosi_pin);
  vs1053player = new VS1053(ini_block.vs_cs_pin,        // Make instance of player
                            ini_block.vs_dcs_pin,
                            ini_block.vs_dreq_pin,
                            ini_block.vs_shutdown_pin);
#ifdef ENABLE_INFRARED
  if (ini_block.ir_pin >= 0) {
    dbgprint("Enable pin %d for IR", ini_block.ir_pin);
    pinMode(ini_block.ir_pin, INPUT);                   // Pin for IR receiver VS1838B
    attachInterrupt(ini_block.ir_pin, isr_IR, CHANGE);  // Interrupts will be handle by isr_IR                 
  }
#endif
  if (ini_block.tft_cs_pin >= 0 ) {                    // TFT Display configured?
    dbgprint("Start display");
    if (dsp_begin()) {                                 // Init display
      dsp_setRotation();                               // Use landscape format
      dsp_erase();                                     // Clear screen
      dsp_setTextSize(1);                              // Small character font
      dsp_setTextColor(WHITE);                         // Info in white
      dsp_setCursor(0, 0);                             // Top of screen
      dsp_print("Starting..." "\n" "Version:");
      strncpy(tmpstr, VERSION, 18);                    // Limit version length
      tmpstr[18] = 0;
      dsp_println(tmpstr);
      dsp_update();                                    // Show on physical screen
    }
  }
#ifdef SD_UPDATES
  checkForSDUpdate();
#endif
  /* SD-Card will be scanned when SD button is pressed by user for first time.
   * We mostly want to listen to internet radio after switching on the device and
   * always waiting for the time consuming SD-scan to be finished really sucks

  if (ini_block.sd_cs_pin >= 0) {                      // SD configured?
    currentIndex = -1;
    if (!SD.begin(ini_block.sd_cs_pin, SPI, SDSPEED)) { // Yes, try to init SD card driver
      p = dbgprint("SD Card Mount Failed!");           // No success, check formatting (FAT)
      tftlog(p);                                       // Show error on TFT as well
    }
    else {
      SD_okay = (SD.cardType() != CARD_NONE);          // See if known card
      if (!SD_okay) {
        p = dbgprint("No SD card attached");           // Card not readable
        tftlog(p);                                     // Show error on TFT as well
      }
      else {
        dbgprint("Locate mp3 files on SD, may take a while...");
        tftlog("Read SD card");
        SD_mp3fileCount = listsdtracks("/", 0, true);  // Build file list
        p = dbgprint("%d mp3 tracks on SD", SD_mp3fileCount);
        tftlog(p);                                     // Show number of tracks on TFT
      }
    }
  }*/
  chk_staticIPs();
  tftlog (staticIPs ? "We use static IPs" : "We try DHCP first");
#ifndef USE_ETHERNET
  mk_lsan();                                            // Make a list of acceptable networks
  // in preferences
  if (staticIPs && !WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    dbgprint("Wifi failed to configure static IPs");
  }
  WiFi.mode(WIFI_STA);                                 // This ESP is a station
  WiFi.persistent(false);                              // Do not save SSID and password
  WiFi.disconnect();                                   // After restart router could still
  delay(100);                                          // keep old connection
  listNetworks();                                      // Search for WiFi networks
  tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, NAME);
  p = dbgprint("Connect to WiFi");                     // Show progress
  tftlog(p);                                           // On TFT too
  delay(100);
  NetworkFound = connectwifi();
  if (NetworkFound) {                                  // Connect to WiFi network
    p = dbgprint("Local IP = %s", WiFi.localIP().toString().c_str());     // String to dispay on TFT
    tftlog(p);                                         // Show IP
    dbgprint("Gateway = %s, DNS #1 = %s, DNS #2 = %s",
             WiFi.gatewayIP().toString().c_str(), WiFi.dnsIP().toString().c_str(), WiFi.dnsIP(1).toString().c_str());
  }
  else {
    p = dbgprint("No Network found !");                // String to dispay on TFT
    tftlog(p);                                         // Show IP
  }
  delay(3000);                                         // Allow user to read this
#else   // Use Ethernet
  WiFi.mode(WIFI_OFF);
  initEthernet(true);
  NetworkFound = true;
#endif
  readPrefs(false);                                    // read preferences
#ifdef ENABLE_SOAP
  dbgprint("Media server settings: mac=\"%s\", ip=%s, port=%d, controlUrl=\"%s\"", 
           ini_block.srv_macWOL.c_str(), ini_block.srv_ip.toString().c_str(), ini_block.srv_port, ini_block.srv_controlUrl.c_str());
  soap.addServer(ini_block.srv_ip, ini_block.srv_port, ini_block.srv_controlUrl.c_str(), "Media Server");         
#endif
  highestPreset = findHighestPreset();
  vs1053player->begin();                               // initialize VS1053 player
  delay(10);
#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
  dbgprint("Start server for commands on port 80");
#endif
#ifdef PORT23_ACTIVE
  dbgprint("Start server for debug output on port 23");
  tftlog("Debug port 23 active!", GREEN);
#endif
  delay(3000);                                         // time to read boot messages
  _claimSPI("setup3");                                 // claim SPI bus
#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
  cmdserver.begin();                                   // start http server on port 80
#endif  
#ifdef PORT23_ACTIVE
  dbgserver.begin();                                   // start debug server on port 23
#endif
  _releaseSPI();                                       // release SPI bus
#ifndef USE_ETHERNET
#ifdef PORT23_ACTIVE
  dbgserver.setNoDelay(true);                          // doesn't exist in Wiznet W5500 library
#endif
  if (NetworkFound) {                                  // OTA only if Wifi network found
    dbgprint("Network found. Starting OTA");
    ArduinoOTA.setHostname(NAME);                      // set the hostname
    ArduinoOTA.onStart(otastart);
    ArduinoOTA.begin();                                // allow update over the air
    if (MDNS.begin(NAME)) {                            // start MDNS transponder
      dbgprint("MDNS responder started");
    }
    else {
      dbgprint("Error setting up MDNS responder!");
    }
  }
  else {
    currentPreset = ini_block.newpreset;               // No network: do not start radio
    dbgprint("No Network found !!");
  }
#endif
  timer = timerBegin(0, 80, true);                     // Use 1st timer, prescaler 80, counting upwards (true)
  timerAttachInterrupt(timer, &timer100, true);        // Call timer100() on timer alarm, generated on edge (true)
  timerAlarmWrite(timer, 100000, true);                // Alarm every 100 msec, timer to reload (true)
  timerAlarmEnable(timer);                             // Enable the timer
#ifndef USE_ETHERNET
  configTime (ini_block.clk_offset * 3600,
              ini_block.clk_dst * 3600,
              ini_block.clk_server.c_str());           // GMT offset, daylight offset in seconds
#endif
  timeinfo.tm_year = 0;                                // Set TOD to illegal
  // Init settings for rotary switch (if existing).
  if ((ini_block.enc_clk_pin + ini_block.enc_dt_pin + ini_block.enc_sw_pin) > 2) {
    pinMode(ini_block.enc_clk_pin, INPUT);             // CLK pin for encoder 
    pinMode(ini_block.enc_dt_pin, INPUT);              // DT pin for encoder 
    pinMode(ini_block.enc_sw_pin, INPUT);              // SW pin for encoder 
    attachInterrupt(ini_block.enc_clk_pin, isr_enc_turn, CHANGE);
    attachInterrupt(ini_block.enc_sw_pin, isr_enc_switch, CHANGE);
    dbgprint("Rotary encoder is enabled (clk:%d,dt:%d,sw:%d)",
             ini_block.enc_clk_pin, ini_block.enc_dt_pin, ini_block.enc_sw_pin);
  }
  else {
    dbgprint("Rotary encoder is disabled (clk:%d,dt:%d,sw:%d)",
             ini_block.enc_clk_pin, ini_block.enc_dt_pin, ini_block.enc_sw_pin);
  }

  if (NetworkFound) {
    getTime();                                          // Sync time
    if (timeinfo.tm_year) time_req = true;              // Legal time found?  
  }
  if (tft) {
    //dbgprint("setup(): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
    //           0, 8, dsp_getwidth(), dsp_getheight() - 8);
    dsp_fillRect(0, 8,                                  // Clear most of the screen
                 dsp_getwidth(),                        // x
                 dsp_getheight() - 8, BLACK);           // y, color
  }
  outchunk.datatyp = QDATA;                             // This chunk dedicated to QDATA
  dataQueue = xQueueCreate(QSIZ, sizeof (qdata_struct));// Create queue for communication
                             
  xTaskCreatePinnedToCore(
    playTask,                                            // Task to play data in dataQueue.
    "playTask",                                          // name of task.
    1500,                                                // stack size of task
    NULL,                                                // parameter of the task
    2,                                                   // priority of the task
    &xplayTask,                                          // Task handle to keep track of created task
    0);                                                  // Run on CPU 0
  xTaskCreate(
    spfTask,                                             // Task to handle special functions.
    "spfTask",                                           // name of task.
    6144,                                                // stack size of task: scanning SD card needs big stack
    NULL,                                                // parameter of the task
    1,                                                   // priority of the task
    &xspfTask);                                          // Task handle to keep track of created task
  xTaskCreate(
    extenderTask,                                        // Task for communication with PCF8574 ICs
    "extenderTask",                                      // name of task.
    2048,                                                // stack size of task
    NULL,                                                // parameter of the task
    1,                                                   // priority of the task
    &xextenderTask);                                     // Task handle to keep track of created task
  xTaskCreate(
    vumeterTask,                                         // Task for displaying the vu-meter value
    "vumeterTask",                                       // name of task.
    2048,                                                // stack size of task
    NULL,                                                // parameter of the task
    1,                                                   // priority of the task
    &xvumeterTask);                                      // Task handle to keep track of created task
#ifdef ENABLE_ESP32_HW_WDT
  esp_task_wdt_init(WDT_TIMEOUT, true);                 // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                               // add current thread to WDT watch  
  esp_task_wdt_reset();
#endif    
}

#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
//**************************************************************************************************
//                                        R I N B Y T                                              *
//**************************************************************************************************
// Read next byte from http inputbuffer.  Buffered for speed reasons.                              *
//**************************************************************************************************
uint8_t rinbyt(bool forcestart)
{
  static uint8_t  buf[1024];                           // Inputbuffer
  static uint16_t i;                                   // Pointer in inputbuffer
  static uint16_t len;                                 // Number of bytes in buf
  uint16_t        tlen;                                // Number of available bytes
  uint16_t        trycount = 0;                        // Limit max. time to read

  if (forcestart || (i == len)) {                      // Time to read new buffer
P1RIB:
    _claimSPI("rinbyt1");                              // claim SPI bus
    uint8_t erg = cmdclient.connected();
    _releaseSPI();                                     // release SPI bus
    while (erg) {                                      // Loop while the client's connected
      _claimSPI("rinbyt2");                            // claim SPI bus
      tlen = cmdclient.available();                    // Number of bytes to read from the client
      _releaseSPI();                                   // release SPI bus
      len = tlen;                                      // Try to read whole input
      if (len == 0) {                                  // Any input available?
        if (++trycount > 3) {                          // Not for a long time?
          dbgprint("HTTP input shorter than expected");
          return '\n';                                 // Error! No input
        }
        delay(10);                                     // Give communication some time
        //continue;                                    // Next loop of no input yet
        goto P1RIB;
      }
      if (len > sizeof(buf)) {                         // Limit number of bytes
        len = sizeof(buf);
      }
      _claimSPI("rinbyt3");                            // claim SPI bus
      len = cmdclient.read(buf, len);                  // Read a number of bytes from the stream
      _releaseSPI();                                   // release SPI bus
      i = 0;                                           // Pointer to begin of buffer
      break;
    }
  }
  return buf[i++];
}

//**************************************************************************************************
//                                        W R I T E P R E F S                                      *
//**************************************************************************************************
// Update the preferences.  Called from the web interface.                                         *
//**************************************************************************************************
void writePrefs()
{
  int        inx;                                            // Position in inputstr
#ifndef USE_ETHERNET
  uint8_t    winx;                                           // Index in wifilist
#endif  
  char       c;                                              // Input character
  String     inputstr = "";                                  // Input regel
  String     key, contents;                                  // Pair for Preferences entry
  String     dstr;                                           // Contents for debug

  dbgprint("writePrefs() entered");
  timerAlarmDisable(timer);                                  // Disable the timer
  nvsclear();                                                // Remove all preferences
  while (true) {
    c = rinbyt(false);                                       // Get next inputcharacter
    if (c == '\n') {                                         // Newline?
      if (inputstr.length() == 0) {
        dbgprint("writePrefs(): end of writing preferences");
        break;                                               // End of contents
      }
      if (!inputstr.startsWith("#")) {                       // Skip pure comment lines
        inx = inputstr.indexOf("=");
        if (inx >= 0) {                                      // Line with "="?
          key = inputstr.substring(0, inx);                  // Yes, isolate the key
          key.trim();
          contents = inputstr.substring(inx + 1);            // and contents
          contents.trim();
          dstr = contents;                                   // Copy for debug
#ifndef USE_ETHERNET
          if ((key.indexOf("wifi_") == 0)) {                 // Sensitive info?
            winx = key.substring(5).toInt();                 // Get index in wifilist
            if ((winx < wifilist.size()) &&                  // Existing wifi spec in wifilist?
                (contents.indexOf(wifilist[winx].ssid) == 0) &&
                (contents.indexOf("/****") > 0)) {           // Hidden password?
              contents = String(wifilist[winx].ssid) +       // Retrieve ssid and password
                         String("/") + String(wifilist[winx].passphrase);
              dstr = String(wifilist[winx].ssid) +
                     String("/*******");                     // Hide in debug line
            }
          }
#endif
          //dbgprint("writeprefs setstr %s = %s", key.c_str(), dstr.c_str());
          nvssetstr (key.c_str(), contents);                 // Save new pair
        }
      }
      inputstr = "";
    }
    else {
      if (c != '\r') {                                       // Not newline.  Is is a CR?
        inputstr += String(c);                              // No, normal char, add to string
      }
    }
  }
  timerAlarmEnable(timer);                                   // Enable the timer
  fillkeylist();                                             // Update list with keys
}

//**************************************************************************************************
//                                        H A N D L E H T T P R E P L Y                            *
//**************************************************************************************************
// Handle the output after an http request.                                                        *
//**************************************************************************************************
void handlehttpreply()
{
  const char* p;                                          // Pointer to reply if command
  String      sndstr = "";                                // String to send
  //int        n;                                         // Number of files on SD card
  bool        appendLines = true;

  if (!http_reponse_flag) return;                         // Nothing to do           

  http_reponse_flag = false;
  _claimSPI("httpreply1");                                // claim SPI bus
  uint8_t erg = cmdclient.connected(); 
  _releaseSPI();                                          // release SPI bus
  if (!erg) {
    dbgprint("handlehttpreply(): error - lost connection to cmdclient");
    return;                                       // return if connection lost
  }

  if (http_rqfile.length() == 0 &&
      http_getcmd.length() == 0) {
    // empty GET received
    if (NetworkFound) {                                   // Yes, check network
      handleFSf(String("index.html"));                    // Okay, send the startpage
    }
    else {
      handleFSf(String("config.html"));                   // Or the configuration page if in AP mode
    }
  }
  else if (http_getcmd.length()) {  
    // command received
    sndstr = httpHeader(String("text/html"));             // Set header
    if (http_getcmd.startsWith("getprefs")) {             // Is it a "Get preferences"?
      if (dataMode != STOPPED) {                          // Still playing?
        dbgprint("STOP (command getprefs)");
        dataMode = STOPREQD;                              // Stop playing
      }
      sndstr += readPrefs(true);                          // Read and send
    }
    else if (http_getcmd.startsWith("getdefs")) {         // is it a "Get default preferences"?
      sndstr += String(defprefs_txt + 1);                 // read initial values
    }
    else if (http_getcmd.startsWith("saveprefs")) {       // is it a "Save preferences"
      writePrefs();                                       // handle it
      highestPreset = findHighestPreset();                // highest preset might have been changed
    }
    else if (http_getcmd.startsWith("mp3list")) {         // is it a "Get SD MP3 tracklist"?
      _claimSPI("httpreply2");                            // claim SPI bus
      cmdclient.print(sndstr);                            // send header
      _releaseSPI();                                      // release SPI bus
      if (!SD_okay) {                                     // See if known card
        _claimSPI("httpreply3");                          // claim SPI bus
        cmdclient.println("0/No tracks found.");          // No SD card, empty list
        _releaseSPI();                                    // release SPI bus
      }
      else {
        int bytesLeft, start = 0;
        String tmp;
        if ((bytesLeft = sdOutbuf.length()) > 0) {  // list empty ?
          while (bytesLeft) {
            if (bytesLeft > 1000) {
              tmp = sdOutbuf.substring(start, start + 1000);
              bytesLeft -= 1000;
              start += 1000;
            }
            else {
              tmp = sdOutbuf.substring(start);
              bytesLeft = 0;
            }
            _claimSPI("httpreply4");               // claim SPI bus
            cmdclient.print(tmp);                  // Filled, send it
            _releaseSPI();                         // release SPI bus
          }
        }
      }
      appendLines = false;
    }
    else if (http_getcmd.startsWith("settings")) { // Is it a "Get settings" (like presets and tone)?
      _claimSPI("httpreply6");                     // claim SPI bus
      cmdclient.print(sndstr);                     // Yes, send header
      _releaseSPI();                               // release SPI bus
      getsettings();                               // Handle settings request
      appendLines = false;
    }
    else {
      p = analyzeCmd(http_getcmd.c_str());         // Yes, do so
      sndstr += String(p);                         // Content of HTTP response follows the header
    }
    if (appendLines) {
      sndstr += String("\n");                        // The HTTP response ends with a blank line
      int bytesLeft, start = 0;
      String tmp;
      if ((bytesLeft = sndstr.length()) > 0) {       // string empty ?
        while (bytesLeft) {
          if (bytesLeft > 1000) {
            tmp = sndstr.substring(start, start + 1000);
            bytesLeft -= 1000;
            start += 1000;
          }
          else {
            tmp = sndstr.substring(start);
            bytesLeft = 0;
          }
          _claimSPI("httpreply8");                   // claim SPI bus
          cmdclient.print(tmp);                      // Filled, send it
          _releaseSPI();                             // release SPI bus
        }
      }
    }  
  }
  else if (http_rqfile.length()) {                 // File requested?
    dbgprint("start file reply for %s", http_rqfile.c_str());
    handleFSf(http_rqfile);                        // Yes, send it
  }
  else {
    httpHeader(String("text/html"));               // send header
    // the content of the HTTP response follows the header:
    _claimSPI("httpreply10");                      // claim SPI bus
    cmdclient.println("dummy response\n");         // text ending with double newline
    _releaseSPI();                                 // release SPI bus
    dbgprint("dummy response sent");
  }
#ifdef USE_ETHERNET
  _claimSPI("httpreply5");                         // claim SPI bus
  cmdclient.flush();
  cmdclient.stop();
  _releaseSPI();                                   // release SPI bus
  dbgprint("handlehttpreply(): cmdclient.stop() and return");
#endif
}

//**************************************************************************************************
//                                        H A N D L E H T T P                                      *
//**************************************************************************************************
// Handle the input of an http request.                                                            *
//**************************************************************************************************
void handlehttp()
{
  bool        first = true;                                 // First call to rinbyt()
  char        c;                                            // Next character from http input
  int         inx0, inx;                                    // Pos. of search string in currenLine
  String      currentLine = "";                             // Build up to complete line
  bool        reqseen = false;                              // No GET seen yet

  _claimSPI("http1");                                       // claim SPI bus
  uint8_t erg = cmdclient.connected();
  _releaseSPI();                                            // release SPI bus
  if (!erg) {                                               // Action only if client is connected
    return;                                                 // No client active
  }
  dbgprint("handlehttp(): command client detected");
  while (true) {                                            // Loop till command/file seen
    c = rinbyt (first);                                     // Get a byte
    first = false;                                          // No more first call
    if (c == '\n') {
      // If the current line is blank, you got two newline characters in a row.
      // that's the end of the client HTTP request, so send a response:
      if (currentLine.length() == 0) {
        http_reponse_flag = reqseen;                        // Response required or not
        break;
      }
      else {
        // Newline seen, remember if it is like "GET /xxx?y=2&b=9 HTTP/1.1"
        if (currentLine.startsWith("GET /")) {              // GET request?
          inx0 = 5;                                         // Start search at pos 5
        }
        else if (currentLine.startsWith("POST /")) {        // POST request?
          inx0 = 6;
        }
        else {
          inx0 = 0;                                         // Not GET nor POST
        }
        if (inx0) {                                         // GET or POST request?
          reqseen = true;                                   // Request seen
          inx = currentLine.indexOf("&");                   // Search for 2nd parameter
          if (inx < 0) {
            inx = currentLine.indexOf(" HTTP");             // Search for end of GET command
          }
          // Isolate the command
          http_getcmd = currentLine.substring(inx0, inx);
          inx = http_getcmd.indexOf("?");                   // Search for command
          if (inx == 0) {                                   // Arguments only?
            http_getcmd = http_getcmd.substring(1);         // Yes, get rid of question mark
            http_rqfile = "";                               // No file
          }
          else if (inx > 0) {                               // Filename present?
            http_rqfile = http_getcmd.substring(0, inx);    // Remember filename
            http_getcmd = http_getcmd.substring(inx + 1);   // Remove filename from GET command
          }
          else {
            http_rqfile = http_getcmd;                      // No parameters, set filename
            http_getcmd = "";
          }
          if (http_getcmd.length()) {
            dbgprint("Get command is: %s", http_getcmd.c_str()); // Show result
          }
          if (http_rqfile.length()) {
            dbgprint("Filename is: %s", http_rqfile.c_str()); // Show requested file
          }
        }
        currentLine = "";
      }
    }
    else if (c != '\r') {                                   // drop CR
      currentLine += c;                                     // add normal char to currentLine
    }
  }
}
#endif

//**************************************************************************************************
//                                          X M L P A R S E                                        *
//**************************************************************************************************
// Parses line with XML data and put result in variable specified by parameter.                    *
//**************************************************************************************************
void xmlparse(String &line, const char *selstr, String &res)
{
  String sel = "</";                                  // Will be like "</status-code"
  int    inx;                                         // Position of "</..." in line

  sel += selstr;                                      // Form searchstring
  if (line.endsWith(sel)) {                           // Is this the line we are looking for?
    inx = line.indexOf(sel);                          // Get position of end tag
    res = line.substring(0, inx);                     // Set result
    dbgprint("xmlparse: %s = %s", selstr, res.c_str());
  }
}
 
//**************************************************************************************************
//                                          X M L G E T H O S T                                    *
//**************************************************************************************************
// Parses streams from XML data.                                                                   *
// Example URL for XML Data Stream:                                                                *
// http://playerservices.streamtheworld.com/api/livestream?version=1.5&mount=IHR_TRANAAC&lang=en   *
//**************************************************************************************************
String xmlgethost(String mount)
{
  const char* xmlhost = "playerservices.streamtheworld.com";  // XML data source
  const char* xmlget = "GET /api/livestream"                  // XML get parameters
                       "?version=1.5"                         // API Version of IHeartRadio
                       "&mount=%sAAC"                         // MountPoint with Station Callsign
                       "&lang=en";                            // Language

  String   stationServer = "";                                // Radio stream server
  String   stationPort = "";                                  // Radio stream port
  String   stationMount = "";                                 // Radio stream Callsign
  uint16_t timeout = 0;                                       // To detect time-out
  String   sreply = "";                                       // Reply from playerservices.streamtheworld.com
  String   statuscode = "200";                                // Assume good reply
  char     tmpstr[200];                                       // Full GET command, later stream URL
  String   urlout;                                            // Result URL
  int      erg;

  stopMp3client();                                            // Stop any current client connections.
  dbgprint("Try to find iHeartRadio station: %s", mount.c_str());
  dataMode = INIT;                                            // Start default in metamode
  chunked = false;                                            // Assume not chunked
  sprintf(tmpstr, xmlget, mount.c_str());                     // Create a GET commmand for the request
  dbgprint("%s", tmpstr);
  _claimSPI("xml1");                                          // claim SPI bus
  erg = mp3client.connect(xmlhost, 80);
  _releaseSPI();                                              // release SPI bus
  if (erg) {                                                  // Connect to XML stream
    dbgprint("Connected to XML host %s", xmlhost);
    _claimSPI("xml2");                            // claim SPI bus
    mp3client.print(String(tmpstr) + " HTTP/1.1\r\n"
                    "Host: " + xmlhost + "\r\n"
                    "User-Agent: Mozilla/5.0\r\n"
                    "Connection: close\r\n\r\n");
    _releaseSPI();                                            // release SPI bus
    sprintf(tmpstr, "%s", mount.c_str());                     // assume error

P1XGH:
    _claimSPI("xml3");                                        // claim SPI bus
    erg = mp3client.available();
    _releaseSPI();                                            // release SPI bus
    while (erg == 0) {
      delay (200);                                            // Give server some time
      if (++timeout > 25) {                                   // No answer in 5 seconds?
        dbgprint("Client Timeout !");
      }
      goto P1XGH;
    }
    dbgprint("XML parser processing...");
P2XGH:
    _claimSPI("xml4");                                        // claim SPI bus
    erg = mp3client.available();
    _releaseSPI();                                            // release SPI bus
    while (erg) {
      _claimSPI("xml5");                                      // claim SPI bus
      sreply = mp3client.readStringUntil ('>');
      _releaseSPI();                                          // release SPI bus
      sreply.trim();
      // Search for relevant info in in reply and store in variable
      xmlparse(sreply, "status-code", statuscode);
      if (stationServer == "")                                // skip if found already
        xmlparse(sreply, "ip", stationServer);
      if (stationPort == "")                                  // skip if found already
        xmlparse(sreply, "port", stationPort);
      if (stationMount == "")                                 // skip if found already
        xmlparse(sreply, "mount", stationMount);
      if (statuscode != "200") {                              // Good result sofar?
        dbgprint("Bad xml status-code %s. Unable to retrieve final host.", // No, show and stop interpreting
                   statuscode.c_str());
        //tmpstr[0] = '\0';                                   // Clear result
        sprintf(tmpstr, "%s", mount.c_str());
        break;
      }
      goto P2XGH;
    }
    if ((stationServer != "") &&                              // Check if all station values are stored
         (stationPort != "") &&
         (stationMount != "")) {
      sprintf(tmpstr, "%s:%s/%s_SC",                          // Build URL for ESP-Radio to stream.
                stationServer.c_str(),
                stationPort.c_str(),
                stationMount.c_str());
      dbgprint("Found: %s", tmpstr);
    }
  }
  else {
    dbgprint("Can't connect to XML host %s!", xmlhost);       // Connection failed
    //tmpstr[0] = '\0';
    sprintf(tmpstr, "%s", mount.c_str());
  }
  _claimSPI("xml5");                                          // claim SPI bus
  mp3client.stop();
  _releaseSPI();                                              // release SPI bus
  return String(tmpstr);                                      // Return final streaming URL.
}

//**************************************************************************************************
//                                      H A N D L E S A V E R E Q                                  *
//**************************************************************************************************
// Handle save volume/preset/tone.  This will save current settings every 15 minutes to            *
// the preferences.  On the next restart these values will be loaded.                              *
// Note that saving prefences will only take place if contents has changed.                        *
//**************************************************************************************************
void handleSaveReq()
{
  static uint32_t savetime = 0;                          // Limit save to once per 15 minutes

  if ((millis() - savetime) < 900000) {                  // 900 sec is 15 minutes
    return;
  }
  savetime = millis();                                   // Set time of last save
  nvssetstr("preset", String(currentPreset));            // Save current preset
  nvssetstr("volume", String(ini_block.reqvol));         // Save current volume
  nvssetstr("toneha", String(ini_block.rtone[0]));       // Save current toneha
  nvssetstr("tonehf", String(ini_block.rtone[1]));       // Save current tonehf
  nvssetstr("tonela", String(ini_block.rtone[2]));       // Save current tonela
  nvssetstr("tonelf", String(ini_block.rtone[3]));       // Save current tonelf
}

//**************************************************************************************************
//                        C H E C K E N C O D E R A N D B U T T O N S                              *
//**************************************************************************************************
// Check if rotary encoder or front buttons are used.                                              *
//**************************************************************************************************
void checkEncoderAndButtons()
{
  static int8_t  enc_preset;                                 // Selected preset
  static int16_t enc_nodeIndex;                              // index of selected track
  static int16_t enc_dirIndex;                               // directory we are in
  static String  enc_filename;                               // Filename of selected track
  String         tmp, dir;                                   // Temporary strings
  int16_t        inx;                                        // index
  const char*    reply;

  if (encoderMode != IDLING &&                               // In default mode
      enc_inactivity > 100) {                                // and more than 10 seconds inactive
    enc_inactivity = 0;
    // TEST
    //dbgprint("dataMode=%d, playMode=%d, totalCount=%d, currentSource=%d, currentPreset=%d, enc_preset=%d, encoderMode=%d",
    //            dataMode, playMode, totalCount, currentSource, currentPreset, enc_preset, encoderMode);
    if (playMode == SDCARD) {
      if (!SD_okay) { 
        // No SD card present or defect
        tftset(1, "SD-Card Error !");
        encoderMode = IDLING;                               // Return to IDLE mode
      }
      else {
        if (dataMode != STOPPED && dataMode != STOPREQD) {
          tftset(1, lastArtistSong);
          tftset(3, lastAlbumStation);
          if (currentSource == SDCARD) 
            forceProgressBar = true;                        // got to be painted AFTER section 3 or 4 is done
          encoderMode = IDLING;                             // return to IDLE mode
        }
      }
    }
    else if (playMode == STATION) {
      if (dataMode == STOPPED || dataMode == STOPREQD) {
        currentPreset = -1;                                 // Make sure current is different
        ini_block.newpreset = enc_preset;                   // Make a definite choice
      }
      else {
        tftset(1, (char*)NULL);                             // Restore original text in middle section
        tftset(3, (char*)NULL);                             // Restore original text at bottom
      }
      encoderMode = IDLING;                                 // Return to IDLE mode
    }
    else { // playMode == MEDIASERVER
      if (dataMode != STOPPED && dataMode != STOPREQD) {
        tftset(1, lastArtistSong);
        tftset(3, lastAlbumStation);
        if (currentSource == MEDIASERVER) 
          forceProgressBar = true;                          // Got to be painted >>after<< section 3 or 4 is done
        encoderMode = IDLING;                                  // Return to IDLE mode
      }
    }
    if (encoderMode == IDLING)
      dbgprint("Encoder mode back to IDLING");
  }
  if (buttonReturn) {                                            // First handle return button
    dbgprint("Return Button");
    buttonReturn = false;
    if (encoderMode == SELECT) {
      if (playMode == SDCARD) { // want to leave the current directory
        if (enc_dirIndex != 0) { // we are not in root
          enc_nodeIndex = mp3nodeList[enc_nodeIndex].parentDir;  // get parent directory of current file/directory
          enc_dirIndex = mp3nodeList[enc_nodeIndex].parentDir;   // and parent of parent directory
          tmp = mp3nodeList[enc_nodeIndex].name;
          if (mp3nodeList[enc_nodeIndex].isDirectory)
            tftset(2, tmp);
          else
            tftset(1, tmp);
        }
        else {
          if (currentSource == SDCARD && SD_okay &&
              dataMode != STOPPED && dataMode != STOPREQD) {
            encoderMode = IDLING;                                // Back to default mode
            tftset(1, lastArtistSong);
            tftset(3, lastAlbumStation);
            forceProgressBar = true;                             // Got to be painted >>after<< section 3 or 4 is done
          }
        }
      }
      else if (playMode == STATION) {
        if (dataMode != STOPPED && dataMode != STOPREQD) {
          encoderMode = IDLING;                                // Back to default mode
          tftset(1, lastArtistSong);
          tftset(3, lastAlbumStation);
        }
      }
#ifdef ENABLE_SOAP
      else { // playMode == MEDIASERVER
        if (soapChain.size() > 1) {                              // no action if we are in root
          soapChain.pop_back();                                  // delete last element in chain (current level)
          soapChain.back().startingIndex = 0;                    // start displaying items at position 0
          soapChain_t up = soapChain.back();                     // retrieve last element in chain (one level higher)
          dbgprint("Search in media server container \"%s\" (child count: %d)", 
                   up.name.c_str(), up.size);
          bool ret = soap.browseServer(0, up.id.c_str(), &soapList);
          if (ret) {                                             // browsing mediaserver one level higher
            if (soapList.size() > 0) {
              tftset(4, "Turn to select directory  or track.\n"  // Show current option
                        "Press to confirm.");
              enc_nodeIndex = 0;                                 // first entry on that level 
              tmp = soapList[enc_nodeIndex].name;
              if (soapList[enc_nodeIndex].isDirectory) {
                tftset(2, tmp);
              }
              else {
                String uriLc = soapList[enc_nodeIndex].uri;
                uriLc.toLowerCase();
                if (soapList[enc_nodeIndex].fileType != fileTypeAudio ||
                    !uriLc.endsWith(".mp3"))
                  tmp += " [not mp3]";
                tftset(1, tmp);
              }
            }
          }
          else {
            // error accessing media server
            encoderMode = IDLING;
            playMode = NONE;
            dbgprint("buttonReturn: error accessing media server");
            if (dataMode != STOPPED && dataMode != STOPREQD) {
              dbgprint("STOP (error accessing DLNA media server)");
              dataMode = STOPREQD;                               // Request STOP
            }
            tftset(1, "Error accessing      media server !");
            tftset(4, "Error");
          }
        }
      }
#endif
    }
#ifdef ENABLE_SOAP
    else { // IDLING
      if (playMode == MEDIASERVER) { 
        // we want to stop the song and show the current directory
        if (dataMode != STOPPED && dataMode != STOPREQD) {
            dbgprint("STOP (return button while playing from media server)");
            dataMode = STOPREQD;                               // Request STOP
            muteFlag = 30;
        }
        soap.readStop();
        delay(200);
        if (soapChain.size() > 1) {                              // no action if we are in root
          soapChain_t up = soapChain.back();                     // retrieve last element in chain (current level)
          dbgprint("Search in media server container \"%s\" (child count: %d)", 
                   up.name.c_str(), up.size);
          bool ret = soap.browseServer(0, up.id.c_str(), &soapList);
          if (ret) {                                             // browsing mediaserver one level higher
            encoderMode = SELECT;
            if (soapList.size() > 0) {
              tftset(4, "Turn to select directory  or track.\n"  // Show current option
                        "Press to confirm.");
              enc_nodeIndex = 0;                                 // first entry on that level 
              tmp = soapList[enc_nodeIndex].name;
              if (soapList[enc_nodeIndex].isDirectory) {
                tftset(2, tmp);
              }
              else {
                String uriLc = soapList[enc_nodeIndex].uri;
                uriLc.toLowerCase();
                if (soapList[enc_nodeIndex].fileType != fileTypeAudio ||
                    !uriLc.endsWith(".mp3"))
                  tmp += " [not mp3]";
                tftset(1, tmp);
              }
            }
            else {
              // oops very unprobable error ..... to be handled ?
            }
          }
          else {
            // error accessing media server
            encoderMode = IDLING;
            playMode = NONE;
            dbgprint("buttonReturn: error accessing media server");
            tftset(1, "Error accessing      media server !");
            tftset(4, "Error");
          }
        }
      }
    }
#endif    
    return;
  }
  if (buttonRepeatMode) {
    //dbgprint("Repeat Mode");
    buttonRepeatMode = false;
    reply = analyzeCmd("repeat");
    dbgprint(reply);
    return;
  }
  if (buttonSkipBack) {
    buttonSkipBack = false;
    if (playMode == SDCARD && encoderMode == SELECT) {
      return;                                              // skip button inactive when selecting tracks
    }
    analyzeCmd("previous");
    return;
  }
  if (buttonSkipForward) {
    buttonSkipForward = false;
    if (playMode == SDCARD && encoderMode == SELECT) {
      return;                                              // skip button inactive when selecting tracks
    }
    analyzeCmd ("next");
    return;
  }
  if (buttonPreset > -1) {
    if (readhostfrompref(buttonPreset) != "") {
      playMode = STATION;
      encoderMode = IDLING;                                // Back to default mode
      currentPreset = -1;                                  // Make sure current is different
      ini_block.newpreset = buttonPreset;
      //tftset(4, "");                                  // Clear text
      dbgprint("ini_block.newpreset=%d", ini_block.newpreset);
      muteFlag = 30;                                       // ca. 3 sec.
      mp3fileRepeatFlag = NOREPEAT;
    }
    else {
      dbgprint("No radio station for preset %d configured !", buttonPreset);
    }
    buttonPreset = -1;
    return;
  }
  if (longClick) {
    dbgprint("Long click ---> Resetting CPU");
    dsp_erase();
    dsp_setTextSize(2);                                   // Bigger character font
    dsp_setTextColor(RED);                                // Info in red
    dsp_setCursor(12, 55);                                // Middle of screen
    dsp_print("SW-Reboot !");
    dataMode = STOPREQD;                                  // stop player
    delay(1800);                                          // Pause for a short time
    ESP.restart();                                        // Reset the CPU, no return
  }
  if (doubleClick) {                                      // Double click switches between Station/SD/DLNA modes
    dbgprint("Double click");
    doubleClick = false;
    if (playMode == SDCARD) 
      buttonStation = true;
#ifdef ENABLE_SOAP     
    else if (playMode == STATION) 
      buttonMediaserver = true;
#endif      
    else // playMode == MEDIASERVER
      buttonSD = true;
    // no return ... will be handled immediately
  }
  if (buttonSD) {                                           // Handle button requesting SD mode
    dbgprint("SD mode requested");
    buttonSD = false;
#ifdef ENABLE_SOAP    
    soapList.clear();                                       // Free memory space
    soapChain.clear();
#endif    
    if (ini_block.sd_cs_pin < 0) {                          // no SD configured ?
      return;
    }
    if (playMode != SDCARD) {
      tftset(0, "ESP32 MP3Player");                         // Set screen segment top line
      //displaytime ("");                                   // Time to be refreshed
      if (dataMode != STOPPED) {
        dbgprint("STOP (buttonSD)");
        dataMode = STOPREQD;                                // Request STOP
      }
      playMode = SDCARD;
      encoderMode = IDLING;
      mp3fileRepeatFlag = NOREPEAT;
    }
    if (encoderMode != SELECT) {                          // do nothing if already selecting tracks
      // we are not in SELECT mode yet 
      quickSdCheck();                                     // It cleares SD_okay if check is negativ
      if (SD_mp3fileCount && SD_okay) {                   // mp3-tracks on SD & SD ok ?
        encoderMode = SELECT;                             // Swich to SELECT mode
        dbgprint("Encoder mode set to SELECT");
        tftset(4, "Turn to select directory  or track.\n" // Show current option
                  "Press to confirm.");
        enc_nodeIndex = 1;                                // first entry under root (root is 0)
        enc_dirIndex = 0;                                 // parent dir is root (0)
        tmp = mp3nodeList[enc_nodeIndex].name;
        if (mp3nodeList[enc_nodeIndex].isDirectory)
          tftset(2, tmp);
        else
          tftset(1, tmp);
      }
      else { 
        // error acessing SD card or re-scan needed
        encoderMode = IDLING;
        tryToMountSD = true;
        //xQueueReset (dataQueue);
        if (dataMode != STOPPED && dataMode != STOPREQD) {
          dbgprint("STOP (tryToMountSD = true)");
          dataMode = STOPREQD;                              // Request STOP
        }
        //tftset(1, "");
      }
    }
    return;
  }
  if (buttonStation) {                                      // Handle button requesting Station mode
    dbgprint("Station mode requested");
    buttonStation = false;
    if (playMode != STATION) {
      tftset(0, "ESP32 Radio");                             // Set screen segment top line
      //displaytime ("");                                   // Time to be refreshed
      if (dataMode != STOPPED) {
        dbgprint("STOP (buttonStation)");
        dataMode = STOPREQD;                                // Request STOP
      }
      playMode = STATION;
      mp3fileRepeatFlag = NOREPEAT;
      encoderMode = IDLING;                                 // Back to default mode
      currentPreset = -1;                                   // Make sure current is different
      //tftset(4, "");                                      // Clear text
      muteFlag = 30;                                        // ca. 3 sec.
    }
    return;
  }
#ifdef ENABLE_SOAP     
  if (buttonMediaserver) {                                  // Handle button requesting Mediaserver mode
    dbgprint("Mediaserver mode requested");
    buttonMediaserver = false;
    mp3nodeList.clear();                                    // Free memory space
    SD_mp3fileCount = 0;
    if (playMode != MEDIASERVER) {
      tftset(0, "ESP32 DLNA");                              // Set screen segment top line
      //displaytime ("");                                   // Time to be refreshed
      if (dataMode != STOPPED) {
        dbgprint("STOP (buttonMediaserver)");
        dataMode = STOPREQD;                                // Request STOP
        muteFlag = 30;                                      // 3 secs muted
      }
      if (playMode == STATION)
        stopMp3client();                                    // Sockets are sparse, so we need to close it here already
      playMode = MEDIASERVER;
      encoderMode = IDLING;
      mp3fileRepeatFlag = NOREPEAT;
      xQueueReset(dataQueue);
    }
    if (encoderMode != SELECT) {                            // do nothing if already selecting tracks
      // we are not in SELECT mode yet 
      soapChain.clear();
      soapChain_t tmpId = { .id = "0", .name = "Root", .size = 0, .startingIndex = 0 };
      soapChain.push_back(tmpId);                           // first entry always represents root = "0"
      dbgprint("Search in media server container \"0\" (Root)");
      bool ret = soap.browseServer(0, "0", &soapList);
      if (ret) {           
        // browsing root of mediaserver was successful
        if (soapList.size() > 0) {
          tftset(4, "Turn to select directory  or track.\n" // Show current option
                    "Press to confirm.");
          enc_nodeIndex = 0;                                // first entry on that level 
        //enc_dirIndex = 0;                                 // parent dir is root (0)
          tmp = soapList[enc_nodeIndex].name;
          encoderMode = SELECT;                             // Swich to SELECT mode
          dbgprint("Encoder mode set to SELECT");
        }
        else {
          encoderMode = IDLING;                             // Swich to SELECT mode
          tmp = "No media server content !";
          //tftset(4, "");
        }
        if (soapList.size() == 0 || soapList[enc_nodeIndex].isDirectory)
          tftset(2, tmp);
        else
          tftset(1, tmp);
      }
      else { 
        // error accessing media server
        dbgprint("buttonMediaserver: error accessing media server");
        encoderMode = IDLING;
        playMode = NONE;
        tftset(1, "Error accessing      media server !");
        tftset(4, "Error");
        // probably media server still sleeping, send WOL message
        if (ini_block.srv_macWOL.length() > 0) {
          dbgprint("Send wake up message to MAC: %s", ini_block.srv_macWOL.c_str());
          soap.wakeUpServer(ini_block.srv_macWOL.c_str()); 
        }
      }
    }
    // TEST
    dbgprint("Total free memory of all regions=%d (minEver=%d), freeHeap=%d (minEver=%d), minStack=%d (in Bytes)", 
             ESP.getFreeHeap(), ESP.getMinFreeHeap(), xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)); 
    //              
    return;
  }  
#endif  
  if (singleClick) {
    dbgprint("Single click");
    singleClick = false;
    if (encoderMode == SELECT) {
      if (playMode == STATION) {
        encoderMode = IDLING;                                // back to default mode
        currentPreset = -1;                                  // make sure current preset is different
        ini_block.newpreset = enc_preset;                    // make a definite choice
        //tftset(4, "");                                     // clear text
        dbgprint("ini_block.newpreset=%d", ini_block.newpreset);
        muteFlag = 30;                                       // ca. 3 sec.
        mp3fileRepeatFlag = NOREPEAT;
      }
      else if (playMode == SDCARD) {
        if (!mp3nodeList[enc_nodeIndex].isDirectory) {
          // regular file
          if (dataMode != STOPPED) {
            dbgprint("STOP (singleClick & playMode == SDCARD)");
            dataMode = STOPREQD;                             // Request STOP, do not touch longClick flag
          }
          encoderMode = IDLING;                              // Back to default mode
          enc_filename = getSDfilename(enc_nodeIndex);       // Set new filename
          host = enc_filename;                               // Selected track as new host
          if (host.startsWith("error")) {
            dbgprint("SD problem: Can't find track");
            tftset(4, "Error");                              // text on bottom in green
          }
          else {
            dbgprint("Selected host=%s", host.c_str());
            currentIndex = enc_nodeIndex;
            hostreq = true;                                   // Request this host
            mp3fileRepeatFlag = NOREPEAT;
            mp3filePause = false;
            muteFlag = 10;                                    // ca. 1 sec.
            //xQueueReset (dataQueue);
            //tftset(4, "");                                  // Clear text
          }
        }
        else {
          // try to enter selected directory
          if ((inx = firstSDindexInDir(enc_nodeIndex)) != enc_nodeIndex) {
            // directory contains entries (files or dirs)
            enc_dirIndex = enc_nodeIndex;
            enc_nodeIndex = inx;                               // new current index
            tmp = mp3nodeList[inx].name;
            if (mp3nodeList[inx].isDirectory) {
              tftset(2, tmp);                                // show directories in yellow
            }  
            else {
              if ((inx = tmp.indexOf(".mp3")) > 0 ||
                  (inx = tmp.indexOf(".MP3")) > 0) {
                tmp = tmp.substring(0, inx);                 // remove file extension
              }
              tftset(1, tmp);                                // show files in cyan
            }
          }
        }
      }
#ifdef ENABLE_SOAP      
      else { // playMode == MEDIASERVER
        if (soapList[enc_nodeIndex].isDirectory) {
          // enter directory when searchable and not empty
          if (soapList[enc_nodeIndex].size > 0 &&                  // container contains elements
              soapList[enc_nodeIndex].searchable) {                // and is searchable
            soapChain_t tmpId;
            tmpId.id = soapList[enc_nodeIndex].id;
            tmpId.name = soapList[enc_nodeIndex].name;
            tmpId.size = soapList[enc_nodeIndex].size;
            tmpId.startingIndex = 0;
            dbgprint("Search in media server container \"%s\" (child count: %d)", 
                     soapList[enc_nodeIndex].name.c_str(), soapList[enc_nodeIndex].size);
            bool ret = soap.browseServer(0, soapList[enc_nodeIndex].id.c_str(), &soapList);
            if (ret) { 
              // browsing media server was successful
              if (soapList.size() > 0) {
                tftset(4, "Turn to select directory  or track.\n"  // Show current option
                          "Press to confirm.");
                soapChain.push_back(tmpId);                        // to find our way back up          
                enc_nodeIndex = 0;                                 // first entry on that level 
                tmp = soapList[enc_nodeIndex].name;
                if (soapList[enc_nodeIndex].isDirectory) {
                  tftset(2, tmp);                                  // show directories in yellow
                }
                else {
                  if ((inx = tmp.indexOf(".mp3")) > 0 ||
                      (inx = tmp.indexOf(".MP3")) > 0) {
                    tmp = tmp.substring(0, inx);                   // remove extension (if any)
                  }
                  tftset(1, tmp);                                  // show files in cyan
                }
              }
              else {
                // strange error: container should contain elements but browse list is empty
                dbgprint("singleClick: error browsing container on media server - list size: 0 !");
                //tftset(1, "Error browsing       media server !");
                tftset(4, "Error: no items returned");
              }
            }
            else {
              // error accessing media server
              encoderMode = IDLING;
              playMode = NONE;
              dbgprint("singleClick: error accessing media server");
              tftset(1, "Error accessing      media server !");
              tftset(4, "Error");
            }
          } 
          else {
            tftset(4,"Sorry, no content !");
            delay(1300);
            tftset(4, "Turn to select directory  or track.\n"        // Show current option
                      "Press to confirm.");
          } 
        }
        else {
          // item in list is a file
          if (dataMode != STOPPED) {
            dbgprint("STOP (singleClick & playMode == MEDIASERVER)");
            dataMode = STOPREQD;                                     // Request STOP, do not touch longClick flag
          } 
          String uriLc = soapList[enc_nodeIndex].uri;
          uriLc.toLowerCase();
          if (soapList[enc_nodeIndex].size == 0 ||
              soapList[enc_nodeIndex].fileType != fileTypeAudio ||
              (!uriLc.endsWith(".mp3"))) {
            dbgprint("error: %s is not a mp3 track", soapList[enc_nodeIndex].name.c_str());
            tftset(4, "Error: Not a MP3 track !");                   // text on bottom in green
          }
          else {
            host = "soap/" + soapList[enc_nodeIndex].uri;
            hostObject = soapList[enc_nodeIndex];
            dbgprint("Selected host=%s", host.c_str());
            encoderMode = IDLING;                                    // Back to default mode
            currentIndex = enc_nodeIndex;
            hostreq = true;                                          // Request this host
            mp3fileRepeatFlag = DIRECTORY;
            mp3filePause = false;  
            muteFlag = 20;                                           // ca. 2 sec.
            //xQueueReset (dataQueue);
            //tftset(4, "");                                         // Clear text
          }
        }
      }
#endif      
    }
    else { // not in SELECT mode
      // we mute when in station mode and we pause when in sdcard mode
      if (SD_okay && playMode == SDCARD &&                       // while playing files from SD
          currentSource == SDCARD && dataMode == DATA) {
        mp3filePause = !mp3filePause;
        dbgprint("Playing mp3 file %s", mp3filePause ? "has paused." : "continues.");
        if (mp3filePause) {
          xQueueReset (dataQueue);
        }
      }
      else {
        muteFlag = (!muteFlag) ? -1 : 0;
        dbgprint("Output is now %s", muteFlag ? "muted" : "unmuted");
      }
    }
    // TEST
    dbgprint("Total free memory of all regions=%d (minEver=%d), freeHeap=%d (minEver=%d), minStack=%d (in Bytes)", 
             ESP.getFreeHeap(), ESP.getMinFreeHeap(), xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)); 
    //
    return;
  }
  if (rotationcount == 0) return;
  
  // Evaluating Encoder turns
  dbgprint("rotationcount: %d", rotationcount);
  // TEST
  //dbgprint("rotationcount=%d, dataMode=%d, playMode=%d, totalCount=%d, currentSource=%d, currentPreset=%d, encoderMode=%d",
  //            rotationcount, dataMode, playMode, totalCount, currentSource, currentPreset, encoderMode);
  if (encoderMode == IDLING) {
    if ((SD_okay && playMode == SDCARD &&                   // while playing files from SD
         currentSource == SDCARD && dataMode == DATA && !mp3filePause) ||
        (playMode == MEDIASERVER &&                         // or playing from media server
         currentSource == MEDIASERVER && dataMode == DATA)) {
      if (rotationcount > 0) {
        if (playMode == SDCARD || playMode == MEDIASERVER)
          mp3fileJumpForward = true;
      }  
      else {  
        if (playMode == SDCARD)
          mp3fileJumpBack = true;
      }  
    }
    else if (playMode == STATION) {                        // we are in station mode
      encoderMode = SELECT;                                // Switch to SELECT mode
      dbgprint("Encoder mode set to SELECT");
      tftset(4, "Turn to select station.\n"                // Show current option
               "Press to confirm.");
      if (currentPreset >= 0 && currentPreset <= highestPreset)
        enc_preset = currentPreset;
      else
        enc_preset = 0;
      tmp = readhostfrompref(enc_preset);                  // get host spec and possible comment
      // Show just comment if available.  Otherwise the preset itself.
      if ((inx = tmp.indexOf("#")) > 0) {                  // hash sign present?
        tmp.remove(0, inx + 1);                            // remove non-comment part
      }
      chomp(tmp);                                          // remove garbage from description
      dbgprint("Selected station is %d, %s", enc_preset, tmp.c_str());
      tftset(2, tmp);                                      // Set screen segment
    }
#ifdef ENABLE_SOAP      
    else { // playMode == MEDIASERVER
      // is handled above in SDCARD branch already
    }
#endif
  }
  else { // encoderMode == SELECT
    if (playMode == STATION) {
      if ((enc_preset + rotationcount) < 0) {
        enc_preset = highestPreset;                          // from 0 to highest preset
      }
      else {
        enc_preset += rotationcount;                         // next preset
      }
      tmp = readhostfrompref (enc_preset);                   // get host spec and possible comment
      if (tmp == "") {                                       // end of presets?
        enc_preset = 0;                                      // wrap
        tmp = readhostfrompref (enc_preset);                 // get host spec and possible comment
      }
      // Show just comment if available.  Otherwise the preset itself.
      if ((inx = tmp.indexOf("#")) > 0) {                    // hash sign present?
        tmp.remove (0, inx + 1);                             // remove non-comment part
      }
      chomp(tmp);                                            // remove garbage from description
      dbgprint("Selected station is %d, %s", enc_preset, tmp.c_str());
      tftset(2, tmp);                                        // set screen segment
    }
    else if (playMode == SDCARD) {
      if (!mp3nodeList[enc_nodeIndex].isDirectory && noSubDirInSameDir(enc_nodeIndex))
        // if knob is turned left we jump from first to last file if necessary
        enc_nodeIndex = nextSDfileIndexInSameDir (enc_nodeIndex, rotationcount);
      else
        // if knob is turned left we do not jump from first to last directory/file
        enc_nodeIndex = nextSDindexInSameDir (enc_nodeIndex, rotationcount);
      enc_dirIndex = mp3nodeList[enc_nodeIndex].parentDir;
      tmp = mp3nodeList[enc_nodeIndex].name;
      if (mp3nodeList[enc_nodeIndex].isDirectory) {
        tftset(2, tmp);
      }
      else {
        if ((inx = tmp.indexOf(".mp3")) > 0 ||
            (inx = tmp.indexOf(".MP3")) > 0) {
          tmp = tmp.substring(0, inx);
        }
        tftset(1, tmp);
      }
    }
#ifdef ENABLE_SOAP      
    else { // playMode == MEDIASERVER
      enc_nodeIndex += rotationcount;
      if (enc_nodeIndex < 0) {
        // begin of browse list reached
        if (soapChain.back().startingIndex > 0) {
          soapChain.back().startingIndex -= SOAP_DEFAULT_BROWSE_MAX_COUNT;
          if (soapChain.back().startingIndex < 0)
            soapChain.back().startingIndex = 0;
          // browse request with new starting index  
          dbgprint("Search again in media server container \"%s\" (child count: %d, startingIndex: %d)", 
                   soapChain.back().name.c_str(), soapChain.back().size, soapChain.back().startingIndex);
          bool ret = soap.browseServer(0, soapChain.back().id.c_str(), &soapList, soapChain.back().startingIndex);
          if (ret) { 
            // browsing mediaserver successful
            enc_nodeIndex = soapList.size() - 1;                 // new index in browse list
            if (soapList.size() > 0) {
              tftset(4, "Turn to select directory  or track.\n"  // Show current option
                        "Press to confirm.");
              tmp = soapList[enc_nodeIndex].name;
              if (soapList[enc_nodeIndex].isDirectory) {
                tftset(2, tmp);
              }
              else {
                String uriLc = soapList[enc_nodeIndex].uri;
                uriLc.toLowerCase();
                if (soapList[enc_nodeIndex].fileType != fileTypeAudio ||
                    !uriLc.endsWith(".mp3"))
                  tmp += " [not mp3]";
                tftset(1, tmp);
              }
            }
            else {
              // still to be done (very unlikely error)
            }
          }
          else {
            // error accessing media server
            encoderMode = IDLING;
            playMode = NONE;
            dbgprint("rotationcount %d: error accessing media server for more items", rotationcount);
            tftset(1, "Error accessing      media server !");
            tftset(4, "Error");
            rotationcount = 0;
            return;
          }
        }
        else {
          enc_nodeIndex = 0;                                     // no circling
          dbgprint("begin of list reached");
        }
      }  
      else if (enc_nodeIndex >= soapList.size()) {
        // end of browse list reached
        size_t sizeDir = soapChain.back().size;                  // elements in our current directory 
        if (sizeDir > (soapList.size() + soapChain.back().startingIndex)) {
          // items left to be shown
          size_t more = sizeDir - soapList.size() - soapChain.back().startingIndex;
          dbgprint("end of this browse list but %d more items to come in this directory", more);
          size_t newStartingIndex = soapChain.back().startingIndex + soapList.size();
          // browse request with new starting index  
           dbgprint("Search again in media server container \"%s\" (child count: %d, startingIndex: %d)", 
                     soapChain.back().name.c_str(), soapChain.back().size, newStartingIndex);
          bool ret = soap.browseServer(0, soapChain.back().id.c_str(), &soapList, newStartingIndex);
          if (ret) { 
            // browsing root of mediaserver was successful
            enc_nodeIndex = 0;                                   // new index in browse list
            soapChain.back().startingIndex = newStartingIndex;
            if (soapList.size() > 0) {
              tftset(4, "Turn to select directory  or track.\n"  // Show current option
                        "Press to confirm.");
              tmp = soapList[enc_nodeIndex].name;
              if (soapList[enc_nodeIndex].isDirectory) {
                tftset(2, tmp);
              }
              else {
                String uriLc = soapList[enc_nodeIndex].uri;
                uriLc.toLowerCase();
                if (soapList[enc_nodeIndex].fileType != fileTypeAudio ||
                    !uriLc.endsWith(".mp3"))
                  tmp += " [not mp3]";
                tftset(1, tmp);
              }
            }
            else {
              // still to be done (very unlikely error)
            }
          }
          else {
            // error accessing media server
            encoderMode = IDLING;
            playMode = NONE;
            dbgprint("rotationcount %d: error accessing media server for more items", rotationcount);
            tftset(1, "Error accessing      media server !");
            tftset(4, "Error");
            rotationcount = 0;
            soapChain.back().startingIndex = 0;
            return;
          }
        }
        else {
          enc_nodeIndex = soapList.size() - 1;
          dbgprint("no more items in this directory \"%s\" to come - end of list", soapChain.back().name.c_str());
        }
      }
      tmp = soapList[enc_nodeIndex].name;
      // rudimentary line limiting
#define MAX_NAME_LENGTH 96
      if (soapList[enc_nodeIndex].isDirectory) {
        if (tmp.length() > MAX_NAME_LENGTH) {
          tmp.remove(MAX_NAME_LENGTH - 3);
          tmp += "...";
        }  
        tftset(2, tmp);
      }
      else {
        String uriLc = soapList[enc_nodeIndex].uri;
        uriLc.toLowerCase();
        if (soapList[enc_nodeIndex].fileType != fileTypeAudio ||
            !uriLc.endsWith(".mp3")) {
          if (tmp.length() > MAX_NAME_LENGTH) {
            tmp.remove(MAX_NAME_LENGTH - 13);
            tmp += "...";
          }  
          tmp += " [not mp3]";
        }
        tftset(1, tmp);
      }
    }
#endif    
  }
  rotationcount = 0;
}

//**************************************************************************************************
//                                           M P 3 L O O P                                         *
//**************************************************************************************************
void mp3loop()
{
  static uint8_t  tmpbuff[5000];                        // Input buffer for mp3 stream
  uint32_t        maxchunk;                             // Max number of bytes to read
  int             res = 0;                              // Result reading from mp3 stream
  uint32_t        av = 0;                               // Available in stream
  int             fileIndex;                            // Next file index of track on SD
  uint32_t        qspace;                               // Free space in data queue
  uint32_t        i;

  if (dataMode == STOPREQD) {                           // STOP requested?
    dbgprint("mp3loop: STOP requested");
    //if (currentSource != SDCARD && currentSource != MEDIASERVER)
      xQueueReset (dataQueue);
    if (currentSource == SDCARD) {
      dbgprint("mp3loop: close mp3file");
      claimSPI("close");                                // claim SPI bus
      mp3file.close();
      releaseSPI();                                     // release SPI bus
      mp3fileLength = mp3fileBytesLeft = 0;
    }
    else if (currentSource == STATION) {
      stopMp3client();                                  // Disconnect if still connected
    }
#ifdef ENABLE_SOAP    
    else { // MEDIASERVER
      soap.readStop();
      mp3fileLength = mp3fileBytesLeft = 0;
      dbgprint("mp3loop: media server data connection closed");
    }
#endif    
    chunked = false;                                    // Not longer chunked
    datacount = 0;                                      // Reset datacount
    outqp = outchunk.buf;                               // and pointer
    //if (currentSource != SDCARD && currentSource != MEDIASERVER)    
      queuefunc(QSTOPSONG);                             // Queue a request to stop the song
    metaint = 0;                                        // No metaint known now
    dataMode = STOPPED;                                 // yes, state becomes STOPPED
    currentSource = NONE;                               // currently no socket open
  }
  // Try to keep the Queue to playTask filled up by adding as much bytes as possible
  if (dataMode & (INIT | HEADER | DATA |                // Test if playing
                  METADATA | PLAYLISTINIT |
                  PLAYLISTHEADER | PLAYLISTDATA)) {
    maxchunk = sizeof(tmpbuff);                         // Reduce byte count for this mp3loop()
    qspace = uxQueueSpacesAvailable(dataQueue) *        // Compute free space in data queue
             sizeof(qdata_struct);
    if (currentSource == SDCARD) {                      // Playing file from SD card?
      if (SD_okay) {
        if (mp3filePause) {
          mp3fileJumpForward = false;
          mp3fileJumpBack = false;
        }
        else {
          if (mp3fileJumpForward) {
            int jumpSize, pos;
            muteFlag = 3;                                  // temporarily muteing helps against chirps
            mp3fileJumpForward = false;
            jumpSize = mp3fileLength / 10;
            jumpSize >>= 2; jumpSize <<= 2;
            if (mp3fileBytesLeft < jumpSize) {
              jumpSize = mp3fileBytesLeft;
            }
            claimSPI("sdread1");                            // claim SPI bus
            pos = mp3file.position();
            mp3file.seek(pos + jumpSize);
            releaseSPI();                                   // release SPI bus
            mp3fileBytesLeft -= jumpSize;                   // Number of bytes left
            xQueueReset (dataQueue);
            qspace = uxQueueSpacesAvailable(dataQueue) *    // recalculate free space in data queue
                     sizeof(qdata_struct);
            //dbgprint("F mp3fileLength=%d, jumpSize=%d, pos=%d, neu mp3fileBytesLeft=%d",
            //           mp3fileLength, jumpSize, pos, mp3fileBytesLeft);
          }
          else if (mp3fileJumpBack) {
            int jumpSize, pos;
            muteFlag = 3;                                   // temporarily muteing helps against chirps
            mp3fileJumpBack = false;
            jumpSize = mp3fileLength / 10;
            jumpSize >>= 2; jumpSize <<= 2;
            if (jumpSize > mp3fileLength - mp3fileBytesLeft) {
              jumpSize = mp3fileLength - mp3fileBytesLeft;
            }
            claimSPI("sdread2");                            // claim SPI bus
            pos = mp3file.position();
            mp3file.seek(pos - jumpSize);
            releaseSPI();                                   // release SPI bus
            mp3fileBytesLeft += jumpSize;                   // Number of bytes left
            xQueueReset (dataQueue);
            qspace = uxQueueSpacesAvailable(dataQueue) *    // recalculate free space in data queue
                     sizeof(qdata_struct);
          }
          av = mp3fileBytesLeft;                            // Bytes left in file
          if (maxchunk > av) {                              // Reduce byte count for this mp3loop()
            maxchunk = av;
          }
          if (maxchunk > qspace) {                          // Enough space in queue?
            maxchunk = qspace;                              // No, limit to free queue space
          }
          if (maxchunk) {                                   // Anything to read?
            claimSPI("sdread3");                            // claim SPI bus
            res = mp3file.read (tmpbuff, maxchunk);         // Read a block of data
            releaseSPI();                                   // release SPI bus
            mp3fileBytesLeft -= res;                        // Number of bytes left
            if (res <= 0) {
              dbgprint("mp3loop: STOP (mp3file.read() error)");
              dataMode = STOPREQD;
              SD_okay = false;
              currentIndex = -1;
              mp3fileLength = mp3fileBytesLeft = 0;
              tftset(1, "SD-Card Error !");
              tftset(4, "Error reading SD card");
              // not needed
              //claimSPI("sdclose");                        // claim SPI bus
              //mp3file.close();                            // close file
              //releaseSPI();                               // release SPI bus
            }
          }
        }
      }
    }
    else if (currentSource == STATION) { // STATION
      _claimSPI("mp3loop1");                               // claim SPI bus
      av = mp3client.available();                          // Available from stream
      _releaseSPI();                                       // release SPI bus
      if (maxchunk > av) {                                 // Limit read size
        maxchunk = av;
      }
      if (maxchunk > qspace) {                          // Enough space in queue?
        maxchunk = qspace;                              // No, limit to free queue space
      }
      if (maxchunk) {                                   // Anything to read?
        _claimSPI("mp3loop2");                          // claim SPI bus
        res = mp3client.read(tmpbuff, maxchunk);        // Read a number of bytes from the stream
        _releaseSPI();                                  // release SPI bus
        if (res == 0 || res == -1)
          dbgprint("res: %d", res); 
      }
      else {
        if (dataMode == PLAYLISTDATA) {                    // End of playlist
          playlist_num = 0;                                // And reset
          dbgprint("End of playlist seen");
          dataMode = STOPPED;
          ini_block.newpreset++;                           // Go to next preset
        }
      }
    }
#ifdef ENABLE_SOAP    
    else { // MEDIASERVER
      if (mp3fileJumpForward) {
        mp3fileJumpForward = false;
        muteFlag = 2;                                      // temporarily muteing helps against chirps
        // LS Mini has problems -> [RST, ACK] in Wireshark from LS Mini ... why ???
        int jumpSize = mp3fileLength / 10;
        jumpSize >>= 2; jumpSize <<= 2;
        if (mp3fileBytesLeft < jumpSize) {
          jumpSize = mp3fileBytesLeft;
        }
        int toRead = jumpSize;
        for (; toRead > 0;) {
          int ret = soap.read(tmpbuff, sizeof(tmpbuff) < toRead ? sizeof(tmpbuff) : toRead);
          if (ret <= 0) {
            // read error or EOF
            soap.readStop();
            break;
          }  
          toRead -= ret;
          //mp3fileBytesLeft -= ret;                         // Number of bytes left
        }
        mp3fileBytesLeft = soap.available();               // Bytes left in file
        xQueueReset (dataQueue);
        qspace = uxQueueSpacesAvailable(dataQueue) *       // recalculate free space in data queue
                  sizeof(qdata_struct);
      }
      av = soap.available();                               // Bytes left in file
      //av = mp3fileBytesLeft;
      if (maxchunk > av) {                                 // Reduce byte count for this mp3loop()
        maxchunk = av;
      }
      if (maxchunk > qspace) {                             // Enough space in queue?
        maxchunk = qspace;                                 // No, limit to free queue space
      }
      if (maxchunk) {                                      // Anything to read?
        res = soap.read(tmpbuff, maxchunk);                // Read a block of data
        mp3fileBytesLeft -= res;                           // Number of bytes left
        if (res <= 0) {
          soap.readStop();
          dbgprint("mp3loop: STOP (soap.read() error)");
          dataMode = STOPREQD;
          mp3fileLength = mp3fileBytesLeft = 0;
          tftset(1, "Media Server Error !");
          tftset(4, "Error reading from media server");
        }
      }
    }
#endif    
    for (i = 0; i < res; i++) {
      handlebyte_ch(tmpbuff[i]);                           // Handle one byte
    }
  }
  if (currentSource == SDCARD) {                           // Playing from SD?
    if (dataMode & DATA && !mp3filePause &&                // Test if playing
        av == 0) {                                         // End of mp3 data?
      dbgprint("mp3loop: STOP (end of mp3 file -> close mp3file)");
      claimSPI("close2");                                  // claim SPI bus
      mp3file.close();                                     // Close file
      releaseSPI();                                        // release SPI bus
      dataMode = STOPREQD;                                 // End of local mp3-file detected
      delay(100);
      if (SD_okay && currentIndex > 0) {
        if (!mp3fileRepeatFlag)
          fileIndex = nextSDfileIndex(currentIndex, +1);   // Select the next file on SD
        else if (mp3fileRepeatFlag == SONG)
          fileIndex = currentIndex;                        // Select the same file
        else if (mp3fileRepeatFlag == DIRECTORY)
          fileIndex = nextSDfileIndexInSameDir(currentIndex, +1); // select next file in same directory
        else
          fileIndex = 0;                                   // random mode
        host = getSDfilename(fileIndex);
        if (host.startsWith("error")) {
          dbgprint("SD problem: Can't find file with index %d", fileIndex);
        }
        else {
          hostreq = true;                                  // Request this host
        }
      }
    }
  }
#ifdef ENABLE_SOAP  
  else if (currentSource == MEDIASERVER) {
    if (dataMode & DATA && av == 0) {                      // playing and end of mp3 data? 
      dbgprint("mp3loop: STOP (end of file from media server)");
      soap.readStop();
      dataMode = STOPREQD;                                     // End of local mp3-file detected
      delay(100);
      if (mp3fileRepeatFlag != NOREPEAT) {
        int16_t newIndex;
        if (mp3fileRepeatFlag == SONG) {
          newIndex = currentIndex;                             // play same file
        }
        else {
          newIndex = nextSoapFileIndex(currentIndex, +1);      // Select next mp3 file in list
        }
        if (newIndex != -1) {
          dbgprint("Next file soapList[%d]: \"%s\"",newIndex, soapList[newIndex].name.c_str());
          host = "soap/" + soapList[newIndex].uri;
          hostObject = soapList[newIndex];
          currentIndex = newIndex;
          hostreq = true;                                      // Request this host
        }
        else {
          dbgprint("Soap list problem: Can't find next entry in list");
        }
      }
      else {
        // stop playing after this file and show directory content
        String tmp;
        soapChain_t dir = soapChain.back();                    // retrieve last element in chain
        dbgprint("Search in media server container \"%s\" (child count: %d)", 
                 dir.name.c_str(), dir.size);
        bool ret = soap.browseServer(0, dir.id.c_str(), &soapList);
        if (ret) {                                             // browsing mediaserver one level higher
          encoderMode = SELECT;
          if (soapList.size() > 0) {
            tftset(4, "Turn to select directory  or track.\n"  // Show current option
                      "Press to confirm.");
            currentIndex = 0;                                  // first entry on that level 
            tmp = soapList[currentIndex].name;
            if (soapList[currentIndex].isDirectory) {
              tftset(2, tmp);
            }
            else {
              String uriLc = soapList[currentIndex].uri;
              uriLc.toLowerCase();
              if (soapList[currentIndex].fileType != fileTypeAudio ||
                  !uriLc.endsWith(".mp3"))
                tmp += " [not mp3]";
              tftset(1, tmp);
            }
          }
        }
        else {
          // error accessing media server
          encoderMode = IDLING;
          playMode = NONE;
          dbgprint("End of file: error re-accessing media server");
          tftset(1, "End of file.         Error re-accessing   media server !");
          tftset(4, "Error");
        }
      }
    }
  }
#endif  
  if (ini_block.newpreset != currentPreset) {              // new station or next from playlist requested?
    dbgprint("mp3loop: ini_block.newpreset[%d] != currentPreset[%d]",
               ini_block.newpreset, currentPreset);
    if (dataMode != STOPPED) {                             // Yes, still busy?
      dbgprint("mp3loop: STOP7 (dataMode != STOPPED)");
      dataMode = STOPREQD;                                 // Yes, request STOP
    }
    else {
      if (playlist_num) {                                  // Playing from playlist?
        // Yes, retrieve URL of playlist
        playlist_num += ini_block.newpreset -
                        currentPreset;                     // Next entry in playlist
        ini_block.newpreset = currentPreset;               // Stay at current preset
      }
      else {
        host = readhostfrompref();                         // Lookup preset in preferences
        chomp (host);                                      // Get rid of part after "#"
      }
      dbgprint("mp3loop: New preset/file requested (%d/%d) from %s",
               ini_block.newpreset, playlist_num, host.c_str());
      if (host != "") {                                    // Preset in ini-file?
        hostreq = true;                                    // Force this station as new preset
      }
      else {
        // This preset is not available, return to preset 0, will be handled in next mp3loop()
        dbgprint("mp3loop: No host for this preset");
        ini_block.newpreset = 0;                           // Wrap to first station
      }
    }
  }
  if (hostreq) {                                           // New preset or station?
    dbgprint("mp3loop: hostreq=true");
    hostreq = false;
    mp3filePause = false;
    currentPreset = ini_block.newpreset;                   // Remember current preset

    // Check source type
    if (host.indexOf("sdcard/") == 0)
      currentSource = SDCARD;
    else if (host.indexOf("soap/") == 0)
      currentSource = MEDIASERVER;
    else
      currentSource = STATION;
    
    if (currentSource == SDCARD) {                         // play file from SD card?
      muteFlag = 15;
      playMode = SDCARD;
      mp3fileJumpForward = false;
      mp3fileJumpBack = false;
      if (connecttofile()) {                               // open mp3-file
        dataMode = DATA;                                   // start in DATA mode
        dbgprint("mp3loop: connecttofile() returns ok, dataMode=DATA");
      }
      else {
        SD_okay = false;
        currentIndex = -1;
        dataMode = STOPPED;                                // error happened by opening file on SD
        currentSource = NONE;
        host = "";
        dbgprint("mp3loop: connecttofile() returned error");
        tftset(1, "SD-Card Error !         If SD was swapped, please press SD Button again.");
        tftset(4, "Error opening file.");
      }
    }
    else if (currentSource == STATION) {
      muteFlag = 25;
      playMode = STATION;
      if (host.startsWith("ihr/")) {                       // iHeartRadio station requested?
        host = host.substring(4);                          // Yes, remove "ihr/"
        host = xmlgethost(host);                           // Parse the xml to get the host
      }
      if (host.length() > 0) {                             // Basic check
#ifdef USE_ETHERNET
        if (reInitEthernet) {                              // set in timer10sec()
          reInitEthernet = false;
          dbgprint("mp3loop: we are requested to re-initialize the ethernet card");
          initEthernet(false);                             // no TFT output
        }
#endif        
        connectToHost();                                   // Switch to new host
      }
      else {
        dbgprint("mp3loop: host value invalid. Try next preset.");
        ini_block.newpreset++;
        if (ini_block.newpreset > highestPreset)
          ini_block.newpreset = 0;
      }
    }
#ifdef ENABLE_SOAP    
    else { // MEDIASERVER
      #define MAX_DLNA_RETRIES 3
      for (i = 0; i < MAX_DLNA_RETRIES; i++) {
        if (soap.readStart(&hostObject, &mp3fileLength)) break;  // request media server file
        delay(200);
      }
      if (i == MAX_DLNA_RETRIES) {
        // error requesting file from media server
        dataMode = STOPPED;
        currentSource = NONE;
        host = "";
        tftset(4, "Error retrieving file.");
        dbgprint("readStart(%s:%d/%s) returned error", 
                 hostObject.downloadIp.toString().c_str(), hostObject.downloadPort, hostObject.uri.c_str());
      }
      else {
        mp3fileBytesLeft = mp3fileLength;                  // file length as reported from media server
        dbgprint("readStart(%s:%d/%s) successful", 
                 hostObject.downloadIp.toString().c_str(), hostObject.downloadPort, hostObject.uri.c_str());
        if (handleID3(host)) {                             // check for ID3 tags
          dataMode = DATA;                                 // Start in DATA mode
          icyname = "";                                    // No icy name yet
          chunked = false;                                 // File not chunked
          metaint = 0;                                     // no meta data
          dbgprint("mp3loop: handleID3() returns ok, dataMode=DATA");
        }
        else {
          // error reading from media server
          dataMode = STOPPED;
          currentSource = NONE;
          tftset(4, "Error reading file from media server.");
          dbgprint("handleID3(\"%s\") returned error", host.c_str());          
          host = "";
        }
      }
    }
#endif
  }
}

#ifdef PORT23_ACTIVE
//**************************************************************************************************
//                          H A N D L E C L I E N T O N P O R T 2 3                                *
//**************************************************************************************************
// Handle connect/disconnect of clients on port 23. Debug output gets sent via dbgprint() if open. *
//**************************************************************************************************
void handleClientOnPort23()
{
  static char dbgTextBuf[MAXSIZE_TELNET_CMD];          //someplace to put received cmd
  static int  dbgCharsReceived = 0;
  static bool EOLseen = false;
  
  if (!dbgConnectFlag)
  {
    _claimSPI("port231");                          // claim SPI bus
    dbgclient = dbgserver.accept();
    _releaseSPI();                                   // release SPI bus
    if (dbgclient)
    {
      dbgConnectFlag = true;
      _claimSPI("port232");                        // claim SPI bus
      dbgclient.print("\r\nESP32-Radio Debug Server on port 23\r\n");
#ifdef CHECK_LOOP_TIME
      dbgclient.print("Press 't'+ENTER to reset loop timer.\r\n");
#endif
      dbgclient.print("Press 'q'+ENTER to quit.\r\n\r\n");
      //dbgclient.println("");
      _releaseSPI();                                 // release SPI bus
      dbgprint("Debug Client on port 23 connected.");  // Get's already sent to debug client on port 23
    }
  }
  else { // connect flag still set
    _claimSPI("port233");                          // claim SPI bus
    // we don't accept another client, so recline
    //dbgserver.available().stop();                   // not needed... ?
    uint8_t con = dbgclient.connected();
    _releaseSPI();                                   // release SPI bus
    if (con) {                                        // debug client still connected ?
      _claimSPI("port234");                        // claim SPI bus
      int charsWaiting = dbgclient.available();
      _releaseSPI();                                 // release SPI bus
      if (charsWaiting) {                            // data available
        do {
          _claimSPI("port235");                    // claim SPI bus
          char c = dbgclient.read();
          _releaseSPI();                             // release SPI bus
          dbgTextBuf[dbgCharsReceived++] = c;
          charsWaiting--;
          if (c ==  0x0D)                            // end of line (Enter key)
            EOLseen = true;
        } while (dbgCharsReceived <= MAXSIZE_TELNET_CMD && charsWaiting > 0);   
        if (EOLseen) {
          EOLseen = false;
          dbgCharsReceived = 0;
          switch (dbgTextBuf[0]) {
            case 0x0D: break;                         // ignore empty line
            case 'q':  
              _claimSPI("port236");                // claim SPI bus
              dbgclient.print("\r\nBye bye.\r\n");
              dbgclient.flush();
              dbgclient.stop();
              _releaseSPI();                         // release SPI bus
              dbgConnectFlag = false;
              dbgprint("Debug Client on port 23 disconnected on his request.");
              break;
            case '?':
              _claimSPI("port237");                // claim SPI bus
#ifdef CHECK_LOOP_TIME
              dbgclient.print("Press 't'+ENTER to reset loop timer.\r\n");
#endif
              dbgclient.print("Press 'q'+ENTER to quit.\r\n\r\n");
              //dbgclient.println("");
              _releaseSPI();                         // release SPI bus
              break;            
#ifdef CHECK_LOOP_TIME
            case 't':                                  // CTRL-T resets loop timer
              maxLoopTime = 0;
              dbgprint("Loop timer resetted.");
              break;
#endif
            default: 
              _claimSPI("port238");                // claim SPI bus
              dbgclient.print("\r\nUnrecognized command.  ? for help.\r\n");
              _releaseSPI();                         // release SPI bus
              break;
          }
        }
        if (dbgCharsReceived >= MAXSIZE_TELNET_CMD) {
          dbgCharsReceived = 0;
          //EOLseen = false;
          _claimSPI("port239");                    // claim SPI bus
          dbgclient.print("\r\nUnrecognized command.  ? for help.\r\n");
          _releaseSPI();                             // release SPI bus
        }
      }
    }
    else { // connection gone (eg. client has disconnected)
      _claimSPI("port2310");                       // claim SPI bus
      dbgclient.flush();
      dbgclient.stop();
      _releaseSPI();                                 // release SPI bus
      dbgConnectFlag = false;                         // remember connection state
      EOLseen = false;
      dbgCharsReceived = 0;
      dbgprint("Debug Client on port 23 has disconnected.");
    }
  }
}
#endif

//**************************************************************************************************
//                                           L O O P                                               *
//**************************************************************************************************
// Main loop of the program.                                                                       *
//**************************************************************************************************
void loop()
{
  //delay(1);                                             // resets task switcher watchdog, just in case it's needed
#ifdef ENABLE_ESP32_HW_WDT
  esp_task_wdt_reset();                                 // avoid panic and subsequent ESP32 restart
#endif   
#ifdef CHECK_LOOP_TIME
  uint32_t timing;                                      // for calculation of loop() round times
  timing = millis();
#endif
  mp3loop();                                            // Do mp3 related actions
  if (resetreq) {                                       // Reset requested?
    delay(1000);                                        // Yes, wait some time
    ESP.restart();                                      // Reboot
  }
  scanserial();                                         // Handle serial input
#ifdef ENABLE_DIGITAL_INPUTS  
  scandigital();                                        // Scan digital inputs
#endif  
#ifdef ENABLE_INFRARED
  scanIR();                                             // See if IR input
#endif
#ifndef USE_ETHERNET
  ArduinoOTA.handle();                                  // Check for OTA
#endif
  mp3loop();                                            // Do more mp3 related actions
#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
  handlehttpreply();
  _claimSPI("loop");                                    // claim SPI bus
  cmdclient = cmdserver.available();                    // Check Input from client
  _releaseSPI();                                        // release SPI bus
  if (cmdclient) {                                      // Client connected ?
    dbgprint("loop(): command client available");
    handlehttp();
  }
#endif  
  //
  handleSaveReq();                                      // See if time to save settings
  checkEncoderAndButtons();                             // check rotary encoder & button functions
#ifdef PORT23_ACTIVE
  handleClientOnPort23();                               // check possible debug client requests
#endif
#ifdef CHECK_LOOP_TIME
  timing = millis() - timing;
  if (timing > maxLoopTime) {                           // new maximum found?
    maxLoopTime = timing;                               // yes, set new maximum
    dbgprint("Max duration loop() = %d ms", timing);    // and report it
  }
#endif
}

//**************************************************************************************************
//                                    C H K H D R L I N E                                          *
//**************************************************************************************************
// Check if a line in the header is a reasonable headerline.                                       *
// Normally it should contain something like "icy-xxxx:abcdef".                                    *
//**************************************************************************************************
bool chkhdrline(const char* str)
{
  char b;                                             // Byte examined
  int  len = 0;                                       // Length of string

  while ((b = *str++)) {                              // Search to end of string
    len++;                                            // Update string length
    if (!isalpha(b)) {                                // Alpha (a-z, A-Z)
      if (b != '-') {                                 // Minus sign is allowed
        if (b == ':') {                               // Found a colon?
          return ((len > 5) && (len < 50));           // Yes, okay if length is okay
        }
        else {
          return false;                               // Not a legal character
        }
      }
    }
  }
  return false;                                       // End of string without colon
}

//**************************************************************************************************
//                            S C A N _ C O N T E N T _ L E N G T H                                *
//**************************************************************************************************
// If the line contains content-length information: set clength (content length counter).          *
//**************************************************************************************************
void scan_content_length(const char* metalinebf)
{
  if (strstr(metalinebf, "Content-Length")) {        // Line contains content length
    clength = atoi(metalinebf + 15);                 // Yes, set clength
    dbgprint("Content-Length is %d", clength);       // Show for debugging purposes
  }
}

//**************************************************************************************************
//                                   H A N D L E B Y T E _ C H                                     *
//**************************************************************************************************
// Handle the next byte of data from server or mp3 file.                                           *
// Chunked transfer encoding aware. Chunk extensions are not supported.                            *
//**************************************************************************************************
void handlebyte_ch(uint8_t b)
{
  static int       chunksize = 0;                      // Chunkcount read from stream
  static uint16_t  playlistcnt;                        // Counter to find right entry in playlist
  static int       LFcount;                            // Detection of end of header
  static bool      ctseen = false;                     // First line of header seen or not
  static bool      nameseen = false;                   // Name of station seen
  static bool      brseen = false;                     // Bitrate of station seen

  if (chunked &&
       (dataMode & (DATA |                             // Test op DATA handling
                    METADATA |
                    PLAYLISTDATA))) {
    if (chunkcount == 0) {                             // Expecting a new chunkcount?
      if (b == '\r') {                                 // Skip CR
        return;
      }
      else if (b == '\n') {                            // LF ?
        chunkcount = chunksize;                        // Yes, set new count
        chunksize = 0;                                 // For next decode
        return;
      }
      // We have received a hexadecimal character.  Decode it and add to the result.
      b = toupper(b) - '0';                            // Be sure we have uppercase
      if (b > 9) {
        b = b - 7;                                     // Translate A..F to 10..15
      }
      chunksize = (chunksize << 4) + b;
      return;
    }
    chunkcount--;                                      // Update count to next chunksize block
  }
  if (dataMode == DATA) {                              // Handle next byte of MP3/Ogg data
    *outqp++ = b;
    if (outqp == (outchunk.buf + sizeof(outchunk.buf))) { // Buffer full?
      outchunk.datatyp = QDATA;                        // This chunk dedicated to QDATA
      // Send data to playTask queue.  If the buffer cannot be placed within 200 ticks,
      // the queue is full, while the sender tries to send more.  The chunk will be dis-
      // carded in that case.
      xQueueSend(dataQueue, &outchunk, 200);           // send to queue (wait max 200 ticks [resp. 200ms])
      outqp = outchunk.buf;                            // item empty now
    }
    //*/
    if (metaint) {                                     // No METADATA on Ogg streams or mp3 files
      if (--datacount == 0) {                          // End of datablock?
        dataMode = METADATA;
        metalinebfx = -1;                              // Expecting first metabyte (counter)
      }
    }
    return;
  }
  if (dataMode == INIT) {                              // Initialize for header receive
    ctseen = false;                                    // Contents type not seen yet
    nameseen = false;                                  // Name of station not seen yet
    brseen = false;                                    // Bitrate of station not seen yet
    metaint = 0;                                       // No metaint found
    LFcount = 0;                                       // For detection end of header
    bitrate = 0;                                       // Bitrate still unknown
    dbgprint("Switch to HEADER"); 
    dataMode = HEADER;                                 // Handle header
    totalCount = 0;                                    // Reset totalCount
    metalinebfx = 0;                                   // No metadata yet
    metalinebf[0] = '\0';
    muteFlag = 16;                                     // ca. 1,6 sek.
  }
  if (dataMode == HEADER) {                            // Handle next byte of MP3 header
    b = utf8ascii(b);
    if ((b > 0x7F) ||                                  // Ignore unprintable characters
        (b == '\r') ||                                 // Ignore CR
        (b == '\0')) {                                 // Ignore NULL
      // Yes, ignore
    }
    else if (b == '\n') {                              // Linefeed ?
      LFcount++;                                       // Count linefeeds
      metalinebf[metalinebfx] = '\0';                  // Take care of delimiter
      if (chkhdrline(metalinebf)) {                    // Reasonable input?
        dbgprint("Headerline: %s", metalinebf);        // Show headerline
        String metaline = String(metalinebf);          // Convert to string
        String lcml = metaline;                        // Use lower case for compare
        lcml.toLowerCase();
        if (lcml.startsWith("location: http://")) {    // Redirection?
          host = metaline.substring(17);               // Yes, get new URL
          hostreq = true;                              // And request this one
          dbgprint("Redirection to new host");
        }
        else if (lcml.indexOf("content-type") == 0) {  // Line beginning with "Content-Type: xxxx/yyy"
          ctseen = true;                               // Yes, remember seeing this
          //String ct = metaline.substring(13);        // Set contentstype. Not used yet
          //ct.trim();
          //dbgprint("%s seen.", ct.c_str());
        }
        else if (lcml.startsWith("icy-br:")) {
          brseen = true;
          bitrate = metaline.substring(7).toInt();     // Found bitrate tag, read the bitrate
          if (bitrate == 0) {                          // For Ogg br is like "Quality 2"
            bitrate = 87;                              // Dummy bitrate
          }
        }
        else if (lcml.startsWith("icy-metaint:")) {
          metaint = metaline.substring(12).toInt();    // Found metaint tag, read the value
        }
        else if (lcml.startsWith("icy-name:")) {
          icyname = metaline.substring(9);             // Get station name
          icyname.trim();                              // Remove leading and trailing spaces
          if (icyname.length() > 0) {
            nameseen = true;
            if (icyname.length() > 62) {               // line limiting
              icyname.remove(59);
              icyname += "...";
            }
          }
        }
        else if (lcml.startsWith("transfer-encoding:")) {
          // Station provides chunked transfer
          if (lcml.endsWith("chunked")) {
            chunked = true;                            // Remember chunked transfer mode
            chunkcount = 0;                            // Expect chunkcount in DATA
          }
        }
      }
      metalinebfx = 0;                                 // Reset this line
      if ((LFcount == 2) && ctseen) {                  // Content type seen and a double LF?
        dbgprint("Switch to DATA, bitrate is %dkbps"   // Show bitrate
                 ", metaint is %d", bitrate, metaint); // and metaint
        dataMode = DATA;                               // Expecting data now
        datacount = metaint;                           // Number of bytes before first metadata
        skipStationAllowed = true;                     // now allow skip buttons in station mode
        if (!nameseen) {                               // no name seen, that's highly unusual
          // so we try to find it in the preset list
          String tmp;
          int8_t inx;

          icyname = host;                              // by default display host instead of station name
          if (icyname.length() > 62) {                 // line limiting
            icyname.remove(59);
            icyname += "...";
          }
          //nameseen = true;

          dbgprint("No icy-name! Search name for host \"%s\" in presets", host.c_str());
          for (int i = 0; i <= 99; i++) {
            tmp = readhostfrompref(i);                 // Get host spec and possible comment
            if (tmp == "") {                           // End of presets?
              dbgprint("No preset for this host found! Index which breaked = %d.", i);
              break;                                   // no matching entry found
            }
            if (tmp.indexOf(host) >= 0) {              // Found host in preset ?
              dbgprint("Found host in preset %d.", i);
              inx = tmp.indexOf("#");                  // Get position of "#"
              if (inx > 0) {                           // Hash sign present?
                tmp.remove(0, inx + 1);            // Yes, remove non-comment part
                chomp(tmp);                        // Remove garbage from description
                tmp.trim();                           // Remove leading & trailing whitespace
                if (tmp.length() > 0) {                // Something left ?
                  inx = tmp.indexOf(" - ");
                  if (inx >= 0) {
                    tmp.remove(0, inx + 3);
                  }
                  dbgprint("Name found = \"%s\"", tmp.c_str());
                  icyname = tmp;
                }
              }
              break;
            }
          }
        }
        if (brseen) {
          icyname += " (" + String(bitrate) + "kbps)";
        }
        tftset(1, "...waiting for text...");           // Set screen segment middle part
        tftset(3, icyname);                            // Set screen segment bottom part
        lastAlbumStation = icyname;
        queuefunc(QSTARTSONG);                         // Queue a request to start song
      }
    }
    else {
      metalinebf[metalinebfx++] = (char)b;             // Normal character, put new char in metaline
      if (metalinebfx >= METASIZ) {                    // Prevent overflow
        metalinebfx--;
      }
      LFcount = 0;                                     // Reset double CRLF detection
    }
    return;
  }
  if (dataMode == METADATA) {                          // Handle next byte of metadata
    if (metalinebfx < 0) {                             // First byte of metadata?
      metalinebfx = 0;                                 // Prepare to store first character
      metacount = b * 16 + 1;                          // New count for metadata including length byte
    }
    else {
      metalinebf[metalinebfx++] = (char)b;             // Normal character, put new char in metaline
      if (metalinebfx >= METASIZ) {                    // Prevent overflow
        metalinebfx--;
      }
    }
    if (--metacount == 0) {
      metalinebf[metalinebfx] = '\0';                  // Make sure line is limited
      if (strlen(metalinebf)) {                        // Any info present?
        // metaline contains artist and song name.  For example:
        // "StreamTitle='Don McLean - American Pie';StreamUrl='';"
        // Sometimes it is just other info like:
        // "StreamTitle='60s 03 05 Magic60s';StreamUrl='';"
        // Isolate the StreamTitle, remove leading and trailing quotes if present.
        showStreamTitle(metalinebf);                   // Show artist and title if present in metadata
      }
      if (metalinebfx > (METASIZ - 10)) {              // Unlikely metaline length?
        dbgprint("Metadata block too long! Skipping all Metadata from now on.");
        metaint = 0;                                   // Probably no metadata or network problem
      }
      datacount = metaint;                             // Reset data count
      //bufcnt = 0;                                    // Reset buffer count
      dataMode = DATA;                                 // Expecting data
    }
  }
  if (dataMode == PLAYLISTINIT) {                      // Initialize for receive .m3u file
    // We are going to use metadata to read the lines from the .m3u file
    // Sometimes this will only contain a single line
    metalinebfx = 0;                                   // Prepare for new line
    LFcount = 0;                                       // For detection end of header
    dataMode = PLAYLISTHEADER;                         // Handle playlist data
    playlistcnt = 1;                                   // Reset for compare
    totalCount = 0;                                    // Reset totalCount
    clength = 0xFFFF;                                  // Content-length unknown
    dbgprint("Read from playlist");
  }
  if (dataMode == PLAYLISTHEADER) {                    // Read header
    if ((b > 0x7F) ||                                  // Ignore unprintable characters
        (b == '\r') ||                                 // Ignore CR
        (b == '\0')) {                                 // Ignore NULL
      return;                                          // Quick return
    }
    else if (b == '\n') {                              // Linefeed ?
      LFcount++;                                       // Count linefeeds
      metalinebf[metalinebfx] = '\0';                  // Take care of delimeter
      dbgprint("Playlistheader: %s", metalinebf);      // Show playlistheader
      scan_content_length(metalinebf);                 // Check if it is a content-length line
      metalinebfx = 0;                                 // Ready for next line
      if (LFcount == 2) {
        dbgprint("Switch to PLAYLISTDATA, "            // For debug
                 "search for entry %d", playlist_num);
        dataMode = PLAYLISTDATA;                       // Expecting data now
        return;
      }
    }
    else {
      metalinebf[metalinebfx++] = (char)b;             // Normal character, put new char in metaline
      if (metalinebfx >= METASIZ) {                    // Prevent overflow
        metalinebfx--;
      }
      LFcount = 0;                                     // Reset double CRLF detection
    }
  }
  if (dataMode == PLAYLISTDATA) {                      // Read next byte of .m3u file data
    clength--;                                         // Decrease content length by 1
    if ((b > 0x7F) ||                                  // Ignore unprintable characters
        (b == '\r') ||                                 // Ignore CR
        (b == '\0')) {                                 // Ignore NULL
      // Yes, ignore
    }
    if (b != '\n') {                                   // Linefeed?
      // No, normal character in playlistdata,
      metalinebf[metalinebfx++] = (char)b;             // add it to metaline
      if (metalinebfx >= METASIZ) {                    // Prevent overflow
        metalinebfx--;
      }
    }
    if ((b == '\n') ||                                 // linefeed ?
         (clength == 0)) {                             // Or end of playlist data contents
      int inx;                                         // Pointer in metaline
      metalinebf[metalinebfx] = '\0';                  // Take care of delimeter
      dbgprint("Playlistdata: %s", metalinebf);        // Show playlistheader
      if (strlen(metalinebf) < 5) {                    // Skip short lines
        metalinebfx = 0;                               // Flush line
        metalinebf[0] = '\0';
        return;
      }
      String metaline = String(metalinebf);            // Convert to string
      if (metaline.indexOf("#EXTINF:") >= 0) {         // Info?
        if (playlist_num == playlistcnt) {             // Info for this entry?
          inx = metaline.indexOf(",");                 // Comma in this line?
          if (inx > 0) {
            // Show artist and title if present in metadata
            showStreamTitle(metaline.substring(inx + 1).c_str(), true);
          }
        }
      }
      if (metaline.startsWith("#")) {                  // Commentline?
        metalinebfx = 0;                               // Yes, ignore
        return;                                        // Ignore commentlines
      }
      // Now we have an URL for a .mp3 file or stream.  Is it the rigth one?
      dbgprint("Entry %d in playlist found: %s", playlistcnt, metalinebf);
      if (playlist_num == playlistcnt) {
        inx = metaline.indexOf("http://");             // Search for "http://"
        if (inx >= 0) {                                // Does URL contain "http://"?
          host = metaline.substring(inx + 7);          // Yes, remove it and set host
        }
        else {
          host = metaline;                             // Yes, set new host
        }
        connectToHost();                               // Connect to it
      }
      metalinebfx = 0;                                 // Prepare for next line
      host = playlist;                                 // Back to the .m3u host
      playlistcnt++;                                   // Next entry in playlist
    }
  }
}

#if defined(ENABLE_CMDSERVER) && !defined(PORT23_ACTIVE)
//**************************************************************************************************
//                                     G E T C O N T E N T T Y P E                                 *
//**************************************************************************************************
// Returns the contenttype of a file to send.                                                      *
//**************************************************************************************************
String getContentType(String filename)
{
  if      (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".png"))  return "image/png";
  else if (filename.endsWith(".gif"))  return "image/gif";
  else if (filename.endsWith(".jpg"))  return "image/jpeg";
  else if (filename.endsWith(".ico"))  return "image/x-icon";
  else if (filename.endsWith(".css"))  return "text/css";
  else if (filename.endsWith(".zip"))  return "application/x-zip";
  else if (filename.endsWith(".gz"))   return "application/x-gzip";
  else if (filename.endsWith(".mp3"))  return "audio/mpeg";
  else if (filename.endsWith(".pw"))   return "";        // Passwords are secret
  return "text/plain";
}

//**************************************************************************************************
//                                        H A N D L E F S F                                        *
//**************************************************************************************************
// Handling of requesting pages from the PROGMEM. Example: favicon.ico                             *
//**************************************************************************************************
void handleFSf(const String& pagename)
{
  String      ct;                                    // Content type
  const char* p;
  int         l;                                     // Size of requested page
  int         TCPCHUNKSIZE = 1024;                   // Max number of bytes per write

  dbgprint("FileRequest received %s", pagename.c_str());
  ct = getContentType(pagename);                     // Get content type
  if ((ct == "") || (pagename == "")) {              // Empty is illegal
    _claimSPI("handlefsf1");                         // claim SPI bus
    cmdclient.println("HTTP/1.1 404 Not Found");
    cmdclient.println("");
//#ifdef USE_ETHERNET
//    cmdclient.flush();                             // not needed in the end
//    cmdclient.stop();
//#endif
    _releaseSPI();                                   // release SPI bus
    return;
  }
  else {
    if (pagename.indexOf("index.html") >= 0) {       // Index page is in PROGMEM
      p = index_html;
      l = sizeof(index_html);
    }
    else if (pagename.indexOf("radio.css") >= 0) {   // CSS file is in PROGMEM
      p = radio_css + 1;
      l = sizeof(radio_css);
    }
    else if (pagename.indexOf("config.html") >= 0) { // Config page is in PROGMEM
      p = config_html;
      l = sizeof(config_html);
    }
    else if (pagename.indexOf("mp3play.html") >= 0) {// Mp3player page is in PROGMEM
      p = mp3play_html;
      l = sizeof(mp3play_html);
    }
    else if (pagename.indexOf("about.html") >= 0) {  // About page is in PROGMEM
      p = about_html;
      l = sizeof(about_html);
    }
    else if (pagename.indexOf("favicon.ico") >= 0) { // Favicon icon is in PROGMEM
      p = (char*)favicon_ico;
      l = sizeof(favicon_ico);
    }
    else {
      p = index_html;
      l = sizeof(index_html);
    }
    if (*p == '\n') {                                // If page starts with newline:
      p++;                                           // Skip first character
      l--;
    }
    dbgprint("Length of page is %d", strlen (p));
    _claimSPI("handlefsf2");                        // claim SPI bus
    cmdclient.print(httpHeader(ct));                // Send header
    _releaseSPI();                                  // release SPI bus
    // The content of the HTTP response follows the header:
    if (l < 10) {
      _claimSPI("handlefsf3");                      // claim SPI bus
      cmdclient.println ("Testline<br>");
      _releaseSPI();                                // release SPI bus
    }
    else {
      while (l) {                                   // Loop through the output page
        if (l <= TCPCHUNKSIZE) {                    // Near the end?
          _claimSPI("handlefsf4");                  // claim SPI bus
          cmdclient.write(p, l);                    // Yes, send last part
          _releaseSPI();                            // release SPI bus
          l = 0;
        }
        else {
          _claimSPI("handlefsf5");                  // claim SPI bus
          cmdclient.write(p, TCPCHUNKSIZE);         // Send part of the page
          _releaseSPI();                            // release SPI bus
          p += TCPCHUNKSIZE;                        // Update startpoint and rest of bytes
          l -= TCPCHUNKSIZE;
        }
      }
    }
    // The HTTP response ends with another blank line:
    _claimSPI("handlefsf6");                        // claim SPI bus
    cmdclient.println();
//#ifdef USE_ETHERNET
//    cmdclient.flush();                            // not needed in the end
//    cmdclient.stop();
//#endif
    _releaseSPI();                                  // release SPI bus
    dbgprint("Response send");
  }
}
#endif

//**************************************************************************************************
//                                         C H O M P                                               *
//**************************************************************************************************
// Do some filtering on de inputstring:                                                            *
//  - String comment part (starting with "#").                                                     *
//  - Strip trailing CR.                                                                           *
//  - Strip leading spaces.                                                                        *
//  - Strip trailing spaces.                                                                       *
//**************************************************************************************************
void chomp(String &str)
{
  int inx;                                           // Index in de input string

  if ((inx = str.indexOf("#")) >= 0) {               // Comment line or partial comment?
    str.remove(inx);                                 // Yes, remove
  }
  str.trim();                                        // Remove spaces and CR
}

//**************************************************************************************************
//                                     A N A L Y Z E C M D                                         *
//**************************************************************************************************
// Handling of the various commands from remote webclient or Serial                                *
// Version for handling string with: <parameter>=<value>                                           *
//**************************************************************************************************
const char* analyzeCmd(const char* str)
{
  char*       value;                            // Points to value after equalsign in command
  const char* res;                              // Result of analyzeCmd

  value = strstr(str, "=");                     // See if command contains a "="
  if (value) {
    *value = '\0';                              // Separate command from value
    res = analyzeCmd(str, value + 1);           // Analyze command and handle it
    *value = '=';                               // Restore equal sign
  }
  else {
    res = analyzeCmd(str, "0");                 // No value, assume zero
  }
  return res;
}

//**************************************************************************************************
//                                     A N A L Y Z E C M D                                         *
//**************************************************************************************************
// Handling of the various commands from remote webclient or serial                                *
// par holds the parametername and val holds the value.                                            *
// "wifi_00" and "preset_00" may appear more than once, like wifi_01, wifi_02, etc.                *
// Examples with available parameters:                                                             *
//   preset     = 12                        // Select start preset to connect to                   *
//   preset_00  = <mp3 stream>              // Specify station for a preset 00-99 *)               *
//   toneha     = <0..15>                   // Setting treble gain                                 *
//   tonehf     = <0..15>                   // Setting treble frequency                            *
//   tonela     = <0..15>                   // Setting bass gain                                   *
//   tonelf     = <0..15>                   // Setting treble frequency                            *
//   station    = <mp3 stream>              // Select new station (will not be saved)              *
//   station    = <URL>.mp3                 // Play standalone .mp3 file (not saved)               *
//   station    = <URL>.m3u                 // Select playlist (will not be saved)                 *
//   stop                                   // Stop playing                                        *
//   resume                                 // Resume playing                                      *
//   mute                                   // Mute/unmute the music (toggle)                      *
//   wifi_00    = mySSID/mypassword         // Set WiFi SSID and password *)                       *
//   clk_server = pool.ntp.org              // Time server to be used *)                           *
//   clk_offset = <-11..+14>                // Offset with respect to UTC in hours *)              *
//   clk_dst    = <1..2>                    // Offset during daylight saving time in hours *)      *
//   mp3track   = <nodeIndex>               // Play track from SD card, nodeID 0 = random          *
//   settings                               // Returns setting like presets and tone               *
//   status                                 // Show current URL to play                            *
//   test                                   // For test purposes                                   *
//   debug      = 0 or 1                    // Switch debugging on or off                          *
//   reset                                  // Restart the ESP32                                   *
//  Commands marked with "*)" are sensible during initialization only                              *
//   repeat                                 // repeat cmd                                          *
//   mode                                   // switch between SDCARD / STATION mode                *
//   previous/next                          // previous/next mp3 file or station                   *
//   mediaserver_xxx                        // media server settings                               *
//**************************************************************************************************
const char* analyzeCmd(const char* par, const char* val)
{
  String             argument;                       // Argument as string
  String             value, valout;                  // Value of an argument as a string
  int                ivalue;                         // Value of argument as an integer
  static char        reply[180];                     // Reply to client, will be returned
  //bool               relative;                       // Relative argument (+ or -)
  String             tmpstr;                         // Temporary for value
  uint32_t           av;                             // Available in stream/file

  strcpy(reply, "Command accepted");                 // Default reply
  argument = String(par);                            // Get the argument
  chomp (argument);                                  // Remove comment and useless spaces
  if (argument.length() == 0) {                      // Lege commandline (comment)?
    return reply;                                    // Ignore
  }
  argument.toLowerCase();                            // Force to lower case
  value = String(val);                               // Get the specified value
  chomp (value);                                     // Remove comment and extra spaces
  ivalue = value.toInt();                            // Also as an integer
  ivalue = abs (ivalue);                             // Make positive
  //relative = argument.indexOf("up") == 0;          // + relative setting?
  if (argument.indexOf("down") == 0) {               // - relative setting?
    //relative = true;                                 // It's relative
    ivalue = - ivalue;                               // But with negative value
  }
  if (value.startsWith("http://")) {                 // Does (possible) URL contain "http://"?
    value.remove (0, 7);                             // Yes, remove it
  }
  if (value.length()) {
    tmpstr = value;                                  // Make local copy of value
    if (argument.indexOf("passw") >= 0) {            // Password in value?
      tmpstr = String("*******");                    // Yes, hide it
    }
    dbgprint("Command: %s with parameter %s",
               argument.c_str(), tmpstr.c_str());
  }
  else {
    dbgprint("Command: %s (without parameter)",
               argument.c_str());
  }
  if (argument.indexOf("volume") == 0) {             // absolute Volume setting?
    ini_block.reqvol = ivalue;
    if (ini_block.reqvol < 60) {
      ini_block.reqvol = 60;                         // Limit to min value 60
    }
    else if (ini_block.reqvol > 92) {
      ini_block.reqvol = 92;                         // Limit to value 92
    }
    muteFlag = 0;                                    // Stop possibly muting
    sprintf(reply, "Volume is now %d",               // Reply new volume
              ini_block.reqvol);
  }
  else if (argument.indexOf("volume") > 0) {         // relative Volume setting?
    ini_block.reqvol += ivalue;
    if (ini_block.reqvol < 60) {
      ini_block.reqvol = 60;                         // Limit to min value 60
    }
    else if (ini_block.reqvol > 92) {
      ini_block.reqvol = 92;                         // Limit to max value 92
    }
    muteFlag = 0;                                    // Stop possibly muting
    sprintf(reply, "Volume is now %d",               // Reply new volume
            ini_block.reqvol);
  }
  else if (argument == "mute") {                     // Mute/unmute request
    muteFlag = (!muteFlag) ? -1 : 0;
    sprintf(reply, "Output is now %s",               // Reply mute status
            muteFlag ? "muted" : "unmuted");
  }
  else if (argument == "repeat") {                   // repeate request
    if (currentSource == SDCARD && currentIndex > 0 &&
         playMode == SDCARD && SD_okay) {
      if (mp3fileRepeatFlag == NOREPEAT) {
        mp3fileRepeatFlag = SONG;                    // repeat same file
        sprintf(reply, "Repeat Mode = SONG");
      }
      else if (mp3fileRepeatFlag == SONG) {
        mp3fileRepeatFlag = DIRECTORY;               // repeat same directory
        sprintf(reply, "Repeat Mode = DIRECTORY");
      }
      else if (mp3fileRepeatFlag == DIRECTORY) {
        mp3fileRepeatFlag = RANDOM;                  // random play
        sprintf(reply, "Repeat Mode = RANDOM");
      }
      else {
        mp3fileRepeatFlag = NOREPEAT;                // clear repeat flag
        sprintf(reply, "Repeat Mode = NOREPEAT");
      }
    }
    else if (currentSource == MEDIASERVER && currentIndex >= 0) {
      if (mp3fileRepeatFlag == NOREPEAT) {
        mp3fileRepeatFlag = SONG;                    // repeat same file
        sprintf(reply, "Repeat Mode = SONG");
      }
      else if (mp3fileRepeatFlag == SONG) {
        mp3fileRepeatFlag = DIRECTORY;               // repeat same directory
        sprintf(reply, "Repeat Mode = DIRECTORY");
      }
      else if (mp3fileRepeatFlag == DIRECTORY) {
        mp3fileRepeatFlag = NOREPEAT;               // repeat same directory
        sprintf(reply, "Repeat Mode = NOREPEAT");
      }
    }
    else {
      sprintf(reply, "Command \'Repeat\' not allowed in this state.");
    }
  }
  else if (argument.indexOf("ir_") >= 0) {           // Ir setting?
    // No action required
  }
  else if (argument.indexOf("preset_") >= 0) {       // Enumerated preset?
    // No action required
  }
  else if (argument.indexOf("preset") == 0) {        // Preset station?
    mp3fileRepeatFlag = NOREPEAT;  
    playMode = STATION;
    ini_block.newpreset = ivalue;                    // Otherwise set station
    playlist_num = 0;                                // Absolute, reset playlist
    sprintf(reply, "Preset is now %d",               // Reply new preset
            ini_block.newpreset);
  }
  else if (argument.indexOf("pause") >= 0) {
    // TEST
    //dbgprint("dataMode=%d, playMode=%d, totalCount=%d, currentSource=%d, currentPreset=%d, encoderMode=%d",
    //            dataMode, playMode, totalCount, currentSource, currentPreset, encoderMode);
    if (dataMode == DATA && currentSource == SDCARD &&
         playMode == SDCARD && SD_okay && currentIndex > 0) {
      mp3filePause = !mp3filePause;
      sprintf(reply, "Playing mp3 file %s",              // Reply pause status
                mp3filePause ? "has paused." : "continues.");
      if (mp3filePause) {
        xQueueReset (dataQueue);
      }
      else {
        //muteFlag = 6;                                     // suppresses chirps
      }
    }
    else {
      sprintf(reply, "Command \'Pause\' not allowed in this state.");
    }
  }
  else if (argument.indexOf("previous") >= 0 ||
           argument.indexOf("downpreset") >= 0) {
    if (currentSource == SDCARD && playMode == SDCARD) {
      if (SD_okay && currentIndex > 0) {
        int fileIndex;                                  // Previous file index of track on SD
        if (mp3fileRepeatFlag == SONG ||
             ((mp3fileLength - mp3fileBytesLeft) > (mp3fileLength / 30)))
          fileIndex = currentIndex;                     // Select the same file again
        else if (mp3fileRepeatFlag == NOREPEAT)
          fileIndex = nextSDfileIndex(currentIndex, -1); // Select the previous file on SD
        else if (mp3fileRepeatFlag == DIRECTORY)
          fileIndex = nextSDfileIndexInSameDir(currentIndex, -1); // select previous file in same directory
        else
          fileIndex = 0;                                // random mode
        host = getSDfilename(fileIndex);
        if (host.startsWith("error")) {
          dbgprint("SD problem: can't find file with index %d", fileIndex);
        }
        else {
          dbgprint("STOP (command: previous file on SD)");
          dataMode = STOPREQD;                          // Request STOP
          hostreq = true;                               // Request this host
          sprintf(reply, "Next playing %s", host.c_str());
          utf8ascii(reply);
          muteFlag = 10;
        }
      }
    }
    else if (currentSource == STATION && playMode == STATION) {
      xQueueReset (dataQueue);
      muteFlag = 30;
      ini_block.newpreset -= 1;                         // Yes, adjust currentPreset
      if (ini_block.newpreset < 0)
        ini_block.newpreset = highestPreset;
      sprintf(reply, "Preset is now %d",                // Reply new preset
                     ini_block.newpreset);
    }
#ifdef ENABLE_SOAP    
    else if (currentSource == MEDIASERVER && playMode == MEDIASERVER) {
      const char* p;
      int16_t newIndex;

      if (mp3fileRepeatFlag == SONG ||
            ((mp3fileLength - mp3fileBytesLeft) > (mp3fileLength / 30))) {
        newIndex = currentIndex;                        // Select the same file again
      }  
      else {
        newIndex = nextSoapFileIndex(currentIndex, -1); // Select the previous file on soap list
      }
      if (newIndex != -1) {
        p = dbgprint("Next playing \"%s\"", soapList[newIndex].name.c_str());
        host = "soap/" + soapList[newIndex].uri;
        hostObject = soapList[newIndex];
        currentIndex = newIndex;
        dbgprint("STOP (command: previous file in soap list)");
        dataMode = STOPREQD;                            // Request STOP
        hostreq = true;                                 // Request this host
        muteFlag = 10;
      }
      else {
        p = dbgprint("Soap list problem: can't find previous entry");
      }
      strcpy(reply, p);
      utf8ascii(reply);  
    }
#endif
  }
  else if (argument.indexOf("next") >= 0 ||
           argument.indexOf("uppreset") >= 0) {
    if (currentSource == SDCARD && playMode == SDCARD) {
      if (SD_okay && currentIndex > 0) {
        int fileIndex;                                               // Next file index of track on SD
        if (!mp3fileRepeatFlag)
          fileIndex = nextSDfileIndex (currentIndex, +1);            // Select the next file on SD
        else if (mp3fileRepeatFlag == SONG)
          fileIndex = currentIndex;                                  // Select the same file
        else if (mp3fileRepeatFlag == DIRECTORY)
          fileIndex = nextSDfileIndexInSameDir (currentIndex, +1);   // select next file in same directory
        else
          fileIndex = 0;                                             // random mode
        host = getSDfilename (fileIndex);
        if (host.startsWith("error")) {
          dbgprint("SD problem: can't find file with index %d", fileIndex);
        }
        else {
          dbgprint("STOP (command: next file on SD)");
          dataMode = STOPREQD;                          // Request STOP
          hostreq = true;                               // Request this host
          sprintf(reply, "Next playing: %s", host.c_str());
          utf8ascii(reply);
          muteFlag = 10;
        }
      }
    }
    else if (currentSource == STATION && playMode == STATION) {
      xQueueReset (dataQueue);
      muteFlag = 30;
      ini_block.newpreset += 1;                         // Yes, adjust currentPreset
      if (ini_block.newpreset > highestPreset)
        ini_block.newpreset = 0;
      sprintf(reply, "Preset is now %d",                // Reply new preset
                ini_block.newpreset);
    }
#ifdef ENABLE_SOAP    
    else if (currentSource == MEDIASERVER && playMode == MEDIASERVER) {
      const char* p;
      int16_t newIndex;

      if (mp3fileRepeatFlag == SONG) {
        newIndex = currentIndex;                        // Select the same file again
      }  
      else {
        newIndex = nextSoapFileIndex(currentIndex, +1); // Select the next file on soap list
      }
      if (newIndex != -1) {
        p = dbgprint("Next playing: \"%s\"", soapList[newIndex].name.c_str());
        host = "soap/" + soapList[newIndex].uri;
        hostObject = soapList[newIndex];
        currentIndex = newIndex;
        dbgprint("STOP (command: next file in soap list)");
        dataMode = STOPREQD;                            // Request STOP
        hostreq = true;                                 // Request this host
        muteFlag = 10;
      }
      else {
        p = dbgprint("Soap list problem: Can't find next entry");
      }
      strcpy(reply, p);
      utf8ascii(reply);  
    }
#endif
  }
  else if (argument == "stop") {                        // (un)Stop requested?
    // TEST
    //dbgprint("dataMode=%d, playMode=%d, totalCount=%d, currentSource=%d, currentPreset=%d, encoderMode=%d",
    //            dataMode, playMode, totalCount, currentSource, currentPreset, encoderMode);
    if (dataMode & (HEADER | DATA | METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER | PLAYLISTDATA)) {
      dbgprint("STOP (command: stop)");
      dataMode = STOPREQD;                              // Request STOP
      strcpy(reply, "Player stopped.");
      tftset(1, "Player stopped.");                     // Set screen segment text middle part
    }
    else {
      if (dataMode == INIT)
        sprintf(reply, "Player unstopped.");
      else
        sprintf(reply, "Playing from %.82s continues.", host.c_str());
      hostreq = true;                                   // Request UNSTOP
      muteFlag = 20;
    }
    return reply;
  }
  else if (argument == "mode") {
    if (currentSource == SDCARD) {
      dbgprint("STOP (command: mode switch to STATION)");
      dataMode = STOPREQD;                              // Stop player
      currentPreset = -1;                               // Switch to station mode
      muteFlag = 15;
      encoderMode = IDLING;
    }
    else if (currentSource == STATION) {
      if (SD_okay && SD_mp3fileCount > 0) {
        mp3fileRepeatFlag = NOREPEAT; 
        mp3filePause = 0;
        int fileIndex = nextSDfileIndex(0, 0);          // Select the first file on SD
        host = getSDfilename(fileIndex);
        if (host.startsWith("error")) {
          dbgprint("SD problem: Can't find file with index %d", fileIndex);
        }
        else {
          dbgprint("STOP (command: mode switch to SDCARD)");
          dataMode = STOPREQD;                          // Stop player
          hostreq = true;                               // Request this host
        }
      }
    }
  }
  else if (argument == "mp3track" ||                    // Select a track from SD card?
            argument == "station") {                    // Station in the form address:port
    if (value.length() == 0 ||
         (argument == "station" && value.indexOf('.') < 0)) {
      return "Error: parameter value invalid !";
    }
    if (argument == "mp3track") {                       // MP3 track to search for
      playMode = SDCARD;
      if (!SD_okay) {                                   // SD card present?
        dbgprint("SD problem: Can't execute command %s=%s", par, val);
        return "SD problem: Can't execute command !";
      }
      else if (SD_okay && !SD_mp3fileCount) {
        dbgprint("SD empty: Can't execute command %s=%s", par, val);
        return "SD empty: Can't execute command !";
      }
      valout = getSDfilename(value.toInt());            // like "sdcard/........"
      if (valout.startsWith("error")) {
        dbgprint("SD problem: Can't find file %s", value.c_str());
        return "SD problem: Can't find selected file.";
      }
      if (value.toInt() == 0) mp3fileRepeatFlag = RANDOM;
      sprintf(reply, "Playing %s", valout.c_str());
    }
    else { // argument == station
      playMode = STATION;
      mp3fileRepeatFlag = NOREPEAT;
      valout = value;
      sprintf(reply, "Connecting to %s", valout.c_str());
    }
    if (dataMode & (HEADER | DATA | METADATA | PLAYLISTINIT |
                      PLAYLISTHEADER | PLAYLISTDATA)) {
      dbgprint("STOP (command: mp3track or station)");
      dataMode = STOPREQD;                            // Request STOP
    }
    host = valout;                                    // Save it for storage and selection later
    hostreq = true;                                   // Force this station or file as new preset
    utf8ascii(reply);                                 // Remove possible strange characters in reply
  }
  else if (argument == "status") {                    // Status request
    if (dataMode == STOPPED) {
      sprintf(reply, "Player stopped");               // Format reply
    }
    else {
      if (currentSource == SDCARD || currentSource == MEDIASERVER) {
        sprintf(reply, "Playing %s", host.c_str());
        utf8ascii(reply);
      }
      else if (currentSource == STATION) {
        sprintf(reply, "%s - %s", icyname.c_str(),
                  icystreamtitle.c_str());            // streamtitle from metadata
      }
    }
  }
  else if (argument.startsWith("reset")) {            // reset request
    resetreq = true;                                  // reset all
  }
  else if (argument == "jumpforward") {               // jump forward in mp3 file
    if (currentSource == SDCARD) {
      if (dataMode == DATA) mp3fileJumpForward = true;
    }
    else if (currentSource == STATION) {
      sprintf(reply, "Command not accepted in station mode"); // format reply
    }
    else { // MEDIASERVER
      if (dataMode == DATA) {
        // to be implemented if needed
      }
    }
  }
  else if (argument == "jumpback") {                  // jump backwards in mp3 file
    if (currentSource == SDCARD) {
      if (dataMode == DATA) mp3fileJumpBack = true;
    }
    else if (currentSource == STATION) {
      sprintf(reply, "Command not accepted in station mode"); // format reply
    }
    else { // MEDIASERVER
      if (dataMode == DATA) {
        // to be implemented if needed
      }
    }
  }
  else if (argument == "rescan") {                    // re-Scan SD-Card
    buttonSD = true;                                  // we simulate a pressed button
    enc_inactivity = 0;
  }
  else if (argument == "test") {                      // test command
    if (currentSource == SDCARD) {
      av = mp3fileBytesLeft;                          // available bytes in file
    }
    else { // STATION or MEDIASERVER
      _claimSPI("analyzecmd1");
      av = mp3client.available();                     // available in stream
      _releaseSPI();
    }
    sprintf(reply, "Free memory %d, chunks in queue %d, stream %d, bitrate %d kbps, vol %d",
              ESP.getFreeHeap(), uxQueueMessagesWaiting (dataQueue), av, mbitrate, ini_block.reqvol);
    dbgprint("Total free memory of all regions=%d (minEver=%d), freeHeap=%d (minEver=%d), minStack=%d (in Bytes)", 
             ESP.getFreeHeap(), ESP.getMinFreeHeap(), xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)); 
    dbgprint("Stack minimum mainTask was %d", uxTaskGetStackHighWaterMark (mainTask));
    dbgprint("Stack minimum playTask was %d", uxTaskGetStackHighWaterMark (xplayTask));
    dbgprint("Stack minimum spfTask  was %d", uxTaskGetStackHighWaterMark (xspfTask));
    dbgprint("Stack minimum extenderTask  was %d", uxTaskGetStackHighWaterMark (xextenderTask));
#if defined VU_METER && defined LOAD_VS1053_PATCH
    dbgprint("Stack minimum vumeterTask  was %d", uxTaskGetStackHighWaterMark (xvumeterTask));
#endif    
    dbgprint("Volume setting is %d", ini_block.reqvol);
  }
  // Commands for bass/treble control
  else if (argument.startsWith("tone")) {            // tone command
    if (argument.indexOf("ha") > 0) {                // high amplitue? (for treble)
      ini_block.rtone[0] = ivalue;                   // prepare to set ST_AMPLITUDE
    }
    if (argument.indexOf("hf") > 0) {                // high frequency? (for treble)
      ini_block.rtone[1] = ivalue;                   // prepare to set ST_FREQLIMIT
    }
    if (argument.indexOf("la") > 0) {                // low amplitue? (for bass)
      ini_block.rtone[2] = ivalue;                   // prepare to set SB_AMPLITUDE
    }
    if (argument.indexOf("lf") > 0) {                // high frequency? (for bass)
      ini_block.rtone[3] = ivalue;                   // prepare to set SB_FREQLIMIT
    }
    reqtone = true;                                  // set change request
    sprintf(reply, "Parameter for bass/treble %s set to %d",
            argument.c_str(), ivalue);
  }
  else if (argument == "debug") {                    // debug on/off request?
    DEBUG = ivalue;                                  // set flag accordingly
  }
  else if (argument == "getnetworks") {              // list all WiFi networks?
    sprintf(reply, networks.c_str());                // reply is SSIDs
  }
  else if (argument.startsWith("clk_")) {            // TOD parameter?
    if (argument.indexOf("server") > 0) {            // NTP server spec?
      ini_block.clk_server = value;                  // set server
    }
#ifdef USE_ETHERNET
    if (argument.indexOf("tzstring") > 0) {          // time zone string?
      ini_block.clk_tzstring = value;                // set time zone string
    }
#else
    if (argument.indexOf("offset") > 0) {            // offset with respect to UTC spec?
      ini_block.clk_offset = ivalue;                 // set offset
    }
    if (argument.indexOf("dst") > 0) {               // offset duringe DST spec?
      ini_block.clk_dst = ivalue;                    // set DST offset
    }
#endif
  }
#ifdef ENABLE_SOAP
  else if (argument.startsWith("srv_")) {            // media server parameter?
    if (argument.indexOf("ip") > 0) {                // media server ip ?
      ini_block.srv_ip.fromString(value.c_str());
      //dbgprint("New media server ip=%s", ini_block.srv_ip.toString().c_str());
    }
    if (argument.indexOf("port") > 0) {              // media server port ?
      ini_block.srv_port = ivalue;
      //dbgprint("New media server port=%d", ini_block.srv_port);
    }
    if (argument.indexOf("controlurl") > 0) {        // media server port ?
      ini_block.srv_controlUrl = value;
      //dbgprint("New media server controlUrl=\"%s\"", ini_block.srv_controlUrl.c_str());
    }
    if (argument.indexOf("mac") > 0) {               // media server mac needed for WOL ?
      ini_block.srv_macWOL = value;
      //dbgprint("New media server macWOL=\"%s\"", ini_block.srv_macWOL.c_str());
    }
  }
#endif  
  else {
    sprintf(reply, "%s called with illegal parameter: %s",
            NAME, argument.c_str());
  }
  return reply;                                      // return reply to the caller
}

//**************************************************************************************************
//                                     H T T P H E A D E R                                         *
//**************************************************************************************************
// Set http headers to a string.                                                                   *
//**************************************************************************************************
String httpHeader(String contentstype)
{
  return String("HTTP/1.1 200 OK\nContent-type:") +
         contentstype +
         String("\n"
                "Server: " NAME "\n"
                "Cache-Control: " "max-age=3600\n"
                "Last-Modified: " VERSION "\n\n");
}

//**************************************************************************************************
//* Functions that are called from spfTask.                                                         *
//**************************************************************************************************

//**************************************************************************************************
//                                 D I S P L A Y S D S T A T U S                                   *
//**************************************************************************************************
// SD card missing or not ok: display a red "S", for repeat mode "r" (Song) or "R" (Directory),    *
// for random mode 'F' (fluke....r/R for random is already in use).                                *
// Status only painted if it has changed. A char on the screen is 8 pixels high and 6 pixels wide! *
//**************************************************************************************************
void displaySDstatus()
{
  static enum_repeat_mode repeatFlagOld = NOREPEAT;           // for compare
  static bool             sdmissingFlagOld = false;           // for compare
  enum_repeat_mode        repeatFlag = NOREPEAT;
  bool                    sdmissingFlag = !SD_okay;

  if (currentSource == SDCARD) {
    repeatFlag = (mp3fileRepeatFlag != NOREPEAT && SD_okay) ? mp3fileRepeatFlag : NOREPEAT;
  }  
  else if (currentSource == MEDIASERVER) {
    repeatFlag = mp3fileRepeatFlag;
  }
  if (currentSource == SDCARD && sdmissingFlag) {             // all the others are useless in this case
    repeatFlagOld = repeatFlag;
  }
  if (tft) {                                                  // TFT active?
    if (repeatFlagOld == repeatFlag &&
        ((currentSource == SDCARD && sdmissingFlagOld == sdmissingFlag) ||
        (currentSource == MEDIASERVER))) {
      return;                                                 // no change at all
    }
    uint8_t pos = dsp_getwidth() + SDSTATUSPOS;               // X-position of character
    //dbgprint("displayrandom(): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
    //           pos, 0, 6, 8);
    dsp_fillRect(pos, 0, 6, 8, BLACK);                        // clear the space for new character
    if (currentSource == SDCARD && 
        sdmissingFlagOld != sdmissingFlag) {                  // any Difference?
      if (sdmissingFlag) {
        dsp_setTextColor(RED);                                // set the requested color
        dsp_setCursor(pos, 0);                                // prepare to show the info
        dsp_print('S');                                       // show the character
      }
      sdmissingFlagOld = sdmissingFlag;
    }
    else if (repeatFlagOld != repeatFlag) {
      if (repeatFlag != NOREPEAT) {
        dsp_setTextColor(GREEN);                              // set the requested color
        dsp_setCursor(pos, 0);                                // prepare to show the info
        if (repeatFlag == SONG)
          dsp_print('r');                                     // show the character
        else if (repeatFlag == DIRECTORY)
          dsp_print('R');
        else // RANDOM
          dsp_print('F');
      }
      repeatFlagOld = repeatFlag;
    }
  }
}

//**************************************************************************************************
//                               D I S P L A Y M U T E P A U S E                                   *
//**************************************************************************************************
// If muted show 'M' on the LCD, if paused a 'P' at a fixed position in a specified color.         *
// The status is only displayed if it has changed.                                                 *
// A character on the screen is 8 pixels high and 6 pixels wide.                                   *
//**************************************************************************************************
void displayMutePause()
{
  static uint8_t mpflagOld = 0;                   // For compare
  uint8_t mpflag = 0;

  if (tft) {                                      // TFT active?
    if (muteFlag == -1) mpflag |= 0x01;
    else mpflag &= 0xFE;
    if (mp3filePause) mpflag |= 0x02;
    else mpflag &= 0xFD;
    if (mpflagOld != mpflag) {                    // Any Difference?
      uint8_t pos = dsp_getwidth() + MUTEPOS;     // X-position of character
      //dbgprint("displaymute(): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
      //           pos, 0, 6, 8);
      dsp_fillRect(pos, 0, 6, 8, BLACK);          // Clear the space for new character
      if (mp3filePause) { 
        // Pause prevails over mute
        dsp_setTextColor(GREEN);                  // Set the requested color
        dsp_setCursor(pos, 0);                    // Prepare to show the info
        dsp_print('P');                           // Show the character
      }
      else if (muteFlag == -1) {
        dsp_setTextColor(GREEN);                  // Set the requested color
        dsp_setCursor(pos, 0);                    // Prepare to show the info
        dsp_print('M');                           // Show the character
      }
      mpflagOld = mpflag;                         // Remember for next compare
    }
  }
}

//**************************************************************************************************
//                                    D I S P L A Y P R O G R E S S                                *
//**************************************************************************************************
// Show the mp3 file progress as an indicator (two red lines) on the very bottom of the display.   *
//**************************************************************************************************
#define STEPS 20
void displayProgress()
{
  if (tft && (currentSource == SDCARD || currentSource == MEDIASERVER) && encoderMode != SELECT) {
    static uint8_t oldprog = 0;                        // previous progress
    uint8_t        newprog = 0;                        // current setting
    uint8_t        pos;                                // position of progress indicator
    uint32_t       steps = mp3fileLength / STEPS;
    uint32_t       played = mp3fileLength - mp3fileBytesLeft;

    for (int i = 1; i * steps <= played && mp3fileLength; i++) {
      newprog = i;
    }
    if ((forceProgressBar || (newprog != oldprog))     // force painting or has indicator changed
         && !tftdata[3].update_req && !tftdata[4].update_req) { // and no work for section 3 or 4
      forceProgressBar = false;
      oldprog = newprog;                               // remember for next compare
      pos = map(newprog, 0, STEPS, 0, dsp_getwidth()); // compute position on TFT
      //dbgprint("displayvolume(): fillRect: x=%d y=%d w=%d h=%d col=RED",
      //           0, dsp_getheight() - 2, pos, 2);
      dsp_fillRect(0, dsp_getheight() - 2, pos, 2, RED);  // Paint red part
      //dbgprint("displayvolume(): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
      //           pos, dsp_getheight() - 2, dsp_getwidth() - pos, 2);
      dsp_fillRect(pos, dsp_getheight() - 2,
                   dsp_getwidth() - pos, 2, BLACK);    // paint remainder black
    }
  }
}

//**************************************************************************************************
//                                      D I S P L A Y T I M E                                      *
//**************************************************************************************************
// Show the time on the LCD at a fixed position in a specified color                               *
// To prevent flickering, only the changed part of the timestring is displayed.                    *
// An empty string will force a refresh on next call.                                              *
// A character on the screen is 8 pixels high and 6 pixels wide.                                   *
//**************************************************************************************************
void displayTime(const char* str, uint16_t color)
{
  static char oldstr[6] = ".....";                // for compare
  uint8_t     i;                                  // index in strings
  uint8_t     pos = dsp_getwidth() + TIMEPOS;     // X-position of character

  if (str[0] == '\0') {                           // empty string?
    for (i = 0; i < 5; i++) {                     // set oldstr to dots
      oldstr[i] = '.';
    }
    return;                                       // no actual display yet
  }
  if (tft) {                                      // TFT active?
    dsp_setTextColor(color);                      // Set the requested color
    for (i = 0; i < 5; i++) {                     // Compare old and new
      if (str[i] != oldstr[i]) {                  // any change ?
        //dbgprint("displayTime(): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
        //           pos, 0, 6, 8);
        dsp_fillRect(pos, 0, 6, 8, BLACK);        // Clear the space for new character
        dsp_setCursor(pos, 0);                    // Prepare to show the info
        dsp_print(str[i]);                        // show the character
        oldstr[i] = str[i];                       // remember for next compare
      }
      pos += 6;                                   // next position
    }
  }
}

//**************************************************************************************************
//                                     L I M I T S T R I N G                                       *
//**************************************************************************************************
// Reduce string to a length that it's hight will fit into num pixels on the screen. The calcu-    *
// lation considers the actual font size.                                                          *
//**************************************************************************************************
String limitString(String& strg, int maxPixel) {
  int16_t  x = 0, y = 0;
  uint16_t w = 0, h = 0;

  while (strg.length() > 32) {
    dsp_getTextBounds(strg, 0, 0, &x, &y, &w, &h);
    if (h <= maxPixel) break;
    strg = strg.substring(0, strg.length() - 1); // cut 1 byte off at the end
  }
  strg.trim();
  return strg;
}

//**************************************************************************************************
//                                      D I S P L A Y I N F O                                      *
//**************************************************************************************************
// Show a string on the LCD at a specified y-position (0..4) in a specified color.                 *
// The parameter is the index in tftdata[]. Strings are evaluated for wide/narrow characters and   *
// their length adjusted to not wrap around into the next section.                                 *
//**************************************************************************************************
void displayinfo(uint16_t inx)
{
  scrseg_struct *p = &tftdata[inx];
  uint16_t width = dsp_getwidth(),                      // normal number of colums
           len = p->str.length();
  int8_t   i;
  String   strg;

  if (tft && len > 0) {                                 // any action required ?
    if (inx == 0) {                                     // topline is shorter
      width += SDSTATUSPOS;                             // leave space for flags + time
    }
    //dbgprint("displayinfo(%d): fillRect: x=%d y=%d w=%d h=%d col=BLACK",
    //           inx, 0, p->y, width, p->height);
    dsp_fillRect(0, p->y, width, p->height, BLACK);     // clear the space for new info
    if ((dsp_getheight() > 64) && (p->y > 1)) {         // need any space for divider?
      //dbgprint("displayinfo(): fillRect: x=%d y=%d w=%d h=%d col=GREEN",
      //           0, p->y - 1, width, 1);
      dsp_fillRect(0, p->y - 1, width, 1, GREEN);       // generate divider between segment 0 and 1/2
    }

    p->str = utf8ascii(p->str.c_str());                 // convert possible UTF8 chars
    dsp_setTextColor(p->color);                         // set requested color
    if (inx == 1 || inx == 2) {                         // name on 1. line, Title on 2. line (if 2 lines)
      dsp_setFontBig();
      dsp_setCursor(0, p->y + 18);                      // set cursor for first line for big font
      i = p->str.indexOf('\n');                         // find position of CR (separates the line)
      if (i > 0) {                                      // separate segments ?
        // Processing first line
        strg = p->str.substring(0, i);                  // extract first line
        strg = limitString(strg, (p->height / 2) - 2 ); // make sure it fits into 2 lines on the display
        dsp_print(strg.c_str());                        // show the string on TFT
        // show divider in the middle of segment
        dsp_fillRect(0, p->y + (p->height / 2), width, 1, YELLOW);
        // Processing second line
        dsp_setCursor(0, p->y + 62);                    // set cursor for second line for big font
        strg = p->str.substring(i + 1);                 // extract second line
        strg = limitString(strg, (p->height / 2) - 2);  // make sure it fits into 2 lines on the display
        dsp_print(strg.c_str());                               // show the string on TFT
      }
      else {                                            // just one line
        // got to fit into 4 lines on display
        strg = p->str;
        strg = limitString(strg, p->height - 2);        // make sure it fits into 4 lines on the display
        dsp_print(strg.c_str());                        // show string on TFT
      }
      dsp_setFont();                                    // back to normal font
    }
    else {                                              // segments 0, 3 or 4
      if (inx == 3 || inx == 4) {                       // segment at bottom
        unsigned int len = (currentSource != STATION) ? 53 : 72;// in radio mode is room for third line
        if (p->str.length() > len) p->str.setCharAt(len, '\0'); // rudimentary line limiting
      }
      if (inx == 0)
        dsp_setCursor(0, p->y);                         // prepare to show the info on top line
      else
        dsp_setCursor(0, p->y + 4);                     // prepare to show the info on bottom line
      dsp_print(p->str.c_str());                        // show the string on TFT
    }
  }
}

#ifdef USE_ETHERNET
//**************************************************************************************************
//                            G E T L O C A L T I M E E T H                                        *
//**************************************************************************************************
// Retrieve local time from NTP server & convert to string. Only needed when using Ethernet/LAN.   *
//**************************************************************************************************
bool getLocalTimeEth(struct tm *info)
{
  static bool initialized = false;
  int ret, tt = 0, num = 0;
  unsigned long epoch;
  byte packetBuffer[NTP_PACKET_SIZE];         // buffer to hold incoming and outgoing packets

  if (!initialized) {                        // executing only once
    initialized = true;
    setenv("TZ", ini_block.clk_tzstring.c_str(), 1);
    tzset();
  }

  memset(packetBuffer, 0, NTP_PACKET_SIZE);   // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;                        // Stratum, or type of clock
  packetBuffer[2] = 6;                        // Polling Interval
  packetBuffer[3] = 0xEC;                     // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  _claimSPI("time1");                     // claim SPI bus
  udpclient.begin(localPortNtp);              // needed for ntp time server
  ret = udpclient.beginPacket(ini_block.clk_server.c_str(), 123);
  _releaseSPI();                            // release SPI bus
  if (ret) {                                 //NTP requests are to port 123
    // the host exists (we got positive DNS answer)
    _claimSPI("time2");                   // claim SPI bus
    udpclient.write(packetBuffer, NTP_PACKET_SIZE);
    udpclient.endPacket();
    _releaseSPI();                          // release SPI bus
    do {
      delay(100);
      _claimSPI("time3");                 // claim SPI bus
      num = udpclient.parsePacket();
      _releaseSPI();                        // release SPI bus
      tt++;
    } while ((tt <= 15) && !num);
  }
  if (num) {
    dbgprint("NTP server response after %d ms from %s", tt * 100, ini_block.clk_server.c_str());
    _claimSPI("time4");                   // claim SPI bus
    udpclient.read(packetBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer
    udpclient.stop();
    _releaseSPI();                          // release SPI bus
    long highWord = word(packetBuffer[40], packetBuffer[41]);
    long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // this is NTP time (seconds since Jan 1 1900):
    long secsSince1900 = highWord << 16 | lowWord;       // combine the four bytes (two words) into a long integer
    epoch = secsSince1900 - 2208988800UL;                // convert into Unix time (secs since 1.1.1970)
    dbgprint("Posix time zone string: \"%s\"", ini_block.clk_tzstring.c_str());
    localtime_r((const time_t*)&epoch, info);
    dbgprint("TZ applied -> Date: %d.%d.%d Time: %02d:%02d",
               info->tm_mday, info->tm_mon + 1, info->tm_year + 1900, info->tm_hour, info->tm_min);
    return true;
  }
  dbgprint("Can't connect NTP-Server %s. Incorrect server name ?", ini_block.clk_server.c_str());
  _claimSPI("time5");                   // claim SPI bus
  udpclient.stop();
  _releaseSPI();                          // release SPI bus
  return false;
}
#endif  // USE_ETHERNET

//**************************************************************************************************
//                                         G E T T I M E                                           *
//**************************************************************************************************
// Retrieve the local time from NTP server and convert to string.                                  *
// Will be called every minute.                                                                    *
//**************************************************************************************************
void getTime()
{
  static int16_t delaycount = 0;                           // To reduce number of NTP requests
  static int16_t retrycount = 5;                           // max 5 attempts to sync time

  if (tft) {                                               // TFT used?
    if (timeinfo.tm_year) {                                // Legal time found?
      sprintf(timetxt, "%02d:%02d",                        // Yes, format to a string
              timeinfo.tm_hour,
              timeinfo.tm_min);
    }
    if (--delaycount <= 0) {                               // Sync every 24 hours
      delaycount = 1440;                                   // Reset counter to 1440min (24 hours)
      if (timeinfo.tm_year) {                              // Legal time found?
        dbgprint("Sync TOD, old value is %s", timetxt);
      }
      dbgprint("Sync TOD");
#ifdef USE_ETHERNET
      if (!getLocalTimeEth (&timeinfo)) {                  // Read from NTP server
#else
      if (!getLocalTime (&timeinfo)) {                     // Read from NTP server
#endif
        dbgprint("Failed to obtain time!");                // Error
        timeinfo.tm_year = 0;                              // Set current time to illegal
        if (retrycount) {                                  // Give up syncing?
          retrycount--;                                    // No try again
          delaycount = 1;                                  // Retry after 1 minute
        }
      }
      else {
        sprintf(timetxt, "%02d:%02d",                      // Format new time to a string
                timeinfo.tm_hour,
                timeinfo.tm_min);
        dbgprint("Sync TOD successful, new value is %s", timetxt);
      }
    }
  }
}

//**************************************************************************************************
//                                H A N D L E _ T F T _ T X T                                      *
//**************************************************************************************************
// Check if tft refresh is requested.                                                              *
//**************************************************************************************************
bool handle_tft_txt()
{
  for (uint16_t i = 0; i < TFTSECS; i++) {              // handle all sections
    if (tftdata[i].update_req) {                        // refresh requested ?
      displayinfo(i);                                   // do the refresh
      dsp_update();                                     // send updates to the screen
      tftdata[i].update_req = false;                    // reset request
      return true;                                      // handle only 1 request at a time
    }
  }
  return false;                                         // no request was pending
}

//**************************************************************************************************
//                                     P L A Y T A S K                                             *
//**************************************************************************************************
// Play stream data from input queue.                                                              *
// Handle all I/O to VS1053B during normal playing.                                                *
// Handles display of text, time and volume on TFT as well.                                        *
//**************************************************************************************************
void playTask(void * parameter)
{
  while (true) {
    if (xQueueReceive (dataQueue, &inchunk, 5)) {
      while (!vs1053player->data_request()) {                    // If FIFO is full..
        vTaskDelay(1);                                           // Yes, take a break
      }
      switch (inchunk.datatyp) {                                 // What kind of chunk?
        case QDATA:
          claimSPI("chunk");                                     // claim SPI bus
          vs1053player->playChunk(inchunk.buf,                   // DATA, send to player
                                  sizeof(inchunk.buf));
          releaseSPI();                                          // release SPI bus
          totalCount += sizeof(inchunk.buf);                     // Count the bytes
          break;
        case QSTARTSONG:
          claimSPI("startsong");                                 // claim SPI bus
          vs1053player->startSong();                             // START, start player
          releaseSPI();                                          // release SPI bus
          break;
        case QSTOPSONG:
          claimSPI("stopsong");                                  // claim SPI bus
          vs1053player->setVolume(0);                            // Mute
          vs1053player->stopSong();                              // STOP, stop player
          releaseSPI();                                          // release SPI bus
          vTaskDelay(500 / portTICK_PERIOD_MS);                  // Pause for a short time
          break;
        default:
          break;
      }
    }
    // TEST 
    //esp_task_wdt_reset();                                      // Protect against idle cpu
  }
  //vTaskDelete(NULL);                                           // Will never arrive here
}

//**************************************************************************************************
//                                   H A N D L E _ S P E C                                         *
//**************************************************************************************************
// Handle special (non-stream data) functions for spfTask.                                         *
//**************************************************************************************************
void handle_spec()
{
  const char* p;
  uint8_t     vol;

  // Do some special functions if necessary
  if (tft) {                                                 // Need to update TFT?
    handle_tft_txt();                                        // Yes, TFT refresh necessary
    displayMutePause();
    displaySDstatus();
    displayProgress();                                       // Show mp3-file progress on display
    dsp_update();                                            // Be sure to paint physical screen
  }
  claimSPI("hspec1");                                        // claim SPI bus
  if (muteFlag > 0) {
    vs1053player->setVolume(0);                              // mute
    muteFlag--;                                              // minus 1 sec
  }
  else {
    if (muteFlag) {                                          // Mute permanently or not?
      vs1053player->setVolume(0);                            // mute
    }
    else {
      vol = (currentSource == SDCARD) ? (ini_block.reqvol + 8) : ini_block.reqvol; // increase loudness when playing mp3 files
      if (vol > 100) vol = 100;
      vs1053player->setVolume (vol);                         // unmute
    }
  }
  if (reqtone) {                                             // Request to change tone?
    reqtone = false;
    vs1053player->setTone(ini_block.rtone);                  // Set SCI_BASS to requested value
  }
  releaseSPI();                                              // release SPI bus
  if (time_req) {                                            // Time to refresh timetxt?
    time_req = false;                                        // Yes, clear request
    if (NetworkFound) {                                      // Time available?
      getTime();                                             // Yes, get the current time
      displayTime(timetxt);                                  // Write to TFT screen
    }
  }
#ifdef USE_ETHERNET
  if (!staticIPs) {                                          // We use DHCP ?
    _claimSPI("hspec3");                                     // claim SPI bus
    int result = Ethernet.maintain();
    _releaseSPI();                                           // release SPI bus
    if (result == 2) {
      dbgprint("DHCP renewal successful");
    }
  }
#endif
  if (tryToMountSD && (!SD_okay || (SD_okay && !SD_mp3fileCount))) {
    tryToMountSD = false;
    dbgprint("handle_spec: tryToMountSD detected");
    currentIndex = -1;
    if (ini_block.sd_cs_pin >= 0) {                          // SD configured?
      claimSPI("hspec4");
      SD.end();                                              // to make a clean start
      releaseSPI();
      delay(50);
      claimSPI("hspec5");
      bool res = SD.begin(ini_block.sd_cs_pin, SPI, SDSPEED);// try to init SD card driver
      releaseSPI();
      if (!res) {
        p = dbgprint("SD Card Mount Failed!");               // no success, check formatting (FAT)
        tftset(1, "SD-Card Error !");
        tftset(4, p);                                        // show error on TFT as well
        lastArtistSong = "SD-Card Error !";
        lastAlbumStation = p;
      }
      else {
        claimSPI("hspec6");
        SD_okay = (SD.cardType() != CARD_NONE);              // see if known card
        releaseSPI();
        if (!SD_okay) {
          p = dbgprint("SD card not readable");              // card not readable
          tftset(1, "SD-Card Error !");
          tftset(4, p);                                      // show error on TFT as well
        }
        else {
          dbgprint("Locate mp3 files on SD, may take a while...");
          tftset(1, "Please wait !");
          tftset(4, "Scan SD card...");
          handle_tft_txt();                                  // immidiate TFT refresh necessary
          handle_tft_txt();                                  // (2 sections)
          // TEST
          dbgprint("Total free memory of all regions=%d (minEver=%d), freeHeap=%d (minEver=%d), minStack=%d (in Bytes)", 
                   ESP.getFreeHeap(), ESP.getMinFreeHeap(), xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)); 
          //            
          SD_mp3fileCount = listsdtracks("/", 0, true);      // build SD file index
          p = dbgprint("%d mp3 tracks on SD", SD_mp3fileCount);
          // TEST
          dbgprint("Total free memory of all regions=%d (minEver=%d), freeHeap=%d (minEver=%d), minStack=%d (in Bytes)", 
                   ESP.getFreeHeap(), ESP.getMinFreeHeap(), xPortGetFreeHeapSize(), xPortGetMinimumEverFreeHeapSize(), uxTaskGetStackHighWaterMark(NULL)); 
          //            
          tftset(4, p);                                      // show number of tracks on TFT
          if (!SD_mp3fileCount)
            tftset(1, "SD Card is empty !");
          if (SD_okay && SD_mp3fileCount)
            buttonSD = true;
        }
      }
    }
  }
}

//**************************************************************************************************
//                                     S P F T A S K                                               *
//**************************************************************************************************
// Handles display of text, time and volume on TFT & some other low priority stuff                 *
// This task runs on a low priority.                                                               *
//**************************************************************************************************
void spfTask (void *parameter)
{
  while (true) {
    handle_spec();                                           // Maybe some special funcs?
    vTaskDelay(80 / portTICK_PERIOD_MS);                     // Pause for a short time
  }
  //vTaskDelete(NULL);                                       // Will never arrive here
}

//**************************************************************************************************
//                   NEEDED FOR SOFT WIRE COMMUNICATION WITH PCF8574                               *
//**************************************************************************************************
// As of today (9/2018) the Arduino "Wire" Library (I2C) for the ESP32 is highly unstable. See     *
// discussions in many web forums. Impulses come delayed or not at all. Sometimes the communica-   *
// tion stops completely. So we don't use the Library "Wire" but low level I2C functions as        *
// provided by Espressif instead                                                                   *
//**************************************************************************************************

#define I2C_MASTER_SCL_IO          (gpio_num_t)I2C_SCL_PIN   //GPIO_NUM_33  // gpio number for I2C master clock
#define I2C_MASTER_SDA_IO          (gpio_num_t)I2C_SDA_PIN   //GPIO_NUM_32  // gpio number for I2C master data
#define I2C_MASTER_NUM             I2C_NUM_0                 // I2C port number for master dev (0/1)
#define I2C_MASTER_TX_BUF_DISABLE  0                         // I2C master does not need buffer
#define I2C_MASTER_RX_BUF_DISABLE  0                         // I2C master does not need buffer
#define I2C_MASTER_FREQ_HZ         100000                    // I2C master clock frequency
#define ACK_CHECK_EN               0x1                       // I2C master will check ack from slave
#define ACK_CHECK_DIS              0x0                       // I2C master will not check ack from slave

void I2CwireBegin (void)
{
  i2c_config_t conf;
  conf.mode             = I2C_MODE_MASTER;
  conf.sda_io_num       = I2C_MASTER_SDA_IO;
  conf.sda_pullup_en    = GPIO_PULLUP_DISABLE;
  conf.scl_io_num       = I2C_MASTER_SCL_IO;
  conf.scl_pullup_en    = GPIO_PULLUP_DISABLE;
  conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
  i2c_param_config(I2C_MASTER_NUM, &conf);
  i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                     I2C_MASTER_RX_BUF_DISABLE,
                     I2C_MASTER_TX_BUF_DISABLE, 0);
}



//**************************************************************************************************
//                     W I R E W R I T E      (writing 1 byte)                                     *
//**************************************************************************************************
uint8_t I2CwireWrite(uint8_t address, uint8_t toSend)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE , ACK_CHECK_EN);
  i2c_master_write_byte(cmd, toSend, ACK_CHECK_EN);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  return ret;
}


//**************************************************************************************************
//                       W I R E R E A D       (reading 1 byte)                                    *
//**************************************************************************************************
uint8_t I2CwireRead(uint8_t address, uint8_t *data)
{
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
  i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
  i2c_master_stop(cmd);
  esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 100 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret) {
    *data = 0xFF;
    return 1;  //  error
  }
  return 0;
}


//**************************************************************************************************
//                                H A N D L E P C F 8 5 7 4                                        *
//**************************************************************************************************
// Check front buttons and set LEDs. We use the original TECHNICS ST-G570 front panel board with   *
// all the buttons and LEDs on it. The following wiring to the PCF8574-1 is used:                  *
// Inputs: P0 = CP103/1, P1 = CP103/2, P2 = CP103/3, P3 = CP102/7 (all inputs via 8k2 Rs to 3V3)   *
// Outputs: P4 = CP103/5, P5 = CP103/6, P6 = CP103/7, P7 = CP103/8                                 *
// Wiring for the PCF8574-2: Outputs: P0 = CP102/9, P5-P7 = LEDs                                   *
// (Please refer to ST-G570 service manual, page 11 !)                                             *
// Returns 0 if no error occurred, otherwise 1.                                                    *
//**************************************************************************************************

void showPreset(int8_t preset)                             // helper function for handlePCF8574()
{
  String tmp = readhostfrompref(preset);                   // get host spec and possible comment
  if (tmp != "") {                                         // valid preset ?
    // Show just comment if available.  Otherwise the preset itself.
    int16_t inx = tmp.indexOf("#");                        // get position of "#"
    if (inx > 0) {                                         // hash sign present?
      tmp.remove(0, inx + 1);                              // yes, remove non-comment part
    }
    chomp(tmp);                                            // remove garbage from description
    tmp.trim();
    //dbgprint("Selected station is %d, %s", preset, tmp.c_str());
    tftset(2, tmp);                                        // set screen segment
    //tftset(4, "New station selected.");
  }
}


bool handlePCF8574()
{
  static uint8_t  oldStatus[6];                            // assume all inputs start high
  static bool     initialized = false;
  uint8_t         stat;                                    // actual status of row
  bool            ret = false;

  if (!initialized) {
    I2CwireBegin();
    memset(oldStatus, 0x0F, 6);
    initialized = true;
  }

  // Checking buttons on first row 
  if (I2CwireWrite(PCF8574_1_ADDR, 0xEF)) {                // pull first row LOW (P4 connected to CP103/5)
    dbgprint("Error I2CwireWrite(PCF8574-1)");
    return false;
  }
  if (I2CwireRead(PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0xE0) && ((stat & 0x0D) != 0x0D)) { // any button in this row pressed ?
    // (row 1 has only 3 buttons assigned)
    if (!(stat & 0x08) && (oldStatus[0] & 0x08)) {          // button just pressed ?
      dbgprint("Memory Button pressed");                    // button name according to Technics manual
      buttonRepeatMode = true;
    }
    else if (!(stat & 0x04) && (oldStatus[0] & 0x04)) {     // button just pressed ?
      dbgprint("FM Mode Button pressed");                   // button name according to Technics manual
      if ((currentSource == SDCARD && playMode == SDCARD) ||
          (currentSource == MEDIASERVER && playMode == MEDIASERVER)) {
        buttonSkipForward = true;
      }
      else if (currentSource == STATION && playMode == STATION && skipStationAllowed) {
        buttonSkipForward = true;
        skipStationAllowed = false;                         // skip to next station not allowed until station connected
        int8_t preset = ini_block.newpreset;
        preset++;
        if (preset > highestPreset)
          preset = 0;
        showPreset(preset);
      }
    }
    else if (!(stat & 0x01) && (oldStatus[0] & 0x01)) {     // button just pressed ?
      dbgprint("T. Mode Button pressed");                   // button name according to Technics manual
      buttonReturn = true;
    }
    oldStatus[0] = (stat & 0x0F);
    ret = true;
  }
  else {
    oldStatus[0] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_1_ADDR, 0xFF);                       // set row to HIGH again
  if (ret) return true;                                     // if key was pressed we don't inquire any further

  // Checking buttons on fifth row 
  if (I2CwireWrite(PCF8574_1_ADDR, 0xDF)) {                 // pull fifth row LOW (P5 connected to CP103/6)
    dbgprint("Error I2CwireWrite(PCF8574-1)");
    return false;
  }
  if (I2CwireRead(PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0xD0) && ((stat & 0x0C) != 0x0C)) { // any button in this row pressed ?
    // (row 5 has only 2 buttons assigned)
    if (!(stat & 0x08) && (oldStatus[1] & 0x08)) {          // button just pressed ?
      dbgprint("CH9 Button pressed");                       // button name according to Technics manual
      buttonPreset = 9;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x04) && (oldStatus[1] & 0x04)) {     // button just pressed ?
      dbgprint("CH0 Button pressed");                       // button name according to Technics manual
      buttonPreset = 0;
      showPreset(buttonPreset);
    }
    //dbgprint("P0-P7 Pins = 0x%02X", stat);                // show status
    oldStatus[1] = (stat & 0x0F);
    ret = true;
  }
  else {
    oldStatus[1] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_1_ADDR, 0xFF);                       // set row to HIGH again
  if (ret) return true;

  // Checking buttons on sixth row 
  if (I2CwireWrite(PCF8574_1_ADDR, 0xBF)) {                 // pull sixth row LOW (P6 connected to CP103/7)
    dbgprint("Error I2CwireWrite(PCF8574-1)");
    return false;
  }
  if (I2CwireRead (PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0xB0) && ((stat & 0x0F) != 0x0F)) { // any button in this row pressed ?
    // (row 6 has 4 buttons assigned)
    if (!(stat & 0x08) && (oldStatus[2] & 0x08)) {          // button just pressed ?
      dbgprint("CH5 Button pressed");                       // button name according to Technics manual
      buttonPreset = 5;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x04) && (oldStatus[2] & 0x04)) {     // button just pressed ?
      dbgprint("CH6 Button pressed");                       // button name according to Technics manual
      buttonPreset = 6;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x02) && (oldStatus[2] & 0x02)) {     // button just pressed ?
      dbgprint("CH7 Button pressed");                       // button name according to Technics manual
      buttonPreset = 7;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x01) && (oldStatus[2] & 0x01)) {     // button just pressed ?
      dbgprint("CH8 Button pressed");                       // button name according to Technics manual
      buttonPreset = 8;
      showPreset(buttonPreset);
    }
    //dbgprint("P0-P7 Pins = 0x%02X", stat);                // show status
    oldStatus[2] = (stat & 0x0F);
    ret = true;
  }
  else {
    oldStatus[2] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_1_ADDR, 0xFF);                       // set row to HIGH again
  if (ret) return true;

  // Checking buttons on seventh row 
  if (I2CwireWrite(PCF8574_1_ADDR, 0x7F)) {                 // pull seventh row LOW (P7 connected to CP103/8)
    dbgprint("Error I2CwireWrite(PCF8574-1)");
    return false;
  }
  if (I2CwireRead(PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0x70) && ((stat & 0x0F) != 0x0F)) { // any button in this row pressed ?
    // (row 3 has 4 buttons assigned)
    if (!(stat & 0x08) && (oldStatus[3] & 0x08)) {          // button just pressed ?
      dbgprint("CH1 Button pressed");                       // button name according to Technics manual
      buttonPreset = 1;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x04) && (oldStatus[3] & 0x04)) {     // button just pressed ?
      dbgprint("CH2 Button pressed");                       // button name according to Technics manual
      buttonPreset = 2;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x02) && (oldStatus[3] & 0x02)) {     // button just pressed ?
      dbgprint("CH3 Button pressed");                       // button name according to Technics manual
      buttonPreset = 3;
      showPreset(buttonPreset);
    }
    else if (!(stat & 0x01) && (oldStatus[3] & 0x01)) {     // button just pressed ?
      dbgprint("CH4 Button pressed");                       // button name according to Technics manual
      buttonPreset = 4;
      showPreset(buttonPreset);
    }
    //dbgprint("P0-P7 Pins = 0x%02X", stat);                // show status
    oldStatus[3] = (stat & 0x0F);
    ret = true;
  }
  else {
    oldStatus[3] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_1_ADDR, 0xFF);                       // set row to HIGH again
  if (ret) return true;

  uint8_t ledMask = 0xFF;                                   // PCF8574-2 is also used for driving 3 LEDs on
  if (playMode == SDCARD || playMode == MEDIASERVER) {
    if (mp3fileRepeatFlag == SONG)                             // P5-P7, so we do it alltogether
      ledMask = 0xDF;
    else if (mp3fileRepeatFlag == DIRECTORY)
      ledMask = 0xBF;
    else if (mp3fileRepeatFlag == RANDOM)
      ledMask = 0x7F;
  }
  // Checking buttons on forth row 
  if (I2CwireWrite(PCF8574_2_ADDR, ledMask & 0xFE)) {       // pull forth row LOW (P0 connected to CP102/9)
    dbgprint("Error I2CwireWrite(PCF8574-2)");
    return false;
  }
  if (I2CwireRead(PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0xF0) && ((stat & 0x0C) != 0x0C)) { // any button in this row pressed ?
                                                            // (row 5 has only 2 buttons assigned)
#ifdef ENABLE_SOAP
    delay(100);                                             // give some time to press AM+FM at once
    if (I2CwireRead(PCF8574_1_ADDR, &stat)) {               // read again 
      dbgprint("Error I2CwireRead()");
      return false;
    }
    if (!(stat & 0x0C) && ((oldStatus[4] & 0x0C) == 0x0C)) {// 2 buttons just pressed at once ?
      dbgprint("FM+AM Buttons pressed at once");            // button name according to Technics manual
      buttonMediaserver = true;
    }
    else 
#endif    
    if (!(stat & 0x08) && (oldStatus[4] & 0x08)) {          // button just pressed ?
      dbgprint("FM Button pressed");                        // button name according to Technics manual
      buttonSD = true;
    }
    else if (!(stat & 0x04) && (oldStatus[4] & 0x04)) {     // button just pressed ?
      dbgprint("AM Button pressed");                        // button name according to Technics manual
      buttonStation = true;
    }
    oldStatus[4] = (stat & 0x0F);
    enc_inactivity = 0;
    ret = true;
    // TEST
    //dbgprint("dataMode=%d, playMode=%d, totalCount=%d, currentSource=%d, currentPreset=%d, encoderMode=%d",
    //          dataMode, playMode, totalCount, currentSource, currentPreset, encoderMode);
  }
  else {
    oldStatus[4] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_2_ADDR, ledMask & 0xFF);             // set row to HIGH again
  if (ret) return true;

  // Checking buttons on third row 
  if (I2CwireWrite(PCF8574_2_ADDR, ledMask & 0xFD)) {       // pull third row LOW (P1 connected to CP102/8)
    dbgprint("Error I2CwireWrite(PCF8574-2)");
    return false;
  }
  if (I2CwireRead(PCF8574_1_ADDR, &stat)) {
    dbgprint("Error I2CwireRead()");
    return false;
  }
  if (((stat & 0xF0) == 0xF0) && ((stat & 0x04) != 0x04)) { // any button in this row pressed ?
    // (row 3 has only 1 buttons assigned)
    if (!(stat & 0x04) && (oldStatus[5] & 0x04)) {          // button just pressed ?
      dbgprint("IF Band Button pressed");                   // button name according to Technics manual
      if ((currentSource == SDCARD && playMode == SDCARD) ||
          (currentSource == MEDIASERVER && playMode == MEDIASERVER)) {
        buttonSkipBack = true;
      }
      else if (currentSource == STATION && playMode == STATION && skipStationAllowed) {
        buttonSkipBack = true;
        skipStationAllowed = false;                         // skip to next station not allowed until station connected
        int8_t preset = ini_block.newpreset;
        preset--;
        if (preset < 0) preset = highestPreset;
        showPreset(preset);
      }
    }
    oldStatus[5] = (stat & 0x0F);
    enc_inactivity = 0;
  }
  else {
    oldStatus[5] = 0x0F;                                    // no button pressed
  }
  I2CwireWrite(PCF8574_2_ADDR, ledMask & 0xFF);             // set row to HIGH again
  return true;
}


//**************************************************************************************************
//                                  E X T E N D E R T A S K                                        *
//**************************************************************************************************
// Handles the two PCF8574 Port Extender ICs (for setting LEDs & reading Buttons)                  *
// This task runs on a low priority.                                                               *
//**************************************************************************************************
void extenderTask (void * parameter)
{
  while (true) {
    if (!handlePCF8574())
      vTaskDelay(20000 / portTICK_PERIOD_MS);          // I2C error occurred, pause 20 sec
    else
      vTaskDelay(60 / portTICK_PERIOD_MS);             // pause only for a short time
  }
  //vTaskDelete(NULL);                                 // will never arrive here
}

#if defined VU_METER && defined LOAD_VS1053_PATCH
//**************************************************************************************************
//                                  V U M E T E R T A S K                                          *
//**************************************************************************************************
// Reads the VU-Meter level from the VS1053B and shows it on the display. Needs patch V2.7 !       *
// Max VU meter value (per channel) is 96 (0dB), but this means the signal is clipping already.    *
// In praxis the highest level encountered will be 95. Every digit less means 1dB less (90 = -6dB) *
// Only levels above a (dynamic) threshold will be displayed. This task runs on a low priority.    *
//**************************************************************************************************
#define VU_MAX_DIFF    21     // only 21 highest dB are shown
#define VU_START_VALUE 88
uint8_t  maxEncountered = VU_START_VALUE, 
         threshold = VU_START_VALUE - VU_MAX_DIFF;

void vumeterTask(void *parameter)
{
  uint16_t vuLevel, vuLevelOld = 0;
  uint8_t  vuLevelL, vuLevelR;
  uint8_t  ampL, ampLOld = 0, ampR, ampROld = 0;
  uint8_t  posX = dsp_getwidth() + VUMETERPOS;         // X-position of VU-meter
  bool     update = false;
 
  while (true) {
    if (tft) { 
      if ((dataMode == INIT || dataMode == STOPREQD || dataMode == STOPPED) && update) {
        dsp_fillRect(posX, 0, 7, 11, BLACK);           // clear the space for VU-Meter
        update = false;
        maxEncountered = VU_START_VALUE;
        threshold = VU_START_VALUE - VU_MAX_DIFF;
      }
      else if (dataMode == DATA && 
               !((currentSource == SDCARD || currentSource == MEDIASERVER) &&
               mp3fileBytesLeft == 0)) {
        claimSPI("vumeter");                           // claim SPI bus
        vuLevel = vs1053player->readVuMeter();         // read VS1053 VU-Meter value
        releaseSPI();                                  // release SPI bus
        if (vuLevel != vuLevelOld) {
          vuLevelOld = vuLevel;
          vuLevelL = (uint8_t)(vuLevel >> 8);
          vuLevelR = (uint8_t)(vuLevel & 0x00FF);
          if (maxEncountered < vuLevelL || maxEncountered < vuLevelR) {
            maxEncountered = (vuLevelL > vuLevelR) ? vuLevelL : vuLevelR;
            threshold = maxEncountered - VU_MAX_DIFF;
          }

          // normalize to NULL 
          vuLevelL = (vuLevelL <= threshold) ? 0 : vuLevelL - threshold;
          vuLevelR = (vuLevelR <= threshold) ? 0 : vuLevelR - threshold;
 
          if (currentSource == STATION) { 
            // Radio (higher base levels, lower dynamic)
            if (vuLevelL == 0)                   ampL = 0;    // left channel  
            else if (vuLevelL < VU_MAX_DIFF - 9) ampL = 1;
            else if (vuLevelL < VU_MAX_DIFF - 8) ampL = 2;
            else if (vuLevelL < VU_MAX_DIFF - 7) ampL = 3;
            else if (vuLevelL < VU_MAX_DIFF - 6) ampL = 4;
            else if (vuLevelL < VU_MAX_DIFF - 4) ampL = 5;
            else if (vuLevelL < VU_MAX_DIFF - 2) ampL = 7;
            else if (vuLevelL < VU_MAX_DIFF)     ampL = 9;
            else                                 ampL = 11;
            if (vuLevelR == 0)                   ampR = 0;    // right channel
            else if (vuLevelR < VU_MAX_DIFF - 9) ampR = 1;
            else if (vuLevelR < VU_MAX_DIFF - 8) ampR = 2;
            else if (vuLevelR < VU_MAX_DIFF - 7) ampR = 3;
            else if (vuLevelR < VU_MAX_DIFF - 6) ampR = 4;
            else if (vuLevelR < VU_MAX_DIFF - 4) ampR = 5;
            else if (vuLevelR < VU_MAX_DIFF - 2) ampR = 7;
            else if (vuLevelR < VU_MAX_DIFF)     ampR = 9;
            else                                 ampR = 11;
          }
          else {
            // SD card or media server (lower base levels, higher dynamic)                            
            if (vuLevelL == 0)                    ampL = 0;   // left channel    
            else if (vuLevelL < VU_MAX_DIFF - 15) ampL = 1;
            else if (vuLevelL < VU_MAX_DIFF - 11) ampL = 2;
            else if (vuLevelL < VU_MAX_DIFF - 9)  ampL = 3;
            else if (vuLevelL < VU_MAX_DIFF - 9)  ampL = 4;
            else if (vuLevelL < VU_MAX_DIFF - 5)  ampL = 5;
            else if (vuLevelL < VU_MAX_DIFF - 3)  ampL = 7;
            else if (vuLevelL < VU_MAX_DIFF - 1)  ampL = 9;
            else                                  ampL = 11;
            if (vuLevelR == 0)                    ampR = 0;   // right channel
            else if (vuLevelR < VU_MAX_DIFF - 15) ampR = 1;
            else if (vuLevelR < VU_MAX_DIFF - 11) ampR = 2;
            else if (vuLevelR < VU_MAX_DIFF - 9)  ampR = 3;
            else if (vuLevelR < VU_MAX_DIFF - 7)  ampR = 4;
            else if (vuLevelR < VU_MAX_DIFF - 5)  ampR = 5;
            else if (vuLevelR < VU_MAX_DIFF - 3)  ampR = 7;
            else if (vuLevelR < VU_MAX_DIFF - 1)  ampR = 9;
            else                                  ampR = 11;
          }
          if (ampL != ampLOld) {
            if (ampL < ampLOld) dsp_fillRect(posX, 0, 3, 11, BLACK); // clear the space for left meter
            if (ampL > 0) {
              dsp_fillRect(posX, 11 - ampL, 3, ampL, YELLOW);
              update = true;
            }  
          }  
          if (ampR != ampROld) {
            if (ampR < ampROld) dsp_fillRect(posX + 4, 0, 3, 11, BLACK); // clear the space for right meter
            if (ampR > 0) {
              dsp_fillRect(posX + 4, 11 - ampR, 3, ampR, YELLOW);
              update = true;
            }
          }  
          ampLOld = ampL;
          ampROld = ampR;
        }
      }
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);               // pause only for a short time
  }
  //vTaskDelete(NULL);                                 // will never arrive here
}
#endif
