# About

This is the firmware for the Commander X16 System Management Controller (SMC).

The SMC is responsible for

- Power control
- The physical buttons on the board
- The activity LED
- Interfacing a PS/2 keyboard
- Interfacing a PS/2 mouse

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
| 0x18      | Master read       | 1 byte            | Get keyboard command status   |
| 0x19      | Master write      | 1 byte            | Send keyboard command         |
| 0x1a      | Master write      | 2 bytes           | Send keyboard command         | 
| 0x20      | Master write      | 1 byte            | Set requested mouse device ID |
| 0x21      | Master read       | 1, 3 or 4 bytes   | Get mouse movement            |
| 0x22      | Master read       | 1 byte            | Get mouse device ID           |
| 0x30      | Master read       | 1 byte            | Firmware version major        |
| 0x31      | Master read       | 1 byte            | Firmware version minor        |
| 0x32      | Master read       | 1 byte            | Firmware version patch        |
| 0x8f      | Master write      | 0x31              | Start bootloader              |

## Power, Reset and Non-Maskable Interrupt (0x01, 0x02, 0x03)

Commands 0x01 (Power Off), 0x02 (Reset) and 0x03 (NMI) have the same function as the board's physical push buttons.

Each function requires the user to send one byte with the value 0x00. Example that powers off the system.

```
I2CPOKE $42,$01,$00
```

## Set Activity LED level (0x05)

The Activity LED is turned off if you write the value 0x00 and turned on if you write the value 0xff.

Writing other values may cause instability issues, and is not recommended.

Example that turns on the Activty LED:

```
I2CPOKE $42,$05,$FF
```

## Get keyboard keycode (0x07)

The SMC translates PS/2 scan codes sent by the keyboard into IBM key codes. A list of key
codes is available in the Kernal source code.

Key codes are one byte. Bit 7 indicates if the key was pressed (0) or released (1).

This command gets one keycode from the buffer. If the buffer is empty, it returns 0.

## Get keyboard command status (0x18)

This offset returns the status of the last host to keyboard command.

The possible return values are:

- 0x00 = idle, no command has been processed
- 0x01 = pending, the last command is being processed
- 0xfa = the last command was successful
- 0xfe = the last command failed

## Send keyboard command (0x19 and 0x1a)

These offsets sends a host to keyboard command.

Offset 0x19 sends a command that expects just the command number, and no data. Example that disables the keyboard:

```
I2CPOKE $42,$19,$f5
```

Offset 0x1a sends a command that expects a command number and one data byte. This can't be sent with the I2CPOKE command.

## Set requested mouse device ID (0x20)

By default the SMC tries to initialize the mouse with support for a scroll wheel and two extra buttons, in total five buttons (device ID 4).
If the connected mouse does not support device ID 4, it then tries to initialize the mouse with support for a scroll wheel but no extra buttons (decive ID 3).
If that also fails, the SMC falls back to a standard mouse with three buttons and no scroll wheel (device ID 0).

This command lets you set what device ID you want to use. The valid options are 0, 3 or 4, as described above. 

Example that forces the SMC to initialize the mouse as a standard mouse with no scroll wheel support:

```
I2CPOKE $42,$20,$00
```

## Get mouse movement (0x21)

This command returns the mouse movement packet.

If no movement packet is available, it returns a 0, that is 1 byte.

If the mouse is initialized as device ID 0, it returns a three byte packet.

And if the mouse is initialized as device ID 3 or 4, it returns a four byte packet.

## Get mouse device ID (0x22)

This returns the currently initialized mouse device ID. The possible values are:

- 0x00 = a standard PS/2 mouse with three buttons, no scroll wheel
- 0x03 = a mouse with three buttons and a scroll wheel
- 0x04 = a mouse with two extra buttons, a total of five buttons, and a scroll wheel
- 0xfc = no mouse connected, or initialization of the mouse failed

The returned device ID may not be the same as the requested device ID. For instance,
if you request mouse device ID 4, but the extra buttons are not available on the
connected mouse, you will get ID 3 (if there was scroll wheel support).

## Firmware version (0x30, 0x31 and 0x32)

The offsets 0x30, 0x31 and 0x32 returns the current firmware version.

## Start bootloader (0x8f)

This command initiates the sequence that starts the bootloader.

The bootloader, if present, is stored at 0x1F00 in the SMC flash memory. It is
used to update the firmware from the Commander X16 without an external programmer.
The new firmware is transmitted over I2C to the bootloader.

Calling this function starts a timer (20 s). Within that time the user must
press the physical Power and Reset buttons on the board for the bootloader
to actually  start. This is a safety measure, so that you don't
start the bootloader by mistake. Doing so will leace the SMC inoperable the
update process is carried through.