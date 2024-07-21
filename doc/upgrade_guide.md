# SMC firmware upgrade guide

## Contents

[In-system update](#in-system-update)
- [Bootloader v1](#bootloader-v1)
- [Bootloader v2](#bootloader-v2)
- ["Bad" bootloader v2](#bad-bootloader-v2)
- [Bootloader v3](#bootloader-v3)

[In-system recovery with bootloader v3](#in-system-recovery-with-bootloader-v3)

[In-system update of the bootloader](#in-system-update-of-the-bootloader)

[External programmer](#external-programmer)

## In-system update

It is possible to update the firmware from the X16 without any external tools:

- Download SMCUPDATE-x.x.x.PRG, where x.x.x is the SMC firmware version
- Copy it to the SD card
- LOAD the SMCUPDATE-x.x.x.PRG program and RUN it on your X16
- The new firmware is embedded into the program and you need no other files
- Follow the on-screen instructions

There are slight differences depending on which bootloader version
is installed on your SMC.

### Bootloader v1

If your SMC has bootloader v1 installed the SMC enters into an
infinite loop after finishing the update. Just disconnect the X16
from mains power, wait until the LEDs go out, and then reconnect the
X16 again.

There are no known issues with bootloader v1.

### Bootloader v2

Bootloader v2 differs from v1 in that the SMC will reset and power
off after finishing the update. There is no need to disconnect the
computer from mains power.

### "Bad" bootloader v2

Some production boards were delivered with a corrupted version
of bootloader v2, a "bad" bootloader.

The SMCUPDATE program will display a warning if there is a 
risk that you have the bad bootloader.

Even with the bad bootloader, the update works until the very
final stage where it enters an infinite loop instead of resetting
the SMC and powering off the system.

If this happens it is important not to disconnect the computer
from mains power. Doing that will brick the SMC firmware, requiring
you to update it with an external programmer.

There is a tested procedure to make the update work with the
bad bootloader. Read about it [here](update_bad_v2.md).

### Bootloader v3

Bootloader v3 will also reset the SMC and power off the
computer when the update is done. There is no need to
disconnect the computer from mains power.

Bootloader v3 is reworked to be safer than the earlier versions.
It also enables the SMCUPDATE program to verify the firmware
after the update.

## In-system recovery with bootloader v3

Bootloader v3 supports recovery of the SMC even if a
firmware update failed and left the SMC inoperable (bricked).
This can happen, for instance, if the update process is
interrupted.

The recovery update requires that you store SMCUPDATE-x.x.x.PRG
on the SD card as file name AUTOBOOT.X16. If you already
have a file named AUTOBOOT.X16, remember to back it up first.

Disconnect the computer from mains power and wait until
the LEDs go out.

Press and hold the Reset button while you reconnect the
computer to mains power. The computer will turn on and 
you may release the Reset button as soon as you see
that the Power LED is on.

The computer autostarts the file AUTOBOOT.X16, which
will update the SMC firmware without user interaction
after a countdown.

If you want to abort you must disconnect the
computer from mains power before the end of the countdown.

The SMC is reset and the computer is turned off after
the update is finished. You may turn on the computer
again and delete AUTOBOOT.X16.


## In-system update of the bootloader

TODO


## External programmer

Programming the X16 with an external programmer
requires that the SMC microchip is removed from the
X16 board.

Ensure that the X16 is disconnected from mains
power before you start, as the SMC is always
powered.

To prevent damaging the X16 board, it is recommended 
to use an anti-static wrist band when touching the board
or its components.

It is possible to program the SMC with a TL866 series
programmer, but at least some of the programmers in that
series have problem setting the extended fuse correctly.

It is therefore recommended to program the SMC
with the commmand line utility avrdude and a 
programmer that is compatible with that utility.

For further information, go to th [guide on recovery with an Arduino](recovery_with_arduino.md).
