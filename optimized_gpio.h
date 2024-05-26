#pragma once
// Optimized GPIO library, for attiny861
//
// By: Eirik Stople
//

#ifdef __cplusplus
extern "C" {
#endif

// Arduino-style interface
extern inline void pinMode_opt(uint8_t pin, uint8_t mode) __attribute__((always_inline));
extern inline void digitalWrite_opt(uint8_t pin, uint8_t val) __attribute__((always_inline));
extern inline uint8_t digitalRead_opt(uint8_t pin) __attribute__((always_inline));

// Convenience functions to choose between input+pullup and output+low, needed by e.g. PS2
extern inline void gpio_inputWithPullup(uint8_t pin) __attribute__((always_inline));
extern inline void gpio_driveLow(uint8_t pin) __attribute__((always_inline));

#ifdef __cplusplus
}
#endif
