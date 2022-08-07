building:

cl65 -t cx16 -O -o ECHOTEST.PRG echotest.c kernapi.asm

or

cl65 -t cx16 -DNOKBD -o ECHOTEST.PRG echotest.c kernapi.asm to leave out
the initial prompt for disabling the VERA IRQ or not. (will leave it enabled)

The included build ECHOTESTNOKBD.PRG is built with the NOKBD flag, bypassing the prompt and leaving the VERA IRQ enabled.

The echo test program expects the SMC to be running the code which proides
SMC offset 8 = echo register.
write into offset 8 is stored as echo_byte.
read from offset 8 returns echo_byte.

ECHOTEST.PRG:
makes up to 65535 attempts to write and then read the echo byte. It keeps a
running display on the screen to indicate the test's progress so you know
things are moving under the hood.

At the beginning of the execution, it asks whether you would like to disable
VERA IRQs or not. Disabling them will ensure no Kernal KBD polling is active,
leaving only the test program to access the I2C bus. On exit (error or clean)
the program restores the IEN register state to VERA as it exits.

During testing:
If i2c_write_byte returns an error, it halts with write error.
If i2c_read_byte returns an error, it halts with read error.
The above typically indicate that the slave (SMC) did not recognize and
respond to its address byte ($42)

If the write and read are successful, but the return value does not match the
value just written, it writes a 1 to offset 9 (which should trigger a DBG print
on the SMC to show what the SMC has in echo_byte.

The test program then halts.

If all attempts succeed, it reports success and exits.

The writes are simply 0, 1, 2, 3, ... 254, 255, 0, 1, 2, ....

