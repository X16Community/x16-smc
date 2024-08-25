# Introduction

This is a client program running on the Commander X16 that updates the ATTiny861 based System Management Controller (SMC) firmware.

The purpose of the program is to make it possible to update the SMC firmware without an external programmer. Firmware data is
transmitted over I2C.


# Prerequisites

To run the SMC update program a bootloader must be installed on the SMC.


# Normal usage

The SMC update program is loaded and run in the same way as a BASIC program.

When started the bootloader will first check that there is a bootloader installed in the SMC. The program will also
check that it supports the installed bootloader version.

The new firmware is embedded into the program file and you do not need any other files.

The bootloader activation starts when the SMC update program sends a command over I2C to the SMC. To minimize the risk
that a user sends this command by mistake, the user also needs to press the power and
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

When the update is done:

- Bootloader v1: The bootloader enters into a loop waiting for the user to remove mains power
- Bootloader v2: The bootloader resets the SMC which shuts down the system. You don't need to remove mains power, just press Power button to start the computer again.
- Bootloader v2 (corrupted "bad" version): Some production boards comes with an unofficial corrupted v2 bootloader. The bootloader will not reset the system. Do not remove mains power, which will prevent the system from starting again. Instead you need to connect SMC pin #10 (reset) to ground with a jumper wire. This will turn off the system that can be started again with the Power button. A special warning is displayed in the update program if your board has the corrupted v2 bootloader.
- Bootloader v3: The bootloader resets the SMC which shuts down the system. You don't need to remove mains power, just press Power button to start the computer again.

# Unbricking the SMC

If the update process fails it is likely that the computer is left in an inoperable state. You need to make a recovery update
of the firmware for the computer to work again.

## Recovery update with the SMCUPDATE program (available from bootloader v3)

From bootloader v3 it is possible to do a recovery update without external tools using the SMCUPDATE program.

Store SMCUPDATE.PRG as AUTOBOOT.X16 in the root folder of the SD card.

Disconnect the computer from mains power at least until the LEDs are turned off.

Press and hold the reset button at the same time as you connect the computer to mains power again. The computer should
automatically turn on and run AUTOBOOT.X16. You may release the reset button as soon as you see that the power LED is on.

The update program detects that the bootloader is active in recovery mode, and does the update without user interaction after a
20 second countdown. If you want to abort the update you must disconnect the computer from mains power again.

## Recovery with external tools

Go to the x16-smc repo to learn about recovery with external programming tools.
