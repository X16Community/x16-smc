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


;******************************************************************************
; Function...: CMD_RECEIVE_PACKET
; Description: Receives a packet byte and stores it into a RAM buffer. This
;              function keeps track of and updates the pointer to the
;              head of the RAM buffer, the current packet size and the
;              checksum of the current packet.
; In.........: r16 byte value
; Out........: Nothing
.macro CMD_RECEIVE_PACKET
    ; Check packet size
    cpi packet_size,10
    brsh cmd_receive_byte_exit                          ; 11th byte or more - packet overflow, an error state
    cpi packet_size,9
    breq cmd_receive_byte3                              ; 10th byte - packet overflow, an error state
    cpi packet_size,8                                                                                
    breq cmd_receive_byte2                              ; 9th byte is the checksum
                                                            
    ; Store byte in buffer
    st Y+,r16                                           ; Store byte in buffer and increment Y = packet_addressH:packet_addressL

    ; Update checksum
cmd_receive_byte2:
    add checksum,r16                                    ; Add without carry

    ; Increment packet size
cmd_receive_byte3:
    inc packet_size

cmd_receive_byte_exit:
.endm
 
;******************************************************************************
; Function...: CMD_COMMIT
; Description: Confirm and request that the current packet is written to
;              flash memory
; In.........: Nothing
; Out........: r17 Response value:
;                  0x01 = OK, buffer written to flash memory
;                  0x02 = Invalid packet size, not equal to 9
;                  0x03 = Checksum error
;                  0x04 = Reserved
;                  0x05 = Target address overflows into bootloader section (0x1E00..)
.macro CMD_COMMIT
    ; Verify packet
    ldi r17, 2
    cpi packet_size, 9                           ; Packet size == 9?
    brne cmd_commit_err
    
    ldi r17,3
    cpi checksum, 0                              ; Checksum == 0?
    brne cmd_commit_err
    
    ldi r17, 5
    cpi target_addrH,0x1e                        ; Target address >= 0x1E00 (in bootloader area)?
    brsh cmd_commit_err

    ; Do we have a full page?
    inc packet_count
    cpi packet_count, 8
    brne cmd_commit_ok

cmd_commit_write:
    ; Erase if first page
    mov r16, chip_erased
    cpi r16, 0
    brne cmd_commit_write2
    rcall flash_erase
    inc chip_erased

    ; Move reset vector to EE_RDY
    lds r26, flash_buf
    lds r27, flash_buf+1
    sbiw r27:r26, ERDYaddr
    andi r27, 0b11001111
    ori r27, 0b11000000
    sts flash_buf+(ERDYaddr*2),r26
    sts flash_buf+(ERDYaddr*2)+1,r27

    ; Replace reset vector with rjmp 0xf00 (= 0xceff)
    ldi r16, 0xff
    ldi r17, 0xce
    sts flash_buf, r16
    sts flash_buf+1, r17

cmd_commit_write2:
    ; Write buffer to flash mem
    rcall flash_write

    ; Update target addr
    adiw target_addrH:target_addrL,32
    adiw target_addrH:target_addrL,32
 
    ; Reset buffer pointer
    ldi packet_headL, low(flash_buf)
    ldi packet_headH, high(flash_buf)

    ; Reset packet count
    clr packet_count

cmd_commit_ok:
    ; Load return value
    ldi r17,1
    
    ; Move buffer tail to head
    movw packet_tailH:packet_tailL, packet_headH:packet_headL
    rjmp cmd_commit_exit

cmd_commit_err:
    ; Rewind buffer head to tail
    movw packet_headH:packet_headL,packet_tailH:packet_tailL

cmd_commit_exit:
    clr packet_size
    clr checksum

.endm

;******************************************************************************
; Function...: CMD_REBOOT
; Description: Writes remainging data in the buffer to flash and resets the SMC
; In.........: Nothing
; Out........: Nothing
.macro CMD_REBOOT
    ; Write possible data in current buffer to flash
    cpi packet_count, 0
    breq cmd_reboot2
    rcall flash_write

cmd_reboot2:
    ; Set watchdog reset after 1 second
    ldi r16,(1<<WDE) | (0<<WDIE) | (0<<WDP3) | (1<<WDP2) | (1<<WDP1) | (0<<WDP0)
    out WDTCR,r16

cmd_reboot_3:
    ; Wait here until reset
    rjmp cmd_reboot_3
.endm

;******************************************************************************
; Function...: CMD_GET_VERSION
; Description: Returns bootloader version stored at 0x1fff
; In.........: Nothing
; Out........: r17 Version number
.macro CMD_GET_VERSION
    movw r19:r18, ZH:ZL
    ldi ZL, 0xff
    ldi ZH, 0x1f
    lpm r17, Z
    movw ZH:ZL, r19:r18
.endm
