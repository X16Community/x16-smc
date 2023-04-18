COL_DEFAULT = $05
COL_BG      = $1f
COL_WARN    = $9e
COL_OK      = $1e
COL_ERR     = $96
COL_SWAP    = $01 
SCR_CLS     = 147
SCR_PETSCII = $8f
SCR_LOWER   = $0e

str_appname:
    .byt SCR_PETSCII, SCR_LOWER, COL_BG, COL_SWAP, COL_DEFAULT, SCR_CLS
    .byt 13, "               COMMANDER X16 SYSTEM MANAGEMENT CONTROLLER UPDATE",13
    .byt 13, "                                  Version 1.0"
    .byt 13,13,13, "This updates the firmware of the ATtiny861 based SMC for X16 GEN-1.",13,13,0

str_nobootloader:
    .byt "Bootloader not found.",0

str_unsupported_bootloader:
    .byt "Unsupported bootloader version: ",0

str_warning:
    .byt COL_WARN, "WARNING: ", COL_DEFAULT
    .byt "The system may become inoperable if the update is",13
    .byt "         interrupted or otherwise fails. In the event of this the",13
    .byt "         SMC firmware must be updated with an external programmer.",13,0

str_continue:
    .byt 13,"Continue (Y/N): ",0

str_filename:
    .byt 13, 13, "HEX file name: ",0

str_loading:
    .byt 13, "Loading HEX file... ",0

str_ok:
    .byt COL_OK, "OK", COL_DEFAULT, 13,0

str_failed:
    .byt COL_ERR, "FAILED", COL_DEFAULT, 13,0

str_overflow:
    .byt COL_ERR, "Failed, bootloader area overflow", COL_DEFAULT, 13,0

str_checksumerr:
    .byt COL_ERR, "failed, checksum error", COL_DEFAULT, 13,0

str_activate_prompt:
    .byt 13, "To activate the SMC bootloader, first press Enter on the keyboard",13
    .byt "and within 20 seconds thereafter press and release the Power and",13
    .byt "Reset buttons on the computer. The Power and Reset buttons must be",13
    .byt "pressed within 0.5 seconds of each other.",13,13
    .byt "Press Enter: ",0

str_activate_countdown:
    .byt 13, "Press Power and Reset buttons... ", 0

str_activate_wait:
    .byt 13, "Waiting for SMC bootloader to initialize... ", 0

str_upload:
    .byt 13, 13, "Uploading firmware",13,0

str_done:
    .byt 13, COL_OK, "Done.", COL_DEFAULT, 13, 13
    .byt "disconnect power and wait approx. 20 seconds before reconnecting to reboot the system.",0

str_updatefailed:
    .byt 13, 13, COL_ERR, "Update failed.", COL_DEFAULT, 0

str_bootloader_not_activated:
    .byt 13, 13, COL_ERR, "Update aborted, bootloader not activated.", COL_DEFAULT, 0