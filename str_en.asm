str_appname:
    .byt 147
    .byt "x16 system management controller update",13
    .byt "version 1.0",13,13
    .byt "this updates the firmware of the attiny861 based smc",13
    .byt "for x16 gen-1.",13,13,0

str_nobootloader:
    .byt "bootloader not found",0

str_unsupported_bootloader:
    .byt "unsupported bootloader version: ",0

str_warning:
    .byt "warning: the system may become inoperable if the update is",13
    .byt "interrupted or otherwise fails. in the event of this the",13
    .byt "smc firmware must be updated with an external programmer.",13,0

str_continue:
    .byt 13,"continue (y/n): ",0

str_filename:
    .byt 13, 13, "hex file: ",0

str_loading:
    .byt 13, "loading hex file... ",0

str_ok:
    .byt "ok",13,0

str_failed:
    .byt "failed",13,0

str_overflow:
    .byt "failed, bootloader area overflow",13,0

str_activate_prompt:
    .byt 13, "to activate the smc bootloader, first press enter on the keyboard",13
    .byt "and within 20 seconds thereafter press and release the power and",13
    .byt "reset buttons on the computer. the power and reset buttons must be",13
    .byt "pressed within 1 second of each other.",13,13
    .byt "press enter: ",0

str_activate_countdown:
    .byt 13, "press power and reset buttons... ", 0

str_activate_wait:
    .byt 13, "waiting for smc bootloader to initalize... ", 0

str_upload:
    .byt 13, 13, "uploading firmware",13,0

str_done:
    .byt 13, "done",0

str_updatefailed:
    .byt 13, "update failed",0