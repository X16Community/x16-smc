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


.equ PAGE_SIZE      = 64    ; In bytes

;******************************************************************************
; Data segment
;******************************************************************************
.dseg

flash_buf:          .byte PAGE_SIZE
flash_zp_buf:       .byte PAGE_SIZE

;******************************************************************************
; Program segment
;******************************************************************************
.cseg

;******************************************************************************
; Function...: flash_write_buf
; Description: Writes flash_buf (64 bytes) to flash memory
; In.........: ZH:ZL Flash memory target address (in bytes, not words)
; Out........: Nothing
flash_write_buf:
    ldi YL,low(flash_buf)           ; Y = RAM buffer pointer
    ldi YH,high(flash_buf)
    ; Fallthrough to flash_write

;******************************************************************************
; Function...: flash_write
; Description: Writes one page (64 bytes) to flash memory
; In.........: YH:YL Pointer to RAM buffer holding values to be written
;              ZH:ZL Flash memory target address (in bytes, not words)
; Out........: Nothing
flash_write:
    ; Erase page
    ldi r17, (1<<PGERS) + (1<<SPMEN)
    rcall flash_spm                 ; Perform erase

    ; Copy data from RAM buffer pointed to by Y into SPM temp buffer
    ldi r16, PAGE_SIZE              ; Init counter
flash_write_loop:
    ld r0, Y+                       ; Copy one word from buffer into r0:r1
    ld r1, Y+
    ldi r17, (1<<SPMEN)
    rcall flash_spm                 ; Perform copy
  
    subi ZL,-2                      ; Z = Z - (-2) => Z = Z + 2
    subi r16,2                      ; counter = counter - 2
    brne flash_write_loop           ; Check if we're done

    ; Write page
    subi ZL,PAGE_SIZE               ; Restore flash memory address to its start value
    ldi r17, (1<<PGWRT) + (1<<SPMEN)
    rjmp flash_spm                  ; Perform write

;******************************************************************************
; Function...: flash_spm
; Description: Performs Store Program Meomory operation
; In.........: r17 Value to be written to SPMCSR before operation, selecting
;                  SPM command
; Out........: Nothing
flash_spm:
    in r19, SPMCSR                  ; Wait for SPM not busy (SPMEN=0)
    sbrc r19, SPMEN
    rjmp flash_spm

flash_spm2:
    in r19, EECR                    ; Wait for EEPROM not busy
    sbrc r19, EEPE
    rjmp flash_spm2

    out SPMCSR, r17                 ; Perform SPM
    spm

flash_spm3:
    in r19, SPMCSR                  ; Wait for SPM to finish
    and r19,r17
    brne flash_spm3

    ret

;******************************************************************************
; Function...: flash_fillbuffer
; Description: Fills RAM buffer
; In.........: YH:YL   Pointer to RAM where fill starts
;              r17:r16 Fill value
;              r18     Number of words to fill
; Out........: Nothing
flash_fillbuffer:
    st Y+,r16
    st Y+,r17
    dec r18
    brne flash_fillbuffer
    ret