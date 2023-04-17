# Purpose

A client program running on the Commander X16 that updates the ATTiny861 based System Management Controller (SMC) firmware.

The purpose of the program is to make it possible to update the SMC firmware without an external programmer. Firmware data is
transmitted over I2C.


# Prerequisites

To run the SMC update program a bootloader must be installed in the SMC, and the SMC firmware must support the bootloader.

* Bootloader: https://github.com/stefan-b-jakobsson/x16-smc-bootloader

* Firmware support, not yet merged into the X16Community master branch: https://github.com/X16Community/x16-smc/pull/5


# Usage

The SMC update program is loaded and run in the same way as a BASIC program.

When started the bootloader will first check that there is a bootloader installed in the SMC. The program will also
check that it supports the installed bootloader API version.

Once you start the firmware update process there is no way to abort and restore the old firmware. There is simply not
enough storage in the ATTiny861 to implement that. A warning is displayed about the risk that the system may
become inoperable ("bricked") if the update process is interrupted or fails.

The new firmware you want to upload must be stored on the SD card (device #8) as an Intel HEX formatted file. This
is the same binary format outputted by the Arduino IDE and used by the avrdude utility.

The firmware must not overwrite the section of flash memory where the bootloader is stored (0x1E00..0x1FFF). The update
will be aborted if the HEX file contains data to be written to this memory section. Consequently, the SMC update
program cannot be used to update the bootloader itself. To do this you need to use an external programmer.

The bootloader activation starts when the SMC update program sends a command over I2C to the SMC. To minimize the risk
that a user sends this command by mistake when testing the I2C functions for other purposes, the user also needs to press the power and
reset buttons on the computer. The buttons must be pressed at the same time or at least within 0.5 seconds of
each other. The upload process is aborted if the buttons are not pressed within approx. 20 seconds.

After the bootloader has been activated, the SMC update program transmits the new firmware to the SMC.

The transmission is broken up into data packets of 8 bytes each. For each successful transmission a dot is printed to the screen.

If the transmission fails, an error number is printed to the screen instead of a dot. The possible error numbers are:

Err # | Description
------| -----------
2     | Data packet size error, indicating loss of a transmitted byte
3     | Checksum error, indicating transmission error
5     | Bootloader section overwrite, should not happen as it's checked before update starts

In the event of an error, the SMC update program attempts to resend the same packet 10 times before aborting the
update.

When the update is done the bootloader waits in an infinite loop for the user to remove power from the
whole system. The SMC reboots to the new firmware as power is reconnected.


https://user-images.githubusercontent.com/70063525/232566890-c397f7e5-9286-43c7-a572-064be95392c7.mov


# Unbricking the SMC

Should the update process fail it is likely that the SMC is left in an inoperable state.

To unbrick the SMC you need to program it with an external programmer.

For more information on this, go to https://github.com/X16Community/x16-docs/blob/master/X16%20Reference%20-%2013%20-%20Upgrade%20Guide.md
