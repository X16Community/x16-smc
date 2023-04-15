#!/bin/bash

if [ $# -ne 1 ] ; then
    echo "Usage: merge.sh firmware.hex"
    exit 1
else
    grep -v ":00000001FF" $1 > build/tmp.hex
    cat build/tmp.hex build/bootloader.hex > build/firmware+bootloader.hex
    rm build/tmp.hex
fi
