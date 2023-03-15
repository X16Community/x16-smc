.include "tn861def.inc"

.org 0x0000

.equ BOOTLOADER_ADDR = 0x00
.org BOOTLOADER_ADDR

.dseg

status: .byte 1
counter: .byte 2

.cseg

    rjmp main

main:
    rcall i2c_init
    rcall i2c_listen

    ;rcall fmw_init
    ;rcall i2c_init

.include "i2c.asm"
;.include "fmw.asm"
;.include "test.asm"
;.include "usart.asm"