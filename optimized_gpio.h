#pragma once
// Optimized GPIO library, for attiny861
//
// By: Eirik Stople
//

#ifdef __cplusplus
extern "C" {
#endif

extern inline void pinMode_opt(uint8_t pin, uint8_t mode) __attribute__((always_inline));
extern inline void digitalWrite_opt(uint8_t pin, uint8_t val) __attribute__((always_inline));
extern inline uint8_t digitalRead_opt(uint8_t pin) __attribute__((always_inline));

#ifdef __cplusplus
}
#endif
