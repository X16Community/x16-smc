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

#pragma once

 // Mouse
void mouseTick();
void mouseReset();
void mouseSetRequestedId(uint8_t);
uint8_t getMouseId();
bool mouseIsReady();
uint8_t getMousePacketSize();

// Keyboard
#define KBD_STATE_OFF                   0x00
#define KBD_STATE_READY                 0x01
#define KBD_STATE_BAT                   0x02
#define KBD_STATE_SET_LEDS              0x03
#define KBD_STATE_SET_LEDS_ACK          0x04
#define KBD_STATE_RESET                 0x10
#define KBD_STATE_RESET_ACK             0x11

void keyboardTick();
void keyboardReset();
uint8_t getKeyboardState();
