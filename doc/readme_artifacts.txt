IMPORTANT INFORMATION - PLEASE READ BEFORE INSTALLING
-----------------------------------------------------

1. Target board for this firmware

    This firmware is made for the System Management
    Controller of the Commander X16 official prototype 4 
    board that is sold through texelec.com.

    It is also compatible with the OtterX board made
    by Joe Burks (Wavicle).

    It is not recommended to use this firmware with
    other boards. Doing so may break your system,
    requiring the SMC to be programmed with an external
    programmer.
    
    
2. The SMCUPDATE-x.x.x.PRG file

    This is a program that you can run on the
    X16 to update the firmware. The firmware is 
    emedded into the program and you do not need any
    other files.


3. The SMC-x.x.x.BIN file

    This file contains the firmware in binary format plus
    a header needed if you program the SMC with the
    following tool:

    https://github.com/FlightControl-User/x16-flash


4. The firmware_with_bootloader.hex file

    This file contains the firmware and a bootloader
    that makes it possible to update the SMC from the
    X16.

    It is the default option if you are programming
    the SMC with an external programmer.


5. The x16-smc.ino.hex and x16-smc.ino.elf files

    These files contain the firmare in hex and elf format
    without a bootloader.

    They are alternative options if you are programming
    the SMC with an external programmer.


6. The bootloader.bin file

    This is the the bootloader in binary format without
    firmware. It can be used to update the bootloader
    with the tool SMCBLW16.PRG that runs on the X16.


7. The bootloader.hex file

    This is the bootloader in hex format without
    firmware.
