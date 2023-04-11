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

## Command 0x80 = Transmit (write)

The transmit command is used to send a data packet.

A packet consists of 8 bytes to be written to flash and 1 checksum byte.

The checksum is the two's complement of the previous bytes in the packet.

## Command 0x81 = Commit (read)

After sending a data packet, use this command to commit the transfer to
flash memory.

The first commit is written to flash memory address 0x0000. The target address
is moved forward 8 bytes on each successful commit.

The command returns 1 byte. The possible return values are:

Value | Description
------|-------------
1     | OK
2     | Error, packet size not 9
3     | Checksum error
4     | Reserved
5     | Error, overwriting bootloader section

## Command 0x82 = Reboot (write)

The reboot command must always be called after the last packet
has been committed. If not, the SMC will be left in an inoperable
state.

The command also writes any buffered data to flash.

Currently the SMC is not rebooted, but left in an infinte loop. To
reboot the SMC you need to remove power to the computer's power
supply.