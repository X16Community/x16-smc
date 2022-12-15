#pragma once
#include <Arduino.h>
#include "mouse.h"
#define SCANCODE_TIMEOUT_MS 50

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

    virtual void resetInput(){
      pinMode(datPin, INPUT);
      pinMode(clkPin, INPUT);
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

      byte curBit = digitalRead(datPin);
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
          if (curBit) curCode |= 1 << (rxBitCount - 1);
          parity += curBit;
          rxBitCount++;
          break;

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
            pinMode(datPin, INPUT);
            digitalWrite(datPin, HIGH);
          }
          else {
            digitalWrite(datPin, LOW);
            pinMode(datPin, OUTPUT);
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
            digitalWrite(datPin, LOW);
            pinMode(datPin, OUTPUT);
          }
          else {
            pinMode(datPin, INPUT);
            digitalWrite(datPin, HIGH);
          }

          //Prepare for stop bit
          rxBitCount++;
          break;

        case 9:
          //Stop bit
          pinMode(datPin, INPUT);
          digitalWrite(datPin, HIGH);
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
        digitalWrite(clkPin, LOW);
        pinMode(clkPin, OUTPUT);

        digitalWrite(datPin, LOW);
        pinMode(datPin, OUTPUT);

        ps2ddr = 1;
        timerCountdown--;
      }

      else if (timerCountdown == 1) {
        //We are at end of the request-to-send clock hold time of minimum 100 us
        pinMode(clkPin, INPUT);

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
      byte headNext = (head + 1) & (size - 1);
      if (headNext != tail) {
        buffer[head] = (byte)(curCode);
        head = headNext;
      }
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

template<uint8_t clkPin, uint8_t datPin, uint8_t size>
class PS2KeyboardPort : public PS2Port<clkPin, datPin, size>
{
  protected:
    volatile bool buffer_overrun = false;      // Set to true on buffer full, and to false when buffer is empty again
    volatile uint8_t scancode_state = 0x00;  // Tracks the type and byte position of the scan code currently receiving (bits 4-7 = scan code type, bits 0-3 = number of bytes)
    volatile uint8_t modifier_state = 0x00;  // Always tracks modifier key state, even if buffer is full
    volatile uint8_t modifier_oldstate = 0x00;   // Previous modifier key state, used to compare what's changed during buffer full
    volatile bool reset_request = false;
    volatile bool nmi_request = false;
    uint8_t modifier_codes[8] = {0x11, 0x12, 0x14, 0x59, 0x11, 0x14, 0x1f, 0x27};  // Last byte of modifier key scan codes: LALT, LSHIFT, LCTRL, RSHIFT, RALT, RCTRL, LWIN, RWIN
    
  public:

    /**
       Processes a scan code byte received from the keyboard
    */
    void processByteReceived(uint8_t value) {
      // If buffer_overrun is set, and if we are not in the middle of receiving a multi-byte scancode (indicated by scancode_state == 0),
      // and if the buffer is empty again, we first output modifier key state changes that happened while the buffer was closed, 
      // and then clear the buffer_overrun flag
      if (buffer_overrun && !this->available() && scancode_state == 0x00) {
        if (putModifiers()) {
          buffer_overrun = false;
        }
      }
      
      // Try adding value to buffer. On buffer full: (a) set buffer_overrun, and (b) remove a partial scancode from the head of the buffer
      if (!buffer_overrun) {
        if (!bufferAdd(value)) {
          buffer_overrun = true;  // Buffer full
          bufferRemovePartialCode();
        }
      }

      // Update scancode state; this is always done, also while buffer_overrun is set
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

          // Update modifier key status and check for Ctrl+Alt+PrtScr/Restore => NMI
          else if (value == 0x12) modifier_state |= PS2_MODIFIER_STATE::LSHIFT;
          else if (value == 0x59) modifier_state |= PS2_MODIFIER_STATE::RSHIFT;
          else if (value == 0x14) modifier_state |= PS2_MODIFIER_STATE::LCTRL;
          else if (value == 0x11) modifier_state |= PS2_MODIFIER_STATE::LALT;
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
          
          break;

        case 0x21:    // After 0xe0 (extended code)
          // Update state
          if (value == 0xf0) scancode_state = 0x32; // Extended break code
          else scancode_state = 0x00;

          // Update modifier key status and check for Ctrl+Alt+Del => Reset
          if (value == 0x14) modifier_state |= PS2_MODIFIER_STATE::RCTRL;
          else if (value == 0x1f) modifier_state |= PS2_MODIFIER_STATE::LWIN;
          else if (value == 0x27) modifier_state |= PS2_MODIFIER_STATE::RWIN;
          else if (value == 0x11) modifier_state |= PS2_MODIFIER_STATE::RALT;
          else if (value == 0x71 && isCtrlAltDown()) reset_request = true;
          
          break;

        case 0x32:    // After 0xe0 0xf0 (extended break code)
          // Update state
          scancode_state = 0x00;

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
        return false;
      }
    }

    /**
       Removes partial scan code from head of buffer
    */
    void bufferRemovePartialCode() {
      if ((scancode_state & 0x0f) > this->count()){
        flush();
      }
      else {
        this->head = (this->head + size - (scancode_state & 0x0f)) & (size - 1);    // scancode_state bits 0-3 = number of bytes stored in buffer since last complete scancode
      }
    }

    void flush(){
      PS2Port<clkPin,datPin,size>::flush();
      
      scancode_state = 0;
      modifier_state = 0;
      buffer_overrun = false;
      nmi_request = false;
      reset_request = false;
    }

    void resetInput() {
      PS2Port<clkPin,datPin,size>::resetInput();
      if (!buffer_overrun) {
        bufferRemovePartialCode();
      }
      scancode_state = 0;
    }


    /**
       Modifier key state changes are tracked when the
       buffer is full to avoid "sticky" keys; this function
       writes the these changes to the buffer
    */
    bool putModifiers() {
      for (uint8_t i = 0; i < 8; i++) {
        if ((modifier_state & (1 << i)) != (modifier_oldstate & (1 << i))) {
          
          if (i > 3) {
            if (!bufferAdd(0xe0)){
              return false;
            }
            scancode_state = 0x21;
          }
          
          if (!(modifier_state & (1 << i))) {
            if (!bufferAdd(0xf0)){
              bufferRemovePartialCode();
              scancode_state = 0x00;
              return false;
            }
            if (scancode_state == 0x21) scancode_state = 0x32; else scancode_state = 0x11;
          }
          
          if (!bufferAdd(modifier_codes[i])){
            bufferRemovePartialCode();
            scancode_state = 0x00;
            return false;
          }
          
          scancode_state = 0x00;
          modifier_oldstate = (modifier_oldstate & ~(1 << i)) | (modifier_state & (1 << i));
        }
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
    return ((modifier_state & PS2_MODIFIER_STATE::LCTRL) || (modifier_state & PS2_MODIFIER_STATE::RCTRL)) && ((modifier_state & PS2_MODIFIER_STATE::LALT) || (modifier_state & PS2_MODIFIER_STATE::RALT));
  }
  
};
