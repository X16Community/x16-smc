.ifndef target_board
    .error "target board not set"

.elseif target_board=1
payload:
    .incbin "resources/x16-smc-default.bin"
payload_end:

.elseif target_board=2
payload:
    .incbin "resources/x16-smc-community.bin"
payload_end:

.else
    .error "unknown target board (1=default, 2=community)"
.endif

payload_padding:
    .repeat 64
        .byte $ff
    .endrepeat
