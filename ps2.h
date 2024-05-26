#pragma once
#include <Arduino.h>
#include "setup_ps2.h"
#include "optimized_gpio.h"
#define SCANCODE_TIMEOUT_MS 50

bool PWR_ON_active();

enum PS2_CMD_STATUS : uint8_t {
  IDLE = 0,
  CMD_PENDING = 1,
  CMD_ACK = 0xFA,
  CMD_ERR = 0xFE
};

/// @brief PS/2 IO Port handler
/// @tparam size Circular buffer size for incoming data, must be a power of 2 and not more than 256
template<uint8_t clkPin, uint8_t datPin, uint8_t size = 16> // Single keycodes can be 4 bytes long. We want a little bit of margin here.
class PS2Port
{
    static_assert(size <= 256, "Buffer size may not exceed 256");                // Hard limit on buffer size
    static_assert((size & (size - 1)) == 0, "Buffer size must be a power of 2");  // size must be a power of 2
    static_assert(digitalPinToInterrupt(clkPin) != NOT_AN_INTERRUPT);

  protected:
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t buffer[size];

    uint8_t curCode;
    byte parity;
    byte rxBitCount;
    uint32_t lastBitMillis;

    volatile uint8_t ps2ddr;
    volatile uint8_t outputBuffer[2];
    volatile uint8_t outputSize = 0;
    volatile uint8_t timerCountdown;
    volatile PS2_CMD_STATUS commandStatus = PS2_CMD_STATUS::IDLE;

    void resetReceiver() {
      resetInput();
      outputSize = 0;
      timerCountdown = 0;
      flush();
    };

    virtual void resetInput() {
      if (PWR_ON_active()) {
        gpio_inputWithPullup(datPin);
        gpio_inputWithPullup(clkPin);
      } else {
        // Prevent powering the keyboard via the pull-ups when system is off
        // Call reset() after changing PWR_ON
        pinMode_opt(datPin, INPUT);
        pinMode_opt(clkPin, INPUT);
      }
      curCode = 0;
      parity = 0;
      rxBitCount = 0;
      ps2ddr = 0;
    }

  public:
    PS2Port() :
      head(0), tail(0), curCode(0), parity(0), lastBitMillis(0), rxBitCount(0), ps2ddr(0), timerCountdown(0)
    {
      resetReceiver();
    };

    /// @brief Begin processing PS/2 traffic
    void begin(void(*irqFunc)()) {
      attachInterrupt(digitalPinToInterrupt(clkPin), irqFunc, FALLING);
    }

    /// @brief Process data on falling clock edge
    /// @attention This is interrupt code
    void onFallingClock() {
      if (ps2ddr == 0)
        receiveBit();
      else
        sendBit();
    }

    void receiveBit()
    {
      uint32_t curMillis = millis();
      if (curMillis >= (lastBitMillis + SCANCODE_TIMEOUT_MS))
      {
        // Haven't heard from device in a while, assume this is a new keycode
        resetInput();
      }
      lastBitMillis = curMillis;

      byte curBit = digitalRead_opt(datPin);
      switch (rxBitCount)
      {
        case 0:
          // Start bit
          if (curBit == 0)
          {
            rxBitCount++;
          } // else Protocol error - no start bit
          break;

        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
          // Data bit, LSb first
          curCode >>= 1;
          if (curBit) curCode |= 0x80;
          // fallthrough
        case 9:
          // parity bit
          parity += curBit;
          // Parity bit will be checked after stop bit
          rxBitCount++;
          break;

        case 10:
          // stop bit
          if (curBit != 1) {
            // Protocol Error - no stop bit
          }
          else if ((parity & 0x1) != 1) {
            // Protocol Error - parity mismatch
          }

          bool suppress_scancode = false;
          //Host to device command response handler
          if (commandStatus == PS2_CMD_STATUS::CMD_PENDING) {
            if (curCode == PS2_CMD_STATUS::CMD_ERR) {
              //Command error - Resend
              commandStatus = PS2_CMD_STATUS::CMD_ERR;
            }
            else if (curCode == PS2_CMD_STATUS::CMD_ACK) {
              if (outputSize == 2) {
                //Send second byte
                sendPS2Command(outputBuffer[1]);
                suppress_scancode = true;
              }
              else {
                //Command ACK
                commandStatus = PS2_CMD_STATUS::CMD_ACK;
              }
            }
          }

          if (!suppress_scancode)
          {
            //Update input buffer
            processByteReceived(curCode);
          }
          //Else Ring buffer overrun, drop the incoming code :(
          DBG_PRINT("keycode: ");
          DBG_PRINTLN((byte)(curCode), HEX);
          rxBitCount = 0;
          parity = 0;
          curCode = 0;
          break;
      }
    }

    /**
       Interrupt handler used when sending data
       to the PS/2 device
    */
    void sendBit() {
      if (timerCountdown > 0) {
        //Ignore clock transitions during the request-to-send
        return;
      }


      switch (rxBitCount)
      {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
          //Output data bits 0-7
          if (outputBuffer[0] & 1) {
            gpio_inputWithPullup(datPin);
          }
          else {
            gpio_driveLow(datPin);
          }

          //Update parity
          parity = parity + outputBuffer[0];

          //Right shift output value - always sending the rightmost bit
          outputBuffer[0] = (outputBuffer[0] >> 1);

          //Prepare for next clock cycle
          rxBitCount++;
          break;

        case 8:
          //Send odd parity bit
          if ((parity & 1) == 1) {
            gpio_driveLow(datPin);
          }
          else {
            gpio_inputWithPullup(datPin);
          }

          //Prepare for stop bit
          rxBitCount++;
          break;

        case 9:
          //Stop bit
          gpio_inputWithPullup(datPin);
          rxBitCount++;
          break;

        case 10:
          //ACK
          resetInput();    //Prepare host to receive device ACK or Resend (error) code
          break;
      }
    }

    /// @brief Returns true if at least one byte is available from the PS/2 port
    inline bool available() {
      return head != tail;
    };

    /// @brief Returns the next available byte from the PS/2 port
    virtual uint8_t next() {
      if (available()) {
        uint8_t value = buffer[tail];
        tail = (tail + 1) & (size - 1);
        return value;
      }
      else {
        return 0;
      }
    }

    virtual void flush() {
      head = tail = 0;
    }

    void reset() {
      resetReceiver();
      commandStatus = 0;
    }

    /**
       Sends a command to the PS/2 device
    */
    void sendPS2Command(uint8_t cmd) {
      commandStatus = PS2_CMD_STATUS::CMD_PENDING;        //Command pending
      outputBuffer[0] = cmd;    //Fill output buffer
      outputBuffer[1] = 0;
      outputSize = 1;        //Output buffer size

      timerCountdown = 3;       //Will determine clock hold time for the request-to-send initiated in the timer 1 interrupt handler
    }

    /**
       Sends a command with data to the PS/2 device

       cmd   - First byte, typically a command
       data  - Second byte, typically a command parameter
    */
    void sendPS2Command(uint8_t cmd, uint8_t data) {
      commandStatus = PS2_CMD_STATUS::CMD_PENDING;        //Command pending
      outputBuffer[0] = cmd;    //Fill output buffer
      outputBuffer[1] = data;
      outputSize = 2;        //Output buffer size

      timerCountdown = 3;       //Will determine clock hold time for the request-to-send initiated in the timer 1 interrupt handler
    }

    PS2_CMD_STATUS getCommandStatus() {
      return commandStatus;
    }

    /*
       The timerInterrupt is made to be called by the Arduino
       timer interrupt handler once every 100 us. It
       controls the timing requirements when writing to
       a PS/2 device
    */
    void timerInterrupt() {
      if (timerCountdown == 0x00) {
        //The host is currently sending or receiving, no operation required
        return;
      }

      else if (timerCountdown == 3) {
        //Initiate request-to-send sequence
        gpio_driveLow(clkPin);
        gpio_driveLow(datPin);

        ps2ddr = 1;
        timerCountdown--;
      }

      else if (timerCountdown == 1) {
        //We are at end of the request-to-send clock hold time of minimum 100 us
        gpio_inputWithPullup(clkPin);

        timerCountdown = 0;
        rxBitCount = 0;
        parity = 0;
      }

      else {
        timerCountdown--;
      }
    }

    uint8_t count() {
      return (size + head - tail) & (size - 1);
    }

    virtual void processByteReceived(uint8_t value) {
    }
};

enum PS2_MODIFIER_STATE : uint8_t {
  LALT = 1,
  LSHIFT = 2,
  LCTRL = 4,
  RSHIFT = 8,
  RALT = 16,
  RCTRL = 32,
  LWIN = 64,
  RWIN = 128
};

const uint8_t PS2_REG_SCANCODES[] PROGMEM = {
  120, 0, 116, 114, 112, 113, 123, 0,
  121, 119, 117, 115, 16, 1, 0, 0,
  60, 44, 0, 58, 17, 2, 0, 0,
  0, 46, 32, 31, 18, 3, 0, 0,
  48, 47, 33, 19, 5, 4,  0, 0,
  61, 49, 34, 21, 20, 6, 0, 0,
  51, 50, 36, 35, 22, 7, 0, 0,
  0, 52, 37, 23, 8, 9, 0, 0,
  53, 38, 24, 25, 11, 10, 0, 0,
  54, 55, 39, 40, 26, 12, 0, 0,
  0, 41, 0, 27, 13, 0, 0, 30,
  57, 43, 28, 0, 29, 0, 0, 0,
  45, 0, 0, 0, 0, 15, 0, 0,
  93, 0, 92, 91, 0, 0, 0, 99,
  104, 98, 97, 102, 96, 110, 90, 122,
  106, 103, 105, 100, 101, 125, 0, 0,
  0, 0, 118
};

template<uint8_t clkPin, uint8_t datPin, uint8_t size>
class PS2KeyboardPort : public PS2Port<clkPin, datPin, size>
{
  protected:
    volatile bool buffer_overrun = false;       // Set to true on buffer full, and to false when buffer is empty again
    volatile uint8_t scancode_state = 0x00;     // Tracks the type and byte position of the scan code currently being received (bits 4-7 = scan code type, bits 0-3 = number of bytes)
    volatile uint8_t modifier_state = 0x00;     // Always tracks modifier key state, even if buffer is full
    volatile uint8_t modifier_oldstate = 0x00;  // Previous modifier key state, used to compare what's changed during buffer full
    volatile bool reset_request = false;
    volatile bool nmi_request = false;
    uint8_t modifier_codes[8] = {0x11, 0x12, 0x14, 0x59, 0x11, 0x14, 0x1f, 0x27};  // Last byte of modifier key scan codes: LALT, LSHIFT, LCTRL, RSHIFT, RALT, RCTRL, LWIN, RWIN
    volatile uint8_t bat = 0;

    /// @brief Converts a PS/2 Set 2 scan code to a IBM System/2 key number
    /// @param scancode A one-byte scan code in the range 1 to 132; no extended scan codes
    uint8_t ps2_to_keycode(uint8_t scancode) {
      if (scancode > 0 && scancode < 132) {
        return pgm_read_byte(&(PS2_REG_SCANCODES[scancode - 1]));
      }
      else {
        return 0;
      }
    }

    /// @brief Converts a PS/2 Set 2 extended scan code to a IBM System/2 key number
    /// @param scancode The second byte of an extended scan code (after 0xe0)
    uint8_t ps2ext_to_keycode(uint8_t scancode) {
      switch (scancode) {
        case 0x11:  // Right Alt
          return 62;
        case 0x14:  // Right Ctrl
          return 64;
        case 0x1f:  // Left GUI
          return 59;
        case 0x27:  // Right GUI
          return 63;
        case 0x2f:  // Menu key
          return 65;
        case 0x69:  // End
          return 81;
        case 0x70:  // Insert
          return 75;
        case 0x71:  // Delete
          return 76;
        case 0x6b:  // Left arrow
          return 79;
        case 0x6c:  // Home
          return 80;
        case 0x75:  // Up arrow
          return 83;
        case 0x72:  // Down arrow
          return 84;
        case 0x7d:  // Page up
          return 85;
        case 0x7a:  // Page down
          return 86;
        case 0x74:  // Right arrow
          return 89;
        case 0x4a:  // KP divide
          return 95;
        case 0x5a:  // KP enter
          return 108;
        case 0x7c:  // KP PrtScr
          return 124;
        case 0x15:  // Pause/Break
          return 126;
      }
      return 0;
    }

  public:
    uint8_t BAT() {
      return bat;
    }

    void clearBAT() {
      bat = 0; 
    }

    /**
       Processes a scan code byte received from the keyboard
    */
    void processByteReceived(uint8_t value) {
      // Handle BAT success (0xaa) or fail (0xfc) code
      if (!keyboardIsReady() && (value == 0xaa || value == 0xfc)) {
        bat = value;
        return;
      }

      // If buffer_overrun is set, and if we are not in the middle of receiving a multi-byte scancode (indicated by scancode_state == 0),
      // and if the buffer is empty again, we first output modifier key state changes that happened while the buffer was closed,
      // and then clear the buffer_overrun flag
      if (buffer_overrun && !this->available() && scancode_state == 0x00) {
        if (putModifiers()) {
          buffer_overrun = false;
        }
      }

      // Update scancode state and add key code to input buffer
      updateState(value);

      // buffer_overrun not set means that there are no modifier key state changes to track; set oldstate = state
      if (!buffer_overrun) {
        modifier_oldstate = modifier_state;
      }
    }

    /**
       A state machine tracking type and length of current scan code
       scancode_state bits 0-3 = Number of bytes stored in buffer since last complete scancode
       scancode_state bits 4-7 = Type of scancode
    */
    void updateState(uint8_t value) {
      switch (scancode_state) {
        case 0x00:    // Start of new scan code
          // Update state
          if (value == 0xf0) scancode_state = 0x11;   // Start of break code
          else if (value == 0xab) scancode_state = 0x51;  // Start of two byte response to read ID command
          else if (value == 0xe0) scancode_state = 0x21;  // Start of extended code
          else if (value == 0xe1) scancode_state = 0x41;  // Start of Pause key code
          else if (!buffer_overrun && value < 0xf0) bufferAdd(ps2_to_keycode(value));

          // Update modifier key status
          if (value == 0x12) modifier_state |= PS2_MODIFIER_STATE::LSHIFT;
          else if (value == 0x59) modifier_state |= PS2_MODIFIER_STATE::RSHIFT;
          else if (value == 0x14) modifier_state |= PS2_MODIFIER_STATE::LCTRL;
          else if (value == 0x11) modifier_state |= PS2_MODIFIER_STATE::LALT;

          // Check Ctrl+Alt+PrtScr/Restore => NMI
          else if (value == 0x84 && isCtrlAltDown()) nmi_request = true;

          break;

        case 0x11:    // After 0xf0 (break code)
          // Update state
          scancode_state = 0x00;

          // Update modifier key status
          if (value == 0x12) modifier_state &= ~PS2_MODIFIER_STATE::LSHIFT;
          else if (value == 0x59) modifier_state &= ~PS2_MODIFIER_STATE::RSHIFT;
          else if (value == 0x14) modifier_state &= ~PS2_MODIFIER_STATE::LCTRL;
          else if (value == 0x11) modifier_state &= ~PS2_MODIFIER_STATE::LALT;

          if (!buffer_overrun) bufferAdd(ps2_to_keycode(value) | 0x80);

          break;

        case 0x21:    // After 0xe0 (extended code)
          // Update state
          if (value == 0xf0) scancode_state = 0x32; // Extended break code
          else {
            if (value != 0x12 && value != 0x59) {
              if (!buffer_overrun) bufferAdd(ps2ext_to_keycode(value));
            }
            scancode_state = 0x00;
          }

          // Update modifier key status
          if (value == 0x14) modifier_state |= PS2_MODIFIER_STATE::RCTRL;
          else if (value == 0x1f) modifier_state |= PS2_MODIFIER_STATE::LWIN;
          else if (value == 0x27) modifier_state |= PS2_MODIFIER_STATE::RWIN;
          else if (value == 0x11) modifier_state |= PS2_MODIFIER_STATE::RALT;

          // Check Ctrl+Alt+Del => Reset
          else if (value == 0x71 && isCtrlAltDown()) reset_request = true;

          break;

        case 0x32:    // After 0xe0 0xf0 (extended break code)
          // Update state
          scancode_state = 0x00;
          if (value != 0x12 && value != 0x59) {
            if (!buffer_overrun) bufferAdd(ps2ext_to_keycode(value) | 0x80);
          }

          // Update modifier key status
          if (value == 0x14) modifier_state &= ~PS2_MODIFIER_STATE::RCTRL;
          else if (value == 0x1f) modifier_state &= ~PS2_MODIFIER_STATE::LWIN;
          else if (value == 0x27) modifier_state &= ~PS2_MODIFIER_STATE::RWIN;
          else if (value == 0x11) modifier_state &= ~PS2_MODIFIER_STATE::RALT;

          break;

        case 0x41:    // After 0xe1 (pause make code, 8 bytes)
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
          scancode_state++;
          break;

        case 0x47:
          if (!buffer_overrun) bufferAdd(126);
          scancode_state = 0x00;
          break;

        case 0x51:    // After 0xab (two byte response to read ID command)
          scancode_state = 0x00;
          break;
      }
    }

    /**
       Adds a byte to head of buffer
       Returns true if successful, else false (if the buffer was full)
    */
    bool bufferAdd(uint8_t value) {
      byte headNext = (this->head + 1) & (size - 1);
      if (headNext != this->tail) {
        this->buffer[this->head] = value;
        this->head = headNext;
        return true;
      }
      else {
        buffer_overrun = true;
        return false;
      }
    }

    /// @brief Clears the input buffer and prepares to port to receive new input
    void flush() {
      PS2Port<clkPin, datPin, size>::flush();

      scancode_state = 0;
      modifier_state = 0;
      buffer_overrun = false;
      nmi_request = false;
      reset_request = false;
    }

    void resetInput() {
      PS2Port<clkPin, datPin, size>::resetInput();
      scancode_state = 0;
    }

    /**
       Modifier key state changes are tracked when the
       buffer is full to avoid "sticky" keys; this function
       writes the these changes to the buffer
    */
    bool putModifiers() {
      uint8_t shifted_bit = 0x01;
      for (uint8_t i = 0; i < 8; i++) {
        if ((modifier_state & shifted_bit) != (modifier_oldstate & shifted_bit)) {
          uint8_t mod = modifier_codes[i];
          if (!(modifier_state & shifted_bit)) {
            mod |= 0x80;
          }
          if (!bufferAdd(mod)) return false;
          modifier_oldstate = (modifier_oldstate & ~shifted_bit) | (modifier_state & shifted_bit);
        }
        shifted_bit <<= 1;
      }
      return true;
    }

    bool getResetRequest() {
      return reset_request;
    }

    void ackResetRequest() {
      reset_request = false;
    }

    bool getNMIRequest() {
      return nmi_request;
    }

    void ackNMIRequest() {
      nmi_request = false;
    }

    bool isCtrlAltDown() {
      uint8_t tmp = modifier_state; // Make a non-volatile copy
      if ((tmp & (PS2_MODIFIER_STATE::LCTRL | PS2_MODIFIER_STATE::RCTRL)) == 0) return false;
      if ((tmp & (PS2_MODIFIER_STATE::LALT | PS2_MODIFIER_STATE::RALT)) == 0) return false;
      return true;
    }
};

template<uint8_t clkPin, uint8_t datPin, uint8_t size>
class PS2MousePort : public PS2Port<clkPin, datPin, size>
{
  private:
    volatile uint8_t newPacket[4];
    volatile uint8_t pindex = 0x00;
    volatile uint8_t i0 = 0xff;

    int16_t fromInt9(uint8_t sign, uint8_t value) {
      if (sign) { 
        return (int16_t)(value | 0xff00);
      }
      else {
        return (int16_t)(value);
      }
    }

    int8_t fromInt4(uint8_t value) {
      return (int8_t)((value & 0x0f) | (value & 0xb00001000 ? 0xf0 : 0x00));
    }

    bool updatePacket() {
      // Abort if no complete packet in buffer
      if (this->count() < getMousePacketSize()) return false;

      // Abort if overflow set
      if ((this->buffer[i0] | newPacket[0]) & 0b11000000) return false;

      // Calculate indices to elements of packet last stored in the buffer
      uint8_t i1, i2, i3;
      i1 = (i0 + 1) & (size - 1);
      i2 = (i0 + 2) & (size - 1);
      i3 = (i0 + 3) & (size - 1);

      // Abort if button state changed
      if (((this->buffer[i0] ^ newPacket[0]) & 0x07)) return false;

      // Add X movements
      int16_t deltaX = fromInt9(this->buffer[i0] & 0b00010000, this->buffer[i1]) + fromInt9(newPacket[0] & 0b00010000, newPacket[1]);
      if (deltaX < -256 || deltaX > 255) return false;

      // Add Y movements
      int16_t deltaY = fromInt9(this->buffer[i0] & 0b00100000, this->buffer[i2]) + fromInt9(newPacket[0] & 0b00100000, newPacket[2]);
      if (deltaY < -256 || deltaY > 255) return false;

      // Add scroll wheel movement
      int8_t deltaW;
      if (getMousePacketSize() == 4) {
        deltaW = fromInt4(this->buffer[i3]) + fromInt4(newPacket[3]);
        if (deltaW < -8 || deltaW > 7) return false;
        this->buffer[i3] = (deltaW & 0x0f) | (newPacket[3] & 0xf0);
      }

      // Update packet
      this->buffer[i0] = (newPacket[0] & 0x0f) | (deltaX<0? 0b00010000: 0) | (deltaY<0? 0b00100000:0);
      this->buffer[i1] = deltaX;
      this->buffer[i2] = deltaY;
      
      return true;
    }

    void bufferAdd(uint8_t value) {
      byte headNext = (this->head + 1) & (size - 1);
      if (headNext != this->tail) {
        this->buffer[this->head] = value;
        this->head = headNext;
      }
    }

  protected:
    void processByteReceived(uint8_t value) {
      if (mouseIsReady()) {

        // Abort if bit 3 of the first byte is not set
        if (!pindex && !(value & 0b00001000)) return;

        // Store value
        newPacket[pindex] = value;
        pindex++;
        
        if (pindex == getMousePacketSize()) {
          if (!updatePacket()) {
            i0 = this->head;
            for (uint8_t i = 0; i < getMousePacketSize(); i++) {
              bufferAdd(newPacket[i]);
            }
          }
          pindex = 0;
        }
      }
      else {
        bufferAdd(value);
      }
    }

    void flush() {
      PS2Port<clkPin, datPin, size>::flush();
      pindex = 0x00;
    }
};
