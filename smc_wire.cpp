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

#include <Arduino.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "smc_wire.h"

/*
   I2C pins
 */
#define I2C_SCL_PINB 2
#define I2C_SDA_PINB 0

/*
  Constants
*/
#define BUFSIZE                       32
#define MASTER_WRITE                  0
#define MASTER_READ                   1
#define SDA_INPUT                     ~(1<<I2C_SDA_PINB)
#define SDA_OUTPUT                    (1<<I2C_SDA_PINB)
#define SCL_INPUT                     ~(1<<I2C_SCL_PINB)
#define SCL_OUTPUT                    (1<<I2C_SCL_PINB)

/*
   USI Control Register Values
*/
#define I2C_LISTEN                    ((1<<USISIE) | (0<<USIOIE) | (1<<USIWM1) | (0<<USIWM0) | (1<<USICS1) | (0<<USICS0) | (0<<USICLK) | (0<<USITC))
#define I2C_ACTIVE                    ((1<<USISIE) | (1<<USIOIE) | (1<<USIWM1) | (1<<USIWM0) | (1<<USICS1) | (0<<USICS0) | (0<<USICLK) | (0<<USITC))

/*
   USI Status Register Values
*/
#define I2C_CLEAR_START_FLAG          (1<<USISIF)
#define I2C_CLEAR_STOP_FLAG           (1<<USIPF)
#define I2C_CLEAR_OVF_FLAG            (1<<USIOIF)
#define I2C_COUNT_BYTE                (I2C_CLEAR_OVF_FLAG | 0x00)
#define I2C_COUNT_BIT                 (I2C_CLEAR_OVF_FLAG | 0x0e)

/*
  I2C Bus States
*/
#define I2C_STATE_STOPPED             0x00

#define I2C_STATE_VERIFY_ADDRESS      0x01
#define I2C_STATE_IGNORE              0x02

#define I2C_STATE_REQUEST_DATA        0x03
#define I2C_STATE_RECEIVE_DATA        0x04
#define I2C_STATE_SLAVE_ABORTED       0x05

#define I2C_STATE_SEND_DATA           0x06
#define I2C_STATE_GET_RESPONSE        0x07
#define I2C_STATE_EVAL_RESPONSE       0x08
#define I2C_STATE_MASTER_ABORTED      0x09

/*
   Static variables
*/
static volatile uint8_t address = 0;
static volatile uint8_t ddr = 0;
static volatile uint8_t state = 0;
static volatile uint8_t buf[BUFSIZE];
static volatile uint8_t bufindex = 0;
static volatile uint8_t buflen = 0;
static volatile void (*receiveHandler)(uint8_t) = NULL;
static volatile void (*requestHandler)() = NULL;


/*
   SmcWire Class Member Function Definitions
*/

// Function:    begin
// 
// Description: Attach as slave to the I2C bus
void SmcWire::begin(uint8_t addr) {
  // Init variables
  address = addr;
  ddr = MASTER_WRITE;
  state = I2C_STATE_STOPPED;
  bufindex = 0;
  buflen = 0;

  // I2C pin setup: 
  // 
  // SCL must be set as output at all times, which is necessary for the USI 
  // module to control the clock pin. SDA is set as input when reading data, and 
  // as output when writing data to the bus. 
  //
  // The output ports of both pins must be set to high at all times to let the USI 
  // module control the bus. If an output port is set to low, the USI module cannot
  // release that pin.
  //
  // Even if the pins are set as outputs, they are never driven high while the
  // two-wire mode is active. Instead the USI module releases a pin if writing a 1
  // to the bus.
  //
  // Based on ATtiny861A datasheet p. 134:
  //
  //   "Two-wire mode. Uses SDA (DI) and SCL (USCK) pins (1).
  //
  //   The Serial Data (SDA) and the Serial Clock (SCL) pins are bi-directional and use
  //   open-collector output drives. The output drivers are enabled by setting the 
  //   corresponding bit for SDA and SCL in the DDRA register.
  //
  //   When the output driver is enabled for the SDA pin, the output driver will force the line 
  //   SDA low if the output of the USI Data Register or the corresponding bit in the PORTA 
  //   register is zero. Otherwise, the SDA line will not be driven (i.e., it is released). 
  //   When the SCL pin output driver is enabled the SCL line will be forced low if the 
  //   corresponding bit in the PORTA register is zero, or by the start detector. 
  //   Otherwise the SCL line will not be driven.
  //
  //   The SCL line is held low when a start detector detects a start condition and the output 
  //   is enabled. Clearing the Start Condition Flag (USISIF) releases the line. The SDA and 
  //   SCL pin inputs is not affected by enabling this mode. Pull-ups on the SDA and SCL port 
  //   pin are disabled in Two-wire mode."
  
  // Set SCL as output and SDA as input
  DDRB = (DDRB & SDA_INPUT) | SCL_OUTPUT;

  // Set SDA and SCL output ports to high
  PORTB = PORTB | (1 << I2C_SDA_PINB) | (1 << I2C_SCL_PINB);

  // Setup USI control and status registers to listen for start/stop conditions
  USICR = I2C_LISTEN;
  USISR = I2C_CLEAR_START_FLAG | I2C_CLEAR_STOP_FLAG | I2C_CLEAR_OVF_FLAG;
}

void SmcWire::onReceive(void (*function)(uint8_t)) {
  receiveHandler = function;
}

void SmcWire::onRequest(void (*function)()) {
  requestHandler = function;
}

void SmcWire::write(uint8_t value) {
  if (ddr == MASTER_READ && buflen < BUFSIZE) {
    buf[buflen] = value;
    buflen++;
  }
}

uint8_t SmcWire::available() {
  return (ddr == MASTER_WRITE) ? buflen - bufindex : 0;
}

uint8_t SmcWire::read() {
  uint8_t value = 0xff;
  if (ddr == MASTER_WRITE && bufindex < buflen) {
    value = buf[bufindex];
    bufindex++;
  }
  return value;
}

void SmcWire::clearBuffer() {
  bufindex = 0;
  buflen = 0;
}


/*
   USI Interrupt Handlers
*/


/*
   Interrupt handler for USI start and stop conditions.

   Even if it's not apparent from the name of the interrupt
   vector, it will catch both start and stop conditions. It
   is necessary to wait for SCL to go low (start condition)
   or SDA to go high (stop condition) to determine the
   cause of the interrupt.
 */
ISR(USI_START_vect) {
  // Ensure SDA is input
  DDRB &= SDA_INPUT;

  // Wait until start or stop condition completes (SCL low or SDA high)
  uint8_t p = (1<<I2C_SCL_PINB);
  while (p == (1<<I2C_SCL_PINB)) {
    p = PINB & ((1<<I2C_SCL_PINB) | (1<<I2C_SDA_PINB));
  }

  // Invoke callback for incoming data
  if (receiveHandler != NULL && ddr == MASTER_WRITE && buflen > 0) {
    receiveHandler(buflen);
  }
  
  // Reset buffer
  buflen = 0;
  bufindex = 0;

  if ((p & (1<<I2C_SCL_PINB)) == 0) {
    // Start condition: Configure to receive address and R/W bit
    state = I2C_STATE_VERIFY_ADDRESS;
    USICR = I2C_ACTIVE;
    USISR = I2C_CLEAR_START_FLAG | I2C_CLEAR_STOP_FLAG | I2C_COUNT_BYTE;
  }
  else {
    // Stop condition: Configure to listen for start/stop conditions
    state = I2C_STATE_STOPPED;
    USICR = I2C_LISTEN;
    USISR = I2C_CLEAR_START_FLAG | I2C_CLEAR_STOP_FLAG | I2C_CLEAR_OVF_FLAG;
  }
}

/**
   Interrupt handler for USI overflow
 */
ISR(USI_OVF_vect) {
  switch (state) {
    
    case I2C_STATE_VERIFY_ADDRESS:
      if (address != 0 && (USIDR >> 1) == address) {
        // Get R/W from USIDR bit 0
        ddr = (USIDR & 1);
        
        if (ddr == MASTER_WRITE) {
          state = I2C_STATE_REQUEST_DATA;
        }
        else {
          if (requestHandler != NULL) requestHandler();
          if (buflen == 0) {
            state = I2C_STATE_IGNORE;
            goto clear_and_listen;
          }
          else {
            state = I2C_STATE_SEND_DATA;
          }
        }

        // Send ACK
        USIDR = 0;
        DDRB |= SDA_OUTPUT;
        USISR = I2C_COUNT_BIT;
      }

      else {
        // Master is addressing another device, ignore (NACK)
        state = I2C_STATE_IGNORE;
        goto clear_and_listen;
      }
      break;

    case I2C_STATE_REQUEST_DATA:
      // Config to read one byte from Master
      state = I2C_STATE_RECEIVE_DATA;
      DDRB &= SDA_INPUT;
      USISR = I2C_COUNT_BYTE;
      break;

    case I2C_STATE_RECEIVE_DATA:
      if (buflen < BUFSIZE) {
        buf[buflen] = USIDR;
        buflen++;

        // Send ACK
        state = I2C_STATE_REQUEST_DATA;
        USIDR = 0;
      }
      else {
        // Send NACK
        state = I2C_STATE_SLAVE_ABORTED;
        USIDR = 0xff;
      }

      DDRB |= SDA_OUTPUT;
      USISR = I2C_COUNT_BIT;
      break;

    case I2C_STATE_SEND_DATA:
      send_data:
        // Next state
        state = I2C_STATE_GET_RESPONSE;
  
        // Fill data register
        if (bufindex < buflen) {
          USIDR = buf[bufindex];
          bufindex++;
        }
        else {
          USIDR = 0xff;
        }

        // Configure to send one byte
        DDRB |= SDA_OUTPUT;
        USISR = I2C_COUNT_BYTE;
        break;

    case I2C_STATE_GET_RESPONSE:
      // Next state
      state = I2C_STATE_EVAL_RESPONSE;

      // Configure to receive ACK/NACK
      DDRB &= SDA_INPUT;
      USISR = I2C_COUNT_BIT;
      break;

    case I2C_STATE_EVAL_RESPONSE:
      if ( (USIDR & 1) == 0) {
        // Master wants more data
        goto send_data;
      }
      else {
        // Master aborted, configure to listen for start/stop conditions
        state = I2C_STATE_MASTER_ABORTED;
      }
      // Fallthrough to default 

    default:
      // Catches:
      //   I2C_STATE_IGNORE
      //   I2C_STATE_SLAVE_ABORTED
      //   I2C_STATE_MASTER_ABORTED
      //   All possible undefined states, should not happen
      
      clear_and_listen:
        DDRB &= SDA_INPUT;
        USICR = I2C_LISTEN;
        USISR = I2C_CLEAR_OVF_FLAG;
        break;
  }
}
