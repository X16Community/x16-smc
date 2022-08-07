#pragma once

// Important: You need to turn OFF DEBUG in order for the i2c communication to work reliably!
//#define DEBUG
//#define USE_SERIAL_DEBUG

// Debug output macros
#define SERIAL_BPS 115200
#if defined(DEBUG) && defined(USE_SERIAL_DEBUG)
#   define   DBG_PRINT(...) do { Serial.print(__VA_ARGS__); } while(0)
#   define   DBG_PRINTLN(...) do { Serial.println(__VA_ARGS__); } while(0)
#else
    // Ensure that DBG_PRINT() with no ending semicolon doesn't compile when debugging is not enabled
#   define DBG_PRINT(...) do {} while(0)
#   define DBG_PRINTLN(...) do {} while(0)
#endif
