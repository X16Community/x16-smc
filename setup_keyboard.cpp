#pragma once

#include "ps2.h"
#include "smc_pins.h"
#include "setup_ps2.h"

/*
    State Machine
*/

// Setup
#define KBD_STATE_OFF                   0x00
#define KBD_STATE_BAT                   0x01
#define KBD_STATE_SET_LEDS              0x02
#define KBD_STATE_SET_LEDS_ACK          0x03
#define KBD_STATE_READY                 0x04

// Reset
#define KBD_STATE_RESET                 0x10
#define KBD_STATE_RESET_ACK             0x11

/*
    Watchdog
*/
#define WATCHDOG_ARM                    255
#define WATCHDOG_DISABLE                0

/*
    PS/2 Keyboard Response Codes
*/
#define PS2_BAT_OK                      0xaa
#define PS2_BAT_FAIL                    0xfc
#define PS2_ACK                         0xfa

/* 
    PS/2 Host to Keyboard Commmands
*/

#define PS2_CMD_SET_LEDS                0xed
#define PS2_CMD_RESET                   0xff

/*
    Variables
*/
extern bool SYSTEM_POWERED;
extern PS2KeyboardPort<PS2_KBD_CLK, PS2_KBD_DAT, 16> Keyboard;
static volatile uint8_t watchdogExpiryState = KBD_STATE_RESET;
static volatile uint8_t kbd_init_state = 0;


void keyboardTick() {
    static uint8_t watchdog = WATCHDOG_DISABLE;

    // Return to OFF state if system powered down
    if (!SYSTEM_POWERED && kbd_init_state != KBD_STATE_OFF) {
      Keyboard.flush();
      kbd_init_state = KBD_STATE_OFF;
      watchdog = WATCHDOG_DISABLE;
      return;
    }

    // State machine
    switch (kbd_init_state) {
        case KBD_STATE_OFF:
            if (SYSTEM_POWERED) {
                kbd_init_state = KBD_STATE_BAT;
                watchdog = WATCHDOG_ARM;
            }
            break;

        case KBD_STATE_BAT:
            if (Keyboard.BAT() == PS2_BAT_OK) {
                kbd_init_state = KBD_STATE_SET_LEDS;
                watchdog = WATCHDOG_ARM;
            } 
            else if (Keyboard.BAT() == PS2_BAT_FAIL) {
                kbd_init_state = KBD_STATE_RESET;
                watchdog = WATCHDOG_ARM;
            }
            break;

        case KBD_STATE_SET_LEDS:
            Keyboard.sendPS2Command(PS2_CMD_SET_LEDS, 0x02);
            kbd_init_state = KBD_STATE_SET_LEDS_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case KBD_STATE_SET_LEDS_ACK:
            if (Keyboard.getCommandStatus() == PS2_CMD_STATUS::CMD_ACK) {
                kbd_init_state = KBD_STATE_READY;
                watchdog = WATCHDOG_DISABLE;
            }
            break;

        case KBD_STATE_READY:
            break;

        case KBD_STATE_RESET:
            Keyboard.flush();
            Keyboard.sendPS2Command(PS2_CMD_RESET);
            kbd_init_state = KBD_STATE_RESET_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case KBD_STATE_RESET_ACK:
            if (Keyboard.getCommandStatus() == PS2_CMD_STATUS::CMD_ACK) {
                kbd_init_state = KBD_STATE_BAT;
                watchdog = WATCHDOG_ARM;
            }
            break;
    }

    if (watchdog > 0) {
        watchdog--;
        if (watchdog == 0) {
            kbd_init_state = watchdogExpiryState;
        }
    }
}

void keyboardReset() {
  kbd_init_state = KBD_STATE_RESET;
}

bool keyboardIsReady() {
  return kbd_init_state == KBD_STATE_READY;
}
