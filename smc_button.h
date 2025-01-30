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

#include "Arduino.h"
#include "optimized_gpio.h"

#define BUTTON_STATE_RELEASED               0
#define BUTTON_STATE_CLICKED                1
#define BUTTON_STATE_LONG_PRESSED           2

#define TICK_TIME_US                        10000
#define BUTTON_LONG_DELAY_MS                1000
#define BUTTON_DEBOUNCE_DELAY_MS            50

#define TICK_TIME_MS                        (TICK_TIME_US / 1000)
#define BUTTON_LONG_DELAY                   (BUTTON_LONG_DELAY_MS / TICK_TIME_MS)
#define BUTTON_DEBOUNCE_DELAY               (BUTTON_DEBOUNCE_DELAY_MS / TICK_TIME_MS)

class SmcButton{

  private:
    uint8_t pin;
    uint16_t counter=0;
    uint8_t state = BUTTON_STATE_RELEASED;
    void (*onClick)() = NULL;
    void (*onLongPress)() = NULL;

  public:

    SmcButton(uint8_t pinNumber){
      pin = pinNumber;
      pinMode_opt(pin, INPUT_PULLUP);
    }

    void tick(){
      uint8_t pinValue = digitalRead_opt(pin);
      
      if (counter > 0){
        counter--;
      }
      
      if (counter > BUTTON_LONG_DELAY-BUTTON_DEBOUNCE_DELAY){
        return;  
      }
      else if (counter == 0 && pinValue==0 && state == BUTTON_STATE_CLICKED){
        //Long press
        state = BUTTON_STATE_LONG_PRESSED;
        counter = BUTTON_LONG_DELAY;
        if (onLongPress!=NULL){
          onLongPress();
        }
      }
      else{
        //Click
        if (pinValue==0 && state == BUTTON_STATE_RELEASED){
          state = BUTTON_STATE_CLICKED;
          counter = BUTTON_LONG_DELAY;
        }
        //Release
        else if (pinValue == 1 && state == BUTTON_STATE_CLICKED){
          state = BUTTON_STATE_RELEASED;
          counter = BUTTON_LONG_DELAY;
          if (onClick!=NULL){
            onClick();
          }
        }
        else if (pinValue == 1 && state == BUTTON_STATE_LONG_PRESSED){
          state = BUTTON_STATE_RELEASED;
          counter = BUTTON_LONG_DELAY;
        }
      }
    }

    void attachClick(void (*callback)()){
      onClick = callback;
    }

    void attachDuringLongPress(void (*callback)()){
      onLongPress = callback;
    }
};
