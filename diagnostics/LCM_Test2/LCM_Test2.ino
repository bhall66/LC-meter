 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   23 Feb 2022
    Hardware:   W8BH LC Meter
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2022  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Step 2 test (MCU)
 
 **************************************************************************/

void beep() {
  tone(A3,2000,100);
  delay(200);
}

void setup() {
}

void loop() {
  beep(); beep();
  delay(1000);
}
