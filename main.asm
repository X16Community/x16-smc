; Copyright 2023 Stefan Jakobsson
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
.org 0x0f00     ; = byte address 0x1E00

version_id:
    .db 0x8a, 0x02

;******************************************************************************
; Function...: main
; Description: Main entry point for the bootloader
; In.........: Nothing
; Out........: Nothing
main:
    cli
    
    ; Init global variables
    ldi YL,low(flash_zp_buf)
    ldi YH,high(flash_zp_buf)
    movw packet_tailH:packet_tailL, YH:YL
    movw packet_headH:packet_headL, YH:YL

    clr packet_size
    clr packet_count
    clr checksum

    ; Clear zero page buffer
    ldi r16,0xff
    ldi r17,0xff
    ldi r18,PAGE_SIZE/2
    rcall flash_fillbuffer

    ; Setup USI Start and Overflow vectors
    ldi YL,low(flash_buf)                               ; Pointer to start of default buffer
    ldi YH,high(flash_buf)
    
    ldi r16,0x18                                        ; Fill buffer with opcode for RETI instruction to disable all interrupts
    ldi r17,0x95
    ldi r18,19                                          ; 19 items in vector table
    rcall flash_fillbuffer

    ; Reset vector (word address 0x0000)
    subi YL,19*2
    ldi r16, low(0b1100000000000000 + post_reset - 1)
    st Y+,r16
    ldi r16, high(0b1100000000000000 + post_reset - 1)
    st Y+,r16

    ; USI Start vector (word address 0x0007)
    subi YL, -6*2
    ldi r16, low(0b1100000000000000 + i2c_isr_start - 1  - 7)
    st Y+,r16
    ldi r16, high(0b1100000000000000 + i2c_isr_start - 1  - 7)
    st Y+,r16

    ; USI Overflow vector (word address 0x0008)
    ldi r16, low(0b1100000000000000 + i2c_isr_overflow - 1 - 8)
    st Y+,r16
    ldi r16, high(0b1100000000000000 + i2c_isr_overflow - 1 - 8)
    st Y+,r16

    clr ZL                                              ; Set target addess to 0x0000
    clr ZH
    rcall flash_write_buf                               ; Write buffer that contains vector table for the bootloader to flash memory

    ; Set target address to start of second page = 0x0040
    ldi r16,0x40
    clr r17
    movw target_addrH:target_addrL, r17:r16

    ; Init I2C handler
    rcall i2c_init

    sei

wait:
    rjmp wait

;******************************************************************************
; Function...: post_reset
; Description: Entry point for reset after update complete.
;
;              The reset vector (addess 0x0000) is set to point to this function 
;              in the bootloader main function. The reset is initiated by the 
;              I2C reboot command (0x82).
;
;              This function first disables the WDT. This is to prevent repeated
;              resets that would otherwise occur.
;
;              The function then writes firmware code to the first 64 byte 
;              flash memory page. The code must be present in the RAM buffer
;              "flash_zp_buf" before the WDT reset is initiated. That buffer is
;              filled when you upload the first 64 bytes of data with the 
;              I2C functions transmit (0x80) and commit (0x81).
;
;              The RAM buffer is not affected by the WDT reset, as stated here:
;
;              https://microchip.my.site.com/s/article/AVR-Memory-Content-after-RESET-and-SLEEP
;              "Memory Content after a Watchdog Reset:
;              I/O registers will be set to their initial value according to the datasheet.
;              SRAM will be unchanged.
;              32 general purpose registers will be unchanged."
;
;              The I/O registers are, however, set to their default values, which will
;              power off the computer.
;
;              Finally, the function jumps to the start vector (0x0000) of
;              the new firmware. The firmware should then be active without
;              further user interaction.
;
; In.........: flash_zp_buf: RAM buffer containing the first 64 bytes of
;                            the new firmware code
; Out........: Nothing
post_reset:
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

    ; Write zero page to flash memory
    clr ZL
    clr ZH
    ldi YL,low(flash_zp_buf)
    ldi YH,high(flash_zp_buf)
    rcall flash_write

    ; Enable interrupts
    sei

    ; Jump to firmware reset vector
    rjmp 0x0000

.include "i2c.asm"
.include "flash.asm"