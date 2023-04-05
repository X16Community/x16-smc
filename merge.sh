#!/bin/bash

grep -v ":00000001FF" $1 > build/tmp.hex
cat build/tmp.hex build/bootloader.hex > build/firmware+bootloader.hex
rm build/tmp.hex
