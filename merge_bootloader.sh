#!/bin/bash

if [ $# -ne 3 ] ; then
    echo "Usage: merge_bootloader.sh path1 path2 path3"
    echo "  path1: Path to firmware file in Intel HEX format"
    echo "  path2: Path to bootloader file in Intel HEX format"
    echo "  path3: Output file path"
    exit 1
else
    grep -v ":00000001FF" $1 > .build/tmp.hex
    cat .build/tmp.hex $2 > $3
    rm .build/tmp.hex
fi
