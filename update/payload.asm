payload:
    .incbin "resources/x16-smc.ino.bin"
payload_end:

payload_padding:
    .repeat 64
        .byte $ff
    .endrepeat
