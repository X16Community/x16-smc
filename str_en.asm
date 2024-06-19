COL_DEFAULT = $05
COL_BG      = $1f
COL_WARN    = $9e
COL_OK      = $1e
COL_ERR     = $96
COL_SWAP    = $01 
SCR_CLS     = 147
SCR_PETSCII = $8f
SCR_LOWER   = $0e

str_appinfo:
    .byt SCR_PETSCII, SCR_LOWER, COL_BG, COL_SWAP, COL_DEFAULT, SCR_CLS
    .byt "COMMANDER X16 SYSTEM MANAGEMENT CONTROLLER UPDATE", 13
    .byt "Version 2.0", 13, 13
    .byt "This tool updates the firmware of the ATtiny861 based SMC for the Commander X16 Gen-1.",13, 13, 0

str_nobootloader:
    .byt COL_ERR, "ERROR:" , COL_DEFAULT, " SMC bootloader not installed.", 0

str_unsupported_bootloader:
    .byt COL_ERR, "ERROR:", COL_DEFAULT, " Unsupported bootloader version: ", 0

str_warning:
    .byt COL_WARN, "WARNING: ", COL_DEFAULT
    .byt "The computer may become inoperable if the update is interrupted "
    .byt "or if it fails.", 13, 13, 0

str_warning2:
    .byt COL_WARN, "WARNING: ", COL_DEFAULT
    .byt "Version 2 of the bootloader is installed on the SMC. "
    .byt "That version is known to be corrupted on some production "
    .byt "boards.", 13, 13, 0

str_read_instructions:
    .byt "Read instructions at https://github.com/X16Community/x16-smc before "
    .byt "you proceed.", 13, 13, 0

str_continue:
    .byt "Continue (Y/N): ", 0

str_filename:
    .byt 13, "HEX file name: ", 0

str_loading:
    .byt 13, "Loading HEX file... ", 0

str_ok:
    .byt COL_OK, "OK", COL_DEFAULT, 13, 0

str_failed:
    .byt COL_ERR, "FAILED", COL_DEFAULT, 13,0

str_overflow:
    .byt COL_ERR, "FAILED, bootloader area overflow", COL_DEFAULT, 13, 0

str_checksumerr:
    .byt COL_ERR, "FAILED, checksum error", COL_DEFAULT, 13, 0

str_activate_prompt:
    .byt 13, "To start the update first press Enter and then within "
    .byt "20 seconds momentarily press the Power and Reset buttons "
    .byt "at the same time.", 13, 13
    .byt "Press Enter: ", 0

str_activate_countdown:
    .byt 13, "Press Power and Reset buttons... ", 0

str_activate_wait:
    .byt 13, "Bootloader initializing... ", 0

str_upload:
    .byt 13, 13, "Updating the firmware",13,0

str_done:
    .byt 13, COL_OK, "Update successful.", COL_DEFAULT, 13, 13
    .byt "Disconnect mains power and wait 20 seconds before "
    .byt "reconnecting power.", 0

str_done2:
    .byt 13, COL_DEFAULT, "Finishing the update", 13
    .byt "System shutdown... ", 0

str_bad_v2_bootloader:
    .byt 13, 13, COL_WARN, "WARNING:", COL_DEFAULT, " Do NOT turn off the system.", 13, 13
    .byt "The bootloader is corrupted, preventing the update from finishing. "
    .byt "Complete the update by momentarily connecting SMC pin #10 (reset) "
    .byt "to ground. Instructions available at "
    .byt "www.github.com/X16Community/x16-smc", 0

str_updatefailed:
    .byt 13, 13, COL_ERR, "Update failed.", COL_DEFAULT, 0

str_bootloader_not_activated:
    .byt 13, 13, COL_ERR, "Update aborted, bootloader not activated.", COL_DEFAULT, 0
