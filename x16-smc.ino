//Commander X16 - ATTiny861 - ATX Power Control, Reset / NMI
//
// By: Kevin Williams - TexElec.com
// January 22, 2021
// 
// Last Update: July 1, 2021
//
// Use Clockwise Pin Mapping when programming.
//      -----
//  9  =1    =   0
//  8  =     =   1
//  7  =  T  =   2 
//  8  =  I  =  14
// VCC =  N  =  GND
// GND =  Y  =  VCC
//  5  =  8  =  10
//  4  =  6  =  11
//  3  =  1  =  12
// 15  =     =  13
//      -----
//
#include <OneButton.h>
#include <Wire.h>

#define ATTINY861

// Button definitions
#define POWER_BUTTON_PIN         5
#define RESET_BUTTON_PIN         3
#define NMI_BUTTON_PIN           2

#define PWR_OK                  10
#define PWR_ON                   9
#define PWR_ON_MIN             100
#define PWR_ON_MAX             500
//Hold PWR_ON low while computer is on.  High while off.
//PWR_OK -> Should go high 100ms<->500ms after PWR_ON invoked.
//          Any longer or shorter is considered a fault.
// http://www.ieca-inc.com/images/ATX12V_PSDG2.0_Ratified.pdf

//Power LED
#define POW_LED                  8
#define POW_LED_ON_LEVEL       255
#define ACT_LED_DEFAULT_LEVEL    0
#define ACT_LED_ON_LEVEL       255

//Activity (HDD) LED
#define ACT_LED                  7
#define ACT_LED_DEFAULT_LEVEL    0

//Reset & NMI Lines
#define NMIB_PIN                 1
#define RESB_PIN                 0
#define RESB_HOLDTIME          500
#define NMI_HOLDTIME           300

//I2C Pins
#define I2C_SCL_PIN              4
#define I2C_SDA_PIN              6
#define I2C_ADDR              0x42  //I2C Device ID

OneButton POW_BUT(POWER_BUTTON_PIN, true, true);
OneButton RES_BUT(RESET_BUTTON_PIN, true, true);
OneButton NMI_BUT(NMI_BUTTON_PIN, true, true);

bool SYSTEM_POWERED = 0;                             //default state - Powered off
int  I2C_Data[2] = {0, 0};
bool I2C_Active = false;

void setup() {
 //initialize i2C
 Wire.begin(I2C_ADDR);                              //Initialize I2C - Device Mode
 Wire.onReceive(I2C_Receive);                       //Used to Receive Data
 //Wire.onRequest(I2C_Send);                        //Used to Send Data, may never be used
   
 POW_BUT.attachClick(Power_Button_Press);           //Should the Power off be long, or short?
 POW_BUT.attachDuringLongPress(Power_Button_Press); //Both for now

 RES_BUT.attachClick(Reset_Button_Press);           //Short Click = NMI, Long Press = Reset
 RES_BUT.attachDuringLongPress(Reset_Button_Hold);  //Actual Reset Call

 NMI_BUT.attachClick(Reset_Button_Press);           //NMI Call is the same as short Reset 
 //NMI_BUT.attachClick(HardReboot);                 //strangely, this works fine via NMI push, but fails via I2C?
  

 pinMode(PWR_OK, INPUT);
 pinMode(PWR_ON, OUTPUT);
 digitalWrite(PWR_ON, HIGH);
  
 pinMode(ACT_LED, OUTPUT);
 analogWrite(ACT_LED, 0);

 pinMode(POW_LED, OUTPUT);
 analogWrite(POW_LED, 0);

 pinMode(RESB_PIN,OUTPUT); 
 digitalWrite(RESB_PIN,LOW);                  //Hold Reset on statup

 pinMode(NMIB_PIN,OUTPUT);
 digitalWrite(NMIB_PIN,HIGH); 
}

void loop() {
 POW_BUT.tick();                              //Check Button Status
 RES_BUT.tick(); 
 NMI_BUT.tick(); 

 if ((SYSTEM_POWERED == 1) && (!digitalRead(PWR_OK)))
 {
  PowerOffSeq();
  //kill power if PWR_OK dies, and system on
  //error handling?
 }
 delay(10);                                    //Short Delay, required by OneButton if code is short
}

void Power_Button_Press() {
 if (SYSTEM_POWERED == 0) {                    //If Off, turn on
  PowerOnSeq();
 }
 else {                                        //If On, turn off
  PowerOffSeq();                               
 }
}

void I2C_Receive() {
 int ct=0;
 while (Wire.available()) {
  if (ct<2) {                                  //read first two bytes only
   I2C_Data[ct]=Wire.read();
   ct++;  
  }
  else {
   int nothing = Wire.read();                  //eat extra data, should not be sent
  }
 }
 I2C_Process();                                //process received cmd
}

//I2C Commands - Two Bytes per Command
//0x01 0x00      - Power Off
//0x01 0x01      - Hard Reboot (not working for some reason)
//0x02 0x00      - Reset Button Press
//0x03 0x00      - NMI Button press
//0x04 0x00-0xFF - Power LED Level (PWM)             //need to remove, not enough lines 
//0x05 0x00-0xFF - Activity/HDD LED Level (PWM)
void I2C_Process() {
 if (I2C_Data[0] == 1) {                       //1st Byte : Byte 1 - Power Events (Power off & Reboot)
  switch (I2C_Data[1]) {
   case 0:PowerOffSeq();                       //2nd Byte : 0 - Power Off
          break;
   case 1:HardReboot();                        //2nd Byte : 1 - Reboot (Physical Power off, wait 500ms, Power on)
          break;                               //This command triggers, but the system does not power back on?  Not sure why.
  }
 }
 if (I2C_Data[0] == 2) {                       //1st Byte : Byte 2 - Reset Event(s)
  switch (I2C_Data[1]) {
   case 0:Reset_Button_Hold();                 //2nd Byte : 0 - Reset button Press
          break;
  }
 }
 if (I2C_Data[0] == 3) {                       //1st Byte : Byte 3 - NMI Event(s)
  switch (I2C_Data[1]) {
   case 0:Reset_Button_Press();                //2nd Byte : 0 - NMI button Press
          break;
  }
 }
 if (I2C_Data[0] == 4) {                       //1st Byte : Byte 4 - Power LED Level
  analogWrite(POW_LED, I2C_Data[1]);           //2nd Byte : Set Value directly
 }
 if (I2C_Data[0] == 5) {                       //1st Byte : Byte 5 - Activity LED Level
  analogWrite(ACT_LED, I2C_Data[1]);           //2nd Byte : Set Value directly
 }
 I2C_Data[0] = 0;
 I2C_Data[1] = 0;
}

void Reset_Button_Hold() {          
 if (SYSTEM_POWERED == 1) {                    //Ignore unless Powered On
  digitalWrite(RESB_PIN,LOW);                  //Press RESET
  delay(RESB_HOLDTIME);
  digitalWrite(RESB_PIN,HIGH);
 }  
}

void Reset_Button_Press() {
 if (SYSTEM_POWERED == 1) {                    //Ignore unless Powered On
  digitalWrite(NMIB_PIN,LOW);                  //Press NMI
  delay(NMI_HOLDTIME);
  digitalWrite(NMIB_PIN,HIGH);
 }  
}

void PowerOffSeq() {
  digitalWrite(PWR_ON, HIGH);                  //Turn off supply 
  analogWrite(POW_LED, ACT_LED_DEFAULT_LEVEL); //Turn off Power LED
  SYSTEM_POWERED=0;                            //Global Power state Off
  digitalWrite(RESB_PIN,LOW);             
  delay(RESB_HOLDTIME);                        //Mostly here to add some delay between presses
}

void PowerOnSeq() {
 digitalWrite(PWR_ON, LOW);                    //turn on power supply
 unsigned long TimeDelta = 0;           
 unsigned long StartTime = millis();           //get current time
 while (!digitalRead(PWR_OK)) {                //Time how long it takes
  TimeDelta=millis() - StartTime;              //for PWR_OK to go active.
 }
 if ((PWR_ON_MIN > TimeDelta) || (PWR_ON_MAX < TimeDelta)) {
  PowerOffSeq();                               //FAULT!  Turn off supply
  //insert error handler, flash activity light & Halt?  IE, require hard power off before continue?
 }
 else {                                  
  SYSTEM_POWERED=1;                            //Global Power state On
  delay(RESB_HOLDTIME);                        //Allow system to stabilize
  digitalWrite(RESB_PIN,HIGH);                 //Release Reset
  analogWrite(POW_LED, POW_LED_ON_LEVEL);      //Turn on Power LED
 }
}

void HardReboot() {                            //This never works via I2C... Why!!!
 PowerOffSeq();    
 delay(1000);
 PowerOnSeq(); 
}
