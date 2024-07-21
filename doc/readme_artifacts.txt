IMPORTANT INFORMATION - PLEASE READ BEFORE INSTALLING
-----------------------------------------------------

1. Target board for this firmware

    This firmware is made for the System Management
    Controller of the Commander X16 official prototype 4 
    board that is sold through texelec.com.

    It is not recommended to use this firmware with
    any other board. Doing so may break your system,
    requiring the SMC to be programmed with an external
    programmer.
    

1. The firmware_with_bootloader.hex file

    This file contains the firmware and a bootloader
    that makes it possible to update the SMC from the
    X16.

    It is the default option if you are programming
    the SMC with an external programmer.


2. The x16-smc.ino.hex file

    This file contains the firmare in Intel HEX format.
    It does not contain a bootloader.

    It is an alternative option if you are programming
    the SMC with an external programmer.


4. The SMCUPDATE-x.x.x.PRG file

    This file is a program that you can run on the
    X16 to update the firmware. The firmware is 
    emedded into the program and you do not need any
    other files.


5. The SMC-x.x.x.BIN file

    This file contains the firmware in binary format plus
    a header needed if you program the SMC with the
    following tool:

    https://github.com/FlightControl-User/x16-flash
