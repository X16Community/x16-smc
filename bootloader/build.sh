#!/bin/bash

avra -o ./build/bootloader.hex main.asm
rm main.eep.hex	
rm main.obj
