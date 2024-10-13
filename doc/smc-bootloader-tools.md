# SMC bootloader tools
Author: Eirik Stople

## Table Of Contents

1. [Introduction](#introduction)
2. [Requirements](#requirements)
3. [Prepare files](#prepare-files)
4. [BootLoader Dumper (SMCBLD7.PRG)](#bootloader-dumper-smcbld7prg)
5. [BootLoader Writer (SMCBLW19.PRG)](#bootloader-writer-smcblw19prg)
6. [Appendix: Tools overview, more details](#appendix-tools-overview-more-details)
7. [Appendix: Bootloader version history](#appendix-bootloader-version-history)
8. [Appendix: New I2C commands](#appendix-new-i2c-commands)
9. [Appendix: Changelog for SMC tools](#appendix-changelog-for-smc-tools)


## Introduction

The programs found in the [tools](../tools) folder allows X16 to read and write to SMC flash, including the bootloader area. There are 3 tools and 3 use cases:
- Read bootloader and compare against known versions (SMCBLD7.PRG) ("BootLoader Dump")
- Replace the bootloader (SMCBLW19.PRG) ("BootLoader Write")
- Dump the entire SMC flash (SMCDMP8.PRG)


## Requirements

- The bootloader tools requires SMC version 47.2.3 or later.
- Use the command `HELP` to check your version.
- If your version is 45.1.0, and this is what whas delivered with the machine, you most likely have the "bad v2 bootloader".
	- If there is a chance that you have the bad v2 bootloader, follow the guide "how to use the bad version of bootloader v2". https://github.com/X16Community/x16-smc/blob/main/doc/update-with-bad-bootloader-v2.md
- These instructions can also be used if you have a different SMC version, but then, you most likely do not need to do the custom reset procedure
- As an alternative to in-system programming, or to recover after a failed programming, you can use an external programmer to install the latest SMC version, ref https://github.com/X16Community/x16-smc/blob/main/doc/recovery-with-arduino.md
- Use the `HELP` command and verify that you have version 47.2.3 or later before you proceed.


## Prepare files
- Download the files BOOT3.BIN, SMCBLD7.PRG and SMCBLW19.PRG from the tools folder https://github.com/X16Community/x16-smc
- Copy the files to the SD card, e.g. inside UPDATE/SMCTOOLS
- If you have older versions of the bootloader tools on the SD card (e.g. SMCBLW16.PRG), these should be deleted, as these introduces a risk of bricking the SMC if you currently have the v3 bootloader.

## BootLoader Dumper (SMCBLD7.PRG)

Examine your current bootloader using SMCBLD7.PRG.

```
@CD:/UPDATE/SMCTOOLS
LOAD "SMCBLD7.PRG"
RUN
```
- This will dump the bootloader section of flash ($1E00-$1FFF) to screen and to RAM ($7E00-$7FFF).
- Then it will compute, print and compare the CRC-16 of bootloader area with known CRC-16 of existing bootloaders:
   - $19B5 -> Bootloader v1
   - $15C7 -> Bootloader v2
   - $7594 -> Bootloader v2 (bad), this is the one shipped with lots of machines
	   - $4DA7 -> (Bootloader v3p0 (222654, prelim) )
	   - $0CD0 -> (Bootloader v3p1 (8B70F5, prelim) )
	   - $34C7 -> (Bootloader v3p2 (A9A75F, prelim) )
	   - $B135 -> (Bootloader v3p3 (06E330, prelim) )
	   - $6222 -> (Bootloader v3p4 (08A724, prelim) )
	   - $C340 -> (Bootloader v3p5 (952878, prelim) )
	   - $BC21 -> (Bootloader v3p6 (BCF522, prelim) )
   - $BF63 -> Bootloader v3
   - $6995 -> No bootloader installed (all FF), or SMC version is too old (v47 or older)
 - The tool will also check if "Boot v3 failsafe" is installed.
	 - This is a modification installed by bootloader v3, which adds a "failsafe" feature in case you brick your SMC

## BootLoader Writer (SMCBLW19.PRG)

Upgrade bootloader with SMCBLW19.PRG
```
@CD:/UPDATE/SMCTOOLS
LOAD "SMCBLW19.PRG"
RUN
```

- The bootloader programming tool will load the new bootloader from file BOOT3.BIN (bootloader v3) into ram address $7C00-$7DFF.
	- You can also provide a custom bootloader file by pressing "C" and enter file name. E.g. BOOT2.BIN for the non-corrupt version of bootloader 2.
		- NB: If you currently have bootloader V3 installed, that bootloader will modify the main program and install "BOOT V3 FAILSAFE". This is needed for the new failsafe functionality introduced in V3. However, if "BOOT V3 FAILSAFE" is installed, and you then install bootloader V2, you will brick the SMC.
		- Never attempt to downgrade from v3 bootloader to v2 bootloader (unless you know what you do, and have a way of unbricking your SMC)

- The loaded bootloader will be printed to screen. CRC-16 is printed to screen, along with version info, if CRC-16 is recognized.

- Then it will unlock flash programming, which requires a button press (Power + Reset).

- Then the new bootloader is written to flash.

- After write, the program reads back bootloader flash.

- If mismatch, user can try again. If OK, programming have succeeded.

- After programming, examine your current bootloader, using SMCBLD7.PRG
	```
	@CD:/UPDATE/SMCTOOLS
	LOAD "SMCBLD7.PRG"
	RUN
	```

   This performs an independent validation that you have installed the correct bootloader version.

- Boot v3 failsafe
	- After installing bootloader v3 for the first time, the tool SMCBLD7.PRG will report that Boot v3 failsafe is not installed. This gets installed automatically after you use bootloader v3 to perform an SMC update, e.g. by installing the current SMC firmware version again (47.2.3 or newer).


## Appendix: Tools overview, more details

Note: These tools are written in Basic

SMCBLD7.PRG -> Bootloader dump
- This will dump only the bootloader section ($1E00-$1FFF) to screen and to RAM ($7E00-$7FFF).
- It will also evaluate the CRC-16, and compare it with CRC-16 for known bootloader versions.
- It will also evaluste if BOOT V3 FAILSAFE is installed, and warn against downgrading bootloader if so.


SMCBLW19.PRG -> Bootloader programming
- This tool will overwrite the bootloader section of flash ($1E00-$1FFF) with the contents of the specified .BIN file.
- NB: After programming, it is recommended to run SMCBLD7.PRG again, to independently validate bootloader version.

SMCDMP8.PRG -> Dump entire flash
- This will dump the SMC flash to screen as a hex dump, and store it to RAM at address $6000-$7FFF.
- You can store this to SD card using BSAVE "DUMPSMC.BIN",8,1,$6000,$7FFF


## Appendix: Bootloader version history

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

v3:
- Bootloader is rewritten to not depend on interrupt service routines
- Bootloader will modify the installed program, to start by jumping to the start of bootloader.
  The original reset vector is installed in EE_RDY interrupt vector by the bootloader
- Once the bootloader starts after power-on, it will evaluate if reset button is pressed.
  If reset button is pressed, it activates "recovery mode". It powers on the machine, and waits for bootloader commands.
  A custom AUTOBOOT.X16 may be used to recover SMC version 47.2.3, even if keyboard is not working.
- If reset button is not pressed, the bootloader ensures an ordinary startup, by jumping to the EE_RDY interrupt vector,
  which holds the original reset vector.

## Appendix: New I2C commands

New I2C commands to SMC application, added in SMC version 47.2.3:

- Write $90: set page to read/write
- Read  $91: Read byte from flash and increment pointer
- Write $92: Write byte to flash buffer and increment pointer
    - Will write to flash once a full page (64 bytes) has been received
    - Will only manipulate flash in bootloader area (page 120-127)
- Read  $93: Check if flash manipulation is unlocked (0: locked, 1: unlocked, 255: old SMC version)
- Write $93: Request flash manipulation (1: flash manipulation unlock request, 0: flash manipulation lock)

## Appendix: Changelog for SMC tools
- Version 1: Proof of concept, for use with a SMC without button protection
- Version 2: New bootloader programmer for SMC with button protection, also more user friendly
- Version 3: New bootloader programmer, changing text as button combo was changed from pwr+NMI to pwr+reset
- Version 4: Improve readme.txt
- Version 5: Specifies that SMC version 47.2.0 is minimum required version
- Version 6 (inside git repo): Markdown formatting, bootloader 3 support, some simplifactions
- Version 6 (zip): Add info about bootloader v3, new version of tools, and explain why bootloader should not be downgraded after v3 have been installed
- Version 7 (inside git repo): Port the changes from v6 zip into the version inside git repo, and restructure the document
