# Purpose

This is a custom bootloader for the Commander X16 ATTiny861 based System Management Controller (SMC).

The purpose of the project is to make it possible to update the SMC firmware from the Commander X16 without using an external programmer.

Firmware data is transmitted from the computer to the SMC over I2C.

# How it works

## SMC memory map

| Byte address  | Size        | Description                |
|-------------- |-------------| ---------------------------|
| 0x0000-0x1DFF | 7,680 bytes | Firmware area              |
| 0x0000        | 2 bytes     | Reset vector               |
| 0x0012        | 2 bytes     | EE_RDY vector              |
| 0x1E00-0x1FFF | 512 bytes   | Bootloader area            |
| 0x1E00        | 2 bytes     | Bootloader main vector     |
| 0x1E02        | 2 bytes     | Start update vector        |
| 0x1FFE        | 2 bytes     | Bootloader version         |    

The reset vector normally jumps to the start of the firmware. When installing
the bootloader, it is changed to point to the bootloader on
reset vector instead.

The firmware's original reset vector is moved to the unused EE_RDY vector.

## SMC reset procedure

On SMC reset the reset vector at address 0x0000 is always executed.
This happens both when mains power is connected to the computer
and when the SMC reset pin #10 is grounded.

The reset vector jumps to the bootloader main vector.

The bootloader main function checks if the Reset button is being pressed. 
It powers on the system and automatically starts the
update procedure.

If the Reset button was not pressed, the bootloader jumps to
the EE_RDY vector, which holds the firmware´s original reset vector. 
The SMC firmware is started normally.

## Firmware update procedure

The update procedure can be started in two ways. One alternative
is to hold down Reset when the SMC is reset or powered on. 
The other alternative is to call the bootloader start update vector. Calling this 
vector is initiated from the X16 while it is running normally.

Firmware data is transmitted from the CPU to the SMC over I2C as
follows:

- Transmit 8 bytes of firmware data and one checksum byte using
the 0x80 I2C command

- Send the 0x81 I2C command to commit the previous 8 bytes.

- The flash memory is updated in pages of 64 bytes (pages). On every
8th commit, firmware data will be written to flash
memory.

- Just before writing the first flash memory page, all of
the firmware flash area is erased, starting from the last page.

- After all firmware data has been transmitted to the 
bootloader, the process is finished by sending the 0x82
I2C command to reboot the system. This will also write
any remaining data to flash memory. The X16 is 
powered off.


# Building the project

You first need to copy the firmware file (x16-smc.ino.hex) to resources/x16-smc.ino.hex. You may download that file from the x16-smc Github release page. If you compile the firmware yourself it's easier to find the resulting file if you enable verbose output during compilation. Go to the IDE's preferences dialog to do that.

The bootloader is built with make. This outputs the file build/bootloader.hex. If resources/x16-smc.ino.hex is available it will also create the file build/firmware_with_bootloader.hex that contains both the firmware and the bootloader.

Build dependencies:

- AVRA assembler https://github.com/Ro5bert/avra

- Python 3

- Python intelhex, install with pip3 intelhex


# Fuse settings

The bootloader and the SMC firmware depend on several fuse settings as set out below.

The recommended low fuse value is 0xF1. This will run the SMC at 16 MHz.

The recommended high fuse value is 0xD4. This enables Brown-out Detection at 4.3V, which is necessary to prevent flash memory corruption when self-programming is enabled. Serial Programming should be enabled (bit 5) and external reset disable should not be selected (bit 7). These settings are necessary for programming the SMC with an external programmer.

Finally, the extended fuse value must be 0xFE to enable self-programming of the flash memory. The bootloader cannot work without this setting.


# Initial programming of the SMC with avrdude

Before SMC firmware version 47.2.3 it was not possible to update the bootloader from within
the X16 system. In that case the initial programming of the SMC must be done with an
external programmer. The command line utility avrdude is the recommended software to be used
for this purpose.

Example 1. Set fuses
```
avrdude -cstk500v1 -pattiny861 -P<your port> -b19200 -Ulfuse:w:0xF1:m -Uhfuse:w:0xD4:m -Uefuse:w:0xFE:m
```

Example 2. Write to flash
```
avrdude -cstk500v1 -pattiny861 -P<your port> -b19200 -Uflash:w:build/firmware_with_bootloader.hex:i
```

The -c option selects programmer-id; stk500v1 is for using Arduino UNO as an In-System Programmer. If you have another ISP programmer, you may need to change this value accordingly.

The -p option selects the target device, always attiny861.

The -P option selects port name on the host computer.

The -b option sets transmission baudrate; 19200 is a good value.

The -U option performs a memory operation. "-U flash:w:filename:i" writes to flash memory. "-U lfuse:w:0xF1:m" writes the low fuse value.

Please note that some fuse settings may "brick" the ATtiny861, and resetting requires equipment for high voltage programming. Be careful if you choose not to use the recommended values.

The Arduino IDE also uses avrdude in the background. If you have installed the IDE can use it to program the SMC, you may enable verbose output and see what parameters are used by the IDE when it starts avrdude.

From SMC firmware verision 47.2.3 it is also possible to update the firmware from within the system. Use the file build/bootloader.hex if you try this option.

# I2C API

## Command 0x80 = Transmit (master write)

The transmit command is used to send a data packet to the bootloader.

A packet consists of 8 bytes to be written to flash and 1 checksum byte.

The checksum is the two's complement of the least significant byte of the sum of the previous bytes in the packet. The least significant byte of the sum of all 9 bytes in a packet will consequently always be 0.

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

The firmware flash memory are will be erased on the 8th successful commit, just before writing the first 64 bytes to flash memory.

## Command 0x82 = Reboot (master write)

The reboot command must always be called after the last packet
has been committed. If not, the SMC may be left in an inoperable
state.

The command first writes any buffered data to flash.

The bootloader then resets the SMC. The SMC reset shuts down the computer. It
can be restarted by pressing the power button. It is not necessary to power cycle the system
after an update.

# Typical usage

A client program running on the X16 will typically work as set out below:

* Verify that it supports the bootloader API version

* Request that the SMC firmware jumps to the bootloader start update vector

* Send a data packet with command 0x80

* Commit the data packet with command 0x81

* Resend the packet if an error occured, otherwise repeat until all packets have been sent and committed

* Send reboot command
