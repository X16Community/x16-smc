#include "mouse.h"
#include "ps2.h"
#include "smc_pins.h"

extern bool SYSTEM_POWERED;
extern PS2Port<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
extern PS2Port<PS2_MSE_CLK, PS2_MSE_DAT, 8> Mouse;

MOUSE_INIT_STATE_T mouse_init_state = OFF;

// Mouse initialization state machine
void MouseInitTick()
{
  static bool watchdog_armed = false;
  static uint16_t watchdog_timer = 1023;
  static MOUSE_INIT_STATE watchdog_expire_state = OFF;
  
  if (mouse_init_state != OFF && SYSTEM_POWERED == 0)
  {
    mouse_init_state = OFF;
    watchdog_armed = false;
    watchdog_timer = 1023;
    return;
  }
  if (watchdog_armed) {
    if (watchdog_timer == 0) {
      mouse_init_state = watchdog_expire_state;
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
        mouse_init_state = POWERUP_BAT_WAIT;

        // If we don't see the mouse respond, jump to sending it a reset.
        MOUSE_WATCHDOG(START_RESET);
      }
      break;

    case POWERUP_BAT_WAIT:
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if (b == BAT_OK)
        {
          mouse_init_state = POWERUP_ID_WAIT;
          MOUSE_WATCHDOG(START_RESET); // If we don't see the mouse respond, jump to sending it a reset.
        } else if (b == BAT_FAIL) {
          mouse_init_state = FAILED;
        }
      }
      
      break;
      
    case POWERUP_ID_WAIT:
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if (b == MOUSE_ID)
        {
          Mouse.sendPS2Command(mouse_command::SET_SAMPLE_RATE);
          MOUSE_WATCHDOG(START_RESET);
          mouse_init_state = SAMPLERATECMD_ACK_WAIT;
        }
        // Watchdog will eventually send us to START_RESET if we don't get MOUSE_ID
      }
      break;

    case PRE_RESET:
      mouse_init_state = START_RESET;
      break;
      
    case START_RESET:
      Mouse.flush();
      Mouse.sendPS2Command(mouse_command::RESET);
      MOUSE_WATCHDOG(FAILED);
      mouse_init_state = RESET_ACK_WAIT;
      break;

    case RESET_ACK_WAIT:
      if (mstatus == mouse_command::ACK) {
        Mouse.next();
        MOUSE_WATCHDOG(START_RESET);
        mouse_init_state = RESET_BAT_WAIT;
      } else {
        mouse_init_state = FAILED; // Assume an error of some sort.
      }
      break;
      
    case RESET_BAT_WAIT:           // RECEIVE BAT_OK
      // expect self test OK
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if ( b != mouse_command::BAT_OK ) {
          mouse_init_state = FAILED;
        } else {
          MOUSE_WATCHDOG(START_RESET);
          mouse_init_state = RESET_ID_WAIT;
        }
      }
      break;

    case RESET_ID_WAIT:             // RECEIVE MOUSE_ID, SEND SET_SAMPLE_RATE
      // expect mouse ID byte (0x00)
      if (Mouse.available()) {
        uint8_t b = Mouse.next();
        if ( b != mouse_command::MOUSE_ID ) {
          mouse_init_state = FAILED;
        } else {
          Mouse.sendPS2Command(mouse_command::SET_SAMPLE_RATE);
          MOUSE_WATCHDOG(START_RESET);
          mouse_init_state = SAMPLERATECMD_ACK_WAIT;
        }
      }
      break;

    case SAMPLERATECMD_ACK_WAIT:           // RECEIVE ACK, SEND 20 updates/sec
      Mouse.next();
      if (mstatus != mouse_command::ACK)
        mouse_init_state = PRE_RESET;   // ?? Try resetting again, I guess.
      else {
        Mouse.sendPS2Command(60);
        MOUSE_WATCHDOG(START_RESET);
        mouse_init_state = SAMPLERATE_ACK_WAIT;
      }
      break;

    case SAMPLERATE_ACK_WAIT:           // RECEIVE ACK, SEND ENABLE
      Mouse.next();
      if (mstatus != mouse_command::ACK)
        mouse_init_state = PRE_RESET;
      else {
        Mouse.sendPS2Command(mouse_command::ENABLE);
        MOUSE_WATCHDOG(START_RESET);
        mouse_init_state = ENABLE_ACK_WAIT;
      }
      break;

    case ENABLE_ACK_WAIT:         // Receive ACK
      Mouse.next();
      if (mstatus != mouse_command::ACK) {
        mouse_init_state = PRE_RESET;
      } else {
        mouse_init_state = MOUSE_INIT_DONE;
        MOUSE_WATCHDOG_DISARM();
      }
      break;

    case MOUSE_INIT_DONE:
      // done
      MOUSE_WATCHDOG_DISARM();
      mouse_init_state = MOUSE_READY;
      break;

    case MOUSE_READY:
      break;

    default:
      break;
  }
}
