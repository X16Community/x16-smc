#pragma once

#include <Arduino.h>
#include <stdlib.h>

class SmcWire {
  public:
    void begin(uint8_t addr);
    void onReceive(void (*function)(uint8_t len));
    void onRequest(void (*function)());
    void write(uint8_t value);
    uint8_t available();
    uint8_t read();
    void clearBuffer();
};
