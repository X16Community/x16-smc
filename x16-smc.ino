// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks

//#define ENABLE_NMI_BUT
//#define KBDBUF_FULL_DBG

#include "smc_button.h"
#include <Wire.h>
#include "dbg_supp.h"
#include "smc_pins.h"
#include "ps2.h"
#include "mouse.h"

// If not ATtiny861, we expect ATMega328p
#if !defined(__AVR_ATmega328P__) && !defined(__AVR_ATtiny861__)
#error "X16 SMC only builds for ATtiny861 and ATmega328P"
#endif

#define PWR_ON_MIN_MS          100
#define PWR_ON_MAX_MS          500
// Hold PWR_ON low while computer is on.  High while off.
// PWR_OK -> Should go high 100ms<->500ms after PWR_ON invoked.
//           Any longer or shorter is considered a fault.
// http://www.ieca-inc.com/images/ATX12V_PSDG2.0_Ratified.pdf

//Activity (HDD) LED
#define ACT_LED_DEFAULT_LEVEL    0
#define ACT_LED_ON_LEVEL       255

//Reset & NMI Lines
#define RESB_HOLDTIME_MS       500
#define NMI_HOLDTIME_MS        300

//I2C Pins
#define I2C_ADDR              0x42  // I2C Device ID

SmcButton POW_BUT(POWER_BUTTON_PIN);
SmcButton RES_BUT(RESET_BUTTON_PIN);
#if defined(ENABLE_NMI_BUT)
  SmcButton NMI_BUT(NMI_BUTTON_PIN);
#endif

PS2KeyboardPort<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
PS2Port<PS2_MSE_CLK, PS2_MSE_DAT, 8> Mouse;

void keyboardClockIrq() {
    Keyboard.onFallingClock();
}

void mouseClockIrq() {
    Mouse.onFallingClock();
}

bool SYSTEM_POWERED = 0;                                // default state - Powered off
int  I2C_Data[3] = {0, 0, 0};
bool I2C_Active = false;
char echo_byte = 0;

#define RESET_POLARITY_TIMOUT_MS 500
bool RESET_POLARITY_CONFIRMED = false;
uint32_t reset_polarity_timer;
int RESET_ACTIVE = LOW;
int RESET_INACTIVE = HIGH;

void setup() {  
#if defined(USE_SERIAL_DEBUG)
    Serial.begin(SERIAL_BPS);
#endif
#if defined(KBDBUF_FULL_DBG)
    Serial.begin(SERIAL_BPS);
#endif

    //Setup Timer 1 interrupt to run every 25 us >>>
    cli();

#if defined(__AVR_ATtiny861__)
  static_assert(TIMER_TO_USE_FOR_MILLIS != 1);

  TCCR1A = 0;
  TCCR1C = 0;
  TCCR1D = 0;
  TC1H = 0;
  TCNT1 = 0;

  PLLCSR = 0;
  OCR1B = 0;

  TCCR1B = 0b01000101; // Prescaler sel = clk / 16
  OCR1A = 100;         // 100 x 1.0 us = 100 us

  TIMSK |= 1 << OCIE1A;
#endif

#if defined(__AVR_ATmega328P__)
    TCCR1A = 0;
    TCCR1B = 0; 
    TCNT1 = 0;
  
    TCCR1B |= 8;  //Clear Timer on Compare Match (CTC) Mode
    TCCR1B |= 2;  //Prescale 8x => 1 tick = 0.5 us @ 16 MHz
    OCR1A = 200;   //200 x 0.5 us = 100 us
  
    TIMSK1 = (1<<OCIE1A);
#endif

    //Timer 1 interrupt setup is done, enable interrupts
    sei();

    DBG_PRINTLN("Commander X16 SMC Start");
    
    //initialize i2C
    Wire.begin(I2C_ADDR);                               // Initialize I2C - Device Mode
    Wire.onReceive(I2C_Receive);                        // Used to Receive Data
    Wire.onRequest(I2C_Send);                           // Used to Send Data, may never be used

    POW_BUT.attachClick(Power_Button_Press);            // Should the Power off be long, or short?
    POW_BUT.attachDuringLongPress(Power_Button_Press);  // Both for now

    RES_BUT.attachClick(Reset_Button_Press);            // Short Click = NMI, Long Press = Reset
    RES_BUT.attachDuringLongPress(Reset_Button_Hold);   // Actual Reset Call

#if defined(ENABLE_NMI_BUT)
    NMI_BUT.attachClick(Reset_Button_Press);            // NMI Call is the same as short Reset
    NMI_BUT.attachClick(HardReboot);                  // strangely, this works fine via NMI push, but fails via I2C?
#endif

    pinMode(PWR_OK, INPUT);
    pinMode(PWR_ON, OUTPUT);
    digitalWrite(PWR_ON, HIGH);

    pinMode(ACT_LED, OUTPUT);
    analogWrite(ACT_LED, 0);

    pinMode(RESB_PIN,OUTPUT);
    digitalWrite(RESB_PIN, RESET_ACTIVE);                 // Hold Reset on startup

    pinMode(NMIB_PIN,OUTPUT);
    digitalWrite(NMIB_PIN,HIGH);

    // PS/2 host init
    Keyboard.begin(keyboardClockIrq);
    Mouse.begin(mouseClockIrq);
}

void loop() {
    POW_BUT.tick();                             // Check Button Status
    RES_BUT.tick();
#if defined(ENABLE_NMI_BUT)
    NMI_BUT.tick();
#endif
    MouseTick();
    KeyboardTick();
    
    if ((SYSTEM_POWERED == 1) && (!digitalRead(PWR_OK)))
    {
        PowerOffSeq();
        //kill power if PWR_OK dies, and system on
        //error handling?
    }

    if (Keyboard.getResetRequest()) {
     Reset_Button_Hold();
     Keyboard.ackResetRequest();
    }

    if (Keyboard.getNMIRequest()) {
      Reset_Button_Press();
      Keyboard.ackNMIRequest();
    }

    if ((SYSTEM_POWERED == 1) && (RESET_POLARITY_CONFIRMED == false)) {
      if ((millis()-reset_polarity_timer) > RESET_POLARITY_TIMOUT_MS) {
        RESET_POLARITY_CONFIRMED = true;
        RESET_ACTIVE = HIGH;
        RESET_INACTIVE = LOW;
        digitalWrite(RESB_PIN, RESET_INACTIVE);
      }
    }

    // DEBUG: turn activity LED on if there are keys in the keybuffer
    delay(10);                                  // Short Delay, required by OneButton if code is short   
}

void Power_Button_Press() {
    if (SYSTEM_POWERED == 0) {                  // If Off, turn on
        PowerOnSeq();
    }
    else {                                      // If On, turn off
        PowerOffSeq();
    }
}

void I2C_Receive(int) {
    // We are resetting the data beforehand
    I2C_Data[0] = 0;
    I2C_Data[1] = 0;
    I2C_Data[2] = 0;

    int ct=0;
    while (Wire.available()) {
        if (ct<3) {                             // read first three bytes only
            byte c = Wire.read();
            I2C_Data[ct] = c;
            ct++;
        }
        else {
            Wire.read();                        // eat extra data, should not be sent
        }
    }

    if ((ct == 2 && I2C_Data[0] != 0x1a) || (ct == 3 && I2C_Data[0] == 0x1a)) {
        // Offset 0x1a (Send two-byte command) requires three bytes, the other offsets require two bytes
        I2C_Process();                          // process received cmd
    }
}

//I2C Commands - Two Bytes per Command
//0x01 0x00      - Power Off
//0x01 0x01      - Hard Reboot (not working for some reason)
//0x02 0x00      - Reset Button Press
//0x03 0x00      - NMI Button press
//0x04 0x00-0xFF - Power LED Level (PWM)        // need to remove, not enough lines
//0x05 0x00-0xFF - Activity/HDD LED Level (PWM)
void I2C_Process() {
    if (I2C_Data[0] == 1) {                     // 1st Byte : Byte 1 - Power Events (Power off & Reboot)
        switch (I2C_Data[1]) {
            case 0:PowerOffSeq();               // 2nd Byte : 0 - Power Off
                break;
            case 1:HardReboot();                // 2nd Byte : 1 - Reboot (Physical Power off, wait 500ms, Power on)
                break;                          // This command triggers, but the system does not power back on?  Not sure why.
        }
    }
    if (I2C_Data[0] == 2) {                     // 1st Byte : Byte 2 - Reset Event(s)
        switch (I2C_Data[1]) {
            case 0:Reset_Button_Hold();         // 2nd Byte : 0 - Reset button Press
                break;
        }
    }
    if (I2C_Data[0] == 3) {                     // 1st Byte : Byte 3 - NMI Event(s)
        switch (I2C_Data[1]) {
            case 0:Reset_Button_Press();        // 2nd Byte : 0 - NMI button Press
                break;
        }
    }
    // ["Byte 4 - Power LED Level" removed]
    if (I2C_Data[0] == 5) {                     // 1st Byte : Byte 5 - Activity LED Level
        analogWrite(ACT_LED, I2C_Data[1]);      // 2nd Byte : Set Value directly
    }

    if (I2C_Data[0] == 7) {                     // 1st Byte : Byte 7 - Keyboard: read next keycode
        // Nothing to do here, register offset 7 is read only
    }
    if (I2C_Data[0] == 8) {
      echo_byte = I2C_Data[1];
    }
    if (I2C_Data[0] == 9) {
      DBG_PRINT("DBG register 9 called. echo_byte: ");
      DBG_PRINTLN((byte)(echo_byte), HEX);
    }
    
    if (I2C_Data[0] == 0x19){
      //Send command to keyboard (one byte)
      Keyboard.sendPS2Command(I2C_Data[1]);
    }
    if (I2C_Data[0] == 0x1a){
      //Send command to keyboard (two bytes)
      Keyboard.sendPS2Command(I2C_Data[1], I2C_Data[2]);
    }
}

void I2C_Send() {
    // DBG_PRINTLN("I2C_Send");
    int nextKey = 0;
    if (I2C_Data[0] == 0x7) {   // 1st Byte : Byte 7 - Keyboard: read next keycode
      RESET_POLARITY_CONFIRMED = true;
      
      if (kbd_init_state == KBD_READY && Keyboard.available()) {
          nextKey  = Keyboard.next();
          Wire.write(nextKey);
      }
      else {
          Wire.write(0);
      }
    }
    if (I2C_Data[0] == 8) {
      Wire.write(echo_byte);
    }

    if (I2C_Data[0] == 0x18){
      //Get keyboard command status
      Wire.write(Keyboard.getCommandStatus());
    }
    if (I2C_Data[0] == 0x21){
      //Get mouse packet
      if (Mouse.count()>2){
          uint8_t buf[3];  
          buf[0] = Mouse.next();
      
          if ((buf[0] & 0xc8) == 0x08){
              //Valid first byte - Send mouse data packet
              buf[1] = Mouse.next();
              buf[2] = Mouse.next();
              Wire.write(buf,3);
          }
          else{
              //Invalid first byte - Discard, and return a 0
              Wire.write(0);
          }
      }
      else{
          Wire.write(0);
      }
    }
}

void Reset_Button_Hold() {
    Keyboard.flush();
    Mouse.reset();
    if (SYSTEM_POWERED == 1) {                  // Ignore unless Powered On
        digitalWrite(RESB_PIN, RESET_ACTIVE);    // Press RESET
        delay(RESB_HOLDTIME_MS);
        digitalWrite(RESB_PIN, RESET_INACTIVE);
        analogWrite(ACT_LED, 0);
        mouse_init_state = MOUSE_INIT_STATE::START_RESET;
        kbd_init_state = MOUSE_INIT_STATE::START_RESET;
    }
}

void Reset_Button_Press() {
    if (SYSTEM_POWERED == 1) {                  // Ignore unless Powered On
        digitalWrite(NMIB_PIN,LOW);             // Press NMI
        delay(NMI_HOLDTIME_MS);
        digitalWrite(NMIB_PIN,HIGH);
    }
}

void PowerOffSeq() {
    digitalWrite(PWR_ON, HIGH);                 // Turn off supply
    SYSTEM_POWERED=0;                           // Global Power state Off
    digitalWrite(RESB_PIN, RESET_ACTIVE);
    delay(RESB_HOLDTIME_MS);                    // Mostly here to add some delay between presses
}

void PowerOnSeq() {
    digitalWrite(PWR_ON, LOW);                  // turn on power supply
    unsigned long TimeDelta = 0;
    unsigned long StartTime = millis();         // get current time
    while (!digitalRead(PWR_OK)) {              // Time how long it takes
        TimeDelta=millis() - StartTime;         // for PWR_OK to go active.
    }
    if ((PWR_ON_MIN_MS > TimeDelta) || (PWR_ON_MAX_MS < TimeDelta)) {
        PowerOffSeq();                          // FAULT! Turn off supply
        // insert error handler, flash activity light & Halt?   IE, require hard power off before continue?
    }
    else {
        delay(RESB_HOLDTIME_MS);                // Allow system to stabilize
        SYSTEM_POWERED=1;                       // Global Power state On
        reset_polarity_timer = millis();
        digitalWrite(RESB_PIN, RESET_INACTIVE); // Release Reset
    }
}

void HardReboot() {                             // This never works via I2C... Why!!!
    PowerOffSeq();
    delay(1000);
    PowerOnSeq();
}

ISR(TIMER1_COMPA_vect){
#if defined(__AVR_ATtiny861__)
  // Reset counter since timer1 doesn't reset itself.
  TC1H = 0;
  TCNT1 = 0;
#endif
    Keyboard.timerInterrupt();
    Mouse.timerInterrupt();
}
