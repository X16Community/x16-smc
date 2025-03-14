// Copyright 2022-2025 Kevin Williams (TexElec.com), Michael Steil, Joe Burks,
// Stefan Jakobsson, Eirik Stople, and other contributors.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, 
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#include "ps2.h"
#include "smc_pins.h"
#include "setup_ps2.h"

/*
    State Machine
*/

// While System Power OFF
#define MOUSE_STATE_OFF                 0x00

// Power On Test
#define MOUSE_STATE_BAT                 0x10
#define MOUSE_STATE_ID                  0x11

// Intellimouse extension
#define MOUSE_STATE_INTELLI_1           0x20
#define MOUSE_STATE_INTELLI_1_ACK       0x21
#define MOUSE_STATE_INTELLI_2           0x22
#define MOUSE_STATE_INTELLI_2_ACK       0x23
#define MOUSE_STATE_INTELLI_3           0x24
#define MOUSE_STATE_INTELLI_3_ACK       0x25
#define MOUSE_STATE_INTELLI_REQ_ID      0x26
#define MOUSE_STATE_INTELLI_REQ_ID_ACK  0x27
#define MOUSE_STATE_INTELLI_GET_ID      0x28

// Setup
#define MOUSE_STATE_SET_SAMPLERATE      0x30
#define MOUSE_STATE_SET_SAMPLERATE_ACK  0x31
#define MOUSE_STATE_ENABLE              0x32
#define MOUSE_STATE_ENABLE_ACK          0x33
#define MOUSE_STATE_READY               0x34

#define MOUSE_STATE_FAILED              0x40

// Reset
#define MOUSE_STATE_RESET               0x50
#define MOUSE_STATE_RESET_ACK           0x51

/*
    Watchdog
*/
#define WATCHDOG_ARM                    255
#define WATCHDOG_DISABLE                0

/*
    PS/2 Mouse Response Codes
*/
#define PS2_BAT_OK                      0xaa
#define PS2_BAT_FAIL                    0xfc
#define PS2_ACK                         0xfa

/* 
    PS/2 Host to Mouse Commmands
*/
#define PS2_CMD_READ_DEVICE_TYPE        0xf2
#define PS2_CMD_SET_SAMPLE_RATE         0xf3
#define PS2_CMD_SET_RESOLUTION          0xe8
#define PS2_CMD_SET_SCALING             0xe6
#define PS2_CMD_ENABLE                  0xf4
#define PS2_CMD_RESET                   0xff

/*
    Variables
*/
extern bool SYSTEM_POWERED;
extern PS2Port<PS2_MSE_CLK, PS2_MSE_DAT, 16> Mouse;
static volatile uint8_t mouse_id = PS2_BAT_FAIL;
static volatile uint8_t requestedmouse_id = 4;
static volatile uint8_t state = MOUSE_STATE_OFF;
static volatile uint8_t watchdogExpiryState = MOUSE_STATE_OFF;


void mouseTick() {
    static uint8_t watchdog = WATCHDOG_DISABLE;
    uint8_t mouse_id_prev;
    
    // Return to OFF state if system powered down
    if (!SYSTEM_POWERED && state != MOUSE_STATE_OFF) {
        Mouse.flush();
        state = MOUSE_STATE_OFF;
        watchdog = WATCHDOG_DISABLE;
        return;
    }

    // State machine
    switch (state) {
        case MOUSE_STATE_OFF:
            if (SYSTEM_POWERED) {
                state = MOUSE_STATE_BAT;
                watchdogExpiryState = MOUSE_STATE_RESET;
                watchdog = WATCHDOG_ARM;
            }
            break;

        case MOUSE_STATE_BAT:
            if (Mouse.available()) {
                uint8_t t = Mouse.next();
                if (t == PS2_BAT_OK) {
                    state = MOUSE_STATE_ID;
                    watchdog = WATCHDOG_ARM;

                }
                else if (t == PS2_BAT_FAIL) {
                    state = MOUSE_STATE_FAILED;
                    watchdog = WATCHDOG_DISABLE;
                }
            }
            break;

        case MOUSE_STATE_ID:
            if (Mouse.available()) {
                mouse_id = Mouse.next();
                if (mouse_id != 0) {
                    mouse_id = PS2_BAT_FAIL;
                    state = MOUSE_STATE_RESET;
                    watchdog = WATCHDOG_ARM;
                }
                else {
                    if (requestedmouse_id == 3 || requestedmouse_id == 4) {
                        state = MOUSE_STATE_INTELLI_1;
                    }
                    else {
                        state = MOUSE_STATE_SET_SAMPLERATE;
                    }
                    watchdog = WATCHDOG_ARM;
                }
            }
            break;

        case MOUSE_STATE_INTELLI_1:
            Mouse.sendPS2Command(PS2_CMD_SET_SAMPLE_RATE, 200);
            state = MOUSE_STATE_INTELLI_1_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case MOUSE_STATE_INTELLI_2:
            if (mouse_id == 0) {
                // If mouse_id is 0, this is the 1st round of Intellimouse setup, go for ID=3
                Mouse.sendPS2Command(PS2_CMD_SET_SAMPLE_RATE, 100);
            }
            else {
                // If mouse_id is not 0, this is the 2nd round of Intellimouse setup, go for ID=4
                Mouse.sendPS2Command(PS2_CMD_SET_SAMPLE_RATE, 200);
            }
            state = MOUSE_STATE_INTELLI_2_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case MOUSE_STATE_INTELLI_3:
            Mouse.sendPS2Command(PS2_CMD_SET_SAMPLE_RATE, 80);
            state = MOUSE_STATE_INTELLI_3_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case MOUSE_STATE_INTELLI_REQ_ID:
            Mouse.sendPS2Command(PS2_CMD_READ_DEVICE_TYPE);
            state = MOUSE_STATE_INTELLI_REQ_ID_ACK;
            watchdog = WATCHDOG_ARM;
            break;
        
        case MOUSE_STATE_INTELLI_GET_ID:
            if (Mouse.available()) {
                mouse_id_prev = mouse_id;
                mouse_id = Mouse.next();

                if (mouse_id == 0 || mouse_id == requestedmouse_id || mouse_id_prev == 3) {
                    // Intellimouse setup complete:
                    // mouse_id == 0                 => Not an Intellimouse, stop here
                    // mouse_id == requestedmouse_id => We got the requested ID, stop here
                    // mouse_id_prev == 3            => This was the second round, unsuccessul config of ID=4, nothing more to do
                    state = MOUSE_STATE_SET_SAMPLERATE;
                }
                else {
                    // Run Intellimouse setup again to test ID=4
                    state = MOUSE_STATE_INTELLI_1;
                }
                watchdog = WATCHDOG_DISABLE;
            }
            break;

        case MOUSE_STATE_SET_SAMPLERATE:
            Mouse.sendPS2Command(PS2_CMD_SET_SAMPLE_RATE, 60);
            state = MOUSE_STATE_SET_SAMPLERATE_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case MOUSE_STATE_ENABLE:
            Mouse.sendPS2Command(PS2_CMD_ENABLE);
            state = MOUSE_STATE_ENABLE_ACK;
            watchdog = WATCHDOG_ARM;
            break;

        case MOUSE_STATE_READY:
            watchdog = WATCHDOG_DISABLE;
            break;

        case MOUSE_STATE_FAILED:
            watchdog = WATCHDOG_DISABLE;
            break;

        case MOUSE_STATE_RESET:
            Mouse.sendPS2Command(PS2_CMD_RESET);
            state = MOUSE_STATE_RESET_ACK;
            watchdogExpiryState = MOUSE_STATE_RESET;
            watchdog = WATCHDOG_ARM;
            break;
        
        case MOUSE_STATE_RESET_ACK:
            if (Mouse.available() && Mouse.next() == PS2_ACK) {
                state = MOUSE_STATE_BAT;
                Mouse.flush();
                watchdog = WATCHDOG_ARM;
            }
            break;


        default:
            // case MOUSE_STATE_INTELLI_1_ACK:
            // case MOUSE_STATE_INTELLI_2_ACK:
            // case MOUSE_STATE_INTELLI_3_ACK:
            // case MOUSE_STATE_INTELLI_REQ_ID_ACK
            // case MOUSE_STATE_SET_SAMPLERATE_ACK:
            // case MOUSE_STATE_ENABLE_ACK:
            if (Mouse.available()) {
                if (Mouse.next() == PS2_ACK) {
                    state++;
                    watchdog = WATCHDOG_ARM;
                }
            }
            break;
    }

    // Watchdog countdown
    if (watchdog > 0) {
        watchdog--;
        if (watchdog == 0) {
            state = watchdogExpiryState;
        }
    }
}

void mouseReset() {
  state = MOUSE_STATE_RESET;
}

void mouseSetRequestedId(uint8_t id) {
  requestedmouse_id = id;
  mouseReset();
}

uint8_t getMouseId() {
  return mouse_id;
}

bool mouseIsReady() {
  return state == MOUSE_STATE_READY;
}

uint8_t getMousePacketSize() {
  return !mouse_id? 3: 4;
}
