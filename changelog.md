## Power-on procedure
- Reset vector (0x0000) jumps to start of bootloader memory (0x1e00)
- At start of bootloader memory, there is a jump table. It has two
items (0x1e00=system start, and 0x1e02=start update).
- The system start checks if the Reset button is held down. If so
it first powers on the system and then starts the update procedure.
- If the Reset button is not held down, the bootloader jumps
to the firmware vector EE_RDY which during the update is made
to hold the firmware's start vector.

## Update procedure
- If the update procedure was started by holding down Reset button
when plugging the X16 to mains power, the computer has no
keyboard. The update program must be autoloaded from the SD card
and run without user interaction.
- If the update was initiated from the X16, it works as before.
- The bootloader I2C is USI driven but without interrupts, as Eirik demonstrated was
possible.
- The same I2C commands as before are used during the update.
- The firmware flash area is erased before writing the first
page of new firmware data. The erase starts from the last page. If the
update is interrupted during this stage, execution will still end up in
the bootloader, as an erased word (0xffff) is a NOP.
- Before writing the first new page, the firmware's reset vector is moved to
the unused EE_RDY, and the reset vector is changed to point to the start of
bootloader memory.
- The subsequent pages are then written to flash memory. If the update
procedure fails during this stage, the bootloader can be started 
by holding down the Reset button.
- After all pages have been written the bootloader reset command will
reset and power off the system. It should also be safe to just
unplug power at this stage.

## Backward compatibility
- Current firmware will not now how to get the bootloader version.
The bootloader version is planned to be moved to the end of flash (not yet implemented).
Current firmware can, however, start bootloader v3 and do an update.
- The current firmware version may extract the bootloader version using the general flash read function
- Sven's X16-Flash utility will say that there is no bootloader installed, but even so
it continues doing the update. The update will be successful. It doesn't matter
if you disconnect power or send the bootloader reset command, so whatever is
said in the X16-Flash utility is OK.
- The SMCUPDATE tool aborts the update as it can't determine the bootloader version.
