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

.equ PAGE_SIZE      = 0x40      ; In bytes
.equ FIRMWARE_SIZE  = 0x1e00    ; In bytes

;******************************************************************************
; Data segment
;******************************************************************************
.dseg

flash_buf:          .byte PAGE_SIZE

;******************************************************************************
; Program segment
;******************************************************************************
.cseg

;******************************************************************************
; Function...: flash_erase
; Description: Erases firmware (byte address 0x0000-0x1dff),
;              starting from the last page; target address is guaranteed to
;              be 0x0000 when returning from this function
; In.........: Nothing
; Out........: target_addr = 0x0000
flash_erase:
    ldi target_addrL, low(FIRMWARE_SIZE - PAGE_SIZE)
    ldi target_addrH, high(FIRMWARE_SIZE - PAGE_SIZE)
flash_erase_loop:
    ldi r17, (1<<PGERS) + (1<<SPMEN)
    rcall flash_spm
    subi ZL, 64
    sbci ZH, 0
    brcc flash_erase_loop

    movw target_addrH:target_addrL, zeroH:zeroL

    ret

;******************************************************************************
; Function...: flash_write
; Description: Writes one page (64 bytes) to flash memory
; In.........: ZH:ZL Flash memory target address (in bytes, not words)
; Out........: Nothing
flash_write:
    ldi YL,low(flash_buf)
    ldi YH,high(flash_buf)
    ldi r16, PAGE_SIZE              ; Init counter
    
flash_write_loop:
    ; Copy data from RAM buffer pointed to by Y into SPM temp buffer
    ld r0, Y+                       ; Copy one word from buffer into r0:r1
    ld r1, Y+
    ldi r17, (1<<SPMEN)
    rcall flash_spm                 ; Perform copy
  
    adiw ZH:ZL, 2                   ; Z = Z + 2
    subi r16, 2                     ; counter = counter - 2
    brne flash_write_loop           ; Check if we're done

    ; Write page
    sbiw ZH:ZL, PAGE_SIZE/2         ; Restore flash memory address to its start value
    sbiw ZH:ZL, PAGE_SIZE/2

    ldi r17, (1<<PGWRT) + (1<<SPMEN)
    
    ; Fallthrough to flash_spm

;******************************************************************************
; Function...: flash_spm
; Description: Performs Store Program Meomory operation
; In.........: r17 Value to be written to SPMCSR before operation, selecting
;                  SPM command
; Out........: Nothing
flash_spm:
    ; Wait for SPM not busy (SPMEN=0)
    in r19, SPMCSR
    sbrc r19, SPMEN
    rjmp flash_spm

    ; Perform SPM
    out SPMCSR, r17
    spm

flash_spm2:
    ; Wait for SPM to finish
    in r19, SPMCSR
    and r19, r17
    brne flash_spm2

    ret

;******************************************************************************
; Function...: flash_clear_buf
; Description: Fills buffer with 0xff
; In.........: Nothing
; Out........: YH:YL = flash_buf
flash_clear_buf:
    ldi YL, low(flash_buf + PAGE_SIZE)
    ldi YH, high(flash_buf + PAGE_SIZE)
    ldi r16, PAGE_SIZE
    ldi r17, 0xff
flash_clear_buf_loop:
    st -Y, r17
    dec r16 
    brne flash_clear_buf_loop
    ret
