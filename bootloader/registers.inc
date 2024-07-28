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


; r0-r1 reserved for special use (SPM buffer copy)

.def packet_tailL                  = r2       ; Pointer to current RAM buffer tail
.def packet_tailH                  = r3
.def chip_erased                   = r4

; r5-r13 free

.def zeroL                         = r14
.def zeroH                         = r15

; r16-r19 reserved for temporary use

.def packet_size                   = r20      ; Current packet size
.def checksum                      = r21      ; Current packet checksum
.def packet_count                  = r22            
.def i2c_state                     = r23      ; For the I2C state machine
.def i2c_ddr                       = r24      ; Current data direction
.def i2c_command                   = r25      ; Current I2C command

; X (26-r27)

.def packet_headL                  = r28
.def packet_headH                  = r29
.def target_addrL                  = r30
.def target_addrH                  = r31
