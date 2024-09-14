// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks
//     Stefan Jakobsson
//     Eirik Stople

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

#include <avr/boot.h>
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
#define ACT_LED_OFF            LOW
#define ACT_LED_ON             HIGH

// Reset & NMI
#define RESB_HOLDTIME_MS       500
#define AUDIOPOP_HOLDTIME_MS   1
#define NMI_HOLDTIME_MS        300
#define RESET_ACTIVE           LOW
#define RESET_INACTIVE         HIGH

// I2C
#define I2C_ADDR               0x42 // General slave address

#define I2C_CMD_POW_OFF               0x01
#define I2C_CMD_RESET                 0x02
#define I2C_CMD_NMI                   0x03
#define I2C_CMD_SET_ACT_LED           0x05
#define I2C_CMD_GET_KEYCODE           0x07
#define I2C_CMD_ECHO                  0x08
#define I2C_CMD_DBG_OUT               0x09
#define I2C_CMD_GET_LONGPRESS         0x09
#define I2C_CMD_GET_KBD_STATUS        0x18
#define I2C_CMD_KBD_CMD1              0x19
#define I2C_CMD_KBD_CMD2              0x1a
#define I2C_CMD_SET_MOUSE_ID          0x20
#define I2C_CMD_GET_MOUSE_MOV         0x21
#define I2C_CMD_GET_MOUSE_ID          0x22
#define I2C_CMD_GET_VER1              0x30
#define I2C_CMD_GET_VER2              0x31
#define I2C_CMD_GET_VER3              0x32
#define I2C_CMD_SET_DFLT_READ_OP      0x40
#define I2C_CMD_GET_KEYCODE_FAST      0x41
#define I2C_CMD_GET_MOUSE_MOV_FAST    0x42
#define I2C_CMD_GET_PS2DATA_FAST      0x43
#define I2C_CMD_GET_BOOTLDR_VER       0x8e
#define I2C_CMD_BOOTLDR_START         0x8f
#define I2C_CMD_SET_FLASH_PAGE        0x90
#define I2C_CMD_READ_FLASH            0x91
#define I2C_CMD_WRITE_FLASH           0x92
#define I2C_CMD_SELF_PROGRAMMING_MODE 0x93

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

// Button combination, used to start bootloader or activate self programming mode
volatile uint16_t buttonCombinationTimer = 0;
volatile uint8_t buttonCombinationFlags = 0;
enum BUTTON_COMBINATION_ACTION : uint8_t {
  START_BOOTLOADER = 0,
  SELF_PROGRAMMING_MODE = 1
};
volatile BUTTON_COMBINATION_ACTION buttonCombinationAction = START_BOOTLOADER; // 0: Start bootloader, 1: Activate self programming mode
volatile uint8_t selfProgrammingModeActive = 0; // 0: Not active, 1: active

volatile uint16_t flash_read_offset = 0;
volatile uint8_t spm_lowByte = 0;

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
  digitalWrite_opt(PWR_ON, HIGH);
  pinMode_opt(PWR_ON, OUTPUT);

  // Turn Off Activity LED
  pinMode_opt(ACT_LED, OUTPUT);
  digitalWrite_opt(ACT_LED, ACT_LED_OFF);

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

  // Button combination Countdown
  if (buttonCombinationTimer > 0) {
    buttonCombinationTimer--;
  }

  // Short Delay
  _delay_ms(10);
}

void initializeButtonCombination(BUTTON_COMBINATION_ACTION action)
{
  buttonCombinationAction = action;
  buttonCombinationTimer = 2000;
  buttonCombinationFlags = 0;
}

// ----------------------------------------------------------------
// Power, NMI and Reset
// ----------------------------------------------------------------

void DoPowerToggle() {
  LONGPRESS_START=0;		// Ensure longpress flag is 0
  if (buttonCombinationTimer > 0) {
    buttonCombinationFlags |= 1;
    evaluateButtonCombination();
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
  if (buttonCombinationTimer > 0) {
    // Button combination procedure is running, check if Power + Reset pressed
    buttonCombinationFlags |= 2;
    evaluateButtonCombination();
  }
  else if (SYSTEM_POWERED == 1) {
    assertReset();
    _delay_ms(RESB_HOLDTIME_MS);
    deassertReset();
    digitalWrite_opt(ACT_LED, ACT_LED_OFF);
    
    Keyboard.flush();
    Mouse.reset();
    mouseReset();
    keyboardReset();

    defaultRequest = I2C_CMD_GET_KEYCODE_FAST;
  }
}

void DoNMI() {
  if (SYSTEM_POWERED == 1 && buttonCombinationTimer == 0 ) {   // Ignore unless Powered On; also ignore if button combination timer is active
    digitalWrite_opt(NMIB_PIN, LOW);                // Press NMI
    _delay_ms(NMI_HOLDTIME_MS);
    digitalWrite_opt(NMIB_PIN, HIGH);
  }
}

void PowerOffSeq() {
  assertReset();                              // Hold CPU in reset
  digitalWrite_opt(ACT_LED, ACT_LED_OFF);     // Ensure activity LED is off
  _delay_ms(AUDIOPOP_HOLDTIME_MS);                // Wait for audio system to stabilize before power is turned off
  digitalWrite_opt(PWR_ON, HIGH);             // Turn off supply
  Keyboard.reset();                           // Reset and deactivate pullup
  Mouse.reset();                              // Reset and deactivate pullup
  SYSTEM_POWERED = 0;                         // Global Power state Off
  _delay_ms(RESB_HOLDTIME_MS);                    // Mostly here to add some delay between presses
  deassertReset();
}

void PowerOnSeq() {
  assertReset();
  digitalWrite_opt(PWR_ON, LOW);              // turn on power supply
  Keyboard.reset();                           // Reset and activate pullup
  Mouse.reset();                              // Reset and activate pullup
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
      // 0-127: off, 128-255: on (backward compatible)
      if (I2C_Data[1] & 0x80) digitalWrite_opt(ACT_LED, ACT_LED_ON);
      else digitalWrite_opt(ACT_LED, ACT_LED_OFF);
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
        initializeButtonCombination(START_BOOTLOADER);
      }
      break;

    case I2C_CMD_SET_FLASH_PAGE:
      // Set flash read pointer to page N (byte address 64 * N)
      // There are 128 pages. Bootloader is the last 8 pages.
      flash_read_offset = I2C_Data[1] * 64;
      break;

    case I2C_CMD_WRITE_FLASH:
      // Write byte to flash buffer

      // Flash manipulation must be activated with unlock cmd + [power + reset]
      if (selfProgrammingModeActive != 1) break;

      // Only allow flash write in boot area 0x1E00-0x1FFF
      if (flash_read_offset < 0x1E00 || flash_read_offset > 0x1FFF) break;

      // Flash is programmed one page at a time. One page is 64 bytes.
      // A page is programmed by filling a temporary buffer for this purpose.
      // This temporary buffer have to be filled one word (2 bytes) at a time.
      // Thus, store the low byte in ram on every even address, and write the combined word to temporary buffer every odd address.
      // Automatically erase and program the page once all 64 bytes have been written to the temporary buffer.

      if ((flash_read_offset & 1) == 0) {
        // low byte, we need a full word to fill the temp buffer
        spm_lowByte = I2C_Data[1];
        flash_read_offset++;
      }
      else
      {
        uint16_t spm_word = spm_lowByte | (I2C_Data[1] << 8);
        uint16_t page = flash_read_offset & 0xFFC0;
        boot_page_fill_safe(flash_read_offset++, spm_word);

        if ((flash_read_offset & 0x3F) == 0)
        {
          // Automatically flash page on page boundary (64 bytes)
          boot_page_erase(page);
          boot_spm_busy_wait();       // Wait until the memory is erased.
          boot_page_write(page);      // Store buffer in flash page.
          boot_spm_busy_wait();       // Wait until the memory is written.
        }
      }
      break;

    case I2C_CMD_SELF_PROGRAMMING_MODE:
      selfProgrammingModeActive = 0;
      buttonCombinationAction = I2C_Data[1];
      if (buttonCombinationAction == SELF_PROGRAMMING_MODE) {
        initializeButtonCombination(SELF_PROGRAMMING_MODE);
      }
      else
      {
        buttonCombinationTimer = 0;
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

    case I2C_CMD_READ_FLASH: // Raw read from flash
      smcWire.write(pgm_read_byte(flash_read_offset++));
      break;

    case I2C_CMD_SELF_PROGRAMMING_MODE: // Check if self programming mode is activated
      smcWire.write(selfProgrammingModeActive);
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
// Evaluate button combination (power + reset)
// ----------------------------------------------------------------
void evaluateButtonCombination() {
  if (buttonCombinationFlags == 3 && buttonCombinationTimer > 0) {
    // Button combination detected, perform the desired action
    switch (buttonCombinationAction) {
      case START_BOOTLOADER:
        // Jump to bootloader
        ((void(*)(void))BOOTLOADER_START_ADDR)();
        break;

      case SELF_PROGRAMMING_MODE:
        selfProgrammingModeActive = 1;
        break;

      default:
        break;
    }
  }
  else {
    buttonCombinationTimer = 50;
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

bool PWR_ON_active()
{
  // Returns the status of the PWR_ON output port.
  // PWR_ON is active low.
  // Thus, return true if DDR is output (1) and if PORT is low (0).
  // Similar to the SYSTEM_POWERED variable, but this is more accurate when power is changing.
  // A future code improvement can be to make gpio functions to read from PORT/DDR register

  // The following code assumes PWR_ON is pin 5, which is PA5.
#if PWR_ON != 5
  #error Please adjust PWR_ON_active()
#endif

  if ((DDRA & _BV(5)) == 0) return false; // Port is input. This is the case when mouse and keyboard objects are created
  return (PORTA & _BV(5)) ? false : true;
}
