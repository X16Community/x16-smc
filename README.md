# About

This is the firmware for ATtiny861 based System Management Controller in the Commander X16.

# I2C API

| Command # | Master read/write | Data              | Description                   |
| ----------|-------------------|-------------------|-------------------------------|
| 0x01      | Master write      | 0x00              | Power off                     |
| 0x01      | Master write      | 0x01              | Reboot                        |
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
