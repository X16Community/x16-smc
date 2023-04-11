; BUILD: cl65 -o "SMCUPDATE.PRG" -u __EXEHDR__ -t cx16 -C cx16-asm.cfg main.asm

BOOTLOADER_MIN_VERSION = 1
BOOTLOADER_MAX_VERSION = 1

; Kernal functions
KERNAL_CHROUT       = $ffd2
I2C_READ            = $fec6
I2C_WRITE           = $fec9
I2C_ADDR            = $42

KERNAL_SETNAM       = $ffbd
KERNAL_OPEN         = $ffc0
KERNAL_CLOSE        = $ffc3 
KERNAL_SETLFS       = $ffba
KERNAL_CHKIN        = $ffc6
KERNAL_CLRCHN       = $ffcc
KERNAL_READST       = $ffb7
KERNAL_CHRIN        = $ffcf

ROM_BANK            = $01
tmp1                = $22

.macro print str
    ldx #<str
    ldy #>str
    jsr util_print_str
.endmacro

;******************************************************************************
;Function name.......: main
;Purpose.............: Program main entry
;Input...............: Nothing
;Returns.............: Nothing
.proc main
    ; Select ROM bank
    lda ROM_BANK
    pha
    stz ROM_BANK

    ; Print app info
    print str_appname

    ; Check bootloader presence and version
    ldx #I2C_ADDR
    ldy #$8e
    jsr I2C_READ
    cmp #$ff
    beq nobootloader
    cmp #BOOTLOADER_MIN_VERSION
    bcc unsupported_bootloader
    cmp #BOOTLOADER_MAX_VERSION+1
    bcs unsupported_bootloader

    ; Print warning
    print str_warning

    ; Confirm to continue
:   print str_continue
    jsr util_input
    cpy #1
    bne :-
    lda util_input_buf
    cmp #'y'
    beq confirmed
    cmp #'n'
    beq exit
    bra :-
    
confirmed:
    print str_filename
    jsr util_input
    phy

    print str_loading
    
    pla
    ldx #<util_input_buf
    ldy #>util_input_buf
    jsr ihex_load
    bcs err

    lda ihex_top+1
    cmp #$1e
    bcs overflow

    print str_ok

    print str_activate_prompt
    jsr util_input

    jsr upload
    bra exit

err:
    print str_failed

exit:
    pla
    sta ROM_BANK
    rts

overflow:
    print str_overflow
    bra exit

nobootloader:
    print str_nobootloader
    bra exit

unsupported_bootloader:
    pha
    print str_unsupported_bootloader
    pla
    jsr util_print_num
    bra exit
.endproc

;******************************************************************************
;Function name.......: upload
;Purpose.............: Upload SMC firmware to bootloader
;Input...............: Nothing
;Returns.............: Nothing
.proc upload
    ; Disable interrupts
    sei

    ; Send start bootloader command
    ldx #I2C_ADDR
    ldy #$8f
    lda #$31
    jsr I2C_WRITE

    ; Prompt the user to activate bootloader within 20 seconds
    print str_activate_countdown
    ldx #20
    jsr util_countdown

    ; Wait another 10 seconds to ensure bootloader is ready
    print str_activate_wait
    ldx #10
    jsr util_countdown

    ; Print upload begin alert
    print str_upload

    ; Calculate firmware top in buffer
    clc
    lda ihex_top
    adc #<ihex_buffer
    lda ihex_top+1
    adc #>ihex_buffer
    sta ihex_top+1

    ; Setup pointer to start of buffer
    lda #<ihex_buffer
    sta tmp1
    lda #>ihex_buffer
    sta tmp1+1

    ; Init variables
    stz checksum
    stz bytecount
    lda #10
    sta attempts

loop:
    ; Update checksum value
    lda (tmp1)
    clc
    adc checksum
    sta checksum
    
    ; Send packet byte
    ldx #I2C_ADDR
    ldy #$80
    lda (tmp1)
    jsr I2C_WRITE
    
    ; Update bytecount
    inc bytecount
    lda bytecount
    cmp #8              ; 8 bytes sent?
    bne next_byte       ; No

    ldx #I2C_ADDR       ; Yes, send checksum byte
    ldy #$80
    lda checksum
    eor #$ff
    ina
    jsr I2C_WRITE

    ldx #I2C_ADDR       ; Commit command
    ldy #$81
    jsr I2C_READ

    cmp #1
    beq ok
    jsr util_print_num

    dec attempts        ; Reached max attempts?
    beq updatefailed

    sec                 ; Prepare to resend packet
    lda tmp1
    sbc #7
    sta tmp1
    lda tmp1+1
    sbc #0
    sta tmp1+1
    stz bytecount
    stz checksum

    bra loop

ok:
    lda #'.'            ; Print "." to indicate success
    jsr KERNAL_CHROUT

    ; Check if at top of firmware
    lda tmp1+1
    cmp ihex_top+1
    bcc next_packet
    bne exit

    lda tmp1
    cmp ihex_top
    bcs exit

next_packet:
    stz bytecount       ; Reset variables
    stz checksum
    lda #10
    sta attempts

next_byte:
    inc tmp1            ; Increment buffer pointer
    bne loop
    inc tmp1+1
    bra loop
    
exit:
    ; Reboot
    ldx #I2C_ADDR
    ldy #$82
    jsr I2C_WRITE

    print str_done
    cli
    rts

updatefailed:
    print str_updatefailed
    
    ; Try to reboot anyway
    ldx #I2C_ADDR
    ldy #$82
    jsr I2C_WRITE

    cli
    rts
    
checksum: .res 1
bytecount: .res 1
attempts: .res 1
.endproc

.include "util.asm"
.include "str_en.asm"
.include "ihex.asm"