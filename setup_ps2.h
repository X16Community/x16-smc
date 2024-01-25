#pragma once

void mouseTick();
void mouseInit();
void mouseSetRequestedId(uint8_t);
uint8_t getMouseId();

void keyboardTick();
void keyboardInit();
bool keyboardIsReady();
