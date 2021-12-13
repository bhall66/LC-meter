 /**************************************************************************
      Author:   Bruce E. Hall, w8bh.net
        Date:   12 Dec 2021
    Hardware:   ATMEGA328, Nokia5510 display, CoreWeaver PCB
    Software:   Arduino IDE 1.8.13
       Legal:   Copyright (c) 2021  Bruce E. Hall.
                Open Source under the terms of the MIT License. 
    
 Description:   LC Meter, using harware designed by "coreWeaver"
                See https://github.com/coreWeaver/LC-Meter for
                his project information.
                See w8bh.net for description of this software.


     Summary:   The LC meter has two buttons, each with an adjacent LED.
                The "L" button is for inductance measurement.
                The "C" button is for capacitance measurement.

                After the meter warms up and is calibrated, insert the device
                under test and press L or C.
                The meter will take four measurementsts, each one second apart.
                The LED is lit when measurements are being taken.
                The meter also sends its measurement data to the serial port.

                To force a calibration, hold down the L key.
                To get continuous data, hold down the C key.
                To stop continuous data & recalibrate, hold down both keys.
 **************************************************************************/


#include <Adafruit_GFX.h>                     // Adafruit Graphics Library
#include <Adafruit_PCD8544.h>                 // Adafruit Nokia 5110 display library

#define C_KEY            9                    // PB1: "KEY1" pushbutton
#define L_KEY            2                    // PD2: "KEY2" pushbutton
#define C_LED            5                    // PD5: LED fir C_MODE
#define L_LED            6                    // PD6: LED for L_MODE
#define MODE_RELAY       8                    // PB0: DPDT MODE relay (0=L, 1=C)
#define CAL_RELAY       10                    // PB2: SPST CALIBRATE relay
#define VBAT            A5                    // PC5: BATTERY voltage monitor
#define LCD_RESET        3                    // PD3: LCD reset line
#define LCD_CLK         13                    // PB5: LCD data clock
#define LCD_MOSI        11                    // PB3: LCD data input 
#define LCD_DC          12                    // PB4: LCD data/command line

#define C_MODE           1                    // capacitance mode
#define L_MODE           0                    // inductance mode
#define CCAL          1000                    // value of calibration cap, in pF
#define BAUD_RATE     9600                    // serial output data rate
#define WARMUP_TIME     20                    // warm up time, in seconds
#define BATT_TIMER     180                    // time between battery checks, in seconds
#define DATA_TIMER      60                    // time between serial data output, in sec
#define R_RATIO        2.0                    // set to (R5+R6)/R6.  Should be ~2.0
#define CONTRAST        60                    // display contrast. Values 50-70 usually OK.
#define DEVICE_NAME   "LC METER v1"           // device name & version
#define OWNER         "W8BH"                  // Your name or call here

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

Adafruit_PCD8544 lcd = Adafruit_PCD8544       // variable for Nokia display 
  (LCD_CLK, LCD_MOSI, LCD_DC, -1, -1);        // call with pins for CLK, MOSI, D/C, CS, RST


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
  OCR1A   = 31250-1;                          // compare match register 8 Mhz/256/1Hz
  TIMSK1  = bit(OCIE1A);                      // enable timer compare interrupt

  TCNT2   = 0;                                // TIMER2 SETUP: interrupts at 1000 Hz
  TCCR2A  = bit(WGM21);                       // CTC Mode; no external outputs
  TCCR2B  = bit(CS22);                        // prescalar /64  
  OCR2A   = 125-1;                            // 8 Mhz/64/1000Hz = 125 (250 for 16MHz)
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
  const int x=73, y=0;                        // battery icon screen posn
  lcd.fillRect(x,y,11,4,WHITE);               // erase previous battery
  lcd.drawRect(x,y,10,4,BLACK);               // draw empty battery
  lcd.drawRect(x+9,y+1,2,2,BLACK);            // pos terminal on right
  if (vbat>8.3)                               // 100% level
    lcd.drawRect(x+1,y+1,8,2,BLACK);
  else if (vbat>7.8)                          // 75% level
    lcd.drawRect(x+1,y+1,6,2,BLACK);
  else if (vbat>7.2)                          // 50% level
    lcd.drawRect(x+1,y+1,4,2,BLACK);
  else if (vbat>7.0)                          // 25% level
    lcd.drawRect(x+1,y+1,2,2,BLACK);
  lcd.display();                              // show icon on screen
}

void checkBattery() {
  lastBattCheck = seconds;                    // update timer 
  voltage = getBattVoltage();                 // check the battery voltage
  drawBattery(voltage);                       // and draw icon on screen
}


// ===========  HARDWARE INTERFACE ROUTINES  ==================================================

void initPorts() {                            // INITIALIZE ALL MCU PIN INTERFACES
  pinMode(L_KEY,INPUT_PULLUP);                // L pushbutton
  pinMode(C_KEY,INPUT_PULLUP);                // C pushbutton
  pinMode(VBAT, INPUT);                       // battery voltage /2
  pinMode(MODE_RELAY, OUTPUT);                // DPDT relay control
  pinMode(CAL_RELAY,  OUTPUT);                // SPST relay
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

void setMode(int newMode) {                   // set Mode (Inductance vs. Capacitance)
  mode = newMode;                             // remember the mode
  digitalWrite(MODE_RELAY, mode);             // by controlling the DPDT relay (on=cap, off=ind)
}

void insertCCal(bool on = true) {             // insert the Calibration Capacitor into circuit
  digitalWrite(CAL_RELAY,!on);                // grounding pin turn on SPST relay & inserts CCal
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


// ===========  NOKIA DISPLAY ROUTINES   =====================================================

void resetDisplay() {                         // display hardware reset
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, LOW);               // low-going pulse on reset line
  wait(5);                                    // for 5 mS
  digitalWrite(LCD_RESET, HIGH);
}

void initDisplay() {
  resetDisplay();                             // force screen hardware reset
  lcd.begin();                                // get Nokia screen started
  lcd.setContrast(CONTRAST);                  // values 50-70 are usually OK
  lcd.clearDisplay();                         // start with blank screen
  lcd.setTextColor(BLACK,WHITE);              // black text on white background
  lcd.setTextSize(1);                         // standard 5x7 font
  lcd.display();                              // now show it (blank screen)
}

void newScreen(char* title) {                 // start new, titled screen
  lcd.clearDisplay();                         // blank to start
  lcd.setCursor(0,0);                         // top, left cursor position
  lcd.println(title);                         // add title
  lcd.display();                              // and show it
  checkBattery();                             // update icon with each new screen
}

void showCalIcon(int seconds) {               // call with time displayed, in sec
  for (int i=0; i<seconds; i++) {             // animate this icon...
    lcd.fillRect(25,8,32,32,WHITE);           // erase previous icon
    lcd.drawBitmap(25,8,calBitmap1,           // draw icon, no center dot
      32,32,BLACK);  
    lcd.display(); wait(500);                 // show it for half a sec
    lcd.drawBitmap(25,8,calBitmap2,           // draw icon with center dot
      32,32,BLACK);  
    lcd.display(); wait(500);                 // show it for half a sec
  }  
}

void showDone() {
  lcd.setCursor(0,40);                        // at bottom of screen:
  lcd.print ("DONE.");                        // that's all, folks
  lcd.display();                              // show it
}

char endChar() {                              // returns F or H, depending on mode
  return (mode==L_MODE)?'H':'F'; 
}

void fourDigits(float x) {                    // print number with 4 digit precision:
  if (x<10) lcd.print(x,3);                   // x.xxx
  else if (x<100) lcd.print(x,2);             // xx.xx
  else lcd.print(x,1);                        // xxx.x
}

void showResult(float x) {
  if (x<0) lcd.print("0.000 p");              // "too small"
  else if (x<1000) {              
    fourDigits(x);                            // display pico
    lcd.print(" p");  
  } else if (x<1000000) {    
    fourDigits(x/1000);                       // display nano
    lcd.print(" n");
  } else if (x<1000000000) { 
    fourDigits(x/1000000);                    // display micro
    lcd.print(" u");
  } else if (x<1000000000000) {  
    fourDigits(x/1000000000);                 // display milli
    lcd.print(" m");
  } else lcd.print("???? ");                  // "too large" error 
  lcd.println(endChar());                     // end with units
  lcd.display();                              // update display
}

void showData(float msmt) {                   // Show Data on Nokia screen:
  lcd.clearDisplay();
  lcd.setCursor(0,0);
  lcd.print("F1  ");                          // 1. Oscillator Freq without CCal
  lcd.print(F1); lcd.println(" Hz");
  lcd.print("F2  ");                          // 2. Oscillator freq with CCal
  lcd.print(F2); lcd.println(" Hz");
  lcd.print("F3  ");                          // 3. Oscillator freq with DUT
  lcd.print(F3); lcd.println(" Hz");
  lcd.print("Time ");                         // 4. Time since meter turned on
  lcd.print(seconds); lcd.println("  s");
  lcd.print("Batt ");                         // 5. Battery voltage
  lcd.print(voltage); lcd.println("  V");
  lcd.print("Msmt ");                         // 6. Measurement of DUT
  showResult(msmt);
}

void removeDUT(int secs=8) {                  // remind user to remove DUT!
  const int y = 15;                           // put the message mid-screen
  lcd.setCursor(0,y);   
  lcd.println(" Remove");                     // tell user what to do.      
  lcd.println(" DUT Now"); 
  for (int i=secs; i>0; i--) {                // for the specified # of seconds,
    lcd.setCursor(74,y);
    lcd.print(i);                             // show user a countdown
    lcd.display();                            // while we wait 
    wait(1000);                               
  }
  lcd.fillRect(0,y,84,16,WHITE);              // finally, erase the line
}

void stabilize(int secs=3) {                  // allow oscillator time to stabilize
  lcd.setCursor(0,23);                        // (3 seconds by default)
  lcd.print("Analyzing..."); 
  for (int i=secs; i>0; i--) {                // for the specified # of seconds
    lcd.setCursor(74,23);
    lcd.print(i);                             // show user a countdown
    lcd.display();                            // while we wait 
    wait(1000);                               
  }
  lcd.fillRect(0,23,84,8,WHITE);              // finally, erase the line
}

void warmUp(int seconds) {
  newScreen(DEVICE_NAME);                     // start with fresh screen
  lcd.print("Warming Up");
  lcd.setCursor(0,40);                        // go to bottom of screen
  lcd.print(OWNER);                           // and print owner
  lcd.setTextSize(2);                         // big font = big numbers
  for (int i=seconds; i>0; i--) {
    lcd.fillRect(25,20,36,16,WHITE);          // erase previous count
    lcd.setCursor(25,20);                     // center numbers on screen                 
    lcd.print(i);                             // show new count
    lcd.display();                            // & update display each time
    if (LKeyPressed()||CKeyPressed())         // allow escape from long warmup
      break;                                  // outta here
    wait(1000);                               // one second intervals
  }
  lcd.setTextSize(1);                         // return to normal font
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

void doData() {                               // show all data on Nokia screen
  float msmt;
  while (!(LKeyPressed()&&CKeyPressed())) {   // until both keys are pressed.
    if (LKeyPressed()) setMode(L_MODE);       // set Inductance mode with L key
    else if (CKeyPressed()) setMode(C_MODE);  // set Capacitance mode with C key
    if (mode==L_MODE) {                       // doing inductance, so                        
      setLEDs(1,0);                           // keep the L LED on
      msmt = getInductance();                 // and calculate inductance
    } else {                                  // doing capacitance, so
      setLEDs(0,1);                           // keep the C LED on
      msmt = getCapacitance();                // and calculate capacitance
    }
    voltage = getBattVoltage();               // check the battery
    showData(msmt);                           // show all current data
    if (seconds >= (lastData+DATA_TIMER))     // how long since last serial data sent?
      sendData(msmt);                         // send it if enough time has elapsed
    wait(1000);                               // refresh the data screen every second
  }                                           // user held down both keys, so..
  doCalibration();                            // end the session with a calibration
}

void doCalibration() {
  newScreen("Calibration");                   // start with fresh new screen
  removeDUT();                                // remind user to remove DUT
  setMode(C_MODE);                            // calibration needs L&C both in circuit
  setLEDs(1,1);                               // turn on both LEDs
  insertCCal(true);                           // insert calibration cap parallel to LC tank
  showCalIcon(3);                             // allow oscillator time to stabilize
  F2 = frequency;                             // save F2 frequency = C_CAL inline
  setLEDs(0,1);                               // mark removal of C_CAL
  insertCCal(false);                          // remove C_CAL from circuit
  showCalIcon(3);                             // allow oscillator time to stabilize
  F1 = frequency;                             // save F1 frequency = LC without C_CAL
  F3 = 0;                                     // no DUT being measured here
  lastCal = seconds;                          // remember when this cal took place
  setLEDs(0,0);                               // turn off all LEDs
  setMode(L_MODE);                            // turn relay off when not in use
  showDone();                                 // let user know the show is over
  sendCalibration();                          // send calibration data to serial port
}

void doInductance() {
  setMode(L_MODE);                            // relay off when measuring L
  stabilize();                                // allow oscillator time to stabilize
  for (int i=0; i<4; i++) {                   // lets do 4 measurements, 1 sec apart...
    lcd.setCursor(0,23) ;
    lcd.print(" L= ");
    float msmt = getInductance();             // calculate L value
    showResult(msmt);                         // show in on screen
    sendData(msmt);                           // and send it to serial port
    wait(1000);                               // wait one second before doing it again
   }
  showDone();                                 // let user know the show is over
}

void doCapacitance() {
  setMode(C_MODE);                            // pull relay closed to measure C
  stabilize();                                // allow oscillator time to stabilize
  for (int i=0; i<4; i++) {                   // lets do 4 measurements, 1 sec apart...
    lcd.setCursor(0,23);
    lcd.print(" C= ");
    float msmt = getCapacitance();            // calculate L value
    showResult(msmt);                         // show in on screen
    sendData(msmt);                           // and send it to serial port
    wait(1000);                               // wait one second before doing it again
  }
  setMode(L_MODE);                            // turn relay off when no longer needed
  showDone();                                 // let user know the show is over
}


// ===========  BUTTON HANDLERS ==========================================================

void handleLKey() {                           // do something when L key is pressed:
  idle = false;                               // disable battery icon updates
  newScreen("Inductance");                    // start a new screen with title & icon
  lcd.drawBitmap(18,8,coilBitmap,48,16, BLACK);
  lcd.display();
  setLEDs(1,0);                               // turn on the L LED
  wait(500);                                  // how long did user press the L button?
  if (LKeyPressed()) doCalibration();         // L long press = Calibration screen
  else doInductance();                        // L short press = Inductance screen
  setLEDs(0,0);                               // turn off the LEDs     
  idle = true;                                // enable battery icon updates
}

void handleCKey() {                           // do something when C key is pressed:
  idle = false;                               // disable battery icon updates
  newScreen("Capacitance");                   // start a new screen with title & icon
  lcd.drawBitmap(18,8,capBitmap,48,16,BLACK);
  lcd.display();
  setLEDs(0,1);                               // turn on the C LED
  wait(500);                                  // how long did user press the C button?
  if (CKeyPressed()) doData();                // C long press = Diagnostic screen
  else doCapacitance();                       // C short press = Capacitance screen
  setLEDs(0,0);                               // turn off LEDs
  idle = true;                                // enable battery icon updates
}


// ===========  MAIN PROGRAM ==========================================================

void setup() {
  initPorts();                                // initialize all MCU ports
  initTimers();                               // initialize hardware timers
  initDisplay();                              // initialize Nokia display
  initSerial();                               // initialize Serial port           
  setMode(L_MODE);                            // turn off relay, save 30mA
  insertCCal(false);                          // take calibration cap out of circuit
  setLEDs(0,0);                               // start with both LEDs off
  warmUp(WARMUP_TIME);                        // do a warmup countdown
  doCalibration();                            // then calibrate
}

void loop() {                                 // wait for user to press a button...
  if (LKeyPressed()) handleLKey();            // do something when L key is pressed
  else if (CKeyPressed()) handleCKey();       // do something when C key is pressed
}
