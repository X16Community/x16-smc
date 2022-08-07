
I2C_READ_BYTE := $FEC6
I2C_WRITE_BYTE := $FEC9

.export _cx16_k_i2c_readbyte
.export _cx16_k_i2c_writebyte
.import popa

; Kernal API: .X = I2C device ID
;             .Y = offset
; Returns .XY unaltered, .A = reply C=0 on success
; Returns .Xy unaltered, .A = $EE C=1 on error
;
; C API:
; int __fastcall__ cx16_k_i2c_readbyte(u8 offset, u8 device);
; returns -1 if error, else value read.
.proc _cx16_k_i2c_readbyte: near
  tax       ; device ID
  jsr popa
  tay       ; offset
  jsr I2C_READ_BYTE
  ldx #0    ; drop X value in favor of (int).A
  bcc no_error
  ; if error, return -1
  lda #$FF
  ldx #$FF
no_error:
  rts
.endproc

; Kernal API: .X = I2C device ID
;             .Y = offset
; Returns .XY unaltered, .A = <clobbered>
;         C: 0=success, 1=error
;
; C API:
; char __fastcall__ xc16_k_i2c_writebyte(u8 value, u8 offset, u8 device)
; returns 0=success, 1=fail
.proc _cx16_k_i2c_writebyte: near
  tax ; device
  jsr popa
  pha ; offset
  jsr popa ; value
  ply ; offset
  jsr I2C_WRITE_BYTE
  ldx #0
  lda #0
  rol ; put C into A's LSB.
  eor #1 ; reverse meaning of the bit so 0=fail, 1=success
  rts
.endproc
