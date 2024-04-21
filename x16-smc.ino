// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks
//     Stefan Jakobsson

// ----------------------------------------------------------------
// Build Options
// ----------------------------------------------------------------
#if !defined(__AVR_ATtiny861__)
  #error "X16 SMC only builds for ATtiny861"
#endif

//#define COMMUNITYX16_PINS
#define ENABLE_NMI_BUT
//#define KBDBUF_FULL_DBG


// ----------------------------------------------------------------
// Includes
// ----------------------------------------------------------------

#include "optimized_gpio.h"
#include "version.h"
#include "smc_button.h"
#include "dbg_supp.h"
#include "smc_pins.h"
#include "ps2.h"
#include "smc_wire.h"
#include "setup_ps2.h"

#include <util/delay.h>

// ----------------------------------------------------------------
// Definitions
// ----------------------------------------------------------------

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

#define I2C_CMD_POW_OFF            0x01
#define I2C_CMD_RESET              0x02
#define I2C_CMD_NMI                0x03
#define I2C_CMD_SET_ACT_LED        0x05
#define I2C_CMD_GET_KEYCODE        0x07
#define I2C_CMD_ECHO               0x08
#define I2C_CMD_DBG_OUT            0x09
#define I2C_CMD_GET_LONGPRESS      0x09
#define I2C_CMD_GET_KBD_STATUS     0x18
#define I2C_CMD_KBD_CMD1           0x19
#define I2C_CMD_KBD_CMD2           0x1a
#define I2C_CMD_SET_MOUSE_ID       0x20
#define I2C_CMD_GET_MOUSE_MOV      0x21
#define I2C_CMD_GET_MOUSE_ID       0x22
#define I2C_CMD_GET_VER1           0x30
#define I2C_CMD_GET_VER2           0x31
#define I2C_CMD_GET_VER3           0x32
#define I2C_CMD_SET_DFLT_READ_OP   0x40
#define I2C_CMD_GET_KEYCODE_FAST   0x41
#define I2C_CMD_GET_MOUSE_MOV_FAST 0x42
#define I2C_CMD_GET_PS2DATA_FAST   0x43
#define I2C_CMD_GET_BOOTLDR_VER    0x8e
#define I2C_CMD_BOOTLDR_START      0x8f

// Bootloader
#define FLASH_SIZE            (0x2000)
#define BOOTLOADER_SIZE       (0x200)
#define BOOTLOADER_START_ADDR ((FLASH_SIZE-BOOTLOADER_SIZE+2)>>1)


// ----------------------------------------------------------------
// Global Variables
// ----------------------------------------------------------------

// Power, Reset and NMI
volatile bool SYSTEM_POWERED = 0;      // default state - Powered off
volatile SmcButton POW_BUT(POWER_BUTTON_PIN);
volatile SmcButton RES_BUT(RESET_BUTTON_PIN);
#if defined(ENABLE_NMI_BUT)
volatile SmcButton NMI_BUT(NMI_BUTTON_PIN);
#endif
volatile bool powerOffRequest = false;
volatile bool hardRebootRequest = false;
volatile bool resetRequest = false;
volatile bool NMIRequest = false;
uint8_t LONGPRESS_START = 0;	// Used to let CPU know NMI has come with pwr on

// I2C
volatile SmcWire smcWire;
volatile uint8_t  I2C_Data[3] = {0, 0, 0};
volatile char echo_byte = 0;

// PS/2
volatile PS2KeyboardPort<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
volatile PS2MousePort<PS2_MSE_CLK, PS2_MSE_DAT, 16> Mouse;
uint8_t defaultRequest = I2C_CMD_GET_KEYCODE_FAST;

// Bootloader
volatile uint16_t bootloaderTimer = 0;
volatile uint8_t bootloaderFlags = 0;

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
  POW_BUT.attachClick(DoPowerToggle);
  // Set flag to show that LongPress has been used
  POW_BUT.attachDuringLongPress(DoLongPressPowerToggle); 

  RES_BUT.attachClick(DoReset);
  RES_BUT.attachDuringLongPress(DoReset);

#if defined(ENABLE_NMI_BUT)
  NMI_BUT.attachClick(DoNMI);
#endif

  // Setup Power Supply
  pinMode_opt(PWR_OK, INPUT);
  pinMode_opt(PWR_ON, OUTPUT);
  digitalWrite_opt(PWR_ON, HIGH);

  // Turn Off Activity LED
  pinMode_opt(ACT_LED, OUTPUT);
  analogWrite(ACT_LED, 0);

  // Hold Reset
  assertReset();

  // Release NMI
  pinMode_opt(NMIB_PIN, OUTPUT);
  digitalWrite_opt(NMIB_PIN, HIGH);

  // Initialize I2C
  smcWire.begin(I2C_ADDR);
  smcWire.onReceive(I2C_Receive);
  smcWire.onRequest(I2C_Send);

  // PS/2 host init
  Keyboard.begin(keyboardClockIrq);
  Mouse.begin(mouseClockIrq);
}

// ----------------------------------------------------------------
// Main Loop
// ----------------------------------------------------------------
void loop() {
  // Shutdown on PSU Fault Condition
  if ((SYSTEM_POWERED == 1) && (!digitalRead_opt(PWR_OK))) {
    PowerOffSeq();
  }
  
  // Update Button State
  POW_BUT.tick();
  RES_BUT.tick();
  #if defined(ENABLE_NMI_BUT)
  NMI_BUT.tick();
  #endif

  // Update Keyboard and Mouse Initialization State
  mouseTick();
  keyboardTick();

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

  // Short Delay
  _delay_ms(10);
}


// ----------------------------------------------------------------
// Power, NMI and Reset
// ----------------------------------------------------------------

void DoPowerToggle() {
  LONGPRESS_START=0;		// Ensure longpress flag is 0
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

void DoLongPressPowerToggle() {
	DoPowerToggle();
	// LONGPRESS_START flag is only set to 1 if the system
	// is powered on by a LongPress
	LONGPRESS_START = 1;
}

void DoReset() {
  LONGPRESS_START=0;
  if (bootloaderTimer > 0) {
    // Bootload init procedure is running, check if Power + Reset pressed
    bootloaderFlags |= 2;
    startBootloader();
  }
  else if (SYSTEM_POWERED == 1) {
    assertReset();
    _delay_ms(RESB_HOLDTIME_MS);
    deassertReset();
    analogWrite(ACT_LED, 0);
    
    Keyboard.flush();
    Mouse.reset();
    mouseReset();
    keyboardReset();

    defaultRequest = I2C_CMD_GET_KEYCODE_FAST;
  }
}

void DoNMI() {
  if (SYSTEM_POWERED == 1 && bootloaderTimer == 0 ) {   // Ignore unless Powered On; also ignore if bootloader timer is active
    digitalWrite_opt(NMIB_PIN, LOW);                // Press NMI
    _delay_ms(NMI_HOLDTIME_MS);
    digitalWrite_opt(NMIB_PIN, HIGH);
  }
}

void PowerOffSeq() {
  assertReset();                              // Hold CPU in reset
  analogWrite(ACT_LED, ACT_LED_DEFAULT_LEVEL);// Ensure activity LED is off
  _delay_ms(AUDIOPOP_HOLDTIME_MS);                // Wait for audio system to stabilize before power is turned off
  digitalWrite_opt(PWR_ON, HIGH);             // Turn off supply
  SYSTEM_POWERED = 0;                         // Global Power state Off
  _delay_ms(RESB_HOLDTIME_MS);                    // Mostly here to add some delay between presses
  deassertReset();
}

void PowerOnSeq() {
  assertReset();
  digitalWrite_opt(PWR_ON, LOW);              // turn on power supply
  unsigned long TimeDelta = 0;
  unsigned long StartTime = millis();         // get current time
  while (!digitalRead_opt(PWR_OK)) {          // Time how long it takes
    TimeDelta = millis() - StartTime;       // for PWR_OK to go active.
  }
  
  if ((PWR_ON_MIN_MS > TimeDelta) || (PWR_ON_MAX_MS < TimeDelta)) {
    PowerOffSeq();                          // FAULT! Turn off supply
    // insert error handler, flash activity light & Halt?   IE, require hard power off before continue?
  }
  else {
    Keyboard.flush();
    Mouse.reset();
    defaultRequest = I2C_CMD_GET_KEYCODE_FAST;
    _delay_ms(RESB_HOLDTIME_MS);                // Allow system to stabilize
    SYSTEM_POWERED = 1;                     // Global Power state On
  }
  deassertReset();
}

void HardReboot() {
  PowerOffSeq();
  _delay_ms(1000);
  PowerOnSeq();
}

void assertReset() {
  pinMode_opt(RESB_PIN, OUTPUT);
  digitalWrite_opt(RESB_PIN, RESET_ACTIVE);
}

void deassertReset() {
  digitalWrite_opt(RESB_PIN, RESET_INACTIVE);
  pinMode_opt(RESB_PIN, INPUT);
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
    case I2C_CMD_POW_OFF:
      switch (I2C_Data[1]) {
        case 0: 
          powerOffRequest = true;
          break;
        case 1: 
          hardRebootRequest = true;
          break;
      }
      break;
  
    case I2C_CMD_RESET:
      switch (I2C_Data[1]) {
        case 0:
          resetRequest = true;
          break;
      } 
      break;
  
    case I2C_CMD_NMI:
      switch (I2C_Data[1]) {
        case 0: 
          NMIRequest = true;
          break;
      }
      break;

    case I2C_CMD_SET_ACT_LED:
      analogWrite(ACT_LED, I2C_Data[1]);
      break;
    
    case I2C_CMD_ECHO:
      echo_byte = I2C_Data[1];
      break;
    
    case I2C_CMD_DBG_OUT :
      DBG_PRINT("DBG register 9 called. echo_byte: ");
      DBG_PRINTLN((byte)(echo_byte), HEX);
      break;

    case I2C_CMD_KBD_CMD1:
      Keyboard.sendPS2Command(I2C_Data[1]);
      break;
  
    case I2C_CMD_KBD_CMD2:
      if (ct >= 3) {
        Keyboard.sendPS2Command(I2C_Data[1], I2C_Data[2]);
      }
      break;

    case I2C_CMD_SET_MOUSE_ID:
      mouseSetRequestedId(I2C_Data[1]);
      break;  

    case I2C_CMD_SET_DFLT_READ_OP:
      defaultRequest = I2C_Data[1];
      break;

    case I2C_CMD_BOOTLDR_START:
      if (I2C_Data[1] == 0x31) {
        bootloaderTimer = 2000;
        bootloaderFlags = 0;
      }
      break;
  }
  
  I2C_Data[0] = defaultRequest;
}

void I2C_Send() { 
  switch (I2C_Data[0]) {
    case I2C_CMD_GET_KEYCODE_FAST:
      if (!sendKeyCode()) smcWire.clearBuffer();
      break;
    
    case I2C_CMD_GET_PS2DATA_FAST:
      {
        bool kbd_avail = sendKeyCode();
        bool mse_avail = sendMousePacket();
        if (!kbd_avail && !mse_avail) smcWire.clearBuffer();
      }
      break;

    case I2C_CMD_GET_MOUSE_MOV_FAST:
      if (!sendMousePacket()) smcWire.clearBuffer();
      break;
      
    case I2C_CMD_GET_KEYCODE:
      sendKeyCode();
      break;
      
    case I2C_CMD_GET_MOUSE_MOV:
      sendMousePacket();
      break;
    
    case I2C_CMD_ECHO:
      smcWire.write(echo_byte);
      break;

    case I2C_CMD_GET_LONGPRESS:
      smcWire.write(LONGPRESS_START);
      break;

    case I2C_CMD_GET_KBD_STATUS:
      smcWire.write(Keyboard.getCommandStatus());
      break;

    case I2C_CMD_GET_MOUSE_ID:
      smcWire.write(getMouseId());
      break;

    case I2C_CMD_GET_VER1:
      smcWire.write(version_major);
      break;

    case I2C_CMD_GET_VER2:
      smcWire.write(version_minor);
      break;

    case I2C_CMD_GET_VER3:
      smcWire.write(version_patch);
      break;

    case I2C_CMD_GET_BOOTLDR_VER:
      if (pgm_read_byte(0x1e00) == 0x8a) {
        smcWire.write(pgm_read_byte(0x1e01));
      }
      else {
        smcWire.write(0xff);
      }
      break;
  }
  
  I2C_Data[0] = defaultRequest;
}

bool sendKeyCode() {
  if (Keyboard.available()) {
    smcWire.write(Keyboard.next());
    return true;
  }
  smcWire.write(0);
  return false;
}

bool sendMousePacket() {
  if (Mouse.count() >= getMousePacketSize()) {
    uint8_t first = Mouse.next();
    if ((first & 0b00001000) == 0b00001000) {
      // Valid start of packet
      if ((first & 0b11000000) == 0) {
        // No overflow
        smcWire.write(first);
        for (uint8_t i = 1; i < getMousePacketSize(); i++) {
          smcWire.write(Mouse.next());
        }
        return true;
      }
      else {
        // Overflow, eat packet
        for (uint8_t i = 1; i < getMousePacketSize(); i++) {
          Mouse.next();
        }
      }
    }
  }
  smcWire.write(0);
  return false;
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
