# About

This is the firmware for the Commander X16 System Management Controller (SMC).

The SMC is responsible for

- Power control
- The physical buttons on the board
- The activity LED
- Interfacing a PS/2 keyboard
- Interfacing a PS/2 mouse

# Updating the SMC firmware

If you want to update the SMC firmware, please read this [guide](doc/update-guide.md).

# I2C API

## Table of commands
| Command # | Master read/write | Data              | Description                   |
| ----------|-------------------|-------------------|-------------------------------|
| 0x01      | Master write      | 0x00              | Power off                     |
| 0x02      | Master write      | 0x00              | Reset                         |
| 0x03      | Master write      | 0x00              | NMI                           |
| 0x05      | Master write      | 1 byte            | Set activity LED level        |
| 0x07      | Master read       | 1 byte            | Get keyboard keycode          | 
| 0x08      | Master write      | 1 byte            | Echo                          |
| 0x09      | Master write      | 1 byte            | Debug output                  |
| 0x0a      | Master read       | 1 byte            | Get low fuse setting          |
| 0x0b      | Master read       | 1 byte            | Get lock bits                 |
| 0x0c      | Master read       | 1 byte            | Get extended fuse setting     |
| 0x0d      | Master read       | 1 byte            | Get high fuse setting         |
| 0x18      | Master read       | 1 byte            | Get keyboard command status   |
| 0x19      | Master write      | 1 byte            | Send keyboard command         |
| 0x1a      | Master write      | 2 bytes           | Send keyboard command         | 
| 0x1b      | Master read       | 1 byte            | Get keyboard init state       | 
| 0x20      | Master write      | 1 byte            | Set requested mouse device ID |
| 0x21      | Master read       | 1, 3 or 4 bytes   | Get mouse movement            |
| 0x22      | Master read       | 1 byte            | Get mouse device ID           |
| 0x30      | Master read       | 1 byte            | Firmware version major        |
| 0x31      | Master read       | 1 byte            | Firmware version minor        |
| 0x32      | Master read       | 1 byte            | Firmware version patch        |
| 0x40      | Master write      | 1 byte            | Set Default Read Operation    |
| 0x41      | Master read       | 1 byte            | Get Keycode Fast              |
| 0x42      | Master read       | 1 byte            | Get Mouse Movement Fast       |
| 0x43      | Master read       | 1 byte            | Get PS/2 Data Fast            |
| 0x8e      | Master write      | 1 byte            | Get Bootloader Version        |
| 0x8f      | Master write      | 0x31              | Start bootloader              |
| 0x90      | Master write      | 1 byte            | Set flash page (0-127)        |
| 0x91      | Master read       | 1 byte            | Read flash                    |
| 0x92      | Master write      | 1 byte            | Write flash                   |
| 0x93      | Master read       | 1 byte            | Get flash write mode          |
| 0x93      | Master write      | 1 byte            | Request flash write mode      |


## Power, Reset and Non-Maskable Interrupt (0x01, 0x02, 0x03)

The commands 0x01 (Power Off), 0x02 (Reset) and 0x03 (NMI) have the same function as the board's physical push buttons.

Each function requires the user to send one byte with the value 0x00. Example that powers off the system.

```
I2CPOKE $42,$01,$00
```

## Set Activity LED level (0x05)

The Activity LED is turned off if you write the value 0x00 and turned on if you write the value 0xff to this offset.

Writing other values may cause instability issues, and is not recommended.

Example that turns on the Activty LED:

```
I2CPOKE $42,$05,$FF
```

## Get keyboard keycode (0x07)

The SMC translates PS/2 scan codes sent by the keyboard into IBM key codes. A list of key
codes is available in the Kernal source.

Key codes are one byte. Bit 7 indicates if the key was pressed (0) or released (1).

This command gets one keycode from the buffer. If the buffer is empty, it returns 0.

## Get fuses and lock bits (0x0a..0x0d)

These offsets returns the fuse settings and lock bits (low-lock-extended-high).

Expected value:
- Low: $F1
- Lock: $FF
- Extended: $FE
- High: $D4

The reason for this particular order, is size optimization in SMC FW.

## Get keyboard command status (0x18)

This offset returns the status of the last host to keyboard command.

The possible return values are:

- 0x00 = idle, no command has been sent
- 0x01 = pending, the last command is being processed
- 0xfa = the last command was successful
- 0xfe = the last command failed

## Send keyboard command (0x19 and 0x1a)

These offsets send host to keyboard commands.

Offset 0x19 sends a command that expects just the command number, and no data. Example that disables the keyboard:

```
I2CPOKE $42,$19,$f5
```

Offset 0x1a sends a command that expects a command number and one data byte. This can't be done with the I2CPOKE command.

## Get keyboard init state (0x1b)

Returns keyboard init state, which can be any of the following:

- 0x00 = keyboard off, or not connected
- 0x01 = Basic Assurance Test (BAT) succeded
- 0x02 = set LEDs command sent
- 0x03 = set LEDs command ACKed
- 0x04 = keyboard ready

During normal operation, the keyboard init state is 0x04 (ready).

## Set requested mouse device ID (0x20)

By default the SMC tries to initialize the mouse with support for a scroll wheel and two extra buttons, in total five buttons (device ID 4).

If the connected mouse does not support device ID 4, the SMC then tries to initialize the mouse with support for a scroll wheel but no extra buttons (decive ID 3).

If that fails as well, the SMC falls back to a standard mouse with three buttons and no scroll wheel (device ID 0).

This command lets you set what device ID you want to use. The valid options are 0, 3 or 4, as described above. 

Example that forces the SMC to initialize the mouse as a standard mouse with no scroll wheel support:

```
I2CPOKE $42,$20,$00
```

## Get mouse movement (0x21)

This command returns the mouse movement packet.

If no movement packet is available, it returns a 0 (1 byte).

If the mouse is initialized as device ID 0 (standard three button mouse, no scroll wheel), it returns a three byte packet.

And if the mouse is initialized as device ID 3 (scroll wheel) or 4 (scroll wheel+extra buttons), it returns a four byte packet.

## Get mouse device ID (0x22)

This returns the current mouse device ID. The possible values are:

- 0x00 = a standard PS/2 mouse with three buttons, no scroll wheel
- 0x03 = a mouse with three buttons and a scroll wheel
- 0x04 = a mouse with two extra buttons, a total of five buttons, and a scroll wheel
- 0xfc = no mouse connected, or initialization of the mouse failed

The returned device ID may differ from the requested device ID. For instance,
if you requested mouse device ID 4, but the extra buttons are not available on the
connected mouse, you will get ID 3 (if there is a scroll wheel).

Example that returns the current mouse device ID:

```
PRINT I2CPEEK($42,$22)
```

## Firmware version (0x30, 0x31 and 0x32)

The offsets 0x30, 0x31 and 0x32 return the current firmware version (major-minor-patch).

## Fast data fetch commands (0x40..0x43)

If you read from the SMC without first sending a command byte, 
the SMC will return data for the command referred to by the Set Default 
Read Operation command (0x40). This speeds up communication with
the SMC, as it takes some time to send the command byte.

It is possible to set any available command as the default read operation, but
the firmware was especially designed with these commands in mind:

- Get Keycode Fast (0x41) returns a key code if available, otherwise the request is NACKed.
- Get Mouse Movement Fast (0x42) returns a mouse packet if available, otherwise the request is NACKed.
- Get PS/2 Data Fast (0x43) returns both keycode and mouse packet, first a key code (1 byte) and then a mouse packet (3..4 bytes). If only one of them is available, the other will be reported as 0. If neither one is available, the request is NACKed.

## Get bootloader version (0x8e)

Returns the version of a possible bootloader installed at the top of the
flash memory. For further information, read about the Start bootloader command below.

If no bootloader is installed, the command returns 0xff.

## Start bootloader (0x8f)

This command initiates the sequence that starts the bootloader.

The bootloader, if present, is stored at 0x1E00 in the SMC flash memory. It is
used to update the firmware from the Commander X16 without an external programmer.
The new firmware is transmitted over I2C to the bootloader.

Calling this function starts a timer (20 s). Within that time the user must
press the physical Power and Reset buttons on the board for the bootloader
to actually  start. This is a safety measure, so that you don't
start the bootloader by mistake. Doing so will leave the SMC inoperable if the
update process is not carried through.

## Set flash page (0x90)

This command sets the target address for reading and writing
the content of the flash memory with command offsets 0x91 and 0x92.

The address is specified as the page index. A page is 64 bytes, and
consequently the target address is the specified index multiplied by 64.

There are a total of 128 pages. The first 120 pages represent the firmware
area, and the last 8 pages hold the bootloader.

Example that sets the target address to 0:

```
I2CPOKE $42,$90,$00
```

## Read flash (0x91)

Reads one byte from flash memory at the target address specified
with command offset 0x90. 

The target address is incremented after the operation.

Example that reads the value of flash memory address 0 and 1:

```
10 I2CPOKE $42,$90,$00
20 PRINT I2CPEEK($42,$91)
30 PRINT I2CPEEK($42,$91)
```

## Write flash (0x92)

Writes one byte to flash memory at the target address
specified with command offset 0x90. It is only possible to
update the bootloader section of the flash memory (0x1E00-0x1FFF).

Flash memory is programmed one page (64 bytes) at a time. Bytes
sent with this command are stored in a temporary buffer until
a page is filled. When a page is filled, the temporary buffer
is automatically written to flash memory.

The target address is incremented after the operation.

In order to use this command, you first need to activate
flash write mode with command offset 0x93.

Example that writes the values 0 to 63 to start of bootloader
section (0x1E00). The example assumes that the SMC is already
in flash write mode. Please note that this program will
destroy the bootloader.

```
10 REM THIS PROGRAM DESTROYS THE BOOTLOADER
20 REM RUN ONLY IF YOU HAVE AN EXTERNAL PROGRAMMER 
30 I2CPOKE $42,$90,$78
40 FOR I=0 TO 63
50 I2CPOKE $42,$92,I
60 NEXT I
```

## Flash write mode (0x93)

This command supports both reading and writing.

Write the value 0x01 to this command offset to
request flash write mode. A 20 second countdown is started. 
Within that time, the user must momentarily press and release 
the Power and Reset buttons at the same time. The flash write mode
is enabled after the buttons have been pressed.

Reading from this command offset returns the current
flash write mode status (0x00 = not enabled, 0x01 = enabled).

Example that requests that flash write mode, and
then reads the status 20 times. The return value is 0 until
the Power and Reset buttons have been pressed as described
above.

```
10 I2CPOKE $42,$93,$01
20 FOR X=1 TO 20
30 FOR I=0 TO 2000:NEXT I
40 PRINT I2CPEEK($42,$93)
50 NEXT X
```

# Build artifacts

The firmware is automatically built on every push and pull request.

You may download the resulting artifacts from the Actions view. Each build creates two artifacts:

- "SMC default firmware", compatible with the official Commander X16 board sold by TexElec

- "SMC CommunityX16 firmware", compatible with the CommunityX16 board designed by Joe Burks ("Wavicle")
