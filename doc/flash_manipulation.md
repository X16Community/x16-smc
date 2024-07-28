# SMC flash manipulation tools, v5
Author: Eirik Stolpe

These tools allows X16 to dump entire SMC flash, and manipulate bootloader.


## New I2C commands

New I2C commands to SMC application, added in SMC version 47.2.0:

Write $90 -> set page to read/write
Read  $91 -> Read byte from flash and increment pointer
Write $92 -> Write byte to flash buffer and increment pointer
             -> Will write to flash once a full page (64 bytes) have been received
             -> Will only manipulate flash in bootloader area (page 120-127)
Read  $93 -> Check if flash manipulation is unlocked (0: locked, 1: unlocked, 255: old SMC version)
Write $93 -> Request flash manipulation (1: flash manipulation unlock request, 0: flash manipulation lock)


## Usage

1: Install SMC version 47.2.0 or later
-> Follow the guide "how to use the bad version of bootloader v2"
-> Optionally, use an ISP programmer/Arduino

   Use the HELP command and verify that you have version 47.2.0 or later before you proceed.

2: Copy BOOT2.BIN, SMCBLD2.PRG and SMCBLW16.PRG to the SD card

3: (optional) If you want to dump the flash to screen/ram/SD card, you can copy extra/SMCDMP8.PRG to SD card and run it.

4: Examine your current bootloader using SMCBLD2.PRG.
   LOAD "SMCBLD2.PRG"
   RUN

   This will dump the bootloader section of flash ($1E00-$1FFF) to screen and to RAM ($7E00-$7FFF).
   Then it will compute, print and compare the CRC-16 of bootloader area with known CRC-16 of existing bootloaders:
   -$19B5 -> Bootloader v1
   -$15C7 -> Bootloader v2
   -$7594 -> Bootloader v2 (bad), this is the one shipped with lots of machines
   -$6995 -> No bootloader installed (all FF), or SMC version is too old (v47 or older)

5: Upgrade bootloader with SMCBLW16.PRG

   LOAD "SMCBLW16.PRG"
   RUN

   The bootloader programming tool will load the new bootloader from file BOOT2.BIN (bootloader v2) into ram address $7C00-$7DFF.
   (You can optionally choose a different file: BOOT1.BIN (boot v1) or BOOT2BAD.BIN (bad version of v2 bootloader)).
   The loaded bootloader will be printed to screen. CRC-16 is printed to screen, along with version info, if CRC-16 is recognized.
   Then it will unlock flash programming, which requires a button press (Power + Reset).
   Then the new bootloader is written to flash.
   After write, the program reads back bootloader flash.
   If mismatch, user can try again. If OK, programming have succeeded.

6: Examine your current bootloader, after programming, using SMCBLD2.PRG
   LOAD "SMCBLD2.PRG"
   RUN
   
   This performs an independent validation that you have installed the correct bootloader version 2.


## Tools (written in BASIC)

SMCDMP8.PRG -> Dump entire flash
This will dump the SMC flash to screen as a hex dump, and store it to RAM at address $6000-$7FFF.
You can store this to SD card using BSAVE "DUMPSMC.BIN",8,1,$6000,$7FFF

SMCBLD2.PRG -> Bootloader dump
This will dump only the bootloader section ($1E00-$1FFF) to screen and to RAM ($7E00-$7FFF).

SMCBLW16.PRG -> Bootloader programming:
This tool will overwrite the bootloader section of flash ($1E00-$1FFF) with the contents of the specified .BIN file.
NB: After programming, please load SMCBLD2.PRG to independently validate bootloader version.

Ordinary users are encouraged to install the correct v2 bootloader (BOOT2.BIN).
Advanced users can experiment with bootloader v1 or the corrupt version of v2, included in the "extra" folder. Or install newer bootloaders.

## Bootloaders

v1:
- Install a custom zp used for USI interrupts
- Buffer the intended zero page (first 64 bytes)
- Receive the program via I2C and write it to flash, except the first page
- Once all pages except the first have been written, write the first page
- Infinite loop
   - Hence, v1 handles that the user disconnects the power supply

v2:
- Install a custom zp used for USI interrupts AND a new reset vector to jump to "post_reset"
- Buffer the intended zero page (first 64 bytes)
- Receive the program via I2C and write it to flash, except the first page
- Once all pages except the first have been written, activate the watchdog
- Infinite loop
- The watchdog will reset the SMC and jump to post_reset
- post_reset loads the new zp from ram and writes it to flash
- Jump to main (the new program)
- The new program starts as if it is just powered on, causing power supply to be cut
   - In this state, it is safe to disconnect power supply, but it is not needed, as the power button can be used

v2, bad version:
- Install a custom zp used for USI interrupts AND a new reset vector to jump to "post_reset"
- Buffer the intended zero page (first 64 bytes)
- Receive the program via I2C and write it to flash, except the first page
- Once all pages except the first have been written, start infinite loop WITHOUT ACTIVATING WATCHDOG (this is the problem with this version)
   - If power supply is disconnected here, you will get a bricked SMC
   - If SMC is restarted using the reset pin, SMC will not be bricked
- Once SMC restarts (due to reset pin or disconnected power), it jumps to "post_reset"
- post_reset loads the new zp from ram and writes it to flash
   - If reset pin was used, ram contents and zp is preserved
   - If power supply was removed, ram contains random data, and will in all likelyhood brick the SMC
- Jump to main (the new program)
- The new program starts as if it is just powered on



Changelog:
- Bundle 1: Proof of concept, for use with a SMC without button protection
- Bundle 2: New bootloader programmer for SMC with button protection, also more user friendly
- Bundle 3: New bootloader programmer, changing text as button combo was changed from pwr+NMI to pwr+reset
- Bundle 4: Improve readme.txt
- Bundle 5: Specifies that SMC version 47.2.0 is minimum required version
