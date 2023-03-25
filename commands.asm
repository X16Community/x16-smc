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
 
 ; Out: r17 error code
;******************************************************************************
; Function...: CMD_COMMIT
; Description: Receives a packet byte and stores it into a RAM buffer. This

; In.........: r16 byte value
; Out........: r17 Commit response value:
;                  0x00 = OK, packet stored in RAM buffer
;                  0x01 = OK, buffer written to flash memory
;                  0x02 = Invalid packet size, not equal to 9
;                  0x03 = Checksum error
;                  0x04 = Target address overflows into bootloader section (0x1E00..)
.macro CMD_COMMIT
    ; Move target address into Z
    movw ZH:ZL,target_addrH:target_addrL

    ; Verify packet
    ldi r17,2
    cpi packet_size,9                                   ; Packet size == 8?
    brne cmd_commit_err
    ldi r17,3
    cpi checksum,0                                      ; Checksum == 0?
    brne cmd_commit_err
    ldi r17,4
    mov r16,target_addrH
    cpi r16,0x1e                                        ; Target address >= 0x1E00 (in bootloader area)?
    brsh cmd_commit_err

    ; Page not full, increment packet count and prepare for next packet
    inc packet_count
    cpi packet_count,16
    brne cmd_commit_ok
 
cmd_commit_write:
    ; Write buffer to flash mem
    rcall flash_write_buf

    ; Update target addr
    adiw ZH:ZL,32
    adiw ZH:ZL,32
    mov target_addrH:target_addrL, ZH:ZL

    ; Reset buffer pointer
    ldi r16,low(flash_buf)
    ldi r17,high(flash_buf)
    movw packet_headH:packet_headL, r17:r16

    ; Reset packet count
    ldi packet_count,8                                  ; Restart from 8 as 0..7 reserved for the first flash mem page
    
    ldi r17,1
    rjmp cmd_commit_ok2
 
cmd_commit_ok:
    ldi r17,0
cmd_commit_ok2:
    movw packet_tailH:packet_tailL, packet_headH:packet_headL
    rjmp cmd_commit_exit

cmd_commit_err:
    ; Rewind
    movw packet_headH:packet_headL,packet_tailH:packet_tailL

cmd_commit_exit:
    clr packet_size
    clr checksum

.endmacro

;******************************************************************************
; Function...: CMD_FLUSH
; Description: Writes any data in the RAM buffer to flash memory; intended to
;              be called after all packets have been committed to ensure we
;              don't miss anything.
; In.........: Nothing
; Out........: Nothing
.macro CMD_FLUSH
    ; Not yet implemented
.endmacro

;******************************************************************************
; Function...: CMD_REBOOT
; Description: Writes flash_zp_buf to flash memory, setting up the first
;              page of flash memory and then reboots the ATTiny using
;              the Watchdog Timer
; In.........: Nothing
; Out........: Nothing
.macro CMD_REBOOT
    ; Not yet implemented
.endmacro