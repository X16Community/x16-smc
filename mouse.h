#pragma once
#include <stdint.h>

enum mouse_command : uint8_t
{
  RESET = 0xFF,                   // Reset device
  ACK = 0xFA,                     // Acknowledge
  BAT_OK = 0xAA,                  // Basic Acceptance Test passed
  BAT_FAIL = 0xFC,                // Basic Acceptance Test failed
  MOUSE_ID = 0x00,                // Mouse identifier
  INTELLIMOUSE_ID1 = 0x03,        // Intellimouse scroll wheel identifier
  INTELLIMOUSE_ID2 = 0x04,        // Intellimouse scroll wheel and extra button identifier
  SET_SAMPLE_RATE = 0xF3,         // Mouse sample rate command
  READ_DEVICE_TYPE = 0xF2,        // Request device type
  SET_RESOLUTION = 0xE8,
  SET_SCALING = 0xE6,
  SET_STATUS_INDICATORS = 0xED,   // Set status indicators (keyboard LEDs)
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
  
  ENABLE_INTELLIMOUSE,
  INTELLIMOUSE_WAIT_1,
  INTELLIMOUSE_WAIT_2,
  INTELLIMOUSE_WAIT_3,

  READ_DEVICE_ID_WAIT,
  
  SAMPLERATE_ACK_WAIT,

  ENABLE_ACK_WAIT,
  MOUSE_INIT_DONE,
  MOUSE_READY,

  KBD_SET_LEDS1,
  SET_LEDS1_ACK_WAIT,
  KBD_SET_LEDS2,
  SET_LEDS2_ACK_WAIT,
  KBD_READY,
  
  FAIL_RETRY = 254,
  FAILED = 255
} MOUSE_INIT_STATE_T;

extern MOUSE_INIT_STATE_T mouse_init_state;
extern uint8_t mouse_id;
extern uint8_t mouse_id_req;
extern uint8_t kbd_init_state;

#define MOUSE_REARM_WATCHDOG() \
      watchdog_armed = true; \
      watchdog_timer = 255; \
      watchdog_expire_state = (MOUSE_INIT_STATE::START_RESET)

#define MOUSE_WATCHDOG(x) \
      watchdog_armed = true; \
      watchdog_timer = 255; \
      watchdog_expire_state = (x)

#define MOUSE_DISARM_WATCHDOG() watchdog_armed = false
  
void MouseTick();
void KeyboardTick();
