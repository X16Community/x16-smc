.include "tn861def.inc"

.equ BOOTLOADER_ADDR = 0x00
.org BOOTLOADER_ADDR

.dseg

status: .byte 1
counter: .byte 2

.cseg

    rjmp main

main:
    cli

    rcall i2c_init
    rcall i2c_listen

    ;rcall fmw_init
    ;rcall i2c_init

reset:
    ret

delay:
    clr r16
    sts counter,r16
    sts counter+1,r16

delay_loop:
    lds r16,counter
    inc r16
    sts counter,r16
    cpi r16,0
    brne delay_loop

    lds r16,counter+1
    inc r16
    sts counter+1,r16
    cpi r16,50
    brne delay_loop
    ret


.include "i2c.asm"
;.include "fmw.asm"
;.include "test.asm"
;.include "usart.asm"