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
static volatile uint8_t std_address = 0;
static volatile uint8_t kbd_address = 0;
static volatile uint8_t mse_address = 0;
static volatile uint8_t tmp = 0;
static volatile uint8_t ddr = 0;
static volatile uint8_t state = 0;
static volatile uint8_t buf[BUFSIZE];
static volatile uint8_t bufindex = 0;
static volatile uint8_t buflen = 0;
static volatile void (*receiveHandler)(int) = NULL;
static volatile void (*requestHandler)() = NULL;
static volatile void (*keyboardRequestHandler)() = NULL;
static volatile void (*mouseRequestHandler)() = NULL;


/*
   SmcWire Class Member Function Definitions
*/

// Function:    begin
// 
// Description: Attach as slave to the I2C bus responding to three
//              device addresses.
//
// Params:
//              std: general slave address for offset/command based access to the SMC
//              kbd: dedicated slave address that only returns a key code if available
//              mse: dedicated slave address that only returns a mouse packet if available
void SmcWire::begin(uint8_t std, uint8_t kbd, uint8_t mse) {
  // Init vars
  std_address = std;
  kbd_address = kbd;
  mse_address = mse;
  ddr = MASTER_WRITE;
  state = I2C_STATE_STOPPED;
  bufindex = 0;
  buflen = 0;

  // Pin setup: SDA input, SCL output
  DDRB = (DDRB & SDA_INPUT) | SCL_OUTPUT;

  // Set SDA and SCL output buffer high
  PORTB = PORTB | (1 << I2C_SDA_PINB) | (1 << I2C_SCL_PINB);

  // Setup USI control register
  USICR = I2C_LISTEN;
  USISR = I2C_CLEAR_START_FLAG | I2C_CLEAR_STOP_FLAG | I2C_CLEAR_OVF_FLAG;
}

void SmcWire::onReceive(void (*function)(int)) {
  receiveHandler = function;
}

void SmcWire::onRequest(void (*function)()) {
  requestHandler = function;
}

void SmcWire::onKeyboardRequest(void (*function)()) {
  keyboardRequestHandler = function;
}

void SmcWire::onMouseRequest(void (*function)()) {
  mouseRequestHandler = function;
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


/*
   USI Interrupt Handlers
*/


/*
   Interrupt handler for USI start/stop. Even if it's not apparent
   from its name, the USI_START interrupt catches both start
   and stop conditions. SCL is held low until the USISIE flag
   is cleared.
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
      // Get R/W from USIDR bit 0
      ddr = (USIDR & 1);

      // Get slave address from USIDR bits 1-7
      tmp = (USIDR>>1);
      
      if (tmp == 0) {
        // Ignore general calls to slave address 0x00
        state = I2C_STATE_IGNORE;
        goto clear_and_listen;
      }    

      else if (tmp == std_address) {
        if (ddr == MASTER_WRITE) {
          state = I2C_STATE_REQUEST_DATA;
        }
        else {
          state = I2C_STATE_SEND_DATA;
          if (requestHandler != NULL) requestHandler();
        }

        // Send ACK
        USIDR = 0;
        DDRB |= SDA_OUTPUT;
        USISR = I2C_COUNT_BIT;
      }

      else if ((tmp == kbd_address || tmp == mse_address) && ddr == MASTER_READ) {
        if (tmp == kbd_address && keyboardRequestHandler != NULL) {
          keyboardRequestHandler();
        } 
        else if (tmp == mse_address && mouseRequestHandler != NULL) {
          mouseRequestHandler();
        }
          
        if (buflen > 0) {
          // Send ACK
          state = I2C_STATE_SEND_DATA;
          USIDR = 0;
          DDRB |= SDA_OUTPUT;
          USISR = I2C_COUNT_BIT;
        }
        else {
          // No data, ignore request (NACK)
          state = I2C_STATE_IGNORE;
          goto clear_and_listen;
        }
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
