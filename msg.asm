;******************************************************************************
;Function name.......: msg_print
;Purpose.............: Prints a null terminated string
;Input...............: X = Pointer to string, address low
;                      Y = Pointer to string, address high
;Returns.............: Nothing
.proc msg_print
    stx tmp1
    sty tmp1+1
    ldy #0
:   lda (tmp1),y
    beq :+
    jsr KERNAL_CHROUT
    iny
    bra :-
:   rts
.endproc