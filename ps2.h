#define SCANCODE_TIMEOUT_MS 50

// Should clock and data pins be part of the template instead of constructor args?
// Doing so might lead to code bloat if the compiler builds two nearly identical functions
// that differ only by the constants used.

/// @brief PS/2 IO Port handler
/// @tparam size Circular buffer size for incoming data, must be a power of 2 and not more than 256
template<uint8_t size=8>
class PS2Port
{
	static_assert(size <= 256, "Buffer size may not exceed 256");				// Hard limit on buffer size
	static_assert((size & (size-1)) == 0, "Buffer size must be a power of 2");	// size must be a power of 2

private:
	uint8_t clkPin;
	uint8_t datPin;
	volatile uint8_t head;
	volatile uint8_t tail;
	volatile uint8_t buffer[size];

	uint8_t curCode;
	byte parity;
	byte rxBitCount;
	uint32_t lastBitMillis;

	void resetReceiver() {
		curCode = 0;
		parity = 0;
		rxBitCount = 0;
	};

public:
	PS2Port(uint8_t clkPin, uint8_t datPin) :
		clkPin(clkPin), datPin(datPin),
		head(0), tail(0), curCode(0), parity(0), lastBitMillis(0), rxBitCount(0) {};

	/// @brief Begin processing PS/2 traffic
	void begin(void(*irqFunc)()) {
		attachInterrupt(digitalPinToInterrupt(clkPin), irqFunc, FALLING);
	}

	/// @brief Process data on falling clock edge
	/// @attention This is interrupt code
	void onFallingClock()
	{
		uint32_t curMillis = millis();
		if (curMillis >= (lastBitMillis + SCANCODE_TIMEOUT_MS))
		{
			// Haven't heard from device in a while, assume this is a new keycode
			resetReceiver();
		}
		lastBitMillis = curMillis;
		
		byte curBit = digitalRead(datPin);
		switch(rxBitCount)
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
				if (curBit) curCode |= 1 << (rxBitCount-1);
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
				if (curBit != 1){
					// Protocol Error - no stop bit
				}
				else if ((parity & 0x1) != 1){
					// Protocol Error - parity mismatch
				}
				{
					byte headNext = (head+1) & (size-1);
					if (headNext != tail)
					{
//						keycode[head] = (byte)(curCode);
						head = headNext;
					} // else Ring buffer overrun, drop the incoming code :(
				DBG_PRINT("keycode: ");
				DBG_PRINTLN((byte)(curCode), HEX);
				rxBitCount = 0;
				parity = 0;
				curCode = 0;
				}
				break;
		}
	}

	/// @brief Returns true if at least one byte is available from the PS/2 port
	bool available() { return head != tail; };
	
	/// @brief Returns the next available byte from the PS/2 port
	uint8_t next() {
		uint8_t value = buffer[head];
		if (available()) {
			head = (head+1) & (size-1);
		}
	};
};
