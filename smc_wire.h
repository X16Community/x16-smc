#pragma once

#include <Arduino.h>
#include <stdlib.h>

class SmcWire {
  public:
    void begin(uint8_t std, uint8_t kbd, uint8_t mse);
    void onReceive(void (*function)(int len));
    void onRequest(void (*function)());
    void onKeyboardRequest(void (*function)());
    void onMouseRequest(void (*function)());
    void write(uint8_t value);
    uint8_t available();
    uint8_t read();
};
