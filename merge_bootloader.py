# Usage: python merge_bootloader.py file1 file2 file3
#   file1: firmware hex file
#   file2: bootloader hex file
#   file3: new merged file

import sys
from intelhex import IntelHex

# Check argument length
if len(sys.argv) != 4:
    print("Usage: python merge_bootloader.py file1 file2 file3")
    print("  file1: firmware hex file")
    print("  file2: bootloader hex file")
    print("  file3: new merged file")
    exit(1)

# Load firmware
try:
    firmware = IntelHex()
    firmware.fromfile(sys.argv[1], format="hex")
except:
    print("Error loading firmware")
    quit()

# Load bootloader
try:
    bootloader = IntelHex()
    bootloader.fromfile(sys.argv[2], format="hex")
except:
    print("Error loading bootloader")
    quit()

# Move firmware's original reset vector (byte addr 0x00) to EE_RDY (byte addr 0x12)
orig_reset = firmware[0x00] + firmware[0x01] * 256
orig_reset = (((orig_reset - 9) & 0b1100111111111111) | 0b1100000000000000)
firmware[0x12] = orig_reset & 255
firmware[0x13] = orig_reset >> 8

# Set reset vector to opcode for "rjmp 0x1e00" (start of bootloader)
firmware[0x00] = 0xff
firmware[0x01] = 0xce

# Merge firmware and bootloader into new file
try:
    firmware.merge(bootloader, overlap="error")
    firmware.tofile(sys.argv[3], format="hex")
except:
    print("Merge error")
    quit()
