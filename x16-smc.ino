// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks

//#define COMMUNITYX16_PINS
#define ENABLE_NMI_BUT
//#define KBDBUF_FULL_DBG

#include "version.h"
#include "smc_button.h"
#include <Wire.h>
#include "dbg_supp.h"
#include "smc_pins.h"
#include "ps2.h"
#include "mouse.h"

// Build only for ATtiny861
#if !defined(__AVR_ATtiny861__)
#error "X16 SMC only builds for ATtiny861"
#endif

#define PWR_ON_MIN_MS          1
#define PWR_ON_MAX_MS          750
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
#define RESET_ACTIVE   LOW
#define RESET_INACTIVE HIGH

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

void assertReset() {
  pinMode(RESB_PIN,OUTPUT);
  digitalWrite(RESB_PIN, RESET_ACTIVE);
}

void deassertReset() {
    digitalWrite(RESB_PIN, RESET_INACTIVE);
    pinMode(RESB_PIN,INPUT);
}

bool SYSTEM_POWERED = 0;                                // default state - Powered off
int  I2C_Data[3] = {0, 0, 0};
bool I2C_Active = false;
char echo_byte = 0;

uint16_t bootloaderTimer = 0;
uint8_t bootloaderFlags = 0;

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

    //Timer 1 interrupt setup is done, enable interrupts
    sei();

    DBG_PRINTLN("Commander X16 SMC Start");
    
    //initialize i2C
    Wire.begin(I2C_ADDR);                               // Initialize I2C - Device Mode
    Wire.onReceive(I2C_Receive);                        // Used to Receive Data
    Wire.onRequest(I2C_Send);                           // Used to Send Data, may never be used

    POW_BUT.attachClick(DoPowerToggle);            // Should the Power off be long, or short?
    POW_BUT.attachDuringLongPress(DoPowerToggle);  // Both for now

    RES_BUT.attachClick(DoReset);
    RES_BUT.attachDuringLongPress(DoReset);

#if defined(ENABLE_NMI_BUT)
    NMI_BUT.attachClick(DoNMI);
#endif

    pinMode(PWR_OK, INPUT);
    pinMode(PWR_ON, OUTPUT);
    digitalWrite(PWR_ON, HIGH);

    pinMode(ACT_LED, OUTPUT);
    analogWrite(ACT_LED, 0);

    assertReset();                 // Hold Reset on startup
    
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
     DoReset();
     Keyboard.ackResetRequest();
    }

    if (Keyboard.getNMIRequest()) {
      DoNMI();
      Keyboard.ackNMIRequest();
    }

    if (bootloaderTimer > 0) {
      bootloaderTimer--;
    }

    // DEBUG: turn activity LED on if there are keys in the keybuffer
    delay(10);                                  // Short Delay, required by OneButton if code is short   
}

void DoPowerToggle() {
    if (bootloaderTimer > 0) {
      bootloaderFlags |= 1;
      startBootloader();
    }
    else if (SYSTEM_POWERED == 0) {                  // If Off, turn on
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
            case 0:
              DoReset();         // 2nd Byte : 0 - Reset button Press
              break;
        }
    }
    if (I2C_Data[0] == 3) {                     // 1st Byte : Byte 3 - NMI Event(s)
        switch (I2C_Data[1]) {
            case 0:DoNMI();        // 2nd Byte : 0 - NMI button Press
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

    if (I2C_Data[0] == 0x8f && I2C_Data[1] == 0x31) {
      bootloaderTimer = 2000;
      bootloaderFlags = 0;
    }
}

void I2C_Send() {
    // DBG_PRINTLN("I2C_Send");
    int nextKey = 0;
    if (I2C_Data[0] == 0x7) {   // 1st Byte : Byte 7 - Keyboard: read next keycode
      if (kbd_init_state == KBD_READY && Keyboard.available()) {
          nextKey  = Keyboard.next();
          if (nextKey == 0xAA) { kbd_init_state = MOUSE_INIT_STATE::START_RESET; } //Reset the keyboard if hotplugged Adrian Black
          else { Wire.write(nextKey); }
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
          if (buf[0] == 0xaa) { mouse_init_state = MOUSE_INIT_STATE::START_RESET; } //reset mouse if hotplugged Adrian Black
          else {
           if ((buf[0] & 0xc8) == 0x08){
              //Valid first byte - Send mouse data packet
              buf[1] = Mouse.next();
              buf[2] = Mouse.next();
              Wire.write(buf,3);
          }
          else{
              //Invalid first byte - Discard, and return a 0
              mouse_init_state = MOUSE_INIT_STATE::START_RESET; // reset the mouse if the response is invalid Adrian Black
              Wire.write(0);
          }
          }
          
      }
      else{
          Wire.write(0);
      }
    }

    if (I2C_Data[0] == 0x30) {
      Wire.write(version_major);
    }
    
    if (I2C_Data[0] == 0x31) {
      Wire.write(version_minor);
    }

    if (I2C_Data[0] == 0x32) {
      Wire.write(version_patch);
    }

    if (I2C_Data[0] == 0x8e) {
      if (pgm_read_byte(0x1e00) == 0x8a) {
        Wire.write(pgm_read_byte(0x1e01));
      }
      else {
        Wire.write(0xff);
      }
    }
}

void DoReset() {
    if (bootloaderTimer > 0) {
      bootloaderFlags |= 2;
      startBootloader();
      return;
    }

    Keyboard.flush();
    Mouse.reset();
    if (SYSTEM_POWERED == 1) {                  // Ignore unless Powered On
        assertReset();
        delay(RESB_HOLDTIME_MS);
        deassertReset();
        analogWrite(ACT_LED, 0);
        mouse_init_state = MOUSE_INIT_STATE::START_RESET;
        kbd_init_state = MOUSE_INIT_STATE::START_RESET;
    }
}

void DoNMI() {
    if (SYSTEM_POWERED == 1 && bootloaderTimer == 0 ) {   // Ignore unless Powered On; also ignore if bootloader timer is active
        digitalWrite(NMIB_PIN,LOW);                     // Press NMI
        delay(NMI_HOLDTIME_MS);
        digitalWrite(NMIB_PIN,HIGH);
    }
}

void PowerOffSeq() {
    digitalWrite(PWR_ON, HIGH);                 // Turn off supply
    SYSTEM_POWERED=0;                           // Global Power state Off
    assertReset();
    delay(RESB_HOLDTIME_MS);                    // Mostly here to add some delay between presses
    deassertReset();
}

void PowerOnSeq() {
    assertReset();
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
        Keyboard.reset();
        delay(RESB_HOLDTIME_MS);                // Allow system to stabilize
        SYSTEM_POWERED=1;                       // Global Power state On
    }
    deassertReset();
}

void HardReboot() {                             // This never works via I2C... Why!!!
    PowerOffSeq();
    delay(1000);
    PowerOnSeq();
}

void startBootloader() {
  if (bootloaderFlags == 3 && bootloaderTimer > 0) {
    ((void(*)(void))0x1e02)();
  }
  else {
    bootloaderTimer = 50;
  }
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
