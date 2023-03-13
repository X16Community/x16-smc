.equ FMW_PAGE_SIZE = 64

; Status bits
.equ STATUS_BYTECOUNT_UNDERFLOW     = 0
.equ STATUS_BYTECOUNT_OVERFLOW      = 1
.equ STATUS_ADDRESS_OUT_OF_RANGE    = 2
.equ STATUS_CHECKSUM_ERROR          = 3

.dseg

fmw_status: .byte 1
fmw_addr_count: .byte 1
fmw_addr: .byte 2
fmw_byte_count: .byte 1
fmw_batch_count: .byte 1
fmw_checksum: .byte 1
fmw_buf: .byte FMW_PAGE_SIZE

.cseg

;******************************************************************************
; Function...: fmw_init
; Description: Initializes firmware code
; In.........: Nothing
; Out........: Nothing
; Affects....: r16
fmw_init:
    clr r16
    sts fmw_status,r16
    sts fmw_addr_count,r16
    sts fmw_addr,r16
    sts fmw_addr+1,r16
    sts fmw_byte_count,r16
    sts fmw_batch_count,r16
    sts fmw_checksum,r16
    ret

;******************************************************************************
; Function...: fmw_setaddr
; Description: Set flash memory page address
; In.........: r16 = received byte
; Out........: Nothing
; Affects....: r17
fmw_setaddr:
    lds r17,fmw_addr_count
    cpi r17,0
    brne fmw_setaddr2

    ldi r28,low(fmw_addr)       ; Set low addr byte
    ldi r29,high(fmw_addr)
    rjmp fmw_setaddr3

fmw_setaddr2:
    cpi r17,1
    brne fmw_setaddr4
    ldi r28,low(fmw_addr+1)
    ldi r29,high(fmw_addr+1)

fmw_setaddr3:
    st Y,r16                   ; Write value
    inc r17                     ; Increase byte counter
    sts fmw_addr_count,r17

fmw_setaddr4:
    ret

;******************************************************************************
; Function...: fmw_setdata
; Description: Adds firmware data to a buffer to be written to flash memory
; In.........: r16 = received byte
; Out........: Nothing
; Errors.....: The buffer overflow flag is set if more than 9 bytes (8 data
;              bytes + 1 checksum byte) are received before calling fmw_verify 
;              or fmw_write
;              The checksum flag is set if the 9th byte of a batch (checksum)
;              doesn't match
; Affects....: r17, r18, r28, r29
fmw_setdata:
    lds r17,fmw_batch_count     ; Check if batch (8 bytes + 1 byte checksum) complete
    cpi r17,9
    brsh fmw_setdata5           ; More data is overflow => an error condition

    lds r18,fmw_checksum        ; Update checksum
    add r18,r16
    sts fmw_checksum,r18

    cpi r17,8
    breq fmw_setdata2           ; Received byte is checksum

    ldi r28,LOW(fmw_buf)        ; Store input to buffer
    ldi r29,HIGH(fmw_buf)
    lds r17,fmw_byte_count
    add r28,r17
    clr r18
    adc r29,r18
    st Y,r16

    inc r17                     ; Increase byte count
    sts fmw_byte_count,r17

    rjmp fmw_setdata4

fmw_setdata2:
    cpi r18,0
    breq fmw_setdata4           ; Checksum = 0 => no error

fmw_setdata3:
    lds r17,fmw_status          ; Set checksum error flag
    sbr r17,1<<STATUS_CHECKSUM_ERROR
    sts fmw_status,r17

fmw_setdata4:
    lds r17,fmw_batch_count     ; Increae byte count
    inc r17
    sts fmw_batch_count,r17
    ret

fmw_setdata5:
    lds r17,fmw_status          ; Set buffer overflow flag
    sbr r17,1<<STATUS_BYTECOUNT_OVERFLOW
    sts fmw_status,r17
    ret

;******************************************************************************
; Function...: fmw_verify
; Description: Verifies a batch of firmware data (8 bytes + 1 checksum byte)
; In.........: Nothing
; Out........: r16 = status byte
; Affects....: r16, r24, r25
fmw_verify:
    lds r16,fmw_status

    ; Check address
    lds r24,fmw_addr
    lds r25,fmw_addr+1
    cpi r25,high(BOOTLOADER_ADDR-FMW_PAGE_SIZE+1)
    brlo fmw_verify2
    brne fmw_verify1a
    cpi r24,low(BOOTLOADER_ADDR-FMW_PAGE_SIZE+1)
    brlo fmw_verify2
fmw_verify1a:
    sbr r16,1<<STATUS_ADDRESS_OUT_OF_RANGE      ; Set address out of range

fmw_verify2:
    ; Check batch bytecount underflow (<9 bytes)
    lds r17,fmw_batch_count
    cpi r17,9
    brsh fmw_verify3
    sbr r16,1<<STATUS_BYTECOUNT_UNDERFLOW

fmw_verify3:
    ; Prepare for next batch
    clr r17
    sts fmw_status,r17
    sts fmw_batch_count,r17
    sts fmw_checksum,r17
    ret

;******************************************************************************
; Function...: fmw_write
; Description: Writes a batch of firmware data (8 bytes + 1 checksum byte) to
;              flash memory
; In.........: Nothing
; Out........: r16 = status byte
; Affects....: r16, r17, r18, r24, r25
fmw_write:
    ; Verify and get status
    rcall fmw_verify

    push r16                ; Save status code on stack
    cpi r16,0
    brne fmw_write2         ; Error condition, exit with status code in r16

    ; Check if buffer full
    lds r18,fmw_byte_count
    cpi r18,FMW_PAGE_SIZE
    brlo fmw_write2

    ; Erase flash page
    lds r30,fmw_addr
    lds r31,fmw_addr+1
    ldi r17,1<<SPMEN | 1<<PGERS
    rcall fmw_spm

    ; Copy data into flash page buffer
    ldi ZL,0
    ldi ZH,0
    ldi YL,low(fmw_buf)
    ldi YH,high(fmw_buf)

fmw_write_cpy:
    ld r0,Y+
    ld r1,Y+
    ldi r17,1<<SPMEN
    rcall fmw_spm
    subi ZL,-2
    cpi ZL,FMW_PAGE_SIZE
    brlo fmw_write_cpy

    ; Write data to flash memory
    lds ZL,fmw_addr
    lds ZH,fmw_addr+1
    ldi r17,1<<SPMEN | 1<<PGWRT
    rcall fmw_spm

    ; Prepare for next batch
    clr r18                     ; Clear byte counter
    sts fmw_byte_count,r18

    ldi r17,FMW_PAGE_SIZE       ; Advance address
    lds r16,fmw_addr
    add r16,r17
    sts fmw_addr,r16

    lds r16,fmw_addr+1
    ldi r17,0
    adc r16,r17
    sts  fmw_addr+1,r16

fmw_write2:
    pop r16                     ; Pull status byte from stack
    ret

;******************************************************************************
; Function...: fmw_spm
; Description: Executes SPM - Store Program Memory statement
; In.........: r17 = SPMCSR control value
; Out........: Nothing
; Affects....: r17, r18
fmw_spm:
    in r18,SPMCSR   ; Wait for SPMEN to go low (inactive)
    sbrc r18,SPMEN
    rjmp fmw_spm
    out SPMCSR,r17
    spm
    ret