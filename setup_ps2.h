#pragma once

 // Mouse
void mouseTick();
void mouseReset();
void mouseSetRequestedId(uint8_t);
uint8_t getMouseId();
bool mouseIsReady();
uint8_t getMousePacketSize();

// Keyboard
#define KBD_STATE_OFF                   0x00
#define KBD_STATE_READY                 0x01
#define KBD_STATE_BAT                   0x02
#define KBD_STATE_SET_LEDS              0x03
#define KBD_STATE_SET_LEDS_ACK          0x04
#define KBD_STATE_RESET                 0x10
#define KBD_STATE_RESET_ACK             0x11

void keyboardTick();
void keyboardReset();
uint8_t getKeyboardState();
