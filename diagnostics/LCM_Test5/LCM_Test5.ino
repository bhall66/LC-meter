 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   24 Feb 2022
    Hardware:   W8BH LC Meter
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2022  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Step 5 implements a frequency counter (0 Hz to 5 MHz).
                It counts pulses arriving on MCU digital #4.
                The expected frequency for this meter is 500-700 kHz.

 **************************************************************************/

#include "TFT_ST7735.h"                         // https://github.com/Bodmer/TFT_ST7735

#define DPDT_RELAY       8                      // D8: DPDT relay control

volatile unsigned long ovfCounter = 0;          // frequency overflow counter
volatile unsigned long frequency = 0;           // current LM311 oscillator freq (Hz)
volatile unsigned long seconds = 0;             // runtime in seconds
volatile unsigned long ticks = 0;               // millisecond counter

TFT_ST7735 tft = TFT_ST7735();   

 ISR(TIMER1_COMPA_vect) {                       // INTERRUPT SERVICE ROUTINE: Timer1 compare
  int t0 = TCNT0;                               // save TCNT0
  TCNT0 = 0;                                    // & reset TCNT0 as soon as possible
  frequency = 256*ovfCounter + t0;              // calculate total pulses in 1 second
  ovfCounter = 0;                               // reset overflow counter
  seconds++;
}

ISR(TIMER0_COMPA_vect) {                        // INTERRUPT SERVICE ROUTINE: Timer0 compare
  ovfCounter++;
}

ISR(TIMER2_COMPA_vect) {                        // INTERRUPT SERVICE ROUTINE: Timer2 compare
  ticks++;                                      // increment millisecond counter
}

void initTimers() {
  noInterrupts();                               // TIMER0 SETUP: count external pulses
  TCNT0   = 0;                                  // start counting at zero
  TCCR0A  = bit(WGM01);                         // CTC Mode; no external outputs
  TCCR0B  = bit(CS00)+bit(CS01)+bit(CS02);      // use T0 (external) source = Digital 4
  OCR0A   = 256-1;                              // count to 256
  TIMSK0  = bit(OCIE0A);                        // enable timer overflow interrupt
                                                // TIMER1 SETUP: interrupts at 1 Hz
  TCNT1   = 0;                                  // start timer at zero
  TCCR1A  = 0;
  TCCR1B  = bit(WGM12)+bit(CS12);               // CTC Mode; prescalar /256  
  OCR1A   = 62500-1;                            // compare match register 16 Mhz/256/1Hz
  TIMSK1  = bit(OCIE1A);                        // enable timer compare interrupt

  TCNT2   = 0;                                  // TIMER2 SETUP: interrupts at 1000 Hz
  TCCR2A  = bit(WGM21);                         // CTC Mode; no external outputs
  TCCR2B  = bit(CS22);                          // prescalar /64  
  OCR2A   = 250-1;                              // 16 Mhz/64/1000Hz = 250
  TIMSK2  = bit(OCIE2A);                        // enable timer compareA interrupt
  interrupts();
}

void wait(int msDelay) {                        // substitute for Arduino delay() function
  long finished = ticks + msDelay;              // time finished = now + specified delay
  while (ticks<finished) { };                   // spin your wheels until time is up
}

void drawValue(float x, int color) {            // INPUT RANGE 0-9999
  int decimals=0;                               // format xxxx.
  if (x<10) decimals=3;                         // format x.xxx
  else if (x<100) decimals=2;                   // format xx.xx
  else if (x<1000) decimals=1;                  // format xxx.x
  tft.setTextColor(color,TFT_BLACK);            // set the color
  if (decimals)                                 // if number has decimals
    tft.drawFloat(x,decimals,5,60,7);           // draw it as a floating pt number
  else {                                        // otherwise, 
    int i=tft.drawNumber(round(x),5,60,7);      // draw it as an integer 
    tft.drawString(". ",i,60,7);                // followed by decimal point
  }
}

void setup() {
  tft.init();                                   // initialize display
  tft.setRotation(3);                           // in landscape format
  tft.fillScreen(TFT_BLACK);                    // with a black background
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);      // and yellow text
  tft.drawString("FREQUENCY  (kHz)",20,20,2);   // show measurement units
  pinMode(DPDT_RELAY, OUTPUT);                  // control the DPDT relay
  digitalWrite(DPDT_RELAY,1);                   // start oscillator by energizing relay 
  initTimers();                                 // configure hw timers for freq msmt
}

void loop(void) {
  float f = (float)frequency/1000;              // convert incoming freq (Hz) to kHz
  drawValue(f,TFT_YELLOW);                      // show kHz measurement on screen
  wait(2000);                                   // update display every 2 seconds
}
