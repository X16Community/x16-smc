; BUILD: cl65 -o "SMCUPDATE.PRG" -u __EXEHDR__ -t cx16 -C cx16-asm.cfg main.asm


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

tmp1                = $22

.macro print str
    ldx #<str
    ldy #>str
    jsr msg_print
.endmacro

;******************************************************************************
;Function name.......: main
;Purpose.............: Program main entry
;Input...............: Nothing
;Returns.............: Nothing
.proc main
    ; Print app info
    print str_appname
    print str_warning

    ; Continue after warning
:   print str_continue
    ldy #0
:   jsr KERNAL_CHRIN
    cpy #0
    bne :+
    sta response
:   iny
    cmp #13
    bne :--
    lda response
    cmp #'y'
    beq continue
    cmp #'n'
    beq exit
    bra :---
    
continue:
    print str_filename

    ldy #0
:   jsr KERNAL_CHRIN
    cmp #13
    beq :+
    sta filename,y
    iny
    bra :-

:   phy
    print str_loading
    
    pla
    ldx #<filename
    ldy #>filename
    jsr ihex_load
    bcs err
    print str_ok

    print str_upload
    jsr upload
    rts

err:
    print str_failed

exit:
    rts

response: .res 1
filename: .res 256
.endproc

;******************************************************************************
;Function name.......: upload
;Purpose.............: Upload SMC firmware to bootloader
;Input...............: Nothing
;Returns.............: Nothing
.proc upload
    ; Calculate firmware top
    clc
    lda ihex_top
    adc #<ihex_buffer
    sta ihex_top
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

    ; Disable interrupts
    sei

    ; Send start bootloader command
    ldx #I2C_ADDR
    ldy #$8f
    lda #$31
    jsr I2C_WRITE

    ; Wait for the bootloader to start
    jsr delay

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

    cmp #2
    bcc ok
    clc                 ; Print error code
    adc #48
    jsr $ffd2

    dec attempts        ; Reached max attempts?
    beq updatefailed

    sec                 ; Prepare to resend packet
    lda tmp1
    sbc bytecount
    sta tmp1
    lda tmp1+1
    sbc #0
    sta tmp1+1
    stz bytecount
    stz checksum

    bra loop

ok:
    lda #'.'            ; Print "." to indicate success
    jsr $ffd2

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
    cli
    rts
    
checksum: .res 1
bytecount: .res 1
attempts: .res 1
.endproc


;******************************************************************************
;Function name.......: delay
;Purpose.............: Delay approx one second
;Input...............: Nothing
;Returns.............: Nothing
.proc delay
    lda #168
    sta counter
    lda #130
    sta counter+1
    lda #13
    sta counter+2

loop:
    dec counter         ; 6
    bne loop            ; 3     9x
    dec counter+1       ; 6
    bne loop            ; 3     9x / 256
    dec counter+2       ; 6
    bne loop            ; 3     9x / 65535
    rts                 ; =>    9x + 9x/256 + 9x/65536 = 8000000 => 65536x + 256x + x = 8000000/9*65536 => x = 885416
    
    counter: .res 3
.endproc

.include "msg.asm"
.include "str_en.asm"
.include "ihex.asm"