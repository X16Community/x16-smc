// Commander X16 ATX Power Control, Reset / NMI, PS/2
//
// By: Kevin Williams - TexElec.com
//     Michael Steil
//     Joe Burks

#include <OneButton.h>
#include <Wire.h>

#define DEBUG
#define SERIAL_BPS 115200

#if defined(__AVR_ATtiny861__)
#define ATTINY861

/*

ATTINY861 Pinout

     AVR Func         X16 Func   ArdIO   Port             Port   ArdIO   X16 Func    AVR Function
                                              ----\_/----
                                             | *         |
 (SPI MOSI) (SDA)      I2C_SDA     8     PB0 | 1       20| PA0     0     RESB
 (SPI MISO)            ACT_LED     9     PB1 | 2   A   19| PA1     1     NMIB
  (SPI SCK) (SCL)      I2C_SCL    10     PB2 | 3   T   18| PA2     2     RESET_BTN
                   PS2_KBD_DAT    11     PB3 | 4   t   17| PA3     3     NMI_BTN
                                         VCC | 5   i   16| AGND
                                         GND | 6   n   15| AVCC
                   PS2_KBD_CLK    12     PB4 | 7   y   14| PA4     4     POWER_BTN
                   PS2_MSE_DAT    13     PB5 | 8   8   13| PA5     5     POWER_ON
                   PS2_MSE_CLK    14     PB6 | 9   6   12| PA6     6     POWER_OK       (TXD)
  (SPI SS) (RST)                  15     PB7 |10   1   11| PA7     7     IRQB           (RXD)
                                             |           |
                                              -----------
 */


#define I2C_SDA_PIN        8
#define I2C_SCL_PIN       10

#define PS2_KBD_CLK       12
#define PS2_KBD_DAT       11
#define PS2_MSE_CLK       14
#define PS2_MSE_DAT       13

#define NMI_BUTTON_PIN     3
#define RESET_BUTTON_PIN   2
#define POWER_BUTTON_PIN   4

#define RESB_PIN          0
#define NMIB_PIN          1
#define IRQB_PIN          7

#define PWR_ON            5
#define PWR_OK            6

#define ACT_LED           9

#else

// If not ATtiny861, we expect ATMega328p
#if !defined(__AVR_ATmega328P__)
#error "X16 SMC only builds for ATtiny861 and ATmega328P"
#endif

#if defined(DEBUG)
#define USE_SERIAL_DEBUG
#endif

// Button definitions

#define PS2_KBD_CLK       2
#define PS2_KBD_DAT       A1
#define PS2_MSE_CLK       3
#define PS2_MSE_DAT       A3

#define I2C_SDA_PIN       A4
#define I2C_SCL_PIN       A5

#define NMI_BUTTON_PIN    A0
#define RESET_BUTTON_PIN  A2
#define POWER_BUTTON_PIN  4

#define RESB_PIN          5
#define NMIB_PIN          6
#define IRQB_PIN          7

#define PWR_ON            8
#define PWR_OK            9

#define ACT_LED           10

#endif

// Debug output macros
#if defined(DEBUG) && defined(USE_SERIAL_DEBUG)
#   define   DBG_PRINT(...) do { Serial.print(__VA_ARGS__); } while(0)
#   define   DBG_PRINTLN(...) do { Serial.println(__VA_ARGS__); } while(0)
#else
    // Ensure that DBG_PRINT() with no ending semicolon doesn't compile when debugging is not enabled
#   define DBG_PRINT(...) do {} while(0)
#   define DBG_PRINTLN(...) do {} while(0)
#endif

#define PWR_ON_MIN             100
#define PWR_ON_MAX             500
// Hold PWR_ON low while computer is on.  High while off.
// PWR_OK -> Should go high 100ms<->500ms after PWR_ON invoked.
//           Any longer or shorter is considered a fault.
// http://www.ieca-inc.com/images/ATX12V_PSDG2.0_Ratified.pdf

//Activity (HDD) LED
#define ACT_LED_DEFAULT_LEVEL    0
#define ACT_LED_ON_LEVEL       255

//Reset & NMI Lines
#define RESB_HOLDTIME          500
#define NMI_HOLDTIME           300

//I2C Pins
#define I2C_ADDR              0x42  // I2C Device ID

#include "ps2.h"

OneButton POW_BUT(POWER_BUTTON_PIN, true, true);
OneButton RES_BUT(RESET_BUTTON_PIN, true, true);
OneButton NMI_BUT(NMI_BUTTON_PIN, true, true);

PS2Port<> Keyboard(PS2_KBD_CLK, PS2_KBD_DAT);
PS2Port<> Mouse(PS2_MSE_CLK, PS2_MSE_DAT);

void keyboardClockIrq() {
	Keyboard.onFallingClock();
}

void mouseClockIrq() {
	Mouse.onFallingClock();
}

bool SYSTEM_POWERED = 0;							 	// default state - Powered off
int	 I2C_Data[2] = {0, 0};
bool I2C_Active = false;

void setup() {
#if defined(USE_SERIAL_DEBUG)
  Serial.begin(SERIAL_BPS);
#endif
  DBG_PRINTLN("Commander X16 SMC Start");
	//initialize i2C
	Wire.begin(I2C_ADDR);								// Initialize I2C - Device Mode
	Wire.onReceive(I2C_Receive);						// Used to Receive Data
	Wire.onRequest(I2C_Send);							// Used to Send Data, may never be used

	POW_BUT.attachClick(Power_Button_Press);			// Should the Power off be long, or short?
	POW_BUT.attachDuringLongPress(Power_Button_Press);	// Both for now

	RES_BUT.attachClick(Reset_Button_Press);			// Short Click = NMI, Long Press = Reset
	RES_BUT.attachDuringLongPress(Reset_Button_Hold);	// Actual Reset Call

	NMI_BUT.attachClick(Reset_Button_Press);			// NMI Call is the same as short Reset
	//NMI_BUT.attachClick(HardReboot);					// strangely, this works fine via NMI push, but fails via I2C?


	pinMode(PWR_OK, INPUT);
	pinMode(PWR_ON, OUTPUT);
	digitalWrite(PWR_ON, HIGH);

	pinMode(ACT_LED, OUTPUT);
	analogWrite(ACT_LED, 0);

	pinMode(RESB_PIN,OUTPUT);
	digitalWrite(RESB_PIN,LOW);					// Hold Reset on statup

	pinMode(NMIB_PIN,OUTPUT);
	digitalWrite(NMIB_PIN,HIGH);

	// PS/2 host init
	Keyboard.begin(keyboardClockIrq);
	Mouse.begin(mouseClockIrq);
}

void loop() {
	POW_BUT.tick();								// Check Button Status
	RES_BUT.tick();
	NMI_BUT.tick();

	if ((SYSTEM_POWERED == 1) && (!digitalRead(PWR_OK)))
	{
		PowerOffSeq();
		//kill power if PWR_OK dies, and system on
		//error handling?
	}
	delay(10);									// Short Delay, required by OneButton if code is short
}

void Power_Button_Press() {
	if (SYSTEM_POWERED == 0) {					// If Off, turn on
		PowerOnSeq();
	}
	else {										// If On, turn off
		PowerOffSeq();
	}
}

void I2C_Receive(int) {
	DBG_PRINTLN("I2C_Receive");
	int ct=0;
	while (Wire.available()) {
		if (ct<2) {								// read first two bytes only
			byte c = Wire.read();
			DBG_PRINTLN(c, HEX);
			I2C_Data[ct] = c;
			ct++;
		}
		else {
			int nothing = Wire.read();			// eat extra data, should not be sent
		}
	}
	DBG_PRINT("ct: 0x");
	DBG_PRINTLN(ct, HEX);
	if (ct == 2) {
		I2C_Process();							// process received cmd
	}
}

//I2C Commands - Two Bytes per Command
//0x01 0x00		 - Power Off
//0x01 0x01		 - Hard Reboot (not working for some reason)
//0x02 0x00		 - Reset Button Press
//0x03 0x00		 - NMI Button press
//0x04 0x00-0xFF - Power LED Level (PWM)		// need to remove, not enough lines 
//0x05 0x00-0xFF - Activity/HDD LED Level (PWM)
void I2C_Process() {
	DBG_PRINTLN("I2C_Process");
	if (I2C_Data[0] == 1) {						// 1st Byte : Byte 1 - Power Events (Power off & Reboot)
		switch (I2C_Data[1]) {
			case 0:PowerOffSeq();				// 2nd Byte : 0 - Power Off
				break;
			case 1:HardReboot();				// 2nd Byte : 1 - Reboot (Physical Power off, wait 500ms, Power on)
				break;							// This command triggers, but the system does not power back on?  Not sure why.
		}
	}
	if (I2C_Data[0] == 2) {						// 1st Byte : Byte 2 - Reset Event(s)
		switch (I2C_Data[1]) {
			case 0:Reset_Button_Hold();			// 2nd Byte : 0 - Reset button Press
				break;
		}
	}
	if (I2C_Data[0] == 3) {						// 1st Byte : Byte 3 - NMI Event(s)
		switch (I2C_Data[1]) {
			case 0:Reset_Button_Press();		// 2nd Byte : 0 - NMI button Press
				break;
		}
	}
	// ["Byte 4 - Power LED Level" removed]
	if (I2C_Data[0] == 5) {						// 1st Byte : Byte 5 - Activity LED Level
		analogWrite(ACT_LED, I2C_Data[1]);		// 2nd Byte : Set Value directly
	}
	I2C_Data[0] = 0;
	I2C_Data[1] = 0;
}

char *hello = "HELLO WORLD!";

void I2C_Send() {
	DBG_PRINTLN("I2C_Send");
	Wire.write(hello[I2C_Data[0]] << 1);
}

void Reset_Button_Hold() {			
	if (SYSTEM_POWERED == 1) {					// Ignore unless Powered On
		digitalWrite(RESB_PIN,LOW);				// Press RESET
		delay(RESB_HOLDTIME);
		digitalWrite(RESB_PIN,HIGH);
	}
}

void Reset_Button_Press() {
	if (SYSTEM_POWERED == 1) {					// Ignore unless Powered On
		digitalWrite(NMIB_PIN,LOW);				// Press NMI
		delay(NMI_HOLDTIME);
		digitalWrite(NMIB_PIN,HIGH);
	}
}

void PowerOffSeq() {
	digitalWrite(PWR_ON, HIGH);					// Turn off supply
	SYSTEM_POWERED=0;							// Global Power state Off
	digitalWrite(RESB_PIN,LOW);
	delay(RESB_HOLDTIME);						// Mostly here to add some delay between presses
}

void PowerOnSeq() {
	digitalWrite(PWR_ON, LOW);					// turn on power supply
	unsigned long TimeDelta = 0;
	unsigned long StartTime = millis();			// get current time
	while (!digitalRead(PWR_OK)) {				// Time how long it takes
		TimeDelta=millis() - StartTime;			// for PWR_OK to go active.
	}
	if ((PWR_ON_MIN > TimeDelta) || (PWR_ON_MAX < TimeDelta)) {
		PowerOffSeq();							// FAULT! Turn off supply
		// insert error handler, flash activity light & Halt?	IE, require hard power off before continue?
	}
	else {
		SYSTEM_POWERED=1;						// Global Power state On
		delay(RESB_HOLDTIME);					// Allow system to stabilize
		digitalWrite(RESB_PIN,HIGH);			// Release Reset
	}
}

void HardReboot() {								// This never works via I2C... Why!!!
	PowerOffSeq();
	delay(1000);
	PowerOnSeq();
}
