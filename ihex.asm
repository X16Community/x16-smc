IHEX_STATE_STARTCODE        = 1
IHEX_STATE_BYTECOUNT_H      = 2
IHEX_STATE_BYTECOUNT_L      = 3
IHEX_STATE_ADDR_3           = 4
IHEX_STATE_ADDR_2           = 5
IHEX_STATE_ADDR_1           = 6
IHEX_STATE_ADDR_0           = 7
IHEX_STATE_TYPE_H           = 8
IHEX_STATE_TYPE_L           = 9
IHEX_STATE_DATA_H           = 10
IHEX_STATE_DATA_L           = 11
IHEX_STATE_CHECKSUM_H       = 12
IHEX_STATE_CHECKSUM_L       = 13
IHEX_STATE_EOF              = 14
IHEX_STATE_ERR              = 15
IHEX_CHECKSUM_ERR           = 16

;******************************************************************************
;Function name.......: ihex_load
;Purpose.............: Loads and parses an Intel HEX file into a RAM buffer
;Input...............: X = File name address low
;                      Y = File name address high
;                      A = File name length
;Returns.............: C = 0 if successful, else C = 1
.proc ihex_load
    jsr ihex_open

    jsr ihex_clear_buffer
    jsr ihex_clear_record

loop:
    ; Read until startcode found, exit with error if not found
    jsr ihex_read_record
    cmp #IHEX_STATE_STARTCODE
    bne err

    ; Read until state is CHECKSUM_L, exit with error if other state
    jsr ihex_read_record
    cmp #IHEX_STATE_CHECKSUM_L
    bne err

    lda ihex_checksum               ; Verify checksum
    bne checksumerr

    lda ihex_type
    pha
    clc
    adc #48
    pla
    cmp #1                          ; 1 = EOF record
    beq eof

    cmp #0                          ; 0 = Data Record, ignore other records
    bne loop

    ldy ihex_bytecount              ; Get byte count
    beq loop                        ; Fetch next record if bytecount is 0
    dey

    lda ihex_addr                   ; Setup pointer to buffer where data is to be stored
    sta tmp1
    lda ihex_addr+1
    clc
    adc #$40
    sta tmp1+1

:   lda ihex_data,y                 ; Loop that stores the last record in the buffer
    sta (tmp1),y
    cpy #0
    beq loop
    dey
    bra :-

eof:
    ; We reached end of file, return
    jsr ihex_close
    clc
    rts

checksumerr:
    lda #IHEX_CHECKSUM_ERR
    sta ihex_state

err:
    ; An error occured, exit
    jsr ihex_close
    sec
    rts
.endproc

;******************************************************************************
;Function name.......: ihex_clear_buffer
;Purpose.............: Fills the 8 kB buffer that holds the new firmware with
;                      value 0xff
;Input...............: Nothing
;Returns.............: Nothing
.proc ihex_clear_buffer
    stz ihex_top
    stz ihex_top+1

    lda #<ihex_buffer
    sta tmp1
    lda #>ihex_buffer
    sta tmp1+1
    ldy #0
    ldx #$20
    lda #$ff
:   sta (tmp1),y
    iny
    cpy #0
    bne :-
    inc tmp1+1
    dex
    cpx #0
    bne :-
    rts

.endproc

;******************************************************************************
;Function name.......: ihex_open
;Purpose.............: Open a file for sequential reading
;Input...............: Pointer to file name, X=LSB and Y=MSB
;                      Length of file name in A
;Returns.............: C=0 if successful, else C=1
.proc ihex_open
    ;Save input
    phx
    phy
    pha

    ;Convert shifted PETSCII chars (193-223) to ASCII (65-95), needed for file name matching
    stx tmp1
    sty tmp1+1
    sta tmp1+2
    
    ldy #0
:   lda (tmp1),y
    cmp #193
    bcc :+
    cmp #224
    bcs :+
    and #%01111111
    sta (tmp1),y
:   iny
    cpy tmp1+2
    bcc :--

    ;Restore input
    pla
    ply
    plx

    ;Close file #1, and open file #1 for reading
    jsr KERNAL_SETNAM
    
    lda #1
    jsr KERNAL_CLOSE

    lda #1
    ldx #8
    ldy #0
    jsr KERNAL_SETLFS

    jsr KERNAL_OPEN
    bcs err

    ldx #1
    jsr KERNAL_CHKIN
    bcs err

    clc
    rts

err:
    lda #1
    jsr KERNAL_CLOSE
    jsr KERNAL_CLRCHN
    sec
    rts
.endproc

;******************************************************************************
;Function name.......: ihex_read_record
;Purpose.............: Reads one Intel HEX record from file
;Input...............: Nothing
;Returns.............: A = ihex parser state
.proc ihex_read_record
    ; Get next byte from file
    jsr KERNAL_CHRIN
    tay                       ; Preserve value in Y

    ; Check file status
    jsr KERNAL_READST
    tax                       ; Preserve value in X

    and #%01000000            ; EOI?
    beq :+
    jmp eof

:   txa                       ; Restore READST value in A
    and #%10000010            ; Read timeout (bit 1) or device not present (bit 7)?
    beq :+
    jmp err

:   tya                       ; Restore byte read from file into A

    ; Start code?
    cmp #':'
    bne :+

    jsr ihex_clear_record
    lda #IHEX_STATE_STARTCODE
    sta ihex_state
    rts

    ; Hex digit?
:   jsr ihex_char2hex
    cmp #$ff
    beq ihex_read_record      ; Char read from file was not a hex digit - ignore and read next

    ; Handle/store hex digit
    pha                       ; Save hex value on stack
    
    inc ihex_state

    lda ihex_state
    and #1
    bne lo_nibble
    jmp hi_nibble
    
    ; Low nibble
lo_nibble:
    pla                       ; Restore hex value from stack
    ora ihex_curval           ; Combine with high nibble previously received

    ldx ihex_state
    
    cpx #IHEX_STATE_BYTECOUNT_L
    bne :+
    sta ihex_bytecount
    jmp checksum_add

:   cpx #IHEX_STATE_ADDR_2
    bne :+
    sta ihex_addr+1
    jmp checksum_add

:   cpx #IHEX_STATE_ADDR_0
    bne :+
    sta ihex_addr
    jmp checksum_add

:   cpx #IHEX_STATE_TYPE_L
    bne :++
    sta ihex_type
    ldx ihex_bytecount
    bne :+
    inc ihex_state
    inc ihex_state
:   jmp checksum_add

:   cpx #IHEX_STATE_DATA_L
    bne :++
    ldy ihex_bytesreceived
    sta ihex_data,y
    iny
    sty ihex_bytesreceived
    cpy ihex_bytecount
    bcs :+
    dec ihex_state
    dec ihex_state
:   jmp checksum_add

:   cpx #IHEX_STATE_CHECKSUM_L
    bne :+++
    clc
    adc ihex_checksum
    sta ihex_checksum

    clc                     ; Update addr top
    lda ihex_addr
    adc ihex_bytecount
    sta tmpaddr
    lda ihex_addr+1
    adc #0
    sta tmpaddr+1

    lda ihex_top+1
    cmp tmpaddr+1
    bcc :+
    bne :++

    lda ihex_top
    cmp tmpaddr
    bcs :++

:   lda tmpaddr
    sta ihex_top
    lda tmpaddr+1
    sta ihex_top+1

:   lda ihex_state          ; Load return value
    rts

    ; Unknown state
:   lda ihex_state          ; Load return value
    rts

checksum_add:
    clc
    adc ihex_checksum
    sta ihex_checksum
    jmp ihex_read_record

hi_nibble:
    pla
    asl
    asl
    asl
    asl
    sta ihex_curval
    jmp ihex_read_record

eof:
    lda #IHEX_STATE_EOF
    sta ihex_state

close:
    lda #1
    jsr KERNAL_CLOSE
    jsr KERNAL_CLRCHN
    lda ihex_state          ; Load return value
    rts

err:
    lda #IHEX_STATE_ERR
    sta ihex_state
    bra close

tmpaddr: .res 2

.endproc


;******************************************************************************
; Function name.......: ihex_close
; Purpose.............: Closes file
; Input...............: Nothing
; Returns.............: Nothing
.proc ihex_close
    lda #1
    jsr KERNAL_CLOSE
    jsr KERNAL_CLRCHN
    rts
.endproc

;******************************************************************************
; Function name.......: ihex_clear_record
; Purpose.............: Clears currenct record data
; Input...............: Nothing
; Returns.............: Nothing
.proc ihex_clear_record
    stz ihex_state
    stz ihex_bytecount
    stz ihex_addr
    stz ihex_type
    stz ihex_checksum
    stz ihex_bytesreceived
    stz ihex_curval
    rts
.endproc


;******************************************************************************
; Function name.......: ihex_char2hex
; Purpose.............: Converts PETSCII char to HEX value (1 nibble)
; Input...............: A = PETSCII char
; Returns.............: A = HEX value
.proc ihex_char2hex
    cmp #48
    bcc NaN
    cmp #58
    bcc int
    cmp #65
    bcc NaN
    cmp #71
    bcc char1
    cmp #97
    bcc NaN
    cmp #103
    bcc char2
    cmp #193
    bcc NaN
    cmp #199
    bcc char3

NaN:
    lda #$ff
    rts

int:
    sec
    sbc #48
    rts

char1:
    sec
    sbc #55
    rts

char2:
    sec
    sbc #87
    rts

char3:
    sec
    sbc #183
    rts
.endproc

; Global variables
ihex_bytecount:             .res 1
ihex_addr:                  .res 2
ihex_type:                  .res 1
ihex_checksum:              .res 1
ihex_bytesreceived:         .res 1
ihex_state:                 .res 1
ihex_curval:                .res 1
ihex_top:                   .res 2
ihex_data:                  .res 256

ihex_buffer                 = $4000 
