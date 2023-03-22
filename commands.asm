.equ CMD_VERSION               = 0x01

;******************************************************************************
; Function...: CMD_RETURN_VERSION_ID
; Description: Returns current API version ID
; In.........: Nothing
; Out........: r17 API version ID
.macro CMD_RETURN_VERSION_ID
    ldi r17, CMD_VERSION
.endmacro

;******************************************************************************
; Function...: CMD_RECEIVE_PACKET
; Description: Stores one byte of a firmware update packet (total 9 bytes)
;              into a RAM buffer
; In.........: r16 Byte value
; Out........: Nohting
.macro CMD_RECEIVE_PACKET
    ; Set pointer to start of default target RAM buffer
    ldi YL,low(flash_buf)
    ldi YH,high(flash_buf)

    ; Are we in flash mem zero page?
    rcall flash_targetaddr_in_zp
    brcs cmd_receive_packet2                    ; Carry set => we're not in zero page

    ; Target address in zero page, use the special zero page buffer
    ldi YL,low(flash_zp_buf)
    ldi YH,high(flash_zp_buf)

cmd_receive_packet2:
    ; Y = Y + buffer index
    lds r18,flash_bufindex
    add YL,r18
    clr r18
    adc YH,r18

    ; Packet size < 8?
    lds r17,flash_packetsize
    cpi r17,8
    brsh cmd_receive_packet3

    st Y,r16                                    ; Store value in buffer
    
    inc r18                                     ; Increase buffer index
    sts flash_bufindex,r18

cmd_receive_packet3:
    ; Packet size < 9?
    cpi r17,9
    brsh cmd_receive_packet4

    lds r18,flash_checksum
    add r16,r18
    sts flash_checksum,r16

cmd_receive_packet4:
    ; Packet size < 10?
    cpi r17,10
    brsh cmd_receive_packet_exit
    
    inc r17
    sts flash_packetsize,r17

cmd_receive_packet_exit:

.endmacro

.macro CMD_COMMIT
    ; Packet size = 9?
    ldi r17,2
    lds r16,flash_packetsize
    cpi r16,9
    brne cmd_commit_err

    ; Checksum = 0?
    ldi r17,3
    lds r16,flash_checksum
    cpi r16,0
    brne cmd_commit_err

    ; Target address < 0x1e00?
    lds r17,4
    lds r16,flash_targetaddr+1
    cpi r16,0x1e
    brsh cmd_commit_err

    ; Last packet in page?
    lds r16,flash_bufindex
    cpi r16,0x40
    brne cmd_commit_ok                          ; We don't yet have a complete page

    ; Zero page?
    rcall flash_targetaddr_in_zp
    brcc cmd_commit_ok                          ; Yes, nothing more to do now

    ; Write buffer to flash
    lds ZL,flash_targetaddr
    lds ZH,flash_targetaddr
    rcall flash_write_buf

    ; Increase target address
    lds r18,flash_targetaddr
    ldi r19,8
    add r18,r19
    sts flash_targetaddr,r18
    lds r18,flash_targetaddr
    clr r19
    adc r18,r19
    sts flash_targetaddr+1,r18

    ; Load return value = OK
cmd_commit_ok:
    ldi r17,0
    rjmp cmd_commit_exit

cmd_commit_err:
    clr r16
    sts flash_packetsize,r16
    sts flash_checksum,r16
    lds r16,flash_bufindex
    andi r16,0b11111000
    sts flash_bufindex,r16

cmd_commit_exit:
.endmacro

.macro CMD_CLOSE
    lds ZL,flash_targetaddr
    lds ZH,flash_targetaddr
    rcall flash_write_buf
.endmacro

.macro CMD_REBOOT
.endmacro

