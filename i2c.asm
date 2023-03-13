.equ I2C_SLAVE_ADDRESS = 0x42

.equ I2C_CLK           = 2
.equ I2C_SDA           = 0

.equ I2C_IDLE          = 0x00
.equ I2C_CHECK_ADDRESS = 0x01
.equ I2C_ADDRESS_ACK   = 0x02
.equ I2C_ADDRESS_NACK  = 0x03
.equ I2C_RECEIVE       = 0x04
.equ I2C_RECEIVE_ACK   = 0x05
.equ I2C_RECEIVE_NACK  = 0x06
.equ I2C_SEND          = 0x07
.equ I2C_SEND_WAIT     = 0x08

.dseg
i2c_state: .byte 1
i2c_addr: .byte 1

.cseg

.macro i2c_sda_in
    cbi DDRB,I2C_SDA
.endm

.macro i2c_sda_out
    sbi DDRB,I2C_SDA
.endm

;******************************************************************************
; Function...: i2c_init
; Description: Intialize the controller as an I2C slave with the address
;              I2C_SLAVE_ADDRESS
; In.........: Nothing
; Out........: Nothing
i2c_init:
    ldi r16,0xff
    out DDRA,r16
    
    clr r16
    sts i2c_state,r16
    
    ; Setup CLK and SDA pins        <<< This code might very well be incorrect...
    sbi PORTB,I2C_CLK               ; clk = high
    sbi PORTB,I2C_SDA               ; sda = high
    sbi DDRB,I2C_CLK                ; clk = output
    cbi DDRB,I2C_SDA                ; clk = input

    ; Setup USI control register
    ldi r16, 0b00101000             ; Set Two-Wire mode + Timer/Counter0 Compare Match
    out USICR,r16

    ; Clear status register flags
    ldi r16, 0xf0
    out USISR,r16
    
    ret

;******************************************************************************
; Function...: i2c_listen
; Description: Listen for incoming I2C transmissions
; In.........: Nothing
; Out........: Nothing
i2c_listen:
    in r16,USISR                    ; Get USI Status register
    
    ldi r17,i2c_state               ; Load r17 with i2c_state
    cpi r17,I2C_IDLE                ; Check if state is idle
    brne i2c_listen2                ; If not jump to i2c_listen2

    sbrc r16,USISIF                 ; Skip next line if Start Condition Flag is cleared
    rjmp i2c_start                  ; Start Condition Flag is set, jump to i2c_start
    rjmp i2c_listen                 ; Start over

i2c_listen2:
    sbrc r16, USIOIF                ; Skip next line if Counter Overflow Flag is cleared
    rjmp i2c_overflow               ; Counter Overflow Flag is set, jump to i2c_overflow
    rjmp i2c_listen                 ; Start over

i2c_start:
    i2c_sda_in                      ; Set SDA as input

i2c_start_wait:
    sbis PINB,I2C_SDA               ; Skip next line if SDA is high
    rjmp i2c_start_wait             ; SDA is low, keep waiting...
    sbic PINB,I2C_CLK               ; Skip next line if CLK is high
    rjmp i2c_stop                   ; CLK is low, a stop condition

    ldi r16,I2C_CHECK_ADDRESS       ; We have a start condition, set state to check address
    sts i2c_state,r16

    ldi r16,0xf0                    ; Clear Start Condition and Overflow Flag by writing a 1
    out USISR,r16

    rjmp i2c_listen                 ; Start over

i2c_stop:
    ldi r16,I2C_IDLE                ; Set state = idle
    sts i2c_state,r16
    i2c_sda_in                      ; Set SDA as input
    ldi r16,0xf0                    ; Clear Start Condition and Overflow Flag
    out USISR,r16
    rjmp i2c_listen                 ; Start over

i2c_overflow:
    lds r16,i2c_state               ; Load r16 with state
    cpi r16,I2C_CHECK_ADDRESS       ; Is it check address?
    brne i2c_overflow2              ; No...

    ldi r16,I2C_ADDRESS_ACK         ; Yes. Next state is Address ACK
    sts i2c_state,r16
    
    in r17,USIDR                    ; Load r17 from Data Register
    out PORTA,r17                   ; DEBUG: Show value on PORTA            <<<< I can't get this to display the device address that should be received from the master

    sts i2c_addr,r17                ; Store address in RAM location i2c_addr

    clr r16                         ; Clear r16
    out USIDR,r16                   ; Put 0 = ACK in the Data Register

    i2c_sda_out                     ; Set SDA as output

    ldi r16,0xf0 | 0x0e             ; Clear Start Condition Flag and Overflow Flag + Set overflow counter to 0x0e => Send 1 bit
    out USISR,r16
    
    rjmp i2c_listen                 ; Start over
    
i2c_overflow2:
    cpi r16,I2C_ADDRESS_ACK         ; Is state = Address ACK?
    brne i2c_overflow3              ; No...
    
    lds r16,i2c_addr                ; Load r16 from RAM location i2c_addr
    sbrs r16,0                      ; Skip next line if bit 0 is set = Master writes to Slave
    rjmp i2c_overflow2a             ; Bit 0 was clear = Master reads from slave, jump to overflow2a
    
    ldi r16,I2C_RECEIVE             ; Bit 1 was set = Master writes to slave
    sts i2c_state,r16               ; Set that state

    i2c_sda_in                      ; Set SDA as input

    ldi r16,0xf0                    ; Clear Start Condition and Overflow Flag
    out USISR,r16

    rjmp i2c_listen                 ; Start over

i2c_overflow2a:
    ldi r16,I2C_SEND                ; Set state
    sts i2c_state,r16

    i2c_sda_out                     ; SDA as output

    ldi r16,0x40                    ; Clear overflow flag
    out USIDR,r16

    ldi r16,0xf0                    ; Clear start condition and overflow flag
    out USISR,r16

    rjmp i2c_listen                 ; Start over

i2c_overflow3:
    cpi r16,I2C_RECEIVE
    brne i2c_overflow4

    ldi r16,I2C_RECEIVE_ACK
    sts i2c_state,r16

    i2c_sda_out

    clr r16
    out USIDR,r16

    ldi r16,0xf0+0x0e
    out USISR,r16

    rjmp i2c_listen

i2c_overflow4:
    cpi r16,I2C_RECEIVE_ACK
    brne i2c_overflow5

    ldi r16, I2C_RECEIVE
    sts i2c_state,r16

    i2c_sda_in

    ldi r16,0xf0
    out USISR,r16
    rjmp i2c_listen

i2c_overflow5:
    cpi r16,I2C_SEND
    brne i2c_overflow6

    ldi r16,I2C_SEND_WAIT
    sts i2c_state,r16

    i2c_sda_in

    ldi r16, 0xf0+0x0e
    out USISR,r16
    
    rjmp i2c_listen

i2c_overflow6:
    cpi r16,I2C_SEND_WAIT
    brne i2c_overflow7

    ldi r16,I2C_SEND
    sts i2c_state,r16

    i2c_sda_in

    clr r16
    sts i2c_state,r16

    ldi r16,0xf0
    out USISR,r16

    rjmp i2c_listen

i2c_overflow7:
    i2c_sda_in
    ldi r16,0xf0
    out USISR,r16
    rjmp i2c_listen
