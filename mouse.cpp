#include "mouse.h"
#include "ps2.h"
#include "smc_pins.h"

extern bool SYSTEM_POWERED;
extern PS2KeyboardPort<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
extern PS2Port<PS2_MSE_CLK, PS2_MSE_DAT, 8> Mouse;

MOUSE_INIT_STATE_T mouse_init_state = OFF;

// Mouse initialization state machine
void MouseTick()
{
  static bool watchdog_armed = false;
  static uint16_t watchdog_timer = 1023;
  static MOUSE_INIT_STATE watchdog_expire_state = OFF;
  MOUSE_INIT_STATE_T next_state = mouse_init_state;

  if (mouse_init_state != OFF && SYSTEM_POWERED == 0)
  {
    mouse_init_state = OFF;
    watchdog_armed = false;
    watchdog_timer = 1023;
    Mouse.reset();
    return;
  }
  if (watchdog_armed) {
    if (watchdog_timer == 0) {
      next_state = watchdog_expire_state;
      watchdog_armed = false;
    }
    else {
      watchdog_timer--;
    }
  }
  PS2_CMD_STATUS mstatus = Mouse.getCommandStatus();
  if (mstatus == PS2_CMD_STATUS::CMD_PENDING)
  {
    return;
  }
  
  switch(mouse_init_state)
  {
    case OFF:
      if (SYSTEM_POWERED != 0) {
        next_state = POWERUP_BAT_WAIT;

        // If we don't see the mouse respond, jump to sending it a reset.
        MOUSE_REARM_WATCHDOG();
      }
      break;

    case POWERUP_BAT_WAIT:
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if (b == BAT_OK)
        {
          next_state = POWERUP_ID_WAIT;
          MOUSE_REARM_WATCHDOG();
        } else if (b == BAT_FAIL) {
          next_state = FAILED;
        }
        // Let watchdog send us to START_RESET if we don't get BAT
      }
      
      break;
      
    case POWERUP_ID_WAIT:
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if (b == MOUSE_ID)
        {
          Mouse.sendPS2Command(mouse_command::SET_SAMPLE_RATE);
          MOUSE_REARM_WATCHDOG();
          next_state = SAMPLERATECMD_ACK_WAIT;
        }
        // Watchdog will eventually send us to START_RESET if we don't get MOUSE_ID
      }
      break;

    case PRE_RESET:
      next_state = START_RESET;
      break;
      
    case START_RESET:
      Mouse.flush();
      Mouse.sendPS2Command(mouse_command::RESET);
      MOUSE_WATCHDOG(FAILED);
      next_state = RESET_ACK_WAIT;
      break;

    case RESET_ACK_WAIT:
      if (mstatus == mouse_command::ACK) {
        Mouse.next();
        MOUSE_REARM_WATCHDOG();
        next_state = RESET_BAT_WAIT;
      } else {
        next_state = FAILED; // Assume an error of some sort.
      }
      break;
      
    case RESET_BAT_WAIT:           // RECEIVE BAT_OK
      // expect self test OK
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if ( b != mouse_command::BAT_OK ) {
          next_state = FAILED;
        } else {
          MOUSE_REARM_WATCHDOG();
          next_state = RESET_ID_WAIT;
        }
      }
      break;

    case RESET_ID_WAIT:             // RECEIVE MOUSE_ID, SEND SET_SAMPLE_RATE
      // expect mouse ID byte (0x00)
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if ( b != mouse_command::MOUSE_ID ) {
          next_state = FAILED;
        } else {
          Mouse.sendPS2Command(mouse_command::SET_SAMPLE_RATE);
          MOUSE_REARM_WATCHDOG();
          next_state = SAMPLERATECMD_ACK_WAIT;
        }
      }
      break;

    case SAMPLERATECMD_ACK_WAIT:           // RECEIVE ACK, SEND 20 updates/sec
      Mouse.next();
      if (mstatus != mouse_command::ACK)
        next_state = PRE_RESET;   // ?? Try resetting again, I guess.
      else {
        Mouse.sendPS2Command(60);
        MOUSE_REARM_WATCHDOG();
        next_state = SAMPLERATE_ACK_WAIT;
      }
      break;

    case SAMPLERATE_ACK_WAIT:           // RECEIVE ACK, SEND ENABLE
      Mouse.next();
      if (mstatus != mouse_command::ACK)
        next_state = PRE_RESET;
      else {
        Mouse.sendPS2Command(mouse_command::ENABLE);
        MOUSE_REARM_WATCHDOG();
        next_state = ENABLE_ACK_WAIT;
      }
      break;

    case ENABLE_ACK_WAIT:         // Receive ACK
      Mouse.next();
      if (mstatus != mouse_command::ACK) {
        next_state = PRE_RESET;
      } else {
        next_state = MOUSE_INIT_DONE;
        MOUSE_DISARM_WATCHDOG();
      }
      break;

    case MOUSE_INIT_DONE:
      // done
      MOUSE_DISARM_WATCHDOG();
      next_state = MOUSE_READY;
      break;

    case MOUSE_READY:
      break;

    default:
      // This is where the FAILED state will end up
      break;
  }
  mouse_init_state = next_state;
}

uint8_t kbd_init_state = 0;
uint8_t kbd_status_leds = 0;

void KeyboardTick()
{
  static bool watchdog_armed = false;
  static uint16_t watchdog_timer = 255;
  static uint8_t watchdog_expire_state = OFF;
  uint8_t next_state = kbd_init_state;
  static uint8_t blink_counter = 0;
  blink_counter++;

  if (kbd_init_state != OFF && SYSTEM_POWERED == 0)
  {
    kbd_init_state = OFF;
    watchdog_armed = false;
    watchdog_timer = 255;
    return;
  }
  if (watchdog_armed) {
    if (watchdog_timer == 0) {
      next_state = watchdog_expire_state;
      watchdog_armed = false;
    }
    else {
      watchdog_timer--;
    }
  }

  PS2_CMD_STATUS kstatus = Keyboard.getCommandStatus();
  if (kstatus == PS2_CMD_STATUS::CMD_PENDING)
  {
    return;
  }
  
  switch(kbd_init_state)
  {
    case OFF:
      if (SYSTEM_POWERED != 0) {
        next_state = POWERUP_BAT_WAIT;

        // If we don't see the mouse respond, jump to sending it a reset.
        MOUSE_REARM_WATCHDOG();
      }
      break;

    case POWERUP_BAT_WAIT:
      if (Keyboard.available()) {
        uint8_t b = Keyboard.next();
        if (b == BAT_OK)
        {
          kbd_status_leds = 0x2;
          next_state = KBD_SET_LEDS1;
          MOUSE_REARM_WATCHDOG();
        } else if (b == BAT_FAIL) {
          next_state = FAILED;
        }
        // Let watchdog send us to START_RESET if we don't get BAT
      }
      break;

    case START_RESET:
      Keyboard.flush();
      Keyboard.sendPS2Command(mouse_command::RESET);
      MOUSE_WATCHDOG(FAILED);
      next_state = RESET_ACK_WAIT;
      break;

    case RESET_ACK_WAIT:
      if (kstatus == mouse_command::ACK) {
        Keyboard.next();
        MOUSE_REARM_WATCHDOG();
        next_state = POWERUP_BAT_WAIT;
      } else {
        next_state = FAILED; // Assume an error of some sort.
      }
      break;


    case KBD_SET_LEDS1:
      Keyboard.sendPS2Command(SET_STATUS_INDICATORS);
      next_state = SET_LEDS1_ACK_WAIT;
      MOUSE_REARM_WATCHDOG();
      break;

    case SET_LEDS1_ACK_WAIT:
      Keyboard.next();
      if (kstatus != PS2_CMD_STATUS::CMD_ACK) {
        next_state = FAILED;
      } else {
        next_state = SET_LEDS2_ACK_WAIT;
        Keyboard.sendPS2Command(kbd_status_leds);
        MOUSE_REARM_WATCHDOG();
      }
      break;

    case SET_LEDS2_ACK_WAIT:
      Keyboard.next();
      if (kstatus != PS2_CMD_STATUS::CMD_ACK) {
        next_state = FAILED;
      } else {
        next_state = KBD_READY;
        MOUSE_DISARM_WATCHDOG();
      }
      break;

    case KBD_READY:
      break;

    default:
      // This is where the FAILED state will end up
      break;
  }
  kbd_init_state = next_state;
}
