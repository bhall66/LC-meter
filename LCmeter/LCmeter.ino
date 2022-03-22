 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   22 Mar 2022
    Hardware:   W8BH LC meter (BN49)
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2022  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   LC Meter, using the famous AADE design.
                See w8bh.net for description of this software.

     Summary:   The LC meter has two buttons, each with an adjacent LED.
                The "C" button is for capacitance measurement.
                The "L" button is for inductance measurement.

                After the meter warms up and is calibrated, insert the device
                under test and press C or L.
                The meter will take four measurementsts, each one second apart.
                The LED is lit when measurements are being taken.
                The meter also sends its measurement data to the serial port.

                To force a calibration, hold down the C key.
                To get continuous data, hold down the L key.
                To stop continuous data, hold down both keys.
 **************************************************************************/


#include "TFT_ST7735.h"                       // https://github.com/Bodmer/TFT_ST7735

#define L_KEY            2                    // PD2: "KEY2" pushbutton
#define C_KEY            3                    // PB1: "KEY1" pushbutton
#define OSCILLATOR       4                    // PD4: Oscillator input pin 
#define C_LED            5                    // PD5: LED fir C_MODE
#define L_LED            6                    // PD6: LED for L_MODE
#define SPST_RELAY       7                    // PD7: DPDT MODE relay (0=L, 1=C)
#define DPDT_RELAY       8                    // PB0: SPST CALIBRATE relay
#define TFT_DC           9                    // PB1: TFT data/command line
#define TFT_CS          10                    // PB2: TFT chip select line
#define TFT_MOSI        11                    // PB3: TFT data input 
#define TFT_CLK         13                    // PB5: TFT data clock
#define PIEZO           A3                    // PC3: piezo (buzzer) control pin
#define VBAT            A5                    // PC5: BATTERY voltage monitor

#define C_MODE           1                    // capacitance mode
#define L_MODE           0                    // inductance mode
#define CCAL          1000                    // value of calibration cap, in pF
#define BAUD_RATE     9600                    // serial output data rate
#define WARMUP_TIME      8                    // warm up time, in seconds
#define BATT_TIMER     180                    // time between battery checks, in seconds
#define DATA_TIMER      60                    // time between serial data output, in sec
#define R_RATIO        2.0                    // set to (R5+R6)/R6.  Should be ~2.0
#define DEVICE_NAME   "LC meter"              // device name
#define VERSION       "2.0"                   // device version
#define OWNER         "W8BH"                  // Your name or call here
#define SHOW_BATTERY   false                  // if true, show battery icon on screen
#define USE_AUDIO      true                   // if true, use piezo element

#define C_COLOR        TFT_YELLOW             // color for capacitance screen
#define L_COLOR        TFT_CYAN               // color for inductance screen
#define CAL_COLOR      TFT_GREEN              // color of calibration screen
#define STARTUP_COLOR  TFT_WHITE              // color of startup screen
#define BATT_COLOR     TFT_WHITE              // color of battery icon

volatile unsigned long ovfCounter = 0;        // frequency overflow counter
volatile unsigned long frequency = 0;         // current LM311 oscillator freq (Hz)
volatile unsigned long seconds = 0;           // runtime in seconds
volatile unsigned long ticks = 0;             // millisecond counter

long F1 = 0;                                  // osc frequency, no unknown, no CCal
long F2 = 0;                                  // osc frequency, no unknown, with CCal
long F3 = 0;                                  // osc frequency with unknown in circuit
unsigned long lastCal = 0;                    // time of last calibration
unsigned long lastData = 0;                   // time of last serial data output
unsigned long lastBattCheck = 0;              // time of last battery check
float voltage = 0;                            // voltage at last battery check
bool idle = false;                            // true if sketch idling (no screen updates)
int mode = 0;                                 // current LC mode (L=0, C=1)

TFT_ST7735 tft = TFT_ST7735();                // display variable


// ===========  EMBEDDED ICONS  ================================================================

const unsigned char capBitmap[96] PROGMEM = {
  // 48 x 16 pixels = capacitor schematic symbol
  0x00, 0x00, 0x06, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 
  0x00, 0x00, 0x00, 0x00, 0x06, 0x70, 0x00, 0x00, 0x07, 0x00, 0x06, 0x60, 0x00, 0xe0, 0x0f, 0xff, 
  0xfe, 0x7f, 0xff, 0xf0, 0x0f, 0xff, 0xfe, 0x7f, 0xff, 0xf0, 0x07, 0x00, 0x06, 0x60, 0x00, 0xe0, 
  0x00, 0x00, 0x06, 0x70, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 0x00, 0x00, 0x00, 0x00, 0x06, 0x38, 
  0x00, 0x00, 0x00, 0x00, 0x06, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char coilBitmap [96] PROGMEM = {
  // 48 x 16px graphic = coil schematic symbol
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0xf0, 0x78, 0x3c, 0x1e, 0x00, 0x01, 0xf8, 0xfc, 0x7e, 0x3f, 0x00, 0x63, 0x9d, 
  0xce, 0xe7, 0x73, 0x86, 0xff, 0x0f, 0x87, 0xc3, 0xe1, 0xff, 0xfe, 0x07, 0x03, 0x81, 0xc0, 0xff, 
  0x60, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char calBitmap1 [] PROGMEM = {
  // 32 x 32px graphic = open circle with cross-hairs
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0f, 0xf0, 0x00, 
  0x00, 0x3f, 0xfc, 0x00, 0x00, 0x79, 0x9e, 0x00, 0x00, 0xe1, 0x87, 0x00, 0x01, 0x81, 0x81, 0x80, 
  0x03, 0x81, 0x81, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x07, 0x00, 0x00, 0xe0, 0x06, 0x00, 0x00, 0x60, 
  0x3f, 0xf0, 0x0f, 0xfc, 0x3f, 0xf0, 0x0f, 0xfc, 0x06, 0x00, 0x00, 0x60, 0x07, 0x00, 0x00, 0xe0, 
  0x03, 0x00, 0x00, 0xc0, 0x03, 0x81, 0x81, 0xc0, 0x01, 0x81, 0x81, 0x80, 0x00, 0xe1, 0x87, 0x00, 
  0x00, 0x79, 0x9e, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x01, 0x80, 0x00, 
  0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char calBitmap2 [] PROGMEM = {
  // 32 x 32px graphic = bulls eye with cross-hairs
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x0f, 0xf0, 0x00, 
  0x00, 0x3f, 0xfc, 0x00, 0x00, 0x79, 0x9e, 0x00, 0x00, 0xe1, 0x87, 0x00, 0x01, 0x81, 0x81, 0x80, 
  0x03, 0x81, 0x81, 0xc0, 0x03, 0x00, 0x00, 0xc0, 0x07, 0x03, 0xc0, 0xe0, 0x06, 0x07, 0xe0, 0x60, 
  0x3f, 0xf7, 0xef, 0xfc, 0x3f, 0xf7, 0xef, 0xfc, 0x06, 0x07, 0xe0, 0x60, 0x07, 0x03, 0xc0, 0xe0, 
  0x03, 0x00, 0x00, 0xc0, 0x03, 0x81, 0x81, 0xc0, 0x01, 0x81, 0x81, 0x80, 0x00, 0xe1, 0x87, 0x00, 
  0x00, 0x79, 0x9e, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x01, 0x80, 0x00, 
  0x00, 0x01, 0x80, 0x00, 0x00, 0x01, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};


// ===========  HARDWARE TIMER ROUTINES ==========================================================

ISR(TIMER1_COMPA_vect) {                      // INTERRUPT SERVICE ROUTINE: Timer1 compare
  int t0 = TCNT0;                             // save TCNT0
  TCNT0 = 0;                                  // & reset TCNT0 as soon as possible
  frequency = (ovfCounter<<8) + t0;           // calculate total pulses in 1 second
  ovfCounter = 0;                             // reset overflow counter
  seconds++;                                  // increment seconds counter
  if (idle &&                                 // have some free time now?
  ((seconds-lastBattCheck) > BATT_TIMER))     // if so, is it time to recheck battery?  
    checkBattery();                           // yes, do it & update icon
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
  OCR1A   = 62500-1;                          // compare match register 16 Mhz/256/1Hz
  TIMSK1  = bit(OCIE1A);                      // enable timer compare interrupt

  TCNT2   = 0;                                // TIMER2 SETUP: interrupts at 1000 Hz
  TCCR2A  = bit(WGM21);                       // CTC Mode; no external outputs
  TCCR2B  = bit(CS22);                        // prescalar /64  
  OCR2A   = 250-1;                            // 16 Mhz/64/1000Hz = 250
  TIMSK2  = bit(OCIE2A);                      // enable timer compareA interrupt
  interrupts();
}

void wait(int msDelay) {                      // substitute for Arduino delay() function
  long finished = ticks + msDelay;            // time finished = now + specified delay
  while (ticks<finished) { };                 // spin your wheels until time is up
}


// ===========  BATTERY MONITORING ==========================================================

float getBattVoltage() {
  int raw = analogRead(VBAT);                 // read analog (voltage) input as 0-1023
  float voltage = raw*5.0/1024;               // 1024 steps, 5.0V full scale
  return voltage * R_RATIO;                   // account for R5/R6 divider
}

void drawBattery (float vbat) {
  const int x=147, y=2;                       // battery icon screen posn
  tft.fillRect(x-1,y-1,13,7,TFT_BLACK);       // erase previous battery
  tft.drawRect(x,y,10,4,BATT_COLOR);          // draw empty battery
  tft.drawRect(x+9,y+1,2,2,BATT_COLOR);       // with pos terminal on right
  if (vbat>8.3)                               // 100% level
    tft.drawRect(x+1,y+1,8,2,BATT_COLOR);
  else if (vbat>7.8)                          // 75% level
    tft.drawRect(x+1,y+1,6,2,BATT_COLOR);
  else if (vbat>7.2)                          // 50% level
    tft.drawRect(x+1,y+1,4,2,BATT_COLOR);
  else if (vbat>7.0)                          // 25% level
    tft.drawRect(x+1,y+1,2,2,BATT_COLOR);
}

void checkBattery() {
  lastBattCheck = seconds;                    // update timer 
  voltage = getBattVoltage();                 // check the battery voltage
  if (SHOW_BATTERY)
    drawBattery(voltage);                     // and draw icon on screen
}


// ===========  AUDIO ROUTINES ==========================================================

void beep(int pitch=30, int cycles=400) {     
  if (!USE_AUDIO) return;                     // dont continue if user wants silence!
  for (int i=0; i<cycles; i++) {               
    for (int j=0; j<pitch; j++)               // tone() doesn't work, so we need to        
      digitalWrite(PIEZO,1);                  // bitbang the piezo.  Turn it on a while
    for (int j=0; j<pitch; j++)
      digitalWrite(PIEZO,0);                  // then turn it off, wasting cycles.
  }
}

void dit(int pitch=30, int duration=280) {
  beep(pitch,duration);
  wait(duration/4);
}

void dah(int pitch=30, int duration=280) {
  beep(pitch,duration*3);
  wait(duration/4);
}

void roger() {
  dit(); dah(); dit();
}


// ===========  HARDWARE INTERFACE ROUTINES  ==================================================

void initPorts() {                            // INITIALIZE ALL MCU PIN INTERFACES
  pinMode(L_KEY,INPUT_PULLUP);                // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                // C pushbutton
  pinMode(VBAT,        INPUT);                // battery voltage /2
  pinMode(DPDT_RELAY, OUTPUT);                // DPDT relay control
  pinMode(SPST_RELAY, OUTPUT);                // SPST relay control
  pinMode(C_LED,      OUTPUT);                // C LED
  pinMode(L_LED,      OUTPUT);                // L KED
  pinMode(PIEZO,      OUTPUT);                // Piezo (buzzer)
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

void setMode(int newMode) {                   // set Mode (Inductance vs. Capacitance)
  mode = newMode;                             // remember the mode
  digitalWrite(DPDT_RELAY, mode);             // by controlling the DPDT relay (on=cap, off=ind)
}

void insertCCal(bool on = true) {             // insert the Calibration Capacitor into circuit
  digitalWrite(SPST_RELAY,!on);               // grounding pin turn on SPST relay & inserts CCal
}


// ===========  SERIAL PORT ROUTINES  ==========================================================

void initSerial() {
  Serial.begin(BAUD_RATE);                    // initialize serial port
  Serial.println("\n\n");                     // clear some space
  Serial.println(OWNER);                      // announce startup
  Serial.println(DEVICE_NAME); 
}

void sendCalibration() {                      // send calibration data to serial port
  lastData=seconds;
  Serial.print("Time ");                      // 1. Time
  Serial.print(seconds);
  Serial.print(", Batt ");                    // 2. Battery voltage
  Serial.print(voltage);
  Serial.print(", F1 ");                      // 3. Oscillator freq without CCal
  Serial.print(F1);
  Serial.print(" Hz");
  Serial.print(", F2 ");                      // 4. Oscillator freq with CCal
  Serial.print(F2);
  Serial.print(" Hz");
  Serial.println();  
}

void sendData(float measurement) {            // Send msmt data to serial port:
  lastData=seconds;
  Serial.print("Time ");                      // 1. Time
  Serial.print(seconds);
  Serial.print(", Batt ");                    // 2. Battery voltage
  Serial.print(voltage);
  Serial.print(", F3 ");                      // 3. Oscillator frequency with DUT
  Serial.print(F3);
  Serial.print(" Hz");
  Serial.print(", Msmt ");                    // 4. Measurement in pF or pH
  Serial.print(measurement);
  Serial.print(" p");
  Serial.print(endChar());
  Serial.println();  
}


// ===========  ST7735 DISPLAY ROUTINES   =====================================================


void newScreen(char* title, int color) {
  tft.fillScreen(TFT_BLACK);                  // start with black background
  tft.drawRect(0,0,160,128,color);            // draw outline
  tft.fillRect(0,0,160,40,color);             // draw title block
  tft.setTextColor(TFT_BLACK,color);          // set title to inverse color
  tft.drawCentreString(title,80,10,4);        // draw the title
  tft.setTextColor(color,TFT_BLACK);          // set text color to match
  checkBattery();                             // update icon with each new screen 
}

void drawValue(float n) {                    // INPUT RANGE 0-9999
  const int x=12,y=60,f=7;                   // (x,y) coordinates and font#
  int decimals=0;                            // format xxxx.
  if (n<10) decimals=3;                      // format x.xxx
  else if (n<100) decimals=2;                // format xx.xx
  else if (n<1000) decimals=1;               // format xxx.x
  if (n<0)                                   // display undeflow values     
    tft.drawString(".0000",x,y,f);           // as zeros
  else if (n>9999)                           // display overflow values
    tft.drawString("0000.",x,y,f);           // as zeros
  else if (decimals)                         // if number has decimals
    tft.drawFloat(n,decimals,x,y,f);         // draw it as a floating pt number
  else {                                     // otherwise, 
    int i=tft.drawNumber(round(n),x,y,f);    // draw it as an integer 
    tft.drawString(". ",i,y,f);              // followed by decimal point
  }
}

void initDisplay() {
  tft.init();                                 // initialize the display
  tft.setRotation(3);                         // in landscape mode  
}

void newScreen(char* title) {                 // start new, titled screen
  tft.fillScreen(TFT_BLACK);                  // with black background
  tft.setCursor(0,0);                         // top, left cursor position
  tft.println(title);                         // add title
  checkBattery();                             // update icon with each new screen
}

void showCalIcon(int seconds) {               // call with time displayed, in sec
  const int x=12, y=56, w=32, h=32;           // icon (x,y) coordinates, width & height
  for (int i=0; i<seconds; i++) {             // animate this icon...
    tft.fillRect(x,y,w,h,TFT_BLACK);          // erase previous icon
    tft.drawBitmap(x,y,calBitmap1,            // draw icon, no center dot
      w,h,CAL_COLOR);  
    wait(500);                                // show it for half a sec
    tft.drawBitmap(x,y,calBitmap2,            // draw icon with center dot
      w,h,CAL_COLOR);  
    wait(500);                                // show it for half a sec
  }  
}

char endChar() {                              // returns F or H, depending on mode
  return (mode==L_MODE)?'H':'F'; 
}

void showResult(float x) {
  if (x<0) drawValue(x);                     // underflow
  else if (x<1E3)  drawValue(x);             // pico range             
  else if (x<1E6)  drawValue(x/1E3);         // nano range     
  else if (x<1E9)  drawValue(x/1E6);         // micro range           
  else if (x<1E12) drawValue(x/1E9);         // milli range        
  else drawValue(x);                         // overflow
}

void showUnits(float x) {
  char prefix, s[3];
  if (x<0)         prefix = '<';              // underflow error
  else if (x<1000) prefix = 'p';              // pico units
  else if (x<1E6)  prefix = 'n';              // nano units
  else if (x<1E9)  prefix = 'u';              // micro units
  else if (x<1E12) prefix = 'm';              // milli units
  else prefix = '>';                          // overflow error
  s[0]=prefix; s[1]=endChar(); s[2]=0;        // construct unit string
  tft.drawRightString(s,150,10,4);            // show units, right-justified
}

void showData(float msmt) {                   // Show Data on screen:
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,0);
  tft.print("F1  ");                          // 1. Oscillator Freq without CCal
  tft.print(F1); tft.println(" Hz");
  tft.print("F2  ");                          // 2. Oscillator freq with CCal
  tft.print(F2); tft.println(" Hz");
  tft.print("F3  ");                          // 3. Oscillator freq with DUT
  tft.print(F3); tft.println(" Hz");
  tft.print("Time ");                         // 4. Time since meter turned on
  tft.print(seconds); tft.println("  s");
  tft.print("Batt ");                         // 5. Battery voltage
  tft.print(voltage); tft.println("  V");                       
  showResult(msmt);                           // 6. Measurement of DUT
}

void removeDUT(int secs=8) {                  // remind user to remove DUT!
  const int y = 60;                           // put the message mid-screen
  tft.drawString("Remove DUT now",10,y,2);
  for (int i=secs; i>0; i--) {                // for the specified # of seconds,
    tft.drawNumber(i,135,y,2);
    wait(1000);                               
  }
  tft.fillRect(10,y,140,30,TFT_BLACK);        // finally, erase the line
}

void stabilize(int secs=3) {                  // allow oscillator time to stabilize
  tft.drawString("Analyzing ...",20,50,2); 
  for (int i=secs; i>0; i--) {                // for the specified # of seconds
    tft.drawNumber(i,120,50,2);
    wait(1000);                               
  }
  tft.fillRect(20,50,120,30,TFT_BLACK);       // finally, erase the line
}

void splashScreen(int seconds) {
  newScreen(OWNER,STARTUP_COLOR);             // show OWNER name/call
  tft.drawString(DEVICE_NAME,30,50,4);        // show device name
  tft.drawString(VERSION,70,75,2);            // and version number
  tft.drawString("Warming Up...",30,110,2); 
  if (seconds<3) seconds=3;                   // wait a minimum of 3 seconds
  for (int i=seconds; i>0; i--) {             // do a warmup coundown...
    tft.fillRect(120,110,36,16,TFT_BLACK);    // erase previous count
    tft.drawNumber(i,120,110,2);              // and show new count
    if (LKeyPressed()||CKeyPressed())         // allow escape from a long warmup
      break;                                  // outta here
    wait(1000);                               // count down at 1 sec intervals
  }
}


// ===========  LC METER ROUTINES ==========================================================

float getCapacitance() {                      // CAPACITANCE EQUATIONS:     
  F3 = frequency;                             // save current frequency
  float p = (float)F1*F1/F3/F3;               // (F1^2)/(F3^2)
  float q = (float)F1*F1/F2/F2;               // (F1^2)/(F2^2)
  float r = (p-1)/(q-1)*CCAL;                 // this is the capacitance equation
  return r;                                   // result in picoFarads
}

float getInductance() {                       // INDUCTANCE EQUATIONS:
  F3 = frequency;                             // save current frequency
  float p = (float)F1*F1/F3/F3;               // (F1^2)/(F3^2)
  float q = (float)F1*F1/F2/F2;               // (F1^2)/(F2^2)
  float s = 2 * 3.1415926 * F1;               // 2pi*F1
  float r = (p-1)*(q-1)/CCAL/s/s;             // this is the inductance equation
  return (r*1E24);                            // convert to picoHenries
}

void doData() {                               // show all data on screen
  float msmt;
  while (!(LKeyPressed()&&CKeyPressed())) {   // until both keys are pressed.
    if (LKeyPressed()) setMode(L_MODE);       // set Inductance mode with L key
    else if (CKeyPressed()) setMode(C_MODE);  // set Capacitance mode with C key
    if (mode==L_MODE) {                       // doing inductance, so                        
      setLEDs(1,0);                           // keep the L LED on
      tft.setTextColor(L_COLOR,TFT_BLACK);   
      msmt = getInductance();                 // and calculate inductance
    } else {                                  // doing capacitance, so
      setLEDs(0,1);                           // keep the C LED on
      tft.setTextColor(C_COLOR,TFT_BLACK);
      msmt = getCapacitance();                // and calculate capacitance
    }
    voltage = getBattVoltage();               // check the battery
    showData(msmt);                           // show all current data
    showUnits(msmt);
    if (seconds >= (lastData+DATA_TIMER))     // how long since last serial data sent?
      sendData(msmt);                         // send it if enough time has elapsed
    wait(1000);                               // refresh the data screen every second
  }                                           // user held down both keys, so..
  doCalibration();                            // end the session with a calibration
}

void doCalibration() {
  newScreen("Calibration",CAL_COLOR);         // start with fresh new screen
  removeDUT(8);                               // remind user to remove DUT
  setMode(C_MODE);                            // calibration needs L&C both in circuit
  setLEDs(1,1);                               // turn on both LEDs
  tft.drawString("Adding C_CAL",60,60,1);
  insertCCal(true);                           // insert calibration cap parallel to LC tank
  showCalIcon(3);                             // allow oscillator time to stabilize
  F2 = frequency;                             // save F2 frequency = C_CAL inline
  setLEDs(0,1);                               // mark removal of C_CAL
  tft.drawString("Removing C_CAL",60,70,1);
  insertCCal(false);                          // remove C_CAL from circuit
  showCalIcon(3);                             // allow oscillator time to stabilize
  F1 = frequency;                             // save F1 frequency = LC without C_CAL
  F3 = 0;                                     // no DUT being measured here
  lastCal = seconds;                          // remember when this cal took place
  setLEDs(0,0);                               // turn off all LEDs
  setMode(L_MODE);                            // turn relay off when not in use
  tft.drawString("Done!",60,80,1);
  tft.drawString("LC Meter Ready to use",8,108,2);
  sendCalibration();                          // send calibration data to serial port
  roger();                                    // announce end of calibration = Ready
}

void doInductance() {
  setMode(L_MODE);                            // relay off when measuring L
  stabilize();                                // allow oscillator time to stabilize
  tft.fillRect(0,0,160,40,L_COLOR);           // erase title
  tft.drawBitmap(10,14,coilBitmap,            // replacing it with capacitor icon
      48,16,TFT_BLACK);
  for (int i=0; i<4; i++) {                   // do 4 measurements, 1 sec apart
    float msmt = getInductance();             // calculate L value
    tft.setTextColor(L_COLOR,TFT_BLACK);
    showResult(msmt);                         // show msmt on screen
    tft.setTextColor(TFT_BLACK,L_COLOR);      // invert text colors
    showUnits(msmt);                          // and show units in title bar
    sendData(msmt);                           // and send it to serial port
    dit();                                    // announce each measurement
    wait(1000);                               // wait one second before next msmt
   }
}

void doCapacitance() {
  setMode(C_MODE);                            // pull relay closed to measure C
  stabilize();                                // allow oscillator time to stabilize
  tft.fillRect(0,0,160,40,C_COLOR);           // erase title
  tft.drawBitmap(10,14,capBitmap,             // replacing it with capacitor icon
      48,16,TFT_BLACK);
  for (int i=0; i<4; i++) {                   // do 4 measurements, 1 sec apart
    float msmt = getCapacitance();            // calculate L value
    tft.setTextColor(C_COLOR,TFT_BLACK);
    showResult(msmt);                         // show msmt on screen
    tft.setTextColor(TFT_BLACK,C_COLOR);      // invert text colors
    showUnits(msmt);                          // and show units in title bar
    sendData(msmt);                           // and send it to serial port
    dit();                                    // announce each measurement
    wait(1000);                               // wait one second before doing it again
  }
  setMode(L_MODE);                            // turn relay off when no longer needed
}


// ===========  BUTTON HANDLERS ==========================================================

void handleLKey() {                           // do something when L key is pressed:
  idle = false;                               // disable battery icon updates
  newScreen("Inductance",L_COLOR);            // start new titled screen
  setLEDs(1,0);                               // turn on the L LED
  wait(500);                                  // how long did user press the L button?
  if (LKeyPressed()) doData();                // L long press = Engineering screen
  else doInductance();                        // L short press = Inductance screen
  setLEDs(0,0);                               // turn off the LEDs     
  idle = true;                                // enable battery icon updates
}

void handleCKey() {                           // do something when C key is pressed:
  idle = false;                               // disable battery icon updates
  newScreen("Capacitance",C_COLOR);           // start a new titled screen
  setLEDs(0,1);                               // turn on the C LED
  wait(500);                                  // how long did user press the C button?
  if (CKeyPressed()) doCalibration();         // C long press = Calibration screen
  else doCapacitance();                       // C short press = Capacitance screen
  setLEDs(0,0);                               // turn off LEDs
  idle = true;                                // enable battery icon updates
}


// ===========  MAIN PROGRAM ==========================================================

void setup() {
  initPorts();                                // initialize all MCU ports
  initDisplay();                              // initialize ST7735 display
  initSerial();                               // initialize Serial port 
  initTimers();                               // initialize hardware timers         
  setMode(L_MODE);                            // turn off relay, save 30mA
  insertCCal(false);                          // take calibration cap out of circuit
  setLEDs(0,0);                               // start with both LEDs off
  dit(); dah(); dit(); dit();                 // Morse 'L' for LC Meter
  splashScreen(WARMUP_TIME);                  // do splash screen with warmup countdown
  doCalibration();                            // then calibrate
}

void loop() {                                 // wait for user to press a button...
  if (LKeyPressed()) handleLKey();            // do something when L key is pressed
  else if (CKeyPressed()) handleCKey();       // do something when C key is pressed
}
