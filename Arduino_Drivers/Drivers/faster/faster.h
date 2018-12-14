/*
 * Filename: faster.h
 * Author: John Messerly and Christopher Yin 
 * Description: Header file containing methods and data fields for the faster driver.
 * Date: 10.18.18
 *
 * Data Fields:
 *         
 *            int ledWritePins[]
 *            int potPins[] 
 *            int ledPower[]
 *            int cameraPin
 *            
 *            int intensity[] 
 *            int on[]
 *            int mode
 *            
 *            int minFPS
 *            int maxFPS
 *            int maxIntensity
 *            
 *
 * Methods:
 *            void init_lcd();
 *            void updateLCD(int val);
 *            void updateFPS();
 *            void updateLED();
 *            void updateLCD(int val);
 *            void modeCheck();
 *            void startCheck();
 *            void init_LED(int led1,int led2, int led3);
 *            void shutdown_LED();
 *            void camera_write_trig1();
 *            void camera_write_trig2();
 *            void camera_write_trig3();
 *            void camera_write_const();
 *            void dPotWrite(int address, int val)
 *
 */

#ifndef faster
#define faster

// define constants for addressing purposes
#define LED410 0
#define LED470 1
#define LED560 2
#define FPS 3
#define VAL_CURSOR 10
#define CONSTANT_MODE 0
#define TRIGGER1_MODE 1
#define TRIGGER2_MODE 2
#define TRIGGER3_MODE 3

// import libraries
#include <Arduino.h>
#include <SPI.h>
#include <Button.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

/*
 * Begin data field declarations
 */



LiquidCrystal_I2C lcd(0x27,20,4);

int ledWritePins[] = {7,8,9};   //led output pins
int potPins[] = {A2,A1,A0,A3};  //pot read pins
int selectPin = 10;             //digipot select pin
int potChannel[] = {0,2,1};     //digitpot pot address bytes

Button startButton = Button(3,PULLUP);


Button modeButton = Button(4,PULLUP);
int cameraPin = 5;
int dcount = 0;
//state variables
float intensity[] = {-1,-1,-1,-1};
int on[] = {LOW,LOW,LOW};
int mode = CONSTANT_MODE;
boolean start = false;
int potval;         //used in updateLED()
unsigned long temp; //used in updateLED()
int cycle_led = 0;  //used in trigger3 mode


bool switchEnable = false;

int oldMillis = 0;

//technical parameters
int minFPS = 5, maxFPS = 160, maxIntensity = 100, potMin = 830, potMax = 315;

//wave parameters
int t_exposure;      //CALCULATED AS 1/FPS - t_dead
int t_dead = 1;     


float oldLCD[] = {-200.0,-200.0,-200.0,-200.0};

/*
 * Begin forward declaration of functions.
 */

//void setHigh();
void init_lcd();
void updateLCD(int val);
void updateFPS();
void updateLED();
void dPotWrite(int pot, int potval);
void modeCheck();
void startCheck();
void init_LED(int led1,int led2, int led3);
void shutdown_LED();
void camera_write_trig1();
void camera_write_trig2();
void camera_write_trig3();
void camera_write_const();
void dPotWrite(int channel, int potval);

/*
 * Begin function definitions.
 */


/*
 * Name:        init_lcd
 * Purpose:     initialize menu display of LCD
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    Initiates communication with I2C LCD screen. Turns on backlight.
 *    Prints LED names and "FPS" to set positions on screen. Reads LED
 *    and FPS potentiometers, updates values and prints to screen. 
 *    Driver box is default "OFF" and in "CONSTANT" mode.
 */
void init_lcd(){
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,LED410);
  lcd.print("LED415: ");
  lcd.setCursor(0,LED470);
  lcd.print("LED470: ");
  lcd.setCursor(0,LED560);
  lcd.print("LED560: ");
  lcd.setCursor(0,FPS);
  lcd.print("FPS:    ");

  updateLED();
  updateFPS();

  //print capture status
  lcd.setCursor(17,3);
  lcd.print("OFF");

  //print mode
  lcd.setCursor(16,0);
  lcd.print("CNST");
  
}

/*
 * Name:        updateLCD
 * Purpose:     update the intensity value displayed on screen for an
 *                LED or FPS
 * Parameter:   int val - address of LED or FPS
 * Return:      n/a
 * Description: 
 *    Address of "val" corresponds to line number of LCD. Sets cursor
 *    to line "val" (vertically) and to position VAL_CURSOR (horizon-
 *    tally). Prints indent followed by value stored in intensity[]
 *    array at address "val."
 */
void updateLCD(int val){
  // set cursor to appropriate line
  float magChange = oldLCD[val] - intensity[val];
  if (magChange < 0.0) {
    magChange = magChange * -1.0;
  }
  if (magChange < 0.01){
    if (intensity[val] > 0.01){
      return;
    }
  }
  (lcd).setCursor(VAL_CURSOR,val);
  (lcd).print("     ");

  (lcd).setCursor(VAL_CURSOR,val);
  // print updated value

  if (intensity[val] >= 100){
    (lcd).print(int(intensity[val]));
  }
  else if ((intensity[val] >= 10) && (intensity[val] < 100)){
    (lcd).print(intensity[val],1);
  }
  else {
    (lcd).print(intensity[val],2);
  }
  oldLCD[val] = intensity[val];
}

/*
 * Name:        updateFPS
 * Purpose:     update stored value of FPS
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    Stores previously recorded FPS value. Reads in new value to
 *    intensity[] array at address FPS. Value is obtained by
 *    reading voltage at wiper of potentiometer and mapping to
 *    present range of minFPS to maxFPS. Print new value of FPS
 *    to LCD screen if value has changed. Update exposure time
 *    (t_exposure).
 */
void updateFPS(){

  int oldFPS = intensity[FPS];
   
  //update FPS value
  intensity[FPS] = abs(map(analogRead(potPins[FPS]),0,1023,minFPS,maxFPS)-(minFPS+maxFPS));
  
  //update LCD
  if(oldFPS != intensity[FPS]){
    updateLCD(FPS);
    //update exposure time
    cli();
    t_exposure = 1000/intensity[FPS] - t_dead;
    dcount = 0;
    sei();
    //OCR1A = int(t_exposure * 16.66666);
  }
}

/*
 * Name:        updateLED
 * Purpose:     update intensity of LEDs
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    For each LED, read voltage at wiper of corresponding
 *    rotary pot, map to intensity scale from 0-100, and
 *    adjust digipot appropriately. Print new value of
 *    intensity for each LED if value has changed.
 */
void updateLED(){
  for(int led=0;led<3;led++){
    float oldLed = intensity[led];
    //delay(500);
    //update stored led intensity
    temp = analogRead(potPins[led]);
    
    float subPercent = 0.50; // i want to spend x percent between 0 and 1. default to 1
    //float superPercent = 1 - subPercent; // i spend the rest of my time 1 and 99
    float subThresh = 1023 * subPercent;
    float subScale = 1 / subThresh;
    float value = 0;
    int potval = 0;
    int potMin = 0;
    int potMax = 90;
    int potThresh = (potMin + potMax) * subPercent;
    if (temp < subThresh){
      value = ((float) temp) * subScale;
      potval = map(temp,0,subThresh,potMin,potThresh);
    }
    else {
      value = map(temp,subThresh,1023,1,100);
      potval = map(temp,subThresh,1023,potThresh,potMax);
    }

  
     
    intensity[led] = value;
    if (intensity[led] != oldLed){
      updateLCD(led);
    }
     
    dPotWrite(potChannel[led],potval);
    
    
  }
}

/*
 * Name:        dPotWrite
 * Purpose:     update value of rheostat-configured digipot terminals
 * Parameter:
 *              int channel - channel on digipot to be adjusted
 *              int potval - wiper position to be written
 * Return:      n/a
 * Description: 
 *    Digipot controlled via SPI. selectPin is written LOW to load register.
 *    First channel address and then position value are loaded into register.
 *    selectPin is returned to HIGH to transfer 11-bit message to digipot.
 *    See datasheet for AD5204 chip: 
 *    http://www.analog.com/media/en/technical-documentation/data-sheets/AD5204_5206.pdf
 */
void dPotWrite(int channel, int potval){
  digitalWrite(selectPin,LOW);
  //delay(1);
  SPI.transfer(channel);
  SPI.transfer(potval);
  //delay(1);
  digitalWrite(selectPin,HIGH);
}

/*
 * Name:        modeCheck
 * Purpose:     check mode of LED triggering
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    If modeButton has been pressed, cycle to next mode in ordered list:
 *      1) CONSTANT
 *      2) TRIGGER1
 *      3) TRIGGER2
 *      4) TRIGGER3
 *    Print new mode to LCD.
 */
void modeCheck(){
  if(modeButton.uniquePress()){
      mode = (mode+1)%4;
      lcd.setCursor(16,0);
      switch(mode){
        case CONSTANT_MODE:
          lcd.print("CNST");
          break;
        case TRIGGER1_MODE:
          lcd.print("TRG1");
          break;
        case TRIGGER2_MODE:
          lcd.print("TRG2");
          break;
        case TRIGGER3_MODE:
          lcd.print("TRG3");
          break;
      }
  } 
}

/*
 * Name:        startCheck
 * Purpose:     check if start switch is on or off
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    If switch is in "on" position (corresponding to a
 *    button being in a "pressed" state), start is set to
 *    TRUE; otherwise, FALSE.
 */
void startCheck(){
  start = startButton.isPressed();
}


/*
 * Name:        init_LED
 * Purpose:     initialize LED pattern for specific trigger mode
 * Parameter:
 *              int led1 - if led1 is HIGH or LOW
 *              int led2 - if led2 is HIGH or LOW
 *              int led3 - if led3 is HIGH or LOW
 * Return:      n/a
 * Description: 
 *    Store HIGH/LOW values specified by parameters in corresponding
 *    address of on[] array. Turn each LED on/off according to its
 *    on[] value.
 */
void init_LED(int led1,int led2, int led3){
  on[LED410] = led1;
  on[LED470] = led2;
  on[LED560] = led3;

  for(int led=0;led<3;led++){
    digitalWrite(ledWritePins[led],on[led]);
  }
}

/*
 * Name:        shutdown_LED
 * Purpose:     turn off all LEDs
 * Parameter:   void
 * Return:      n/a
 * Description: 
 *    Set all values of on[] array to LOW. Turn off all LEDs.
 */
void shutdown_LED(){
  for(int led=0;led<3;led++){
    on[led] = LOW;
    digitalWrite(ledWritePins[led],on[led]);
  }
}



/*
 * Isolated clocked routine that handles camera triggering
 * (should interrupt at ~1khz i.e. every 1ms)
 */
ISR(TIMER1_COMPA_vect){
  if (switchEnable == true){
    if (dcount >= t_exposure) {
      digitalWrite(cameraPin,LOW);
      delay(1);
      digitalWrite(cameraPin,HIGH);
      dcount = 0;
      switch (mode) {
        case TRIGGER1_MODE:
          for(int led=0;led<=2;led++){ // all three used 
            on[led] = !on[led];
            digitalWrite(ledWritePins[led],on[led]);
          }
          break;
        case TRIGGER2_MODE:
          for(int led=1;led<=2;led++){ // only blue/green used
            on[led] = !on[led];
            digitalWrite(ledWritePins[led],on[led]);
          }
          break;
        case TRIGGER3_MODE:
          on[cycle_led] = LOW;
          cycle_led = (cycle_led + 1)%3;
          on[cycle_led] = HIGH;
          //switch LED states
          for(int led=0;led<3;led++){ // all three used
            digitalWrite(ledWritePins[led],on[led]);
          }
        break;
      }
    }
    else {
      dcount = dcount + 1;
    }
  }
}


  



#endif
