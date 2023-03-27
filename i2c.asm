; Copyright 2023 Stefan Jakobsson
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
.equ I2C_SLAVE_ADDRESS         = 0x44

; I2C Pins
.equ I2C_CLK                   = 2
.equ I2C_SDA                   = 0

; I2C states
.equ STATE_CHECK_ADDRESS       = 0x01
.equ STATE_WAIT_SLAVE_ACK      = 0x02
.equ STATE_RECEIVE_BYTE        = 0x03
.equ STATE_WAIT_SLAVE_NACK     = 0x04
.equ STATE_TRANSMIT_BYTE       = 0x05
.equ STATE_PREP_MASTER_ACK     = 0x06
.equ STATE_WAIT_MASTER_ACK     = 0x07

; USICR - Control Register values
.equ I2C_IDLE                  = 0b10101000
.equ I2C_ACTIVE                = 0b11111000

; USISR - Status register values
.equ I2C_CLEAR_STARTFLAG       = 0b10000000
.equ I2C_COUNT_BYTE            = 0b01110000
.equ I2C_COUNT_BIT             = 0b01111110

;******************************************************************************
; Program segment
;******************************************************************************
.cseg

; Import commands
.include "commands.asm"

;******************************************************************************
; Function...: i2c_init
; Description: Intialize the controller as an I2C slave with the address
;              I2C_SLAVE_ADDRESS
; In.........: Nothing
; Out........: Nothing
i2c_init:
    ; Pin function when two-wire mode enabled according to the ATTiny861a 
    ; datasheet p. 134:
    ;
    ;   "When the output driver is enabled for the SDA pin, the output 
    ;   driver will force the line SDA low if the output of the USI Data 
    ;   Register or the corresponding bit in the PORTA register is zero. 
    ;   Otherwise, the SDA line will not be driven (i.e., it is released). 
    ;   When the SCL pin output driver is enabled the SCL line will be forced 
    ;   low if the corresponding bit in the PORTA register is zero, or by 
    ;   the start detector. Otherwise the SCL line will not be driven."
    ;
    ; Pin setup for two-wire mode based on the information in the datasheet:
    ;
    ;   Pin | DDR | PORT | Notes
    ;   ----+-----+------+------------------------------------------------------
    ;   SDA |  0  |  1   | DDR selects input or output, value set by USIDR
    ;   CLK |  1  |  1   | DDR=1 enables the USI module to hold the CLK

    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input
    sbi DDRB,I2C_CLK                ; CLK as output, don't touch!
    sbi PORTB,I2C_SDA               ; SDA = high, don't touch!
    sbi PORTB,I2C_CLK               ; CLK = high, don't touch!

    ; Setup USI control & status register
    ldi r16, I2C_IDLE
    out USICR,r16
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16
    
    ret

;******************************************************************************
; Function...: i2c_isr_start
; Description: Interrupt handler for start condition
; In.........: Nothing
; Out........: Nothing
i2c_isr_start:
    ; Set next state
    ldi i2c_state,STATE_CHECK_ADDRESS
    
    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input

i2c_isr_start2:
    ; Wait for CLK low (=start condition) or SDA high (=stop condition)
    in r16,PINB
    andi r16,(1<<I2C_CLK) | (1<<I2C_SDA)
    cpi r16, (1<<I2C_CLK) | (0<<I2C_SDA)
    breq i2c_isr_start2

    ; Start or stop?
    ldi r16,I2C_ACTIVE              ; Control value after start condition detected
    sbrc r16,I2C_SDA                ; Skip next line if SDA is low (start condition)
    ldi r16,I2C_IDLE                ; Control value after stop condition detected
    out USICR,r16                   ; Write to control register

    ; Configure, clear all flags and count in byte
    ldi r16,(I2C_CLEAR_STARTFLAG | I2C_COUNT_BYTE)
    out USISR,r16
    
    reti

;******************************************************************************
; Function...: i2c_isr_overflow
; Description: Interrupt handler for USI shift register overflow
; In.........: Nothing
; Out........: Nothing
i2c_isr_overflow:
; Check address
;--------------------------------------------
    cpi i2c_state,STATE_CHECK_ADDRESS
    brne i2c_wait_slave_ack

    in r16,USIDR                        ; Fetch address + R/W bit put on the bus by the master
    mov r17,r16                         ; Copy address + R/W bit to r17

    lsr r16                             ; Right shift value so we're left with just the address
    cpi r16,I2C_SLAVE_ADDRESS           ; Talking to me?
    brne i2c_restart                    ; Not matching address => restart

i2c_address_match:
    ; Store address + r/w bit, we're really only interested in the R/W bit
    mov i2c_ddr,r17

    ; Clear internal offset/command if master write
    sbrs i2c_ddr,0                      ; Skip next line if master read
    clr i2c_command                     ; It was master write => set internal offset to 0

    ; Fallthrough to send ack

; Send ACK
;--------------------------------------------
i2c_ack:
    ; Set next state
    ldi i2c_state,STATE_WAIT_SLAVE_ACK

    ; Clear Data Register bit 7 to send ACK
    cbi USIDR,7
    
    ; Configure to send one bit
    sbi DDRB,I2C_SDA                    ; SDA as output
    ldi r16,I2C_COUNT_BIT
    out USISR,r16                       ; Set Status Register to count 1 bit
    reti

; Wait for slave ack
;--------------------------------------------
i2c_wait_slave_ack:
    cpi i2c_state,STATE_WAIT_SLAVE_ACK
    brne i2c_receive_byte

    ; If master is reading, jump to transmit byte
    sbrc i2c_ddr,0
    rjmp i2c_transmit_byte2

    ; Next state is receive a byte
    ldi i2c_state,STATE_RECEIVE_BYTE
    
    ; Configure to read a byte
    cbi DDRB,I2C_SDA                    ; SDA as input
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16                       ; Set Status Register to count 1 byte
    reti

; Restart
;--------------------------------------------
i2c_restart:
    ; Ensure SDA is input
    cbi DDRB,I2C_SDA

    ; Configure to listen for start condition
    ldi r16, I2C_IDLE
    out USICR,r16
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16
    reti

; Receive data
;--------------------------------------------
i2c_receive_byte:
    cpi i2c_state,STATE_RECEIVE_BYTE
    brne i2c_transmit_byte

    ; Get byte from Data Register
    in r16,USIDR

    ; New offset/command?
    cpi i2c_command,0
    brne i2c_receive_byte3
    mov i2c_command,r16

    ; 0x82 Reboot
    cpi i2c_command,0x82
    brne i2c_ack
    CMD_REBOOT
    rjmp i2c_ack

i2c_receive_byte3:
    ; Offset 0x80 Transmit data packet
    cpi i2c_command,0x80
    brne i2c_ack
    CMD_RECEIVE_PACKET
    rjmp i2c_ack

; Transmit byte
;--------------------------------------------
i2c_transmit_byte:
    cpi i2c_state,STATE_TRANSMIT_BYTE
    brne i2c_prep_master_ack

i2c_transmit_byte2:
    ; Set next state
    ldi i2c_state,STATE_PREP_MASTER_ACK

    ; Load default value
    ldi r17,0                           ; Return 0 if offset unkown

    ; Offset 0x81 - Commit
    cpi i2c_command,0x81
    brne i2c_transmit_byte3
    CMD_COMMIT

i2c_transmit_byte3:
    out USIDR,r17                       ; Store value in Data Register

    ; Configure to send one byte
    sbi DDRB,I2C_SDA                    ; SDA as output
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16                       ; Set Status Register to count 1 byte
    reti

; Prepare to receive master ack
;--------------------------------------------
i2c_prep_master_ack:
    cpi i2c_state,STATE_PREP_MASTER_ACK
    brne i2c_wait_master_ack

    ; Set next state
    ldi i2c_state,STATE_WAIT_MASTER_ACK

    ; Configure to receive one bit
    cbi DDRB,I2C_SDA                    ; SDA as input
    ldi r16,I2C_COUNT_BIT
    out USISR,r16                       ; Set Status Register to count 1 bit
    reti

; Wait for master ack
;--------------------------------------------
i2c_wait_master_ack:    
    ; Get master ack/nack
    in r16,USIDR                        ; Read Data Register
    sbrc r16,0                          ; Master response as bit 0, skip next line if ACK
    rjmp i2c_restart                    ; It was a NACK, goto restart

    ; Master ACK, transmit next byte
    rjmp i2c_transmit_byte2