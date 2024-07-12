; Copyright 2023-2024 Stefan Jakobsson
; 
; Redistribution and use in source and binary forms, with or without modification, 
; are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this 
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice, 
;    this list of conditions and the following disclaimer in the documentation 
;    and/or other materials provided with the distribution.
;
;    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” 
;    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
;    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
;    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
;    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
;    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
;    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
;    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
;    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
;    THE POSSIBILITY OF SUCH DAMAGE.

; Definitions
.include "tn861def.inc"
.include "pins.inc"
.include "registers.inc"

; Macros
.include "command.inc"

.cseg

.org 0xf00

;******************************************************************************
; Bootloader jump table
rjmp main                   ; Byte address: 0x1e00
rjmp bootloader_start       ; Byte address: 0x1e02

;******************************************************************************
; Function...: main
; Description: Bootloader main entry
; In.........: Nothing
; Out........: Nothing
main:
    ; Disable interrupts
    cli
    
    ; Disable watchdog timer
    wdr
    ldi r16,0
    out MCUSR,r16
    in r16,WDTCSR
    ori r16,(1<<WDCE) | (1<<WDE)
    out WDTCSR,r16
    ldi r16,0
    out WDTCSR,r16

    ; Configure Reset button pin (PB4) as input pullup
    cbi DDRB, RESET_BTN
    sbi PORTB, RESET_BTN
    rcall short_delay
    
    ; Jump to power on sequence if Reset button is low
    sbis PINB, RESET_BTN
    rjmp power_on_seq
    
    ; Jump to start vector stored in EE_RDY (=ERDYaddr)
loop:
    rjmp ERDYaddr

;******************************************************************************
; Function...: system_power_on_seq
; Description: System power on sequence
; In.........: Nothing
; Out........: Nothing
power_on_seq:
    ; Assert RESB
    sbi DDRA, RESB
    cbi PORTA, RESB

    ; PSU on
    sbi DDRA, PWR_ON
    cbi PORTA, PWR_ON

    ; RESB hold time is 500 ms
    ldi r18, 0x29
    ldi r17, 0xff
resb_hold_delay:
    rcall short_delay
    dec r17
    brne resb_hold_delay
    dec r18
    brne resb_hold_delay

    ; Deassert RESB
    cbi DDRA, RESB

;******************************************************************************
; Function...: bootloader_start:
; Description: 
; In.........: Nothing
; Out........: Nothing
bootloader_start:
    ; Disable interrupts
    cli

    ; Setup
    clr chip_erased
    ldi YL, low(flash_buf)
    ldi YH, high(flash_buf)
    clr packet_count
    clr packet_size
    clr checksum

    ; Start I2C
    rjmp i2c_main

;******************************************************************************
; Function...: short_delay
; Description: Delays approx 48 us
; In.........: Nothing
; Out........: Nothing
short_delay:
    ldi r16, 0xff
short_delay_loop:
    dec r16
    brne short_delay_loop
    ret

.include "flash.asm"
.include "i2c.asm"
.include "version.asm"
