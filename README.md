# Purpose

This is a custom "bootloader" for the Commander X16 ATTiny861 based System Management Controller (SMC).

The purpose of the project is to make it possible to update the SMC firmware from the Commander X16 without using an external programmer.

Firmware data is transmitted from the computer to the SMC over I2C.


# Building the project

The bootloader is made in AVR assembly for the AVRA assembler: https://github.com/Ro5bert/avra

The following command builds the project: avra -o bootloader.hex main.asm

There is also a small build script in the repository (build.sh).


# Fuse settings

The bootloader and the SMC firmware depend on several fuse settings as set out below.

The recommended low fuse value is 0xF1. This will run the SMC at 16 MHz.

The recommended high fuse value is 0xD4. This enables Brown-out Detection at 4.3V, which is necessary to prevent flash memory corruption when self-programming is enabled. Serial Programming should be enabled (bit 5) and external reset disable should not be selected (bit 7). These settings are necessary for programming the SMC with an external programmer.

Finally, the extended fuse value must be 0xFE to enable self-programming of the flash memory. The bootloader cannot work without this setting.


# Initial programming of the SMC

The initial programming of the SMC must be done with an external programmer.

You may build the SMC firmware with the Arduino IDE as normal.

Use the merge.sh script to concatenate the firmware file (x16-smc.ino.hex) and the bootloader. This will output the file build/firmware+bootloader.hex, which is the file you need to upload to the SMC.

To make it a bit easier to find where the x16.smc.ino.hex file is stored in your file system, you may enable verbose output in the IDE. Go to Arduino/Preferences and tick the Show verbose output during compilation box.


# Memory map

Address         | Description
--------------- | -------------
0x0000..0x1DFF  | Firmware section
0x1E00..0x1FFF  | Bootloader section
0x1E00          | Bootloader magic value = 0x8A
0x1E01          | Bootloader API version
0x1E02          | Bootloader entry point

# I2C API

## Command 0x80 = Transmit (master write)

The transmit command is used to send a data packet to the bootloader.

A packet consists of 8 bytes to be written to flash and 1 checksum byte.

The checksum is the two's complement of the least significant byte of the sum of the previous bytes in the packet. The least signifant byte of the sum of all 9 bytes in a packet will consequently always be 0.

## Command 0x81 = Commit (master read)

After a data packet of 9 bytes has been transmitted it must be committed with this command. 

The first commit will target flash memory address 0x0000. The target address is moved forward 8 bytes on each successful commit.

The command returns 1 byte. The possible return values are:

Value | Description
------|-------------
1     | OK
2     | Error, packet size not 9
3     | Checksum error
4     | Reserved
5     | Error, overwriting bootloader section

## Command 0x82 = Reboot (master write)

The reboot command must always be called after the last packet
has been committed. If not, the SMC will be left in an inoperable
state.

The command first writes any buffered data to flash.

Then the bootloader enters an infinite loop waiting for the user to remove power from the system to reboot the SMC.

# Typical usage

A client program will typically work as set out below:

* Verify that it supports the bootloader API version

* Request that the SMC firmware jumps to the bootloader entry point

* Wait for the bootloader to initialize

* Send a data packet with command 0x80

* Commit the data packet with command 0x81

* Resend the packet if an error occured, otherwise repeat until all packets have been sent and committed

* Send reboot command