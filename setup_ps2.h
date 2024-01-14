#pragma once

void mouseTick();
void mouseReset();
void mouseSetRequestedId(uint8_t);
uint8_t getMouseId();

void keyboardTick();
void keyboardReset();
bool keyboardIsReady();
