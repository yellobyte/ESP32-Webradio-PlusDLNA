// index.html file in raw data format for PROGMEM
//
#define mp3play_html_version 210201
const char mp3play_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
 <head>
  <title>ESP32-radio</title>
  <meta http-equiv="content-type" content="text/html; charset=ISO-8859-1">
  <link rel="stylesheet" type="text/css" href="radio.css">
  <link rel="Shortcut Icon" type="image/ico" href="favicon.ico">
 </head>
 <body>
  <ul>
   <li><a class="pull-left" href="#">ESP32 Radio</a></li>
   <li><a class="pull-left" href="/index.html">Control</a></li>
   <li><a class="pull-left" href="/config.html">Config</a></li>
   <li><a class="pull-left active" href="/mp3play.html">MP3 player</a></li>
   <li><a class="pull-left" href="/about.html">About</a></li>
  </ul>
  <br><br><br>
  <center>
   <h1>** ESP32 Webradio++ **</h1>
   <br><br>
   <button class="button" style="width:200px" onclick="httpGet('rescan')">Re-Scan SD-Card</button>
   <p>To see scan result wait a few seconds and then reload this page.</p>
   <br><br>
   <label for="seltrack"><big>MP3 files on SD card:</big></label>
   <br>
   <select class="select selectw" onChange="trackreq(this)" id="seltrack">
    <option value="-1">Select a track here</option>
   </select>
   <br><br>
   <button class="button" onclick="httpGet('previous')">PREV</button>
   <button class="button" onclick="httpGet('next')">NEXT</button>
   <button class="button" onclick="httpGet('mp3track=0')">RANDOM</button>
   <button class="button" onclick="httpGet('repeat')">REPEAT</button>
   <button class="button" onclick="httpGet('jumpback')">FRewind</button>
   <button class="button" onclick="httpGet('jumpforward')">FForward</button>
   <br><br>
   <br>
   <input type="text" width="600px" size="110" id="resultstr" placeholder="Waiting for info...."><br>
   <br><br>
  </center>
  <script>
   function httpGet ( theReq )
   {
    var theUrl = "/?" + theReq + "&version=" + Math.random() ;
    var xhr = new XMLHttpRequest() ;
    xhr.onreadystatechange = function() {
      if ( xhr.readyState == XMLHttpRequest.DONE )
      {
        resultstr.value = xhr.responseText ;
      }
    }
    xhr.open ( "GET", theUrl ) ;
    xhr.send() ;
   }

   function trackreq ( presctrl )
   {
    if ( presctrl.value != "-1" )
    {
      httpGet ( "mp3track=" + presctrl.value ) ;
    }
   }

   // Fill track list initially
   //
   var i, select, opt, tracks, strparts ;
   select = document.getElementById("seltrack") ;
   var theUrl = "/?mp3list" + "&version=" + Math.random();
   var xhr = new XMLHttpRequest() ;
   xhr.onreadystatechange = function(){
     if ( xhr.readyState == XMLHttpRequest.DONE )
     {
       tracks = xhr.responseText.split ( "\n" ) ;
       for ( i = 0 ; i < ( tracks.length - 1 ) ; i++ ){
         opt = document.createElement( "OPTION" ) ;
         strparts = tracks[i].split ( "/" ) ;
         opt.value = strparts[0] ;
         opt.text = strparts[1] ;
         select.add( opt ) ;
       }
     }
   }
   xhr.open ( "GET", theUrl, false ) ;
   xhr.send() ;
  </script>
 </body>
</html>
)=====" ;
