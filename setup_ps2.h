#pragma once

#define PS2_BAT_OK                      0xaa
#define PS2_BAT_FAIL                    0xfc
#define PS2_ACK                         0xfa

#define KBD_STATE_READY                 0x04

void mouseTick();
void mouseReset();
void mouseSetRequestedId(uint8_t);
uint8_t getMouseId();

void keyboardTick();
void keyboardReset();
uint8_t keyboardGetState();
