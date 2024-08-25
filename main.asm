; Supported bootloaders
BOOTLOADER_MIN_VERSION = 1
BOOTLOADER_MAX_VERSION = 3

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
KERNAL_SCREEN_MODE  = $ff5f

ROM_BANK            = $01
tmp1                = $22

.include "version.asm"

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

    ; Get current firmware version
    ldx #I2C_ADDR
    ldy #$30
    jsr I2C_READ
    sta firmware_version

    ldy #$31
    jsr I2C_READ
    sta firmware_version+1

    ldy #$32
    jsr I2C_READ
    sta firmware_version+2

    ; Check if bootloader already active and responds to I2C offset 0x83 (get bootloader version)
    ldx #I2C_ADDR
    ldy #$83        ; Get bootloader version
    jsr I2C_READ
    sta bootloader_version

    cmp #255        ; Bootloader not active
    bne :+
    jsr manual_install
    bra exit

:   sta bootloader_version
    jsr auto_install

exit:
    pla
    sta ROM_BANK
    rts

.endproc


;******************************************************************************
;Function name.......: manual_install
;Purpose.............: Manual install from the X16 with keyboard support
;Input...............: Nothing
;Returns.............: Nothing
.proc manual_install
    print str_appinfo
    print str_automatic_fw_ver

    ; Check bootloader version
    ldx #I2C_ADDR
    ldy #$8e
    jsr I2C_READ
    sta bootloader_version
    cmp #$ff
    bne :++

    ; Try to get bootloader version from end of flash (>= bootloader v3)
    ldx #I2C_ADDR
    ldy #$90
    lda #$7f
    jsr I2C_WRITE

    lda #62
    sta temp
    ldx #I2C_ADDR
    ldy #$91
 :  jsr I2C_READ
    dec temp
    bne :-

    jsr I2C_READ
    cmp #$8a
    bne nobootloader
    jsr I2C_READ
    sta bootloader_version

    ; Verify bootloader version support
:   cmp #BOOTLOADER_MIN_VERSION
    bcc unsupported_bootloader
    cmp #BOOTLOADER_MAX_VERSION+1
    bcs unsupported_bootloader
    bra warning

nobootloader:
    print str_nobootloader
    rts

unsupported_bootloader:
    pha
    print str_unsupported_bootloader
    pla
    jsr util_print_num
    rts

warning:
    ; Print standard warning
    print str_warning

    ; Print warning about v2 bootloader corruption on some production boards
    lda bootloader_version
    cmp #2
    bne :+
    print str_warning2

:   print str_read_instructions

    ; Confirm to continue
:   print str_continue
    jsr util_input
    cpy #1
    bne :-
    lda util_input_buf
    cmp #'y'
    beq confirmed
    cmp #'Y'
    beq confirmed
    cmp #'n'
    beq exit
    cmp #'N'
    beq exit
    bra :-
    
confirmed:
    print str_activate_prompt
    jsr util_input

    jsr activate

exit:
    rts

.endproc

;******************************************************************************
;Function name.......: auto_install
;Purpose.............: Automatic install without keyboard support
;Input...............: Nothing
;Returns.............: Nothing
.proc auto_install
    print str_appinfo

    lda bootloader_version
    cmp #BOOTLOADER_MIN_VERSION
    bcc unsupported_bootloader
    cmp #BOOTLOADER_MAX_VERSION+1
    bcc version_ok

unsupported_bootloader:
    print str_unsupported_bootloader
    rts

version_ok:
    print str_automatic_fw_ver
    print str_automatic_info
    print str_warning
    print str_automatic_countdown
    ldx #60
    jsr util_countdown
    jmp update_firmware
.endproc


;******************************************************************************
;Function name.......: activate
;Purpose.............: Activates bootloader for manual update
;Input...............: Nothing
;Returns.............: Nothing
.proc activate
    ; Disable interrupts
    sei

    ; Send start bootloader command
    ldx #I2C_ADDR
    ldy #$8f
    lda #$31
    jsr I2C_WRITE

    ; Prompt the user to activate bootloader within 20 seconds, and check if activated
    print str_activate_countdown
    ldx #20
:   jsr util_stepdown
    cpx #0
    beq :+
    
    ldx #I2C_ADDR
    ldy #$8e
    jsr I2C_READ
    cmp #0
    beq :+
    jsr util_delay
    ldx #0
    bra :-

    ; Wait another 5 seconds to ensure bootloader is ready
:   print str_activate_wait
    ldx #5
    jsr util_countdown

    ; Check if bootloader activated
    ldx #I2C_ADDR
    ldy #$8e
    jsr I2C_READ
    cmp #0
    beq :+
    print str_bootloader_not_activated
    cli
    rts

:   jmp update_firmware
.endproc

;******************************************************************************
;Function name.......: update_firmware
;Purpose.............: Updates the firmware
;Input...............: Nothing
;Returns.............: Nothing
.proc update_firmware
    ; Print upload begin alert
    print str_upload

    ; Setup pointer to start of buffer
    lda #<payload
    sta tmp1
    lda #>payload
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
    and #$07            ; 8 bytes sent?
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
    bne :+
    jmp updatefailed

:   sec                 ; Prepare to resend packet
    lda tmp1
    sbc #7
    sta tmp1
    lda tmp1+1
    sbc #0
    sta tmp1+1
    stz checksum

    bra loop

ok:
    lda #'.'            ; Print "." to indicate success
    jsr KERNAL_CHROUT

    ; Check if at end of page
    lda bytecount
    and #$3f
    bne next_packet

    ; Check if at end of firmware
    lda tmp1+1
    cmp #>payload_end
    bcc next_packet
    bne reboot

    lda tmp1
    cmp #<payload_end
    bcs reboot

next_packet:
    stz checksum
    lda #10
    sta attempts

next_byte:
    inc tmp1            ; Increment buffer pointer
    bne loop
    inc tmp1+1
    bra loop
    
reboot:
    jsr verify_firmware

    lda bootloader_version
    cmp #2
    bcs reboot2

reboot1:
    print str_done
    ldx #I2C_ADDR
    ldy #$82
    jsr I2C_WRITE
:   bra :-

reboot2:
    print str_done2
    ldx #5
    jsr util_countdown

    ldx #I2C_ADDR
    ldy #$82
    jsr I2C_WRITE

    ; Wait 2 seconds
    jsr util_delay

    ; Bad bootloader if we are still here
    print str_bad_v2_bootloader
:   bra :-

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

;******************************************************************************
;Function name.......: verify_firmware
;Purpose.............: Compares firmware from SMC flash with new firmware
;Input...............: Nothing
;Returns.............: Nothing
.proc verify_firmware
    ; Check bootloader version >= 3
    lda bootloader_version
    cmp #3
    bcs :+
    rts

:   ; Print message
    print str_verifying

    ; Setup variables
    stz verified_count
    stz verified_count+1
    
    stz fail_count
    stz fail_count+1

    stz fail_flag

    sec
    lda #<payload_end
    sbc #<payload
    sta firmware_size
    lda #>payload_end
    sbc #>payload
    sta firmware_size+1

    ; Setup vector to new firmware
    lda #<payload
    sta tmp1
    lda #>payload
    sta tmp1+1

    ; Copy first page to temp buffer
    ldy #0
:   lda (tmp1),y
    sta buffer,y
    iny
    cpy #64
    bne :-

    ; Move reset vector to EE_RDY in the temp buffer
    sec
    lda buffer+0
    sbc #9
    sta buffer+18
    lda buffer+1
    sbc #0
    and #%11001111
    ora #%11000000
    sta buffer+19

    ; Redirect reset vector to start of flash in the temp buffer
    lda #$ff
    sta buffer
    lda #$ce
    sta buffer+1

    ; Rewind target address
    ldx #I2C_ADDR
    ldy #$84
    lda #0
    jsr I2C_WRITE

    ; Read and compare first page
    ldy #0
compare1:
    phy
    ldx #I2C_ADDR
    ldy #$85
    jsr I2C_READ
    ply
    cmp buffer,y
    bne :+
    jsr compare_ok
    bra next1
:   jsr compare_fail

next1:
    jsr check_done
    iny
    cpy #64
    bne compare1

    ; Move pointer to second page
    clc
    lda tmp1
    adc #64
    sta tmp1
    lda tmp1+1
    adc #0
    sta tmp1+1

compare:
    ldx #I2C_ADDR
    ldy #$85
    jsr I2C_READ
    cmp (tmp1)
    bne :+
    jsr compare_ok
    bra next
:   jsr compare_fail

next:
    inc tmp1
    bne :+
    inc tmp1+1
:   jsr check_done
    bra compare

compare_fail:
    inc fail_count
    bne :+
    inc fail_count+1
:   lda #1
    sta fail_flag

compare_ok:
    inc verified_count
    bne output
    inc verified_count+1

output:
    lda verified_count
    and #%00000111
    beq :+
    rts

:   lda fail_flag
    beq :+
    lda #'F'
    bra :++
:   lda #'.'
:   jsr KERNAL_CHROUT
    stz fail_flag
    rts

check_done:
    lda firmware_size
    bne :+
    dec firmware_size+1
:   dec firmware_size

    lda firmware_size
    ora firmware_size+1
    beq exit
    rts

exit:
    pla
    pla

    lda fail_count
    ora fail_count+1
    bne err

    print str_verify_ok
    rts

err:
    print str_verify_fail
    rts

firmware_size: .res 2
verified_count: .res 2
fail_count: .res 2
fail_flag: .res 1
buffer: .res 64
.endproc

temp: .res 1
firmware_version: .res 3
bootloader_version: .res 1

.include "util.asm"
.include "str_en.asm"
.include "payload.asm"
