#include "smc_wire.h"

/*
 * I2C pins
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
#define I2C_LISTEN                    ((1<<USISIE) + (0<<USIOIE) + (1<<USIWM1) + (0<<USIWM0) + (1<<USICS1) + (0<<USICS0) + (0<<USICLK) + (0<<USITC))
#define I2C_ACTIVE                    ((1<<USISIE) + (1<<USIOIE) + (1<<USIWM1) + (1<<USIWM0) + (1<<USICS1) + (0<<USICS0) + (0<<USICLK) + (0<<USITC))
#define I2C_DISABLED                  0

/*
   USI Status Register Values
*/
#define I2C_CLEAR_START_FLAG          (1<<USISIF)
#define I2C_CLEAR_STOP_FLAG           (1<<USIPF)
#define I2C_CLEAR_OVF_FLAG            (1<<USIOIF)
#define I2C_COUNT_BYTE                (I2C_CLEAR_OVF_FLAG + 0x00)
#define I2C_COUNT_BIT                 (I2C_CLEAR_OVF_FLAG + 0x0e)

/*
   Global Static Variables
*/
static uint8_t address = 0;
static uint8_t ddr = 0;
static uint8_t state = 0;
static uint8_t buf[BUFSIZE];
static uint8_t bufindex = 0;
static uint8_t buflen = 0;
static void (*receiveHandler)(int) = NULL;
static void (*requestHandler)() = NULL;


/*
   SmcWire Class Member Function Definitions
*/


/**
 * Attach device to I2C bus as slave
 * 
 * @param addr I2C address
 */
void SmcWire::begin(uint8_t addr) {
  // Init vars
  address = addr;
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
  USISR = I2C_CLEAR_START_FLAG + I2C_CLEAR_STOP_FLAG + I2C_CLEAR_OVF_FLAG;
}

/**
 * Register callback that receives incoming data
 * 
 * @param function Pointer to function that takes an int param and returns nothing
 */
void SmcWire::onReceive(void (*function)(int)) {
  receiveHandler = function;
}

/*
 * Register callback that fills output buffer with data when requested
 * 
 * @param function Pointer to function that takes no params and returns nothing
 */
void SmcWire::onRequest(void (*function)()) {
  requestHandler = function;
}

/**
 * Adds one byte to the end of the output buffer. Only intended to be invoked from the
 * callback function registered with onRequest()
 * 
 * @param value A byte value
 */
void SmcWire::write(uint8_t value) {
  if (ddr == MASTER_READ && buflen < BUFSIZE) {
    buf[buflen] = value;
    buflen++;
  }
}

/**
 * Adds multiple bytes to the end of the output buffer. Only intended to be invoked
 * from the callback function registered with onRequest()
 * 
 * @param value[] An array of bytes
 * @param size Array length
 */
void SmcWire::write(uint8_t value[], uint8_t size) {
  if (ddr == MASTER_READ) {
    for (uint8_t i = 0; i < size && buflen < BUFSIZE; i++) {
      buf[buflen] = value[i];
      buflen++;
    }
  }
}

/**
 * Returns number of bytes available in the input buffer. Only intended
 * to be invoked from the callback function registered with onReceive()
 * 
 * @return Number of bytes available
 */
uint8_t SmcWire::available() {
  return (ddr == MASTER_WRITE) ? buflen - bufindex : 0;
}


/**
 * Reads on byte from the input buffer. Only intended to be invoked
 * from the callback function registered with onReceive()
 * 
 * @return A byte
 */
uint8_t SmcWire::read() {
  uint8_t value = 0xff;
  if (ddr == MASTER_WRITE && bufindex < BUFSIZE) {
    value = buf[bufindex];
    bufindex++;
  }
  return value;
}

/**
 * Sends attention signal to the Master by pulling SCL low
 * while the bus is idle
 * 
 * @return true if the signal was sent
 */
bool SmcWire::setAttention() {
  return true;
}

/**
 * Releases the attention signal if the Master has acknowledged it by
 * pulling SDA low
 * 
 *@return true if the signal is released, false if signal is not acknowledged 
 *        by the Master or if there is no active signal
 */
bool SmcWire::releaseAttention() {
  return true;
}


/*
   USI Interrupt Handlers
*/


/**
 * Interrupt handler for USI start/stop
 */
ISR(USI_START_vect) {
  // Set SDA as input
  DDRB &= SDA_INPUT;

  //Wait while SCL is high and SDA is low
  uint8_t p = (1<<I2C_SCL_PINB);
  while (p == (1<<I2C_SCL_PINB)) {
    p = PINB & ((1<<I2C_SCL_PINB) | (1<<I2C_SDA_PINB));
  }

  if (receiveHandler != NULL && ddr == MASTER_WRITE && buflen > 0) {
    receiveHandler(buflen);
  }
  
  // Reset buffer
  buflen = 0;
  bufindex = 0;

  // Continue
  if ((p & (1<<I2C_SCL_PINB)) == 0) {
    state = I2C_STATE_VERIFY_ADDRESS;
    USICR = I2C_ACTIVE;
    USISR = I2C_CLEAR_START_FLAG + I2C_CLEAR_STOP_FLAG + I2C_COUNT_BYTE;
  }
  else {
    state = I2C_STATE_STOPPED;
    USICR = I2C_LISTEN;
    USISR = I2C_CLEAR_START_FLAG + I2C_CLEAR_STOP_FLAG + I2C_CLEAR_OVF_FLAG;
  }
}

/**
 * Interrupt handler for USI overflow
 */
ISR(USI_OVF_vect) {
  switch (state) {

    case I2C_STATE_VERIFY_ADDRESS:
      if ( (USIDR >> 1) == address && address != 0) {

        // Data direction from bit 0
        ddr = USIDR & 1;

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
      else {
        // Next state
        state = I2C_STATE_ADDRESS_MISMATCH;
        goto clear_and_listen;
      }
      break;

    case I2C_STATE_REQUEST_DATA:
      // Config to read one byte
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
  
        // Transmit one byte
        if (bufindex < buflen) {
          USIDR = buf[bufindex];
          bufindex++;
        }
        else {
          USIDR = 0x00;
        }
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
        // ACK
        goto send_data;
      }
      else {
        // NACK
        state = I2C_STATE_MASTER_ABORTED;
      }
      // Fallthrough to default 

    default:
      // Catches:
      //   I2C_STATE_ADDRESS_MISMATCH
      //   I2C_STATE_SLAVE_ABORTED
      //   I2C_STATE_MASTER_ABORTED
      //   All possible undefined states, should not happen
      
      clear_and_listen:
        DDRB &= SDA_INPUT;
        USICR = I2C_LISTEN;
        break;
  }
}
