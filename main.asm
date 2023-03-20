.include "tn861def.inc"

.cseg

    rjmp main               ; 1. Reset
    reti                    ; 2.
    reti                    ; 3.
    reti                    ; 4.
    reti                    ; 5. 
    reti                    ; 6.
    reti                    ; 7.
    rjmp i2c_isr_start      ; 8. USI Start
    rjmp i2c_isr_overflow   ; 9. USI Overflow
    reti                    
    reti
    reti
    reti
    reti
    reti
    reti
    reti
    reti
    reti
    reti

main:
    cli
    ldi r16,0xff
    out DDRA,r16
    ldi r16,0
    out PORTA,r16

    rcall i2c_init

    ldi r24,0
    ldi r25,50

    sei

loop:
    cpi r24,0
    brne loop3
    cpi r25,0
    brne loop2

    cli
    ;rcall i2c_init
    sei

loop2:
    dec r25
loop3:
    dec r24

    rjmp loop

.include "i2c.asm"