; Slave address
.equ I2C_SLAVE_ADDRESS      = 0x44

; Pins
.equ I2C_CLK                = 2
.equ I2C_SDA                = 0

; I2C states
.equ STATE_CHECK_ADDRESS    = 0x01
.equ STATE_WAIT_SLAVE_ACK   = 0x02
.equ STATE_RECEIVE_BYTE     = 0x03
.equ STATE_WAIT_SLAVE_NACK  = 0x04
.equ STATE_TRANSMIT_BYTE    = 0x05
.equ STATE_PREP_MASTER_ACK  = 0x06
.equ STATE_WAIT_MASTER_ACK  = 0x07

.equ I2C_RECEIVE            = 0x00
.equ I2C_SEND               = 0x01

; USICR values
.equ I2C_CR_IDLE            = 0b10101000
.equ I2C_CR_STOP            = 0b10111000
.equ I2C_CR_ACTIVE          = 0b11101000

; USISR values
.equ I2C_SR_BYTE            = 0xf0
.equ I2C_SR_BIT             = 0xf0 | 0x0e

;******************************************************************************
; Data segment
;******************************************************************************
.dseg

i2c_state: .byte 1
i2c_offset: .byte 1
i2c_ddr: .byte 1
i2c_echo_buf: .byte 1

;******************************************************************************
; Marcros
;******************************************************************************
.macro I2C_SDA_INPUT
    cbi DDRB,I2C_SDA
.endm

.macro I2C_SDA_OUTPUT
    sbi DDRB,I2C_SDA
.endm

;******************************************************************************
; Program segment
;******************************************************************************
.cseg

;******************************************************************************
; Function...: i2c_init
; Description: Intialize the controller as an I2C slave with the address
;              I2C_SLAVE_ADDRESS
; In.........: Nothing
; Out........: Nothing
i2c_init:
    ; CLK & SDA = low
    cbi PORTB,I2C_CLK
    cbi PORTB,I2C_SDA

    ; CLK & SDA as input
    cbi DDRB,I2C_CLK
    cbi DDRB,I2C_SDA

    ; Setup USI control & status register
    ldi r16, I2C_CR_IDLE
    out USICR,r16
    ldi r16,I2C_SR_BYTE
    out USISR,r16
    
    ret

;******************************************************************************
; Function...: i2c_isr_start
; Description: Interrupt handler for start condition
; In.........: Nothing
; Out........: Nothing
i2c_isr_start:
    ; Set next state
    ldi r16,STATE_CHECK_ADDRESS
    sts i2c_state,r16
    
    ; SDA as input
    cbi DDRB,I2C_SDA

i2c_isr_start2:
    ; Wait for CLK low (=start condition) or SDA high (=stop condition)
    in r16,PINB
    andi r16,(1<<I2C_CLK) | (1<<I2C_SDA)
    cpi r16, (1<<I2C_CLK) | (0<<I2C_SDA)
    breq i2c_isr_start2

    ; Start or stop?
    ldi r16,I2C_CR_ACTIVE       ; Default control value, for start condition
    sbrc r16,I2C_SDA            ; Skip next line if SDA is low (start condition)
    ldi r16,I2C_CR_STOP         ; SDA was set, load control value for stop condition
    out USICR,r16               ; Write to control register

    ; Clear flags and counter
    ldi r16,I2C_SR_BYTE
    out USISR,r16
    
    reti

;******************************************************************************
; Function...: i2c_isr_overflow
; Description: Interrupt handler for USI shift register overflow
; In.........: Nothing
; Out........: Nothing
i2c_isr_overflow:
; Get current state
;--------------------------------------------
    lds r16,i2c_state

    cpi r16,STATE_CHECK_ADDRESS
    brne i2c_receive_byte

; Check address
;--------------------------------------------
    in r16,USIDR                        ; Read address + R/W bit put on the bus by the master
    mov r17,r16

    lsr r16                             ; Right shift value and discard R/W bit
    cpi r16,I2C_SLAVE_ADDRESS           ; Compare address
    breq i2c_address_match
    rjmp i2c_restart                    ; Not matching address => restart

i2c_address_match:
    sbi PORTA,0
    ; Store data direction
    sts i2c_ddr,r17

    ; Set next state
    ldi r16,STATE_WAIT_SLAVE_ACK
    sts i2c_state,r16

    ; Clear internal offset if master write
    clr r16
    sbrs r17,0                          ; Skip next line if master read
    sts i2c_offset,r16                  ; It was master write => set internal offset to 0

    ; Send ACK
i2c_send_bit:
    out USIDR,r16                       ; r16 was cleared above
    I2C_SDA_OUTPUT
    sbi PORTB,I2C_SDA
    ldi r16,I2C_SR_BIT
    out USISR,r16
    reti

; Receive data
;--------------------------------------------
i2c_receive_byte:
    cpi r16,STATE_RECEIVE_BYTE
    brne i2c_wait_slave_ack
    
    sbi PORTA,1
    ; Next state
    ldi r16,STATE_WAIT_SLAVE_ACK
    sts i2c_state,r16

    ; Get data
    in r16,USIDR

    ; Is data new offset?
    lds r17,i2c_offset
    cpi r17,0
    breq i2c_set_offset                 ; Yes it is
    
    ; Handle echo offset
    lds r17,i2c_offset
    cpi r17,0x80
    brne i2c_send_ack
    sts i2c_echo_buf,r16

i2c_send_ack:
    clr r16
    rjmp i2c_send_bit

i2c_set_offset:
    sts i2c_offset,r16
    rjmp i2c_send_ack

; Wait for slave ack
;--------------------------------------------
i2c_wait_slave_ack:
    cpi r16,STATE_WAIT_SLAVE_ACK
    brne i2c_transmit_byte

    sbi PORTA,2
    ; Set SDA as input
    I2C_SDA_INPUT
    cbi PORTB,I2C_SDA

    ; If master read, jump to transmit byte
    lds r16,i2c_ddr
    sbrc r16,0
    rjmp i2c_transmit_byte2

    ; It was master write, set next state
    ldi r16,STATE_RECEIVE_BYTE
    sts i2c_state,r16
    
    ; Configure to read a byte
    ldi r16,I2C_SR_BYTE
    out USISR,r16
    reti

; Transmit byte
;--------------------------------------------
i2c_transmit_byte:
    cpi r16,STATE_TRANSMIT_BYTE
    brne i2c_prep_master_ack

    ; Set next state
    sbi PORTA,3
i2c_transmit_byte2:
    ldi r16,STATE_PREP_MASTER_ACK
    sts i2c_state,r16

    ; Check offset
    ldi r17,0                           ; Default return value

    lds r16,i2c_offset
    cpi r16,0x80
    brne i2c_notexist
    lds r17,i2c_echo_buf

i2c_notexist:
    ; Put value in buffer
    out USIDR,r17

    ; SDA as output
    I2C_SDA_OUTPUT

    ; Set SDA = high
    sbi PORTB,I2C_SDA

    ; Configure to send one byte
    ldi r16,0b01110000
    out USISR,r16
    reti

; Prepare to receive master ack
;--------------------------------------------
i2c_prep_master_ack:
    cpi r16,STATE_PREP_MASTER_ACK
    brne i2c_wait_master_ack

    sbi PORTA,4
    ; SDA as input & low
    cbi PORTB,I2C_SDA
    I2C_SDA_INPUT
    
    ; Set next state
    ldi r16,STATE_WAIT_MASTER_ACK
    sts i2c_state,r16

    ; Configure to receive one bit
    ldi r16,I2C_SR_BIT
    out USISR,r16
    reti

; Wait for master ack
;--------------------------------------------
i2c_wait_master_ack:
    cpi r16,STATE_WAIT_MASTER_ACK
    brne i2c_restart

    sbi PORTA,5
    in r16,USIDR
    sbrc r16,0
    rjmp i2c_restart

    ldi r16,STATE_TRANSMIT_BYTE
    sts i2c_state,r16
    ldi r16,I2C_SR_BYTE
    out USISR,r16
    reti

; Restart
;--------------------------------------------
i2c_restart:
    sbi PORTA,6
    I2C_SDA_INPUT
    cbi PORTB,I2C_SDA

    ldi r16, I2C_CR_IDLE
    out USICR,r16
    ldi r16,I2C_SR_BYTE
    out USISR,r16
    reti