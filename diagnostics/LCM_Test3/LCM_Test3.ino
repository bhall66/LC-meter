 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   23 Feb 2022
    Hardware:   W8BH LC Meter
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2022  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Step 3 test (DISPLAY)
   
                You will need to edit the User_Setup.h file in the TFT_ST7735
                library directory.  Edit the data lines as follows:
                #define TFT_CS  10   // Chip select control pin
                #define TFT_DC   9   // Data Command control pin
                #define TFT_RST -1   // Reset pin

                You may also need to edit the TAB_COLOR #define near the top of
                the User_Setup.h file.  On my display,
                #define TAB_COLOR INITB_BLACKTAB
                works the best.

 **************************************************************************/
 
#include "TFT_ST7735.h"                               // https://github.com/Bodmer/TFT_ST7735

TFT_ST7735 tft = TFT_ST7735();                        // display variable

void setup() {
  tft.init();                                         // initialize the display
  tft.setRotation(3);                                 // in landscape mode 
  tft.fillScreen(TFT_BLUE);                           // with blue background
  tft.drawString("ST7735 Display Test",10,20,2);      // put text on display                         
}

void loop(void) {
}
