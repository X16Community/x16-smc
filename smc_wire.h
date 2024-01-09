#pragma once

#include <Arduino.h>
#include <stdlib.h>
#include <avr/io.h>
#include <avr/interrupt.h>

// I2C Bus States
#define I2C_STATE_STOPPED               0x00

#define I2C_STATE_VERIFY_ADDRESS        0x01
#define I2C_STATE_ADDRESS_MISMATCH      0x02

#define I2C_STATE_REQUEST_DATA          0x03
#define I2C_STATE_RECEIVE_DATA          0x04
#define I2C_STATE_SLAVE_ABORTED         0x05

#define I2C_STATE_SEND_DATA             0x06
#define I2C_STATE_GET_RESPONSE          0x07
#define I2C_STATE_EVAL_RESPONSE         0x08
#define I2C_STATE_MASTER_ABORTED        0x09

#define I2C_STATE_ATTENTION             0x0a

class SmcWire {
  public:
    void begin(uint8_t addr);
    void onRequest(void (*function)());
    void onReceive(void (*function)(int len));
    void write(uint8_t value);
    void write(uint8_t value[], uint8_t len);
    uint8_t available();
    uint8_t read();
    bool setAttention();
    bool releaseAttention();
};
