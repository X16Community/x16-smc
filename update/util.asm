;******************************************************************************
; Function name.......: util_print_str
; Purpose.............: Prints a null terminated string
; Input...............: X = Pointer to string, address low
;                       Y = Pointer to string, address high
; Returns.............: Nothing
.proc util_print_str
    ; Store input
    stx tmp1
    sty tmp1+1
    sta align

    ; Get screen width
    sec
    jsr KERNAL_SCREEN_MODE
    stx screen_width

begin_line:
    ldy #0
    stz breakat
    stz controlchars

search:
    lda (tmp1),y
    beq setend
    cmp #13
    beq setend
    cmp #32
    bne :+

    ; Remember index of last blank space
    sty breakat

:   ; Check if control char
    lda (tmp1),y
    cmp #COL_DEFAULT
    beq :+
    cmp #COL_BG
    beq :+
    cmp #COL_WARN
    beq :+
    cmp #COL_OK
    beq :+
    cmp #COL_ERR
    beq :+
    cmp #COL_SWAP
    beq :+
    cmp #SCR_CLS
    beq :+
    cmp #SCR_PETSCII
    beq :+
    cmp #SCR_LOWER
    beq :+
    bra :++

:   inc controlchars

    ; Check if at end of line
:   iny
    sec
    tya
    sbc controlchars
    cmp screen_width
    bcs output
    bra search

setend:
    sty breakat

output:
    ldy #0
:   lda (tmp1),y
    beq exit
    jsr KERNAL_CHROUT
    cmp #13
    beq advance
    cpy breakat
    beq breakhere
    iny
    bra :-

breakhere:
    sec
    tya
    ina
    sbc controlchars
    cmp screen_width
    bcs advance
    lda #13
    jsr KERNAL_CHROUT

advance:
    clc
    tya
    ina
    adc tmp1
    sta tmp1
    lda tmp1+1
    adc #0
    sta tmp1+1
    jmp begin_line
    
exit:
    rts

breakat: .res 1
controlchars: .res 1
screen_width: .res 1
align: .res 1
.endproc

;******************************************************************************
; Function name.......: util_print_num
; Purpose.............: Prints a byte value as decimal number
; Input...............: A = byte value
; Returns.............: Nothing
.proc util_print_num
    ; Store 8 bit input
    sta input
 
    ; Clear 16 bit output
    stz output
    stz output+1
 
    ; Number of input bits
    ldx #8
 
    ; Set decimal mode
    sed
 
loop:
    ; Rotate input, leftmost bit -> C
    asl input
   
    ; 16 bit addition. Value of C from previous operation is the actual input. Therefore C is not cleared.
    lda output
    adc output
    sta output
   
    lda output+1
    adc output+1
    sta output+1
 
    ; Decrease bit counter, continue if > 0
    dex
    bne loop
 
    ; Go back to binary mode
    cld
 
    ; Print
    ldy #0

    lda output+1                ; Third digit from right
    and #15
    beq :+
    clc
    adc #48
    jsr KERNAL_CHROUT
    iny

:   lda output                  ; Second digit from right
    lsr
    lsr
    lsr
    lsr
    cpy #0
    bne :+
    cmp #0
    beq :++
:   clc
    adc #48
    jsr KERNAL_CHROUT
    iny

:   lda output                  ; First digit from right
    and #15
    clc
    adc #48
    jmp KERNAL_CHROUT
 
input: .res 1   
output: .res 2
.endproc

;******************************************************************************
; Function name.......: util_input
; Purpose.............: Read string from keyboard
; Input...............: Nothing
; Returns.............: Y = string len
;
.proc util_input
    ldy #0
:   jsr KERNAL_CHRIN
    cmp #13
    beq exit
    sta util_input_buf,y
    iny
    bra :-

exit:
    rts
.endproc

;******************************************************************************
; Function name.......: util_stepdown
; Purpose.............: 
; Input...............: X = Start value, or 0 if no value
; Returns.............: X = Current value
.proc util_stepdown
    ; Set start value if X != 0
    cpx #0
    beq delete
    inx
    stx value
    bra printval

delete:
    ldx value

    lda #20
    jsr KERNAL_CHROUT
    
    cpx #10
    bcc printval
    lda #20
    jsr KERNAL_CHROUT

    cpx #100
    bcc printval
    lda #20
    jsr KERNAL_CHROUT

printval:
    lda value
    beq exit
    dea
    sta value
    jsr util_print_num

exit:
    ldx value
    rts

value: .res 1
.endproc

;******************************************************************************
; Function name.......: util_countdown
; Purpose.............: 
; Input...............: X = Countdown start value
; Returns.............: Nothing
.proc util_countdown
    jsr util_stepdown
    cpx #0
    beq exit
    jsr util_delay
    ldx #0
    bra util_countdown
exit:
    rts
.endproc

;******************************************************************************
;Function name.......: util_delay
;Purpose.............: Delay approx one second
;Input...............: Nothing
;Returns.............: Nothing
.proc util_delay
    lda #168
    sta counter
    lda #70
    sta counter+1
    lda #14
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


util_input_buf: .res 256
