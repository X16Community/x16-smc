#pragma once
#if defined(__AVR_ATtiny861__)
#define ATTINY861

/*
ATTINY861 Pinout
     AVR Func         X16 Func   ArdIO   Port             Port   ArdIO   X16 Func    AVR Function
                                              ----\_/----
                                             | *         |
 (SPI MOSI) (SDA)      I2C_SDA     8     PB0 | 1       20| PA0     0     RESB
 (SPI MISO)            ACT_LED     9     PB1 | 2   A   19| PA1     1     NMIB
  (SPI SCK) (SCL)      I2C_SCL    10     PB2 | 3   T   18| PA2     2     PS2_KBD_CLK
                   PS2_KBD_DAT    11     PB3 | 4   t   17| PA3     3     NMI_BTN
                                         VCC | 5   i   16| AGND
                                         GND | 6   n   15| AVCC
                     RESET_BTN    12     PB4 | 7   y   14| PA4     4     POWER_BTN
                   PS2_MSE_DAT    13     PB5 | 8   8   13| PA5     5     POWER_ON
                   PS2_MSE_CLK    14     PB6 | 9   6   12| PA6     6     POWER_OK       (TXD)
  (SPI SS) (RST)                  15     PB7 |10   1   11| PA7     7     IRQB           (RXD)
                                             |           |
                                              -----------
 */


#define I2C_SDA_PIN        8
#define I2C_SCL_PIN       10

#define PS2_KBD_CLK       2
#define PS2_KBD_DAT       11
#define PS2_MSE_CLK       14
#define PS2_MSE_DAT       13

#define NMI_BUTTON_PIN     3
#define RESET_BUTTON_PIN   12
#define POWER_BUTTON_PIN   4

#define RESB_PIN          0
#define NMIB_PIN          1
#define IRQB_PIN          7

#define PWR_ON            5
#define PWR_OK            6

#define ACT_LED           9

#endif

#if defined(__AVR_ATmega328P__)

#if defined(DEBUG)
#define USE_SERIAL_DEBUG
#endif

// Button definitions

#define PS2_KBD_CLK       3  
#define PS2_KBD_DAT       A3
#define PS2_MSE_CLK       2
#define PS2_MSE_DAT       A1

#define I2C_SDA_PIN       A4
#define I2C_SCL_PIN       A5

#define NMI_BUTTON_PIN    A0
#define RESET_BUTTON_PIN  A2
#define POWER_BUTTON_PIN  4

#define RESB_PIN          5
#define NMIB_PIN          6
#define IRQB_PIN          7

#define PWR_ON            8
#define PWR_OK            9

#define ACT_LED           10

#endif
