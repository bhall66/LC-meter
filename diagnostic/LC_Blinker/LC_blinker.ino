 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   09 Dec 2021
    Hardware:   ATMEGA328, Nokia5510 display, CoreWeaver PCB
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2021  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   This sketch tests the display, onboard buttons and LEDs

 **************************************************************************/


#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 lcd = 
 Adafruit_PCD8544(13, 11, 12, -1, 3);         // Pins for CLK,Din,DC,CS,RST

#define C_KEY            9                    // PB1: "KEY1" pushbutton
#define L_KEY            2                    // PD2: "KEY2" pushbutton
#define C_LED            5                    // PD5: LED for C_MODE
#define L_LED            6                    // PD6: LED for L_MODE

void initPorts() {                            // INITIALIZE MCU PINS
  pinMode(L_KEY,INPUT_PULLUP);                // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                // C pushbutton
  pinMode(C_LED,      OUTPUT);                // C LED
  pinMode(L_LED,      OUTPUT);                // L KED
}

void initDisplay() {
  lcd.begin();                                // get Nokia screen started
  lcd.setContrast(60);                        // values 50-70 are usually OK
  lcd.clearDisplay();                         // start with blank screen
  lcd.setTextColor(BLACK,WHITE);              // black text on white background
  lcd.setTextSize(1);                         // standard 5x7 font
  lcd.display();                              // now show it (blank screen)
}

bool LKeyPressed() {                          // is the L key pressed?
  return !digitalRead(L_KEY);                 // pressed = pin is grounded
}

bool CKeyPressed() {                          // is the C key pressed?
  return !digitalRead(C_KEY);                 // pressed = pin is grounded
}

void setLEDs(int led1, int led2) {            // control the LEDs
  digitalWrite(L_LED,led1);                   // turn the L LED on/off
  digitalWrite(C_LED,led2);                   // turn the C LED on/off
}

void setup() {
  initPorts();                                // initialize MCU ports
  initDisplay();                              // initialize NOKIA display
}

void loop() {
  lcd.setCursor(5,15);                        // put messages mid-screen
  if (LKeyPressed() && CKeyPressed()) {       // both keys pressed?
      setLEDs(1,1);                           // ..light both LEDs
      lcd.print("BOTH");                      // ..and indicate both
  } else if (LKeyPressed()) {                 // only left key pressed?
      setLEDs(1,0);                           // ..light left LED  
      lcd.print("LEFT");                      // ..and indicate left
  } else if (CKeyPressed()) {                 // only right key pressed?
      setLEDs(0,1);                           // .. light right LED
      lcd.print("RIGHT");                     // .. and indicate right
  } else lcd.print("Hello.");                 // no keys are pressed right now
  
  lcd.display();                              // update the display
  delay(700);                                 // for a little bit  
  setLEDs(0,0);                               // then turn off the LEDs
  lcd.clearDisplay();                         // & clear the message
  lcd.display();                              // update the display
  delay(300);                                 // repeat every second
}
