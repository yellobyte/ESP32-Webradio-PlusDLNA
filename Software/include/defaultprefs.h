// Default preferences in raw data format for PROGMEM
//
#define defaultprefs_version 220714
const char defprefs_txt[] PROGMEM = R"=====(
# Example configuration, unwanted lines can be deleted or altered, new lines can be added
# Programmable input pins:
gpio_00 = uppreset = 1
gpio_12 = upvolume = 2
gpio_13 = downvolume = 2
gpio_14 = stop
touch_32 = resume
gpio_34 = station = icecast.omroep.nl:80/radio1-bb-mp3
#
ip = 192.168.1.99
ip_dns = 192.168.1.1
ip_gateway = 192.168.1.1
ip_subnet = 255.255.255.0
#
# Enter your WiFi network specs here:
wifi_00 = SSID1/PASSWD1
wifi_01 = SSID2/PASSWD2
#
volume = 90
toneha = 0
tonehf = 0
tonela = 0
tonelf = 0
#
preset = 0
# Some preset examples
preset_00 = mp3channels.webradio.antenne.de:80/antenne    # 0 - Antenne Bayern
preset_01 = mp3channels.webradio.antenne.de:80/oldies-but-goldies # 1 - Antenne Bayern Oldies but Goldies
preset_02 = mp3channels.webradio.antenne.de:80/chillout   # 2 - Antenne Bayern Chillout
preset_03 = mp3channels.webradio.antenne.de:80/lovesongs  # 3 - Antenne Bayern Lovesongs
preset_04 = mp3channels.webradio.antenne.de/90er-hits     # 4 - Antenne Bayern 90er Hits
preset_05 = stream.laut.fm/radio-regenbogen               # 5 - Radio Regenbogen Live
preset_06 = stream.laut.fm/100deutschpop                  # 6 - Deutsch-Pop
preset_07 = stream.laut.fm/oldies                         # 7 - Oldies
preset_08 = stream.laut.fm/classicrock                    # 8 - Classic Rock
preset_09 = stream.laut.fm/romantik_literock              # 9 - Romantik & Literock
preset_10 = liveradio.swr.de/sw282p3/swr3/play.mp3        # 10 - SWR3
preset_11 = stream.rtlradio.de/rtl-de-national/mp3-192    # 11 - RTL Radio
preset_12 = stream.rtlradio.de/rtl-de-beste-hits/mp3-192  # 12 - RTL Radio Aller Zeiten
preset_13 = www.radioeins.de/frankfurt/livemp3            # 13 - RadioEins rbb
preset_14 = stream.srg-ssr.ch/m/drs3/mp3_128              # 14 - SRF 3
preset_15 = stream.srg-ssr.ch/m/rsp/mp3_128               # 15 - Radio Swiss Pop
preset_16 = stream.streambase.ch/radiopilatus/mp3-192/direct # 16 - Radio Pilatus
preset_17 = stream.streambase.ch/rsunshine/mp3-192/direct # 17 - Radio Sunshine
preset_18 = stream3.themediasite.co.uk:8030/stream        # 18 - Oldies FM
preset_19 = sa47.scastream.com.au/2classicrock_32         # 19 - Triple M Classic Rock NSW
preset_20 = sa47.scastream.com.au/6classicrock_32         # 20 - Triple M Classic Rock WA
preset_21 = ihr/WAQXFM                                    # 21 - iHeartRadio 95X [WAQXFM]
preset_22 = ihr/WLAVFM                                    # 22 - iHeartRadio 95 LAV-FM [WLAVFM]
preset_23 = ihr/WHTSFM                                    # 23 - iHeartRadio Z100 [WHTSFM]
#
# Clock offset and daylight saving time
clk_server = ptbtime1.ptb.de                              # Time server to be used
# following 2 entries only used if WiFi is configured
clk_offset = 1                                              # Offset with respect to UTC in hours
clk_dst = 1                                                 # Offset during daylight saving time (hours)
# following entry only used if Ethernet/LAN is configured
clk_tzstring = CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00   # Posix time zone string for CH/D/A
#
# MAC of media server needed for WOL
srv_mac = 24:5E:BE:5D:2F:66
# For accessing a DLNA media server e.g. Twonky running on QNAP TS-253D NAS
srv_ip = 192.168.1.11
srv_port = 9000
srv_controlUrl = dev0/srv1/control
# Some IR codes
ir_02FD = repeat
ir_10EF = preset = 4
ir_18E7 = preset = 2
ir_22DD = mode
ir_30CF = preset = 1
ir_38C7 = preset = 5
ir_42BD = preset = 7
ir_4AB5 = preset = 8
ir_52AD = preset = 9
ir_5AA5 = preset = 6
ir_629D = stop
ir_6897 = downvolume = 2
ir_7A85 = preset = 3
ir_906F = pause
ir_9867 = upvolume = 2
ir_A857 = next
ir_B04F = preset = 0
ir_E01F = previous
ir_E21D = mute
)=====" ;
