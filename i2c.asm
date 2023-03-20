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
.equ I2C_CR_STOP            = 0b10101000
.equ I2C_CR_ACTIVE          = 0b11111000

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
    sbi PORTB,I2C_SDA               ; SDA = high, low only if SDA held low
    sbi PORTB,I2C_CLK               ; CLK = high, low only if CLK held low

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
    sbi PORTB,I2C_SDA               ; SDA = high to prevent driving the line

i2c_isr_start2:
    ; Wait for CLK low (=start condition) or SDA high (=stop condition)
    in r16,PINB
    andi r16,(1<<I2C_CLK) | (1<<I2C_SDA)
    cpi r16, (1<<I2C_CLK) | (0<<I2C_SDA)
    breq i2c_isr_start2

    ; Start or stop?
    ldi r16,I2C_CR_ACTIVE           ; Default control value, for start condition
    sbrc r16,I2C_SDA                ; Skip next line if SDA is low (start condition)
    ldi r16,I2C_CR_STOP             ; SDA was set, load control value for stop condition
    out USICR,r16                   ; Write to control register

    ; Clear start flag & configure to read byte 
    ldi r16,(I2C_CLEAR_STARTFLAG | I2C_COUNT_BYTE)
    out USISR,r16

    ; No pin setup change needed here, contiune to receive data
    
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
    
    ; Pin setup
    sbi DDRB,I2C_SDA                    ; SDA as output
    
    ; Configure to send one bit
    clr r16
    out USIDR,r16
    ldi r16,I2C_COUNT_BIT
    out USISR,r16
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
    
    ; Pin setup
    cbi DDRB,I2C_SDA                    ; SDA as input
    
    ; Configure to read a byte
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16
    reti

; Receive data
;--------------------------------------------
i2c_receive_byte:
    cpi r16,STATE_RECEIVE_BYTE
    brne i2c_transmit_byte

    ; Just ACK and discard
    rjmp i2c_ack

; Restart
;--------------------------------------------
i2c_restart:
    ; Restore pin setup
    cbi DDRB,I2C_SDA                    ; SDA as input
    sbi PORTB,I2C_SDA                   ; SDA = high

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

    ; Load return value in Data Register
    ldi r16,0x45                        ; Test value
    out USIDR,r16

    ; Pin setup
    sbi DDRB,I2C_SDA                    ; SDA as output

    ; Configure to send one byte
    ldi r16,I2C_COUNT_BYTE
    out USISR,r16
    reti

; Prepare to receive master ack
;--------------------------------------------
i2c_prep_master_ack:
    cpi r16,STATE_PREP_MASTER_ACK
    brne i2c_wait_master_ack

    ; Set next state
    ldi r16,STATE_WAIT_MASTER_ACK
    sts i2c_state,r16

    ; Pin setup
    cbi DDRB,I2C_SDA                    ; SDA as input

    ; Configure to receive one bit
    ldi r16,I2C_COUNT_BIT
    out USISR,r16
    reti

; Wait for master ack
;--------------------------------------------
i2c_wait_master_ack:    
    ; Get master ack/nack
    in r16,USIDR
    sbrc r16,7                          ; I2C is clocked in from MSB to LSB => Master response will be in bit 7
    rjmp i2c_restart                    ; It was a NACK, goto restart

    ; Master ACK => Transmit next
    rjmp i2c_transmit_byte2