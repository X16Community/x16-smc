; Copyright 2023-2024 Stefan Jakobsson
; 
; Redistribution and use in source and binary forms, with or without modification, 
; are permitted provided that the following conditions are met:
;
; 1. Redistributions of source code must retain the above copyright notice, this 
;    list of conditions and the following disclaimer.
;
; 2. Redistributions in binary form must reproduce the above copyright notice, 
;    this list of conditions and the following disclaimer in the documentation 
;    and/or other materials provided with the distribution.
;
;    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” 
;    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
;    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
;    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS 
;    BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
;    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
;    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
;    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
;    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
;    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
;    THE POSSIBILITY OF SUCH DAMAGE.

; Slave settings
.equ I2C_SLAVE_ADDRESS         = 0x42

; USICR - Control Register values
.equ I2C_IDLE                  = 0b10101000
.equ I2C_ACTIVE                = 0b11111000

; USISR - Status register values
.equ I2C_CLEAR_STARTFLAG       = 0b10000000
.equ I2C_COUNT_BYTE            = 0b01110000
.equ I2C_COUNT_BIT             = 0b01111110

;******************************************************************************
; Function...: i2c_main
; Description: I2C main function
; In.........: Nothing
; Out........: Nothing
i2c_main:
    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input
    sbi DDRB,I2C_CLK                ; CLK as output, don't touch!
    sbi PORTB,I2C_SDA               ; SDA = high, don't touch!
    sbi PORTB,I2C_CLK               ; CLK = high, don't touch!

    ; Fallthrough to i2c_reset

;******************************************************************************
; Function...: i2c_reset
; Description: USI control & status register setup, then waits for start/stop
;              condition
; In.........: Nothing
; Out........: Nothing
i2c_reset:
    ; USI control & status register setup
    ldi r16, I2C_IDLE
    out USICR,r16
    ldi r16, I2C_COUNT_BYTE
    out USISR,r16
    
    ; Wait for start/stop condition
    rcall i2c_poll
    
    ; i2c_poll is guaranteed not to return here (OVF not active)

;******************************************************************************
; Function...: i2c_start
; Description: Handles I2C start/stop conditions
; In.........: Nothing
; Out........: Nothing
i2c_start:
    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input

i2c_start2:
    ; Wait for CLK low (=start condition) or SDA high (=stop condition)
    in r16,PINB
    andi r16,(1<<I2C_CLK) | (1<<I2C_SDA)
    cpi r16, (1<<I2C_CLK) | (0<<I2C_SDA)
    breq i2c_start2

    sbrc r16, I2C_CLK
    rjmp i2c_reset                  ; Stop condition

    ; Fallthrough to i2c_check_address

;******************************************************************************
; Function...: i2c_check_address
; Description: Check address match
; In.........: Nothing
; Out........: Nothing
i2c_check_address:
    ; Configure to read one byte
    ldi r16, I2C_ACTIVE
    out USICR, r16
    ldi r16, (I2C_CLEAR_STARTFLAG | I2C_COUNT_BYTE)
    out USISR, r16

    ; Wait for I2C overflow condition
    rcall i2c_poll

    ; Get address + R/W bit
    in r16, USIDR
    mov r17, r16                        ; Copy address + R/W to r17

    ; Compare address
    lsr r16
    cpi r16, I2C_SLAVE_ADDRESS
    brne i2c_reset

    ; Store DDR bit
    mov i2c_ddr, r17

    ; Clear internal offset/command if master write
    sbrs i2c_ddr, 0
    clr i2c_command

    ; Fallthrough to i2c_send_ack

;******************************************************************************
; Function...: i2c_send_ack
; Description: Sends ack bit to master
; In.........: Nothing
; Out........: Nothing
i2c_send_ack:
    ; Clear Data Register bit 7 to send ACK
    cbi USIDR, 7
    
    ; Configure to send one bit
    sbi DDRB, I2C_SDA
    ldi r16, I2C_COUNT_BIT
    out USISR, r16

    ; Wait for I2C overflow condition
    rcall i2c_poll

    ; Select receive or transmit
    sbrc i2c_ddr, 0
    rjmp i2c_transmit

    ; Fallthrough to i2c_receive

;******************************************************************************
; Function...: i2c_receive
; Description: Receives one byte sent by the master
; In.........: Nothing
; Out........: Nothing
i2c_receive:    
    ; Configure to read a byte
    cbi DDRB, I2C_SDA
    ldi r16, I2C_COUNT_BYTE
    out USISR, r16

    ; Wait for I2C overflow condition
    rcall i2c_poll

    ; Get byte from Data Register
    in r16, USIDR

    ; Set command, if not previously set
    cpi i2c_command, 0
    brne i2c_receive2
    mov i2c_command, r16
    
    ; Reboot command
    cpi i2c_command, 0x82
    brne i2c_send_ack
    CMD_REBOOT                          ; Guaranteed not to return

i2c_receive2:
    cpi i2c_command, 0x80
    brne i2c_receive3
    CMD_RECEIVE_PACKET
    rjmp i2c_receive4

i2c_receive3:
    cpi i2c_command, 0x84
    brne i2c_send_ack
    CMD_SET_ADDR_PAGE

i2c_receive4:
    clr i2c_command
    rjmp i2c_send_ack


;******************************************************************************
; Function...: i2c_transmit
; Description: Transmits one byte to the master
; In.........: Nothing
; Out........: Nothing
i2c_transmit:
    ; Load default value
    ldi r17, 0x00

    ; Command 0x81 - Commit
    cpi i2c_command,0x81
    brne i2c_transmit2
    CMD_COMMIT
    rjmp i2c_transmit3

i2c_transmit2:
    ; Command 0x83 - Get version
    cpi i2c_command, 0x83
    brne i2c_transmit3
    CMD_GET_VERSION

i2c_transmit3:
    ; Command 0x85 - Read flash
    cpi i2c_command, 0x85
    brne i2c_transmit4
    CMD_READ_FLASH

i2c_transmit4:
    ; Store value in data register
    out USIDR, r17

    ; Configure to send one byte
    sbi DDRB, I2C_SDA
    ldi r16, I2C_COUNT_BYTE
    out USISR, r16

    ; Clear command
    clr i2c_command
    
    ; Wait for I2C overflow condition
    rcall i2c_poll

    ; Configure to read master response (ACK/NACK)
    cbi DDRB, I2C_SDA
    ldi r16, I2C_COUNT_BIT
    out USISR, r16
    
    ; Wait for I2C overflow condition
    rcall i2c_poll

    ; Get master response (ACK/NACK)
    sbic USIDR, 0
    rjmp i2c_reset                      ; NACK
    rjmp i2c_transmit                   ; ACK

;******************************************************************************
; Function...: i2c_poll
; Description: Polls I2C status flags until start/stop or overflow occurs
; In.........: Nothing
; Out........: Nothing
i2c_poll:
    ; Overflow flag
    sbic USISR, 6
    ret

    ; Start flag
    sbis USISR, 7
    rjmp i2c_poll

i2c_poll2:
    pop r16
    pop r16
    rjmp i2c_start
