 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   09 Dec 2021
    Hardware:   ATMEGA328, Nokia5510 display, CoreWeaver PCB
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2021  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   This sketch writes Hello World to the Nokia Display

 **************************************************************************/


#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

Adafruit_PCD8544 lcd = 
 Adafruit_PCD8544(13, 11, 12, A4, 3);   // Pins for CLK,Din,DC,CS,RST


void setup() {
  lcd.begin();                          // initialize the display variable
  lcd.setContrast(60);                  // good values are between 40 and 70  
  lcd.clearDisplay();                   // start with blank screen
  lcd.setTextSize(2);                   // use big and beautiful characters
  lcd.println("Hello");                 // say something profound
  lcd.println("World");
  lcd.display();                        // update the screeen              
} 

void loop() {
}
