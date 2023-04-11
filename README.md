# Purpose

The purpose of this project is to create a custom bootloader for the Commander X16 ATTiny861 based SMC.

Firmware data is transferred from the computer to the ATTiny over I2C.


# Building

The bootloader is made in AVR assembly for the AVRA assembler.

Build command: avra -o bootloader.hex main.asm

There is also a small build script (build.sh).


# Fuse settings

The SMC firmware and the bootloader depends on several fuse settings as set out below:

Low fuse value: 0xF1

Bit | Name   | Description         | Value
----|--------|---------------------|-----------------------------------------
7   | CKDIV8 | Divide clock by 8   | 1 = Disabled
6   | CKOUT  | Clock output enable | 1 = Disabled
5   | SUT1   | Select startup-time | 1 = Consider changing if problems occur
4   | SUT0   | Select startup-time | 1
3   | CKSEL3 | Select clock source | 0 = 16 MHz
2   | CKSEL2 | Select clock source | 0
1   | CKSEL1 | Select clock source | 0
0   | CKSEL0 | Select clock source | 1

High fuse value: 0xD4

Bit | Name     | Description                                       | Value
----|----------|---------------------------------------------------|-----------------------------------------
7   | RSTDISBL | External reset disable                            | 1 = Disabled
6   | DWEN     | DebugWIRE Enable                                  | 1 = Disabled
5   | SPIEN    | Enable Serial Program and Data Downloading        | 0 = Enabled
4   | WDTON    | Watchdog Timer always on                          | 1 = Disabled
3   | EESAVE   | EEPROM memory is preserved through the Chip Erase | 0 = Enabled
2   | BODLEVEL2 | Brown-out Detector trigger level                 | 1 = BOD level 4.3 V
1   | BODLEVEL1 | Brown-out Detector trigger level                 | 0
0   | BODLEVEL0 | Brown-out Detector trigger level                 | 0

RSTDISBL prevents serial programming if enabled.

SPIEN must be enbaled for serial programming.

Brown-out detection is necessary to precent flash memory corruption when the SELFPRGEN fuse bit is enabled.

Extended fuse value: 0xFE

Bit | Name      | Description                                       | Value
----|-----------|---------------------------------------------------|-----------------------------------------
0   | SELFPRGEN | Self-Programming Enable                           | 0 = Enabled


# Version ID

The bootloader version ID is stored in flash memory at the
start of the bootloader section.

Address 0x1E00 contain a magic number (0x8A) and the bootloader API version number is stored in
address 0x1E01.


# I2C API

 
Offset | R/W | Name           | Description
-------+-----+----------------+-----------------------------------------------
0x80   |  W  | Transmit       | Send data packet. A packet consists of 8 bytes 
to be written to flash memory and 1 checksum
byte. The checksum byte is the two's complement
of the sum of the previous bytes in the packet.
0x81   |  R  | Commit         | Commit a packet to flash memory. The first
       |     |                | commit is written to flash memory address
       |     |                | 0x0000. The target address is moved forward 8
       |     |                | bytes on each successful commit.
       |     |                | Returns 1 byte. Possible return values are
       |     |                | 00: OK, packet stored in RAM buffer
       |     |                | 01: OK, RAM buffer written to flash mem
       |     |                | 02: Packet size not 9 bytes
       |     |                | 03: Checksum error
       |     |                | 04: Reserved (may become write to flash failed)
       |     |                | 05: Overwriting bootloader section
       |     |                |
0x82   |  W  | Reboot         | Reboot the ATTiny. Before reboot any buffered
       |     |                | data is written to flash mem
-------+-----+----------------+-------------------------------------------------
 
X16 CLIENT SOFTWARE FUNCTION
 
Stage 1 - Prepare
- Check that a bootloader is present and that the client supports the bootloader API version
- Reserve an 8 kB RAM buffer, and clear it with value 0xff
- Read and parse HEX file into RAM buffer
- Cancel on HEX file errors:
  - Unsupported record type
  - Address out of range
  - Checksum error
 
Stage 2 - Transmit
- Transmit 8 bytes + 1 checksum byte
- Commit
- Retransmit if error response. Abort after 10 attempts.
- Repeat until all data is transmitted
- Reboot