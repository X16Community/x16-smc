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
#if defined(__AVR_ATtiny861__)
#define ATTINY861

/*  - Pinout Updated for Proto 4 / Dev Board
ATTINY861 Pinout
     AVR Func         X16 Func   ArdIO   Port             Port   ArdIO   X16 Func    AVR Function
                                              ----\_/----
                                             | *         |
 (SPI MOSI) (SDA)      I2C_SDA     8     PB0 | 1       20| PA0     0     RESB
 (SPI MISO)               IRQB     9     PB1 | 2   A   19| PA1     1     NMIB
  (SPI SCK) (SCL)      I2C_SCL    10     PB2 | 3   T   18| PA2     2     PS2_KBD_CLK
                   PS2_KBD_DAT    11     PB3 | 4   t   17| PA3     3     POWER_OK
                                         VCC | 5   i   16| AGND
                                         GND | 6   n   15| AVCC
                     RESET_BTN    12     PB4 | 7   y   14| PA4     4     POWER_BTN
                   PS2_MSE_DAT    13     PB5 | 8   8   13| PA5     5     POWER_ON
                   PS2_MSE_CLK    14     PB6 | 9   6   12| PA6     6     ACT_LED        (TXD)
  (SPI SS) (RST)                  15     PB7 |10   1   11| PA7     7     NMI_BTN        (RXD)            
                                             |           |
                                              -----------
 */

#define I2C_SDA_PIN         8
#define I2C_SCL_PIN        10

#define PS2_KBD_CLK         2
#define PS2_KBD_DAT        11
#define PS2_MSE_CLK        14
#define PS2_MSE_DAT        13

#define NMI_BUTTON_PIN      7
#define RESET_BUTTON_PIN   12
#define POWER_BUTTON_PIN    4

#define RESB_PIN            0
#define NMIB_PIN            1
#define IRQB_PIN            9

#define PWR_ON              5
#define PWR_OK              3

#define ACT_LED             6

#endif

#if defined(COMMUNITYX16_PINS)
  #define NMI_BUTTON_PIN     3
  #define IRQB_PIN           7
  #define PWR_OK             6
  #define ACT_LED            9
#endif
