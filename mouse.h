#pragma once
#include <stdint.h>

enum mouse_command : uint8_t
{
  RESET = 0xFF,
  ACK = 0xFA,
  BAT_OK = 0xAA,
  BAT_FAIL = 0xFC,
  MOUSE_ID = 0x00,
  SET_SAMPLE_RATE = 0xF3,
  READ_DEVICE_TYPE = 0xF2,
  SET_RESOLUTION = 0xE8,
  SET_SCALING = 0xE6,
  ENABLE = 0xF4
};

typedef enum MOUSE_INIT_STATE : uint8_t {
  OFF = 0,
  POWERUP_BAT_WAIT,
  POWERUP_ID_WAIT,
  PRE_RESET,
  START_RESET,
  RESET_ACK_WAIT,
  RESET_BAT_WAIT,
  RESET_ID_WAIT,
  
  SAMPLERATECMD_ACK_WAIT,
  SAMPLERATE_ACK_WAIT,

  ENABLE_ACK_WAIT,
  MOUSE_INIT_DONE,
  MOUSE_READY,
  
  FAILED = 255
} MOUSE_INIT_STATE_T;

extern MOUSE_INIT_STATE_T mouse_init_state;

#define MOUSE_WATCHDOG(x) \
      watchdog_armed = true; \
      watchdog_timer = 1023; \
      watchdog_expire_state = (x)

#define MOUSE_WATCHDOG_DISARM() watchdog_armed = false
  
