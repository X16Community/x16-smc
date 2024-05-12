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