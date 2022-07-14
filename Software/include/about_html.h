// about.html file in raw data format for PROGMEM
//
#define about_html_version 220714
const char about_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
 <head>
  <title>About ESP32-radio</title>
  <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
  <link rel="Shortcut Icon" type="image/ico" href="favicon.ico">
  <link rel="stylesheet" type="text/css" href="radio.css">
 </head>
 <body>
  <ul>
   <li><a class="pull-left" href="#">ESP32 Radio</a></li>
   <li><a class="pull-left" href="/index.html">Control</a></li>
   <li><a class="pull-left" href="/config.html">Config</a></li>
   <li><a class="pull-left" href="/mp3play.html">MP3 player</a></li>
   <li><a class="pull-left active" href="/about.html">About</a></li>
  </ul>
  <br><br><br>
  <center>
   <h1>** ESP32 Webradio++ **</h1>
  </center>
	<p>Webradio receiver for ESP32 with TFT display, VS1053 audio decoder, Ethernet module, DLNA connection & Optical digital audio output.<br>
	Based on Ed Smallenburgs (ed@smallenburg.nl) original ESP32 Radio project, documented at <a target="blank" href="https://github.com/Edzelf/ESP32-radio">Github</a>.<br>
  Webinterface design: <a target="blank" href="http://www.sanderjochems.nl/">Sander Jochems</a><br>
  App (Android): <a target="blank" href="https://play.google.com/store/apps/details?id=com.thunkable.android.sander542jochems.ESP_Radio">Sander Jochems</a></p>
	<p>Author of this project: Thomas Jentzsch (yellobyte@bluewin.ch), documented at <a target="blank" href="https://github.com/yellobyte/ESP32-Webradio-PlusDLNA">Github</a>.<br>
	Date: July 2022</p>
 </body>
</html>
)=====" ;
