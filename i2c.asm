; Slave settings
.equ I2C_SLAVE_ADDRESS         = 0x44
.equ I2C_VERSION               = 0x01

; Pins
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

; USICR values
.equ I2C_CR_IDLE               = 0b10101000
.equ I2C_CR_ACTIVE             = 0b11111000

; USISR values
.equ I2C_CLEAR_STARTFLAG       = 0b10000000
.equ I2C_COUNT_BYTE            = 0b01110000
.equ I2C_COUNT_BIT             = 0b01111110

;******************************************************************************
; Data segment
;******************************************************************************
.dseg

i2c_state: .byte 1
i2c_offset: .byte 1
i2c_ddr: .byte 1
i2c_echo_buf: .byte 1


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
    ;   SDA |  0  |  1   | DDR=0 => Pin not driven. DDR=1 => Pin driven by USIDR
    ;       |     |      | or PORT=low
    ;   CLK |  1  |  1   | PORT=1 => Pin not driven. PORT=0 => Held low; The
    ;       |     |      | USI module will never hold the line if DDR=0

    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input
    sbi DDRB,I2C_CLK                ; CLK as output, don't touch!
    sbi PORTB,I2C_SDA               ; SDA = high, don't touch!
    sbi PORTB,I2C_CLK               ; CLK = high, don't touch!

    ; Setup USI control & status register
    ldi r16, I2C_CR_IDLE
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
    ldi r16,STATE_CHECK_ADDRESS
    sts i2c_state,r16
    
    ; Pin setup
    cbi DDRB,I2C_SDA                ; SDA as input

i2c_isr_start2:
    ; Wait for CLK low (=start condition) or SDA high (=stop condition)
    in r16,PINB
    andi r16,(1<<I2C_CLK) | (1<<I2C_SDA)
    cpi r16, (1<<I2C_CLK) | (0<<I2C_SDA)
    breq i2c_isr_start2

    ; Start or stop?
    ldi r16,I2C_CR_ACTIVE           ; Control value after start condition detected
    sbrc r16,I2C_SDA                ; Skip next line if SDA is low (start condition)
    ldi r16,I2C_CR_IDLE             ; Control value after stop condition detected
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
    ; Get current state
    lds r16,i2c_state

; Check address
;--------------------------------------------
cpi r16,STATE_CHECK_ADDRESS
    brne i2c_wait_slave_ack

    in r16,USIDR                        ; Fetch address + R/W bit put on the bus by the master
    mov r17,r16                         ; Copy address + R/W bit to r17

    lsr r16                             ; Right shift value so we're left with just the address
    cpi r16,I2C_SLAVE_ADDRESS           ; Talking to us?
    brne i2c_restart                    ; Not matching address => restart

i2c_address_match:
    ; Store address + r/w bit, we're really only interested in the R/W bit
    sts i2c_ddr,r17

    ; Clear internal offset if master write
    clr r16
    sbrs r17,0                          ; Skip next line if master read
    sts i2c_offset,r16                  ; It was master write => set internal offset to 0

    ; Fallthrough to send ack

; Send ACK
;--------------------------------------------
i2c_ack:
    ; Set next state
    ldi r16,STATE_WAIT_SLAVE_ACK
    sts i2c_state,r16

    ; Clear Data Register to send ACK
    clr r16
    out USIDR,r16
    
    ; Configure to send one bit
    sbi DDRB,I2C_SDA                    ; SDA as output
    clr r16
    out USIDR,r16                       ; Clear Data Register (ACK=0)
    ldi r16,I2C_COUNT_BIT
    out USISR,r16                       ; Set Status Register to count 1 bit
    reti

; Wait for slave ack
;--------------------------------------------
i2c_wait_slave_ack:
    cpi r16,STATE_WAIT_SLAVE_ACK
    brne i2c_receive_byte

    ; If master is reading, jump to transmit byte
    lds r16,i2c_ddr
    sbrc r16,0
    rjmp i2c_transmit_byte2

    ; Next state is receive a byte
    ldi r16,STATE_RECEIVE_BYTE
    sts i2c_state,r16
    
    ; Configure to read a byte
    cbi DDRB,I2C_SDA                    ; SDA as input
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16                       ; Set Status Register to count 1 byte
    reti

; Receive data
;--------------------------------------------
i2c_receive_byte:
    cpi r16,STATE_RECEIVE_BYTE
    brne i2c_transmit_byte

    ; Get byte from Data Register
    in r16,USIDR

    ; New offset?
    lds r17,i2c_offset
    cpi r17,0
    brne i2c_receive_byte2
    
    ; Yes, set new offset
    sts i2c_offset,r16
    rjmp i2c_ack

i2c_receive_byte2:
    ; No, it's data
    ; TODO: Not yet implemented
    rjmp i2c_ack

; Restart
;--------------------------------------------
i2c_restart:
    ; Ensure SDA is input
    cbi DDRB,I2C_SDA

    ; Configure to listen for start condition
    ldi r16, I2C_CR_IDLE
    out USICR,r16
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16
    reti

; Transmit byte
;--------------------------------------------
i2c_transmit_byte:
    cpi r16,STATE_TRANSMIT_BYTE
    brne i2c_prep_master_ack

i2c_transmit_byte2:
    ; Set next state
    ldi r16,STATE_PREP_MASTER_ACK
    sts i2c_state,r16

    ; Get offset
    lds r16, i2c_offset

    ; Load default value
    ldi r17,0                           ; Return 0 if offset unkown

    ; Offset 0x80 - API Version
    cpi r16,0x80
    brne i2c_transmit_byte3
    ldi r17,I2C_VERSION

    ; Offset 0x82 - Commit
    ; TODO: Not yet implemented

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
    cpi r16,STATE_PREP_MASTER_ACK
    brne i2c_wait_master_ack

    ; Set next state
    ldi r16,STATE_WAIT_MASTER_ACK
    sts i2c_state,r16

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