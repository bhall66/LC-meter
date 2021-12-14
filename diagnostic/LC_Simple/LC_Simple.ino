 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   14 Dec 2021
    Hardware:   ATMEGA328, Nokia5510 display, CoreWeaver PCB
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2021  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Tests the LEDs and Serial port.
               
                At 1 Hz, the LEDs alternately flash and a period '.'
                is sent to serial port. 
                This test does not exercise the keys or Nokia display. 
                
 **************************************************************************/

#define C_KEY            9                    // PB1: "KEY1" pushbutton
#define L_KEY            2                    // PD2: "KEY2" pushbutton
#define C_LED            5                    // PD5: LED fir C_MODE
#define L_LED            6                    // PD6: LED for L_MODE


// ===========  HARDWARE INTERFACE ROUTINES  ==================================================

void initPorts() {                            // INITIALIZE ALL MCU PIN INTERFACES
  pinMode(L_KEY,INPUT_PULLUP);                // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                // C pushbutton
  pinMode(C_LED,      OUTPUT);                // C LED
  pinMode(L_LED,      OUTPUT);                // L KED
}

void setLEDs(int led1, int led2) {            // control the LEDs
  digitalWrite(L_LED,led1);                   // turn the L LED on/off
  digitalWrite(C_LED,led2);                   // turn the C LED on/off
}

// ===========  SERIAL ROUTINES   =====================================================

void initSerial() {
  Serial.begin(9600);                          // initialize serial port
  Serial.println("\n\n");                      // clear some space
  Serial.println("W8BH");                      // announce startup
  Serial.println("LC Simple"); 
}

void sendData() {            
  Serial.print(".");
}


// ===========  MAIN PROGRAM ==========================================================

void setup() {
  initPorts();                                // initialize all MCU ports
  initSerial();                               // set serial port to 9600 baud
}

void loop() {                                 // flash both LEDs at 1 Hz:
  setLEDs(1,0);                               // turn L LED on
  delay(500);                                 // for half a second
  setLEDs(0,1);                               // then turn C LED on
  delay(500);                                 // for half a second
  sendData();                                 // send serial data
}
