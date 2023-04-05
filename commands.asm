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


;******************************************************************************
; These functions are implemented as macros so that they can be inlined
; in the I2C ISR functions. By doing this we can save a pair of rcall/ret for
; each function, saving some flash memory space (4 bytes/function).
;******************************************************************************

;******************************************************************************
; Function...: CMD_RECEIVE_PACKET
; Description: Receives a packet byte and stores it into a RAM buffer. This
;              function keeps track of and updates the pointer to the
;              head of the RAM buffer, the current packet size and the
;              checksum of the current packet.
; In.........: r16 byte value
; Out........: Nothing
.macro CMD_RECEIVE_PACKET
    ; Move buffer head into Y register
    movw YH:YL, packet_headH:packet_headL

    ; Check packet size
    cpi packet_size,10
    brsh cmd_receive_byte_exit                          ; 11th byte or more - packet overflow, an error state
    cpi packet_size,9
    breq cmd_receive_byte3                              ; 10th byte - packet overflow, an error state
    cpi packet_size,8                                                                                
    breq cmd_receive_byte2                              ; 9th byte is the checksum
                                                            
    ; Store byte in buffer
    st Y+,r16                                           ; Store byte
    movw packet_headH:packet_headL, YH:YL               ; Update head pointer

    ; Update checksum
cmd_receive_byte2:
    add checksum,r16                                    ; Add without carry

    ; Increment packet size
cmd_receive_byte3:
    inc packet_size

cmd_receive_byte_exit:
.endmacro
 
;******************************************************************************
; Function...: CMD_COMMIT
; Description: Confirm and request that the current packet is written to
;              flash memory
; In.........: Nothing
; Out........: r17 Response value:
;                  0x00 = OK, packet stored in RAM buffer
;                  0x01 = OK, buffer written to flash memory
;                  0x02 = Invalid packet size, not equal to 9
;                  0x03 = Checksum error
;                  0x04 = Reserved for future use
;                  0x05 = Target address overflows into bootloader section (0x1E00..)
.macro CMD_COMMIT
    ; Move target address into Z
    movw ZH:ZL,target_addrH:target_addrL

    ; Verify packet
    ldi r17,2
    cpi packet_size,9                                   ; Packet size == 9?
    brne cmd_commit_err
    ldi r17,3
    cpi checksum,0                                      ; Checksum == 0?
    brne cmd_commit_err
    ldi r17,5
    mov r16,target_addrH
    cpi r16,0x1e                                        ; Target address >= 0x1E00 (in bootloader area)?
    brsh cmd_commit_err

    ldi r17,0                                           ; Load default OK return value

    ; Do we have a full page?
    inc packet_count
    cpi packet_count,16
    breq cmd_commit_write
    cpi packet_count,8
    breq cmd_commit_fullpage
    rjmp cmd_commit_ok

cmd_commit_write:
    ; Write buffer to flash mem
    rcall flash_write_buf

    ; Update target addr
    adiw ZH:ZL,32
    adiw ZH:ZL,32
    movw target_addrH:target_addrL, ZH:ZL

    ; Reset packet count
    ldi packet_count,8                                  ; Restart from 8, as 0..7 reserved for the first flash mem page
    
    ; Load return value
    ldi r17,1
 
cmd_commit_fullpage:
    ; Reset buffer pointer
    ldi YL,low(flash_buf)
    ldi YH,high(flash_buf)
    movw packet_headH:packet_headL, YH:YL

    ; Fill buffer with 0xFF
    mov r19,r17
    ldi r16,0xff
    ldi r17,0xff
    ldi r18,PAGE_SIZE/2
    rcall flash_fillbuffer
    mov r17,r19

cmd_commit_ok:
    ; Move buffer tail to head
    movw packet_tailH:packet_tailL, packet_headH:packet_headL
    rjmp cmd_commit_exit

cmd_commit_err:
    ; Rewind buffer head to tail
    movw packet_headH:packet_headL,packet_tailH:packet_tailL

cmd_commit_exit:
    clr packet_size
    clr checksum

.endmacro

;******************************************************************************
; Function...: CMD_REBOOT
; Description: Writes flash_zp_buf to flash memory, setting up the first
;              page of flash memory and then reboots the ATTiny using
;              the Watchdog Timer
; In.........: Nothing
; Out........: Nothing
.macro CMD_REBOOT
    ; Disable interrupts
    cli

    ; Disable I2C
    ldi r16,I2C_CLEAR_STARTFLAG + I2C_COUNT_BYTE
    out USISR,r16
    
    clr r16
    out USICR,r16

    cbi DDRB,I2C_CLK
    cbi DDRB,I2C_SDA
    cbi PORTB,I2C_CLK
    cbi PORTB,I2C_SDA
    
    ; Write possible data in current buffer to flash
    movw ZH:ZL,target_addrH:target_addrL
    cpi packet_count,9
    brlo cmd_reboot3
    rcall flash_write_buf

cmd_reboot2:
    adiw ZH:ZL,32
    adiw ZH:ZL,32

cmd_reboot3:
    ; Erase rest of flash mem
    cpi ZH,0x1e
    brsh cmd_reboot4
    ldi r17, (1<<PGERS) + (1<<SPMEN)
    rcall flash_spm
    rjmp cmd_reboot2

    ; Write zero page to flash
cmd_reboot4:
    clr ZL
    clr ZH
    ldi YL,low(flash_zp_buf)
    ldi YH,high(flash_zp_buf)
    rcall flash_write

cmd_reboot5:
    ; TODO: Can we do reset instead?
    rjmp cmd_reboot5
.endmacro