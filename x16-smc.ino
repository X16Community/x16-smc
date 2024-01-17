// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks

// ----------------------------------------------------------------
// SMC Pin Layout Options
// ----------------------------------------------------------------
//#define COMMUNITYX16_PINS


// ----------------------------------------------------------------
// Includes
// ----------------------------------------------------------------

#include "version.h"
#include "smc_button.h"
#include "dbg_supp.h"
#include "smc_pins.h"
#include "ps2.h"
#include "smc_wire.h"
#include "setup_ps2.h"


// ----------------------------------------------------------------
// Definitions
// ----------------------------------------------------------------

// Build Options
#if !defined(__AVR_ATtiny861__)
#error "X16 SMC only builds for ATtiny861"
#endif
#define ENABLE_NMI_BUT
//#define KBDBUF_FULL_DBG

// Power
#define PWR_ON_MIN_MS          1
#define PWR_ON_MAX_MS          750
// Hold PWR_ON low while computer is on.  High while off.
// PWR_OK -> Should go high 100ms<->500ms after PWR_ON invoked.
//           Any longer or shorter is considered a fault.
// http://www.ieca-inc.com/images/ATX12V_PSDG2.0_Ratified.pdf

// Activity (HDD) LED
#define ACT_LED_DEFAULT_LEVEL  0
#define ACT_LED_ON_LEVEL       255

// Reset & NMI
#define RESB_HOLDTIME_MS       500
#define AUDIOPOP_HOLDTIME_MS   1
#define NMI_HOLDTIME_MS        300
#define RESET_ACTIVE           LOW
#define RESET_INACTIVE         HIGH

// I2C
#define I2C_ADDR               0x42 // General slave address
#define I2C_KBD                0x43 // Dedicated slave address for key codes
#define I2C_MSE                0x44 // Dedicated slave address for mouse packets

#define CMD_POW_OFF            0x01
#define CMD_RESET              0x02
#define CMD_NMI                0x03
#define CMD_SET_ACT_LED        0x05
#define CMD_GET_KEYCODE        0x07
#define CMD_ECHO               0x08
#define CMD_DBG_OUT            0x09
#define CMD_GET_KBD_STATUS     0x18
#define CMD_KBD_CMD1           0x19
#define CMD_KBD_CMD2           0x1a
#define CMD_SET_MOUSE_ID       0x20
#define CMD_GET_MOUSE_MOV      0x21
#define CMD_GET_MOUSE_ID       0x22
#define CMD_GET_VER1           0x30
#define CMD_GET_VER2           0x31
#define CMD_GET_VER3           0x32
#define CMD_GET_BOOTLDR_VER    0x8e
#define CMD_BOOTLDR_START      0x8f

// Bootloader
#define FLASH_SIZE            (0x2000)
#define BOOTLOADER_SIZE       (0x200)
#define BOOTLOADER_START_ADDR ((FLASH_SIZE-BOOTLOADER_SIZE+2)>>1)


// ----------------------------------------------------------------
// Global Variables
// ----------------------------------------------------------------

// Power, Reset and NMI
bool SYSTEM_POWERED = 0;      // default state - Powered off
SmcButton POW_BUT(POWER_BUTTON_PIN);
SmcButton RES_BUT(RESET_BUTTON_PIN);
#if defined(ENABLE_NMI_BUT)
SmcButton NMI_BUT(NMI_BUTTON_PIN);
#endif
bool powerOffRequest = false;
bool hardRebootRequest = false;
bool resetRequest = false;
bool NMIRequest = false;

// I2C
SmcWire smcWire;
int  I2C_Data[3] = {0, 0, 0};
char echo_byte = 0;

// PS/2
PS2KeyboardPort<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
PS2Port<PS2_MSE_CLK, PS2_MSE_DAT, 16> Mouse;

// Bootloader
uint16_t bootloaderTimer = 0;
uint8_t bootloaderFlags = 0;


// ----------------------------------------------------------------
// Setup
// ----------------------------------------------------------------
void setup() {
  // Serial Debugging
#if defined(USE_SERIAL_DEBUG)
  Serial.begin(SERIAL_BPS);
#endif
#if defined(KBDBUF_FULL_DBG)
  Serial.begin(SERIAL_BPS);
#endif

  // Setup Timer 1 interrupt to run every 100 us >>>
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

  // Timer 1 interrupt setup is done, enable interrupts
  sei();

  DBG_PRINTLN("Commander X16 SMC Start");

  // Initialize Power, Reset and NMI buttons
  POW_BUT.attachClick(DoPowerToggle);            // Should the Power off be long, or short?
  POW_BUT.attachDuringLongPress(DoPowerToggle);  // Both for now

  RES_BUT.attachClick(DoReset);
  RES_BUT.attachDuringLongPress(DoReset);

#if defined(ENABLE_NMI_BUT)
  NMI_BUT.attachClick(DoNMI);
#endif

  // Setup Power Supply
  pinMode(PWR_OK, INPUT);
  pinMode(PWR_ON, OUTPUT);
  digitalWrite(PWR_ON, HIGH);

  // Turn Off Activity LED
  pinMode(ACT_LED, OUTPUT);
  analogWrite(ACT_LED, 0);

  // Hold Reset
  assertReset();

  // Release NMI
  pinMode(NMIB_PIN, OUTPUT);
  digitalWrite(NMIB_PIN, HIGH);

  // Initialize I2C
  smcWire.begin(I2C_ADDR, I2C_KBD, I2C_MSE);    // Initialize I2C - Device Mode
  smcWire.onReceive(I2C_Receive);               // Used to Receive Data
  smcWire.onRequest(I2C_Send);                  // Used to Send Data, may never be used
  smcWire.onKeyboardRequest(Keyboard_Send);
  smcWire.onMouseRequest(Mouse_Send);

  // PS/2 host init
  Keyboard.begin(keyboardClockIrq);
  Mouse.begin(mouseClockIrq);
}

// ----------------------------------------------------------------
// Main Loop
// ----------------------------------------------------------------
void loop() {
  // Update Button State
  POW_BUT.tick();
  RES_BUT.tick();
#if defined(ENABLE_NMI_BUT)
  NMI_BUT.tick();
#endif

  // Update Keyboard and Mouse Initialization State
  mouseTick();
  keyboardTick();

  // Shutdown on PSU Fault Condition
  if ((SYSTEM_POWERED == 1) && (!digitalRead(PWR_OK)))
  {
    PowerOffSeq();
    //kill power if PWR_OK dies, and system on
    //error handling?
  }

  // Process Requests Received over I2C
  if (powerOffRequest) {
    powerOffRequest = false;
    PowerOffSeq();
  }

  if (hardRebootRequest) {
    hardRebootRequest = false;
    HardReboot();
  }

  if (resetRequest) {
    resetRequest = false;
    DoReset();
  }

  if (NMIRequest) {
    NMIRequest = false;
    DoNMI();
  }

  // Process

  // Process Keyboard Initiated Reset and NMI Requests
  if (Keyboard.getResetRequest()) {
    DoReset();
    Keyboard.ackResetRequest();
  }

  if (Keyboard.getNMIRequest()) {
    DoNMI();
    Keyboard.ackNMIRequest();
  }

  // Bootloader Countdown
  if (bootloaderTimer > 0) {
    bootloaderTimer--;
  }

  // Short Delay, required by OneButton if code is short
  delay(10);
}


// ----------------------------------------------------------------
// Power, NMI and Reset
// ----------------------------------------------------------------

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
    mouseReset();
    keyboardReset();
  }
}

void DoNMI() {
  if (SYSTEM_POWERED == 1 && bootloaderTimer == 0 ) {   // Ignore unless Powered On; also ignore if bootloader timer is active
    digitalWrite(NMIB_PIN, LOW);                    // Press NMI
    delay(NMI_HOLDTIME_MS);
    digitalWrite(NMIB_PIN, HIGH);
  }
}

void PowerOffSeq() {
  assertReset();                              // Hold CPU in reset
  analogWrite(ACT_LED, ACT_LED_DEFAULT_LEVEL);// Ensure activity LED is off
  delay(AUDIOPOP_HOLDTIME_MS);                // Wait for audio system to stabilize before power is turned off
  digitalWrite(PWR_ON, HIGH);                 // Turn off supply
  SYSTEM_POWERED = 0;                         // Global Power state Off
  delay(RESB_HOLDTIME_MS);                    // Mostly here to add some delay between presses
  deassertReset();
}

void PowerOnSeq() {
  assertReset();
  digitalWrite(PWR_ON, LOW);                  // turn on power supply
  unsigned long TimeDelta = 0;
  unsigned long StartTime = millis();         // get current time
  while (!digitalRead(PWR_OK)) {              // Time how long it takes
    TimeDelta = millis() - StartTime;       // for PWR_OK to go active.
  }
  if ((PWR_ON_MIN_MS > TimeDelta) || (PWR_ON_MAX_MS < TimeDelta)) {
    PowerOffSeq();                          // FAULT! Turn off supply
    // insert error handler, flash activity light & Halt?   IE, require hard power off before continue?
  }
  else {
    Keyboard.reset();
    Mouse.reset();
    delay(RESB_HOLDTIME_MS);                // Allow system to stabilize
    SYSTEM_POWERED = 1;                     // Global Power state On
  }
  deassertReset();
}

void HardReboot() {                             // This never works via I2C... Why!!!
  PowerOffSeq();
  delay(1000);
  PowerOnSeq();
}

void assertReset() {
  pinMode(RESB_PIN, OUTPUT);
  digitalWrite(RESB_PIN, RESET_ACTIVE);
}

void deassertReset() {
  digitalWrite(RESB_PIN, RESET_INACTIVE);
  pinMode(RESB_PIN, INPUT);
}

// ----------------------------------------------------------------
// I2C Functions
// ----------------------------------------------------------------
void I2C_Receive(int) {
  int ct = 0;
  while (smcWire.available()) {
    if (ct < 3) {                           // read first three bytes only
      byte c = smcWire.read();
      I2C_Data[ct] = c;
      ct++;
    }
    else {
      smcWire.read();                        // eat extra data, should not be sent
    }
  }

  // Exit without further action if input less than two bytes
  if (ct < 2) {
    return;
  }

  // Process bytes received
  switch (I2C_Data[0]) {
    case CMD_POW_OFF:
      switch (I2C_Data[1]) {
        case 0: 
          powerOffRequest = true;
          break;
        case 1: 
          hardRebootRequest = true;
          break;
      }
      break;
  
    case CMD_RESET:
      switch (I2C_Data[1]) {
        case 0:
          resetRequest = true;
          break;
      } 
      break;
  
    case CMD_NMI:
      switch (I2C_Data[1]) {
        case 0: 
          NMIRequest = true;
          break;
      }
      break;

    case CMD_SET_ACT_LED:
      analogWrite(ACT_LED, I2C_Data[1]);
      break;
    
    case CMD_ECHO:
      echo_byte = I2C_Data[1];
      break;
    
    case CMD_DBG_OUT :
      DBG_PRINT("DBG register 9 called. echo_byte: ");
      DBG_PRINTLN((byte)(echo_byte), HEX);
      break;

    case CMD_KBD_CMD1:
      Keyboard.sendPS2Command(I2C_Data[1]);
      break;
  
    case CMD_KBD_CMD2:
      if (ct >= 3) {
        Keyboard.sendPS2Command(I2C_Data[1], I2C_Data[2]);
      }
      break;

    case CMD_SET_MOUSE_ID:
      mouseSetRequestedId(I2C_Data[1]);
      break;

    case CMD_BOOTLDR_START:
      if (I2C_Data[1] == 0x31) {
        bootloaderTimer = 2000;
        bootloaderFlags = 0;
      }
      break;
  }
}

void I2C_Send() { 
  switch (I2C_Data[0]) {
    case CMD_GET_KEYCODE:
      if (keyboardIsReady() && Keyboard.available()) {
        smcWire.write(Keyboard.next());
      }
      else {
        smcWire.write(0);
      }
      break;
  
  case CMD_ECHO:
    smcWire.write(echo_byte);
    break;

  case CMD_GET_KBD_STATUS:
    smcWire.write(Keyboard.getCommandStatus());
    break;

  case CMD_GET_MOUSE_MOV:
    if (Mouse.count() >= 4 || (getMouseId() == 0 && Mouse.count() >= 3)) { 
      Mouse_Send();
    }
    else {
      smcWire.write(0);
    }
    break;
  
  case CMD_GET_MOUSE_ID:
    smcWire.write(getMouseId());
    break;

  case CMD_GET_VER1:
    smcWire.write(version_major);
    break;

  case CMD_GET_VER2:
    smcWire.write(version_minor);
    break;

  case CMD_GET_VER3:
    smcWire.write(version_patch);
    break;

  case CMD_GET_BOOTLDR_VER:
    if (pgm_read_byte(0x1e00) == 0x8a) {
      smcWire.write(pgm_read_byte(0x1e01));
    }
    else {
      smcWire.write(0xff);
    }
    break;
  }
}

void Keyboard_Send() {
  if (keyboardIsReady() && Keyboard.available()) {
      smcWire.write(Keyboard.next());
  }
}

void Mouse_Send() {
  uint8_t packet_size;
  if (getMouseId() == 0) {
    packet_size = 3;
  }
  else {
    packet_size = 4;
  }

  if (Mouse.count() >= packet_size) { 
    uint8_t firstval = Mouse.next();
    if (firstval == 0xaa) {
      mouseReset(); //reset mouse if hotplugged Adrian Black
      smcWire.write(0);
    }
    else {
      if ((firstval & 0xc8) == 0x08) {
        //Valid first byte - Send mouse data packet
        smcWire.write(firstval);
        for (uint8_t i = 1; i < packet_size; i++) {
          smcWire.write(Mouse.next());
        }
      }
      else {
        //Invalid first byte - Discard, and return a 0
        mouseReset(); // reset the mouse if the response is invalid Adrian Black
        smcWire.write(0);
      }
    }
  }
}

// ----------------------------------------------------------------
// PS/2 Functions
// ----------------------------------------------------------------

void keyboardClockIrq() {
  Keyboard.onFallingClock();
}

void mouseClockIrq() {
  Mouse.onFallingClock();
}


// ----------------------------------------------------------------
// Bootloader Startup
// ----------------------------------------------------------------
void startBootloader() {
  if (bootloaderFlags == 3 && bootloaderTimer > 0) {
    ((void(*)(void))BOOTLOADER_START_ADDR)();
  }
  else {
    bootloaderTimer = 50;
  }
}


// ----------------------------------------------------------------
// Timer Intterupt
// ----------------------------------------------------------------
ISR(TIMER1_COMPA_vect) {
#if defined(__AVR_ATtiny861__)
  // Reset counter since timer1 doesn't reset itself.
  TC1H = 0;
  TCNT1 = 0;
#endif
  Keyboard.timerInterrupt();
  Mouse.timerInterrupt();
}
