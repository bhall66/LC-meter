 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   23 Feb 2022
    Hardware:   W8BH LC Meter
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2022  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Step 4 test (Control Circuits)
 
 **************************************************************************/

#include "TFT_ST7735.h"                               // https://github.com/Bodmer/TFT_ST7735

#define C_KEY            3                            // D3: "KEY1" pushbutton
#define L_KEY            2                            // D2: "KEY2" pushbutton
#define C_LED            5                            // D5: LED for C_MODE
#define L_LED            6                            // D6: LED for L_MODE
#define SPST_RELAY       7                            // D7: SPST relay control
#define DPDT_RELAY       8                            // D8: DPDT relay control

TFT_ST7735 tft = TFT_ST7735();                        // display variable

void initPorts() {                                    // INITIALIZE ALL MCU PIN INTERFACES
  pinMode(L_KEY,INPUT_PULLUP);                        // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                        // C pushbutton
  pinMode(C_LED,      OUTPUT);                        // C LED
  pinMode(L_LED,      OUTPUT);                        // L KED
  pinMode(SPST_RELAY, OUTPUT);                        // SPST relay
  pinMode(DPDT_RELAY, OUTPUT);                        // DPDT relay
}

void setLEDs(int led1, int led2) {                    // control the LEDs
  digitalWrite(C_LED,led1);                           // turn the L LED on/off
  digitalWrite(L_LED,led2);                           // turn the C LED on/off
}

void setup() {
  tft.init();                                         // initialize the display
  tft.setRotation(3);                                 // in landscape mode
  tft.fillScreen(TFT_BLACK);                          // with black background
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);            // and yellow text        
  tft.drawString("LC Meter Test #4",10,20,2);         // put text on display
  initPorts();    
  setLEDs(1,1);
  delay(3000);             
}

void loop(void) {
  if (!digitalRead(C_KEY)) {                          // if C key pressed,
     setLEDs(1,0);                                    // ..turn on C_LED
     tft.fillRect(50,50,50,50,TFT_YELLOW);            // ..show yellow square 
     digitalWrite(SPST_RELAY,0);                      // ..and close SPST relay
  }
  else if (!digitalRead(L_KEY)) {                     // if L key pressed,
     setLEDs(0,1);                                    // ..turn on L LED
     tft.fillRect(50,50,50,50,TFT_CYAN);              // ..show blue square
     digitalWrite(DPDT_RELAY,1);                      // ..and close DPDT relay                      
  }
  delay(500);                                         // for half a second
  
  setLEDs(0,0);                                       // turn LEDs off
  digitalWrite(SPST_RELAY,1);                         // open SPST relay
  digitalWrite(DPDT_RELAY,0);                         // open DPDT relay
  tft.fillRect(50,50,50,50,TFT_BLACK);                // erase square
  delay(500);                                         // for half a second
}
