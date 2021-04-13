// bluetft.h
#ifndef bluetft_h
#define bluetft_h
//
// Separated from the main sketch to allow several display types.
// Includes for various ST7735 displays.  Size is 160 x 128.  Select INITR_BLACKTAB
// for this and set dsp_getwidth() to 160.
// Works also for the 128 x 128 version.  Select INITR_144GREENTAB for this and
// set dsp_getwidth() to 128.

#include <Adafruit_ST7735.h>

//#include <Fonts/FreeMonoBoldOblique12pt7b.h>
//#include <Fonts/FreeSerif9pt7b.h>
//#include <Fonts/FreeMono9pt7b.h>
//#include <Fonts/FreeSerifItalic9pt7b.h>
//#include <Fonts/FreeSans9pt7b.h>      // ok for 3 lines
#include "SansSerif_plain_14.h"     // own font, ideal for 4 lines

// Color definitions for the TFT screen (if used)
// TFT has bits 6 bits (0..5) for RED, 6 bits (6..11) for GREEN and 4 bits (12..15) for BLUE.
#define BLACK   ST7735_BLACK
#define BLUE    ST7735_BLUE
#define RED     ST7735_RED
#define GREEN   ST7735_GREEN
#define CYAN    GREEN | BLUE
#define MAGENTA RED | BLUE
#define YELLOW  RED | GREEN
#define WHITE   RED | BLUE | GREEN
#define ORANGE  0xFE00

// Data to display.  There are TFTSECS sections
#define TFTSECS 5
scrseg_struct     tftdata[TFTSECS] =                        // Screen divided in 3 segments + 1 overlay
{                                                           // One text line is 8 pixels (standard font)
  { false, WHITE,   0,  8, "" },                            // 1 top line
  { false, CYAN,   13, 87, "" },                            // 4 lines in the middle in cyan
  { false, YELLOW, 13, 87, "" },                            // 4 lines in the middle in yellow
  { false, YELLOW, 101, 27, "" },                           // 2 lines at the bottom in yellow
  { false, GREEN,  101, 27, "" }                            // 2 lines at the bottom for rotary encoder in green
} ;

Adafruit_ST7735*     tft = NULL ;                           // For instance of display driver

// Various macro's to mimic the ST7735 version of display functions
#define dsp_setRotation()       tft->setRotation ( 1 )             // Use landscape format (1 or 3 for upside down)
//#define dsp_print(a)            tft->print ( a )                   // Print a string 
//#define dsp_println(b)          tft->println ( b )                 // Print a string followed by newline 
//#define dsp_fillRect(a,b,c,d,e) tft->fillRect ( a, b, c, d, e ) ;  // Fill a rectange x,y,w,h,color
#define dsp_setTextSize(a)      tft->setTextSize(a)                // Set the text size
#define dsp_setTextColor(a)     tft->setTextColor(a)               // Set the text color
#define dsp_setCursor(a,b)      tft->setCursor ( a, b )            // Position the cursor x,y
//#define dsp_erase()             tft->fillScreen ( BLACK ) ;        // Clear the screen
#define dsp_getwidth()          160                                // Adjust to your display
#define dsp_getheight()         128                                // Get height of screen
#define dsp_update()                                               // Updates to the physical screen
//#define dsp_usesSPI()           true                               // Does use SPI
//#define dsp_setFontBig()        tft->setFont(&FreeSans9pt7b) ;   // Ok
#define dsp_setFontBig()        tft->setFont(&SansSerif_plain_14) ;  // Test
#define dsp_setFont()           tft->setFont() ;
#define dsp_setTextWrap(a)      tft->setTextWrap(a) ;
#define dsp_getTextBounds(a,b,c,d,e,f,g) tft->getTextBounds(a, b, c, d, e, f, g) ;

bool dsp_begin()
{
  tft = new Adafruit_ST7735 ( ini_block.tft_cs_pin,
                              ini_block.tft_dc_pin, -1 ) ;        // Create an instant for TFT
  // Uncomment one of the following initR lines for ST7735R displays
  //tft->initR ( INITR_GREENTAB ) ;                               // Init TFT interface
  //tft->initR ( INITR_REDTAB ) ;                                 // Init TFT interface
  tft->initR ( INITR_BLACKTAB ) ;                                 // Init TFT interface (160x128)
  //tft->initR ( INITR_144GREENTAB ) ;                            // Init TFT interface
  //tft->initR ( INITR_MINI160x80 ) ;                             // Init TFT interface
  // Uncomment the next line for ST7735B displays
  //tft->initB() ;
  return ( tft != NULL ) ;
}

#define dsp_print(a) \
({ \
  claimSPI ( "tftprint" ) ; \
  tft->print ( a ) ; \
  releaseSPI () ; \
})

#define dsp_println(b) \
({ \
  claimSPI ( "tftprintln" ) ; \
  tft->println ( b ) ; \
  releaseSPI () ; \
})

#define dsp_fillRect(a,b,c,d,e) \
({ \
  claimSPI ( "tftfillrect" ) ; \
  tft->fillRect ( a, b, c, d, e ) ; \
  releaseSPI () ; \
})  

#define dsp_erase() \
({ \
  claimSPI ( "tftfillscrn" ) ; \
  tft->fillScreen ( BLACK ) ; \
  releaseSPI () ; \
})

#endif