 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   14 Dec 2021
    Hardware:   ATMEGA328, Nokia5510 display, CoreWeaver PCB
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2021  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   Tests the use of hardware timers #0,1,2 in the LC Meter.
                
                The display will show 4 items:
                  1.  The elapsed runtime, in hh:mm:ss
                  2.  The seconds counter, from Timer1
                  3.  The milliseconds counter, from Timer2
                  4.  The oscillator frequency, in Hz  
                
                The C LED will also pulse at 1 Hz.
                Press the L key to reset the counters.
                Short probe leads together to start oscillator.
                
 **************************************************************************/


#include <Adafruit_GFX.h>                     // Adafruit Graphics Library
#include <Adafruit_PCD8544.h>                 // Adafruit Nokia 5110 display library

#define C_KEY            9                    // PB1: "KEY1" pushbutton
#define L_KEY            2                    // PD2: "KEY2" pushbutton
#define C_LED            5                    // PD5: LED fir C_MODE
#define L_LED            6                    // PD6: LED for L_MODE
#define LCD_RESET        3                    // PD3: LCD reset line
#define LCD_CLK         13                    // PB5: LCD data clock
#define LCD_MOSI        11                    // PB3: LCD data input 
#define LCD_DC          12                    // PB4: LCD data/command line

volatile unsigned long ovfCounter = 0;        // frequency overflow counter
volatile unsigned long frequency = 0;         // current LM311 oscillator freq (Hz)
volatile unsigned long seconds = 0;           // runtime in seconds
volatile unsigned long ticks = 0;             // millisecond counter
unsigned long t = 0;                          // current runtime, in seconds

Adafruit_PCD8544 lcd = Adafruit_PCD8544       // variable for Nokia display 
  (LCD_CLK, LCD_MOSI, LCD_DC, -1, LCD_RESET); // call with pins for CLK, MOSI, D/C, CS, RST

// ===========  HARDWARE TIMER ROUTINES ==========================================================

ISR(TIMER1_COMPA_vect) {                      // INTERRUPT SERVICE ROUTINE: Timer1 compare
  int t0 = TCNT0;                             // save TCNT0
  TCNT0 = 0;                                  // & reset TCNT0 as soon as possible
  frequency = (ovfCounter*256) + t0;           // calculate total pulses in 1 second
  ovfCounter = 0;                             // reset overflow counter
  seconds++;                                  // increment seconds counter
}

ISR(TIMER0_COMPA_vect) {                      // INTERRUPT SERVICE ROUTINE: Timer0 compare
  ovfCounter++;                               // increment overflow counter
}

ISR(TIMER2_COMPA_vect) {                      // INTERRUPT SERVICE ROUTINE: Timer2 compare
  ticks++;                                    // increment millisecond counter
}

void initTimers() {
  noInterrupts();                           
  TCNT0   = 0;                                // TIMER0 SETUP: count external pulses
  TCCR0A  = bit(WGM01);                       // CTC Mode; no external outputs
  TCCR0B  = bit(CS00)+bit(CS01)+bit(CS02);    // use T0 (external) source = Digital 4
  OCR0A   = 256-1;                            // count to 256
  TIMSK0  = bit(OCIE0A);                      // enable timer overflow interrupt
                                          
  TCNT1   = 0;                                // TIMER1 SETUP: interrupts at 1 Hz
  TCCR1A  = 0;                                // no external outputs
  TCCR1B  = bit(WGM12)+bit(CS12);             // CTC Mode; prescalar /256  
  OCR1A   = 31250-1;                          // compare match register 8 Mhz/256/1Hz
  TIMSK1  = bit(OCIE1A);                      // enable timer compare interrupt

  TCNT2   = 0;                                // TIMER2 SETUP: interrupts at 1000 Hz
  TCCR2A  = bit(WGM21);                       // CTC Mode; no external outputs
  TCCR2B  = bit(CS22);                        // prescalar /64  
  OCR2A   = 125-1;                            // 8 Mhz/64/1000Hz = 125 (250 for 16MHz)
  TIMSK2  = bit(OCIE2A);                      // enable timer compareA interrupt
  interrupts();
}

void resetCounters() {
  noInterrupts();  
  TCNT0 = 0;
  TCNT1 = 0;
  TCNT2 = 0;
  ticks = 0;
  seconds = 0;
  interrupts();
}

void wait(int msDelay) {                      // substitute for Arduino delay() function
  long finished = ticks + msDelay;            // time finished = now + specified delay
  while (ticks<finished) { };                 // spin your wheels until time is up
}


// ===========  HARDWARE INTERFACE ROUTINES  ==================================================

void initPorts() {                            // INITIALIZE ALL MCU PIN INTERFACES
  pinMode(L_KEY,INPUT_PULLUP);                // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                // C pushbutton
  pinMode(C_LED,      OUTPUT);                // C LED
  pinMode(L_LED,      OUTPUT);                // L KED
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

// ===========  NOKIA DISPLAY ROUTINES   =====================================================

void initDisplay() {
  lcd.begin();                                // must call this before HW timer init!!
  lcd.setContrast(60);                        // values 50-70 are usually OK
  lcd.clearDisplay();                         // start with blank screen
  lcd.setTextColor(BLACK,WHITE);              // black text on white background
  lcd.setTextSize(1);                         // standard 5x7 font
  lcd.display();                              // now show it (blank screen)
}

void printTime(long t) {                     // confirm seconds to hh:mm:ss
  long hours = t/3600;                       // get # of hours
  t -= (hours*3600);                         // and subtract it from total
  int minutes = t/60;                        // get # of minutes
  t -= (minutes*60);                         // and subtract it from total
  int seconds = t;                           // all thats left is seconds
  lcd.print(hours); lcd.print("h ");         // print hours
  lcd.print(minutes); lcd.print("m ");       // print minutes
  lcd.print(seconds); lcd.println("s");      // print seconds
}

void showData (long t, long mS) {
  lcd.clearDisplay();
  lcd.setCursor(0,0);
  lcd.println("LC Timers\n");
  lcd.print("Tm "); printTime(t);             // display upTime in hh:mm:ss
  lcd.print(" S "); lcd.println(t);           // display total S on timer1
  lcd.print("mS "); lcd.println(mS);          // display total mS on timer2
  lcd.print("Hz "); lcd.println(frequency);   // display oscillator frequency
  lcd.display();                              // show it!  
}


void initSerial() {
  Serial.begin(9600);                         // initialize serial port
  Serial.println("\n\n");                     // clear some space
  Serial.println("W8BH");                     // announce startup
  Serial.println("LC Timers"); 
}

void sendData() {                             // Send msmt data to serial port:
  Serial.print("Time ");                      // 1. Time
  Serial.print(seconds);
  Serial.print(", F3 ");                      // 2. Oscillator frequency with DUT
  Serial.print(frequency);
  Serial.print(" Hz");
  Serial.println();  
}


// ===========  MAIN PROGRAM ==========================================================

void setup() {
  initPorts();                                // initialize all MCU ports
  initDisplay();                              // initialize Nokia display  
  initTimers();                               // initialize hardware timers
  initSerial();                               // initialize serial port       
  setLEDs(0,0);                               // start with both LEDs off
  showData(t,0);                              // start at time 0
}

void loop() {                               
  if (seconds>t) {                            // At the start of a new second:
    long mS = ticks;                          // remember mS
    t = seconds;                              // remember time in S
    showData(t,mS);                           // show the counter data
    sendData();
    setLEDs(0,1);                             // flash C LED
    wait(100);                                // once per second
    setLEDs(0,0);
  }
  if (LKeyPressed()) {                        // did user press L key = reset counters?
    t = 0;                                    // reset system time
    resetCounters();                          // reset the counters
    showData(t,0);                            // show the reset on display
    setLEDs(1,0);                             // flash the L LED
    wait(100);                                // key debounce & LED dwell time
    setLEDs(0,0);                             // turn off LED
  }
}
