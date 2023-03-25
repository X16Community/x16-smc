.include "tn861def.inc"

;******************************************************************************
; Global variables persistent in CPU registers
;******************************************************************************
.def packet_tailL           = r2              ; Pointer to current RAM buffer tail
.def packet_tailH           = r3

.def packet_headL           = r4              ; Pointer to current RAM buffer head
.def packet_headH           = r5

.def target_addrL           = r6              ; Target address in flash memory
.def target_addrH           = r7

.def packet_size            = r20             ; Current packet byte count
.def packet_count           = r21             ; Number of packets received since last flash write
.def checksum               = r22             ; Current packet checksum
.def i2c_state              = r23             ; Current state for I2C state machine
.def i2c_ddr                = r24             ; I2C data direction, bit 0=0 if master write else master read
.def i2c_command            = r25             ; Current I2C command


;******************************************************************************
; Program segment
;******************************************************************************
.cseg

; Interrupt vectors - only here during testing
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

version_id:
    .db 0x8a, 0x01

main:
    cli
    
    ; Init global variables
    ldi r16,LOW(flash_buf)
    ldi r17,HIGH(flash_buf)
    movw packet_tailH:packet_tailL, r17:r16, 
    movw packet_headH:packet_headL, packet_tailH:packet_tailL
    
    ldi r16,0x00
    ldi r17,0x10
    movw target_addrH:target_addrL, r17:r16

    clr packet_size
    ldi packet_count,8
    clr checksum

    ; Init I2C handler
    rcall i2c_init

    sei

loop:
    ; Do nothing
    rjmp loop

.include "i2c.asm"
.include "flash.asm"