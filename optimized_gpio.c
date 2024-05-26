
// Optimized GPIO library, for attiny861
//
// By: Eirik Stople
//
// Assume that code uses pinouts as defined in ATTinyCore tinyX61_new
//
// ATMEL ATTINY861
//
//                   +-\/-+
//      (D  8) PB0  1|    |20  PA0 (D  0)
//     *(D  9) PB1  2|    |19  PA1 (D  1)
//      (D 10) PB2  3|    |18  PA2 (D  2) INT1
//     *(D 11) PB3  4|    |17  PA3 (D  3)
//             VCC  5|    |16  AGND
//             GND  6|    |15  AVCC
//      (D 12) PB4  7|    |14  PA4 (D  4)
//     *(D 13) PB5  8|    |13  PA5 (D  5)
// INT0 (D 14) PB6  9|    |12  PA6 (D  6)
//      (D 15) PB7 10|    |11  PA7 (D  7)
//                   +----+

#ifndef __AVR_ATtiny861__
  #error Incompatible chip
#endif

#include "Arduino.h"
#include <avr/io.h>
#include "optimized_gpio.h"


static volatile uint8_t* get_ddr_address_from_pin(uint8_t pin)
{
  if (pin >= 8) return &DDRB;
  else return &DDRA;
}

static volatile uint8_t* get_port_address_from_pin(uint8_t pin)
{
  if (pin >= 8) return &PORTB;
  else return &PORTA;
}

static volatile uint8_t* get_pin_address_from_pin(uint8_t pin)
{
  if (pin >= 8) return &PINB;
  else return &PINA;
}

static uint8_t get_bitmask_from_pin(uint8_t pin) {
  return _BV(pin & 0x07);
}


// These inline functions will evaluate if the input parameters are known at compile time using the gcc-specific __builtin_constant_p() function,
// and uses that information to choose the best implementation.

// Write to a pin can be inlined to a single sbi/cbi, if pin and value is known at compile time.
// Write to a pin without knowing pin or value at compile time, requires read-modify-write, which must be done atomically.
// Reading a pin can inline to a single sbic/sbis instruction, if the pin is known compile time.


// If pin and mode is not known compile-time, the inlined pinMode_opt will call this non-inlined function
static void pinMode_opt_params_not_known_compiletime(uint8_t pin, uint8_t mode) __attribute__((noinline));
static void pinMode_opt_params_not_known_compiletime(uint8_t pin, uint8_t mode) {
  volatile uint8_t *ddr_reg = get_ddr_address_from_pin(pin);
  volatile uint8_t *port_reg = get_port_address_from_pin(pin);
  uint8_t bitmask = get_bitmask_from_pin(pin);
  uint8_t tmp = SREG;
  cli();
  if (mode == INPUT) {
    *ddr_reg &= ~bitmask;  // Intermediate step -> Input + (high Z or pullup)
    *port_reg &= ~bitmask; // Input + High Z
  }
  else if (mode == INPUT_PULLUP) {
    *ddr_reg &= ~bitmask; // Intermediate step -> Input + (high Z or pullup)
    *port_reg |= bitmask; // Input + pullup
  }
  else {
    *ddr_reg |= bitmask; // Output, high/low is undefined
  }
  SREG = tmp;
}

inline void pinMode_opt(uint8_t pin, uint8_t mode)
{
  if (__builtin_constant_p(pin) && __builtin_constant_p(mode)) {
    // pin and mode is known at compile time.
    // Safe to ignore atomic operation, as sbi/cbi will be used.
    volatile uint8_t *ddr_reg = get_ddr_address_from_pin(pin);
    volatile uint8_t *port_reg = get_port_address_from_pin(pin);
    uint8_t bitmask = get_bitmask_from_pin(pin);
    if (mode == INPUT) {
      *ddr_reg &= ~bitmask;
      *port_reg &= ~bitmask;
    }
    else if (mode == INPUT_PULLUP) {
      *ddr_reg &= ~bitmask;
      *port_reg |= bitmask;
    }
    else {
      *ddr_reg |= bitmask;
    }
  }
  else
  {
    // pin and mode is not known at compile time.
    // Atomic pin manipulation is required, as algorithm does not use sbi/cbi.
    pinMode_opt_params_not_known_compiletime(pin, mode);
  }
}


// If pin and val is not known compile-time, the inlined digitalWrite_opt will call this non-inlined function
static void digitalWrite_opt_params_not_known_compiletime(uint8_t pin, uint8_t val) __attribute__((noinline));
static void digitalWrite_opt_params_not_known_compiletime(uint8_t pin, uint8_t val)
{
  volatile uint8_t *port_reg = get_port_address_from_pin(pin);
  uint8_t bitmask = get_bitmask_from_pin(pin);
  uint8_t tmp = SREG;
  cli();
  if (val == LOW) {
    *port_reg &= ~bitmask;
  }
  else {
    *port_reg |= bitmask;
  }
  SREG = tmp;
}

inline void digitalWrite_opt(uint8_t pin, uint8_t val)
{
  if (__builtin_constant_p(pin) && __builtin_constant_p(val)) {
    // pin and val is known at compile time.
    // This will be inlined to a single cbi/sbi instruction.
    volatile uint8_t *port_reg = get_port_address_from_pin(pin);
    uint8_t bitmask = get_bitmask_from_pin(pin);
    if (val == LOW) {
      *port_reg &= ~bitmask;
    }
    else {
      *port_reg |= bitmask;
    }
  }
  else
  {
    // pin and val is not known at compile time.
    // Implementation cannot use cbi/sbi. Hence, atomic pin manipulation is required.
    // Call a non-inlined function from this inline function.
    digitalWrite_opt_params_not_known_compiletime(pin, val);
  }
}


static uint8_t digitalRead_opt_params_not_known_compiletime(uint8_t pin) __attribute__((noinline));
static uint8_t digitalRead_opt_params_not_known_compiletime(uint8_t pin)
{
  volatile uint8_t *pin_reg = get_pin_address_from_pin(pin);
  uint8_t bitmask = get_bitmask_from_pin(pin);
  return (*pin_reg & bitmask) ? 1 : 0;
}

inline uint8_t digitalRead_opt(uint8_t pin)
{
  if (__builtin_constant_p(pin)) {
    // pin is known at compile time.
    // The compiler should inline this.
    volatile uint8_t *pin_reg = get_pin_address_from_pin(pin);
    uint8_t bitmask = get_bitmask_from_pin(pin);
    return (*pin_reg & bitmask) ? 1 : 0;
  }
  else
  {
    // pin is not known at compile time.
    // Call a non-inlined function from this inline function.
    return digitalRead_opt_params_not_known_compiletime(pin);
  }
}


inline void gpio_inputWithPullup(uint8_t pin)
{
  // pinMode_opt changes both ddr and port when setting to INPUT_PULLUP
  pinMode_opt(pin, INPUT_PULLUP);
}


// If pin is not known compile-time, the inlined gpio_driveLow will call this non-inlined function
static void gpio_driveLow_params_not_known_compiletime(uint8_t pin) __attribute__((noinline));
static void gpio_driveLow_params_not_known_compiletime(uint8_t pin)
{
  volatile uint8_t *ddr_reg = get_ddr_address_from_pin(pin);
  volatile uint8_t *port_reg = get_port_address_from_pin(pin);
  uint8_t bitmask = get_bitmask_from_pin(pin);
  uint8_t tmp = SREG;
  cli();
  *port_reg &= ~bitmask; // Set port LOW -> Intermediate step, either high Z or driving pin low
  *ddr_reg |= bitmask;   // Set DDR HIGH -> Drive pin low
  SREG = tmp;
}

inline void gpio_driveLow(uint8_t pin)
{
  if (__builtin_constant_p(pin)) {
    // pin and val is known at compile time.
    // This will be inlined to two cbi/sbi instructions.
    digitalWrite_opt(pin, LOW);
    pinMode_opt(pin, OUTPUT);
  }
  else
  {
    // pin is not known at compile time.
    // Implementation cannot use cbi/sbi. Hence, atomic pin manipulation is required.
    // Call a non-inlined function from this inline function.
    gpio_driveLow_params_not_known_compiletime(pin);
  }
}
