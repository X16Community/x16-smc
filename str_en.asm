str_appname:
    .byt 147
    .byt "x16 system management controller update",13
    .byt "version 1.0",13,13,0
    
str_warning:
    .byt "warning: the system may become inoperable if the update is",13
    .byt "interrupted or otherwise fails. in the event of this the",13
    .byt "smc firmware must be updated using an external programmer.",13,0

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

str_upload:
    .byt 13, "uploading firmware... ",13,0

str_done:
    .byt 13, "done",13,0

str_updatefailed:
    .byt 13, "update failed",13,0