//********************************************************************************************
// Initialize the preferences op ESP32-radio.  They can be edited later by the webinterface. *
//********************************************************************************************

#include <Preferences.h>

// Note: Namespace name is limited to 15 chars.  Same name must be used in ESP32-radio.
#define NAME "ESP32Radio"
// Adjust size of buffer to the longest expected string for nvsgetstr
#define NVSBUFSIZE 150

Preferences preferences ;  // Instance of Preferences


//******************************************************************************************
//                              S E T U P                                                  *
//******************************************************************************************
void setup()
{
  //String       str ;
  
  Serial.begin ( 115200 ) ;
  Serial.println() ;
  preferences.begin ( NAME, false ) ;                 // Open for read/write
  preferences.clear() ;                               // Remove all preferences under opened namespace
  // Store the preferences for ESP32-radio
  //preferences.putString ( "gpio_00",  "uppreset = 1" ) ;
  //preferences.putString ( "gpio_12",  "upvolume = 2" ) ;
  //preferences.putString ( "gpio_13",  "downvolume = 2" ) ;
  //
  preferences.putString ( "ip", "192.168.1.99" ) ;
  preferences.putString ( "ip_dns", "192.168.1.1" ) ;
  preferences.putString ( "ip_gateway", "192.168.1.1" ) ;
  preferences.putString ( "ip_subnet", "255.255.255.0" ) ;
  //        
  preferences.putString ( "wifi_00", "SSID1/PASSWORD1" ) ;
  preferences.putString ( "wifi_01", "SSID2/PASSWORD2" ) ;
  //
  preferences.putString ( "volume", "90" ) ;
  preferences.putString ( "toneha", "0" ) ;
  preferences.putString ( "tonehf", "0" ) ;
  preferences.putString ( "tonela", "0" ) ;
  preferences.putString ( "tonelf", "0" ) ;
  //
  preferences.putString ( "preset", "0" ) ;
  preferences.putString ( "preset_00", "mp3channels.webradio.antenne.de:80/antenne    # 0 - Antenne Bayern" ) ;
  preferences.putString ( "preset_01", "mp3channels.webradio.antenne.de:80/oldies-but-goldies # 1 - Antenne Bayern Oldies but Goldies" ) ;
  preferences.putString ( "preset_02", "mp3channels.webradio.antenne.de:80/chillout   # 2 - Antenne Bayern Chillout" ) ;
  preferences.putString ( "preset_03", "mp3channels.webradio.antenne.de:80/lovesongs  # 3 - Antenne Bayern Lovesongs" ) ;
  preferences.putString ( "preset_04", "mp3channels.webradio.antenne.de/90er-hits     # 4 - Antenne Bayern 90er Hits" ) ;
  preferences.putString ( "preset_05", "stream.laut.fm/radio-regenbogen               # 5 - Radio Regenbogen Live" ) ;
  preferences.putString ( "preset_06", "stream.laut.fm/100deutschpop                  # 6 - Deutsch-Pop" ) ;
  preferences.putString ( "preset_07", "stream.laut.fm/oldies                         # 7 - Oldies" ) ;
  preferences.putString ( "preset_08", "stream.laut.fm/classicrock                    # 8 - Classic Rock" ) ;
  preferences.putString ( "preset_09", "stream.laut.fm/romantik_literock              # 9 - Romantik & Literock" ) ;
  preferences.putString ( "preset_10", "liveradio.swr.de/sw282p3/swr3/play.mp3        # 10 - SWR3" ) ;
  preferences.putString ( "preset_11", "stream.rtlradio.de/rtl-de-national/mp3-192    # 11 - RTL Radio" ) ;
  preferences.putString ( "preset_12", "stream.rtlradio.de/rtl-de-beste-hits/mp3-192  # 12 - RTL Radio Aller Zeiten" ) ;
  preferences.putString ( "preset_13", "www.radioeins.de/frankfurt/livemp3            # 13 - RadioEins rbb" ) ;  
  preferences.putString ( "preset_14", "stream.srg-ssr.ch/m/drs3/mp3_128              # 14 - SRF 3" ) ;  
  preferences.putString ( "preset_15", "stream.srg-ssr.ch/m/rsp/mp3_128               # 15 - Radio Swiss Pop" ) ; 
  preferences.putString ( "preset_16", "stream.streambase.ch/radiopilatus/mp3-192/direct # 16 - Radio Pilatus" ) ;  
  preferences.putString ( "preset_17", "stream.streambase.ch/rsunshine/mp3-192/direct # 17 - Radio Sunshine" ) ; 
  preferences.putString ( "preset_18", "stream3.themediasite.co.uk:8030/stream        # 18 - Oldies FM" ) ;    
  preferences.putString ( "preset_19", "sa47.scastream.com.au/2classicrock_32         # 19 - Triple M Classic Rock NSW" ) ;      
  preferences.putString ( "preset_20", "sa47.scastream.com.au/6classicrock_32         # 20 - Triple M Classic Rock WA" ) ;  
  preferences.putString ( "preset_21", "ihr/WAQXFM                                    # 21 - iHeartRadio 95X [WAQXFM]" ) ;                     
  preferences.putString ( "preset_22", "ihr/WLAVFM                                    # 22 - iHeartRadio 95 LAV-FM [WLAVFM]" ) ;                     
  //
  preferences.putString ( "clk_server", "ptbtime1.ptb.de" ) ;        // Setting for Time Of Day clock on TFT
  preferences.putString ( "clk_offset", "1" ) ;
  preferences.putString ( "clk_dst",    "1" ) ;
  preferences.end() ;
  delay ( 1000 ) ;
}


//******************************************************************************************
//                              L O O P                                                    *
//******************************************************************************************
void loop()
{
  Serial.println ( "ESP32_radio_init completed..." ) ;
  delay ( 10000 ) ;
}

