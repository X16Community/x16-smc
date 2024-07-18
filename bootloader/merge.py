import sys
from intelhex import IntelHex

# Load firmware
try:
    firmware = IntelHex()
    firmware.fromfile("../.build/x16-smc.ino.hex", format="hex")
except:
    print("Error loading firmware")
    quit()

# Load bootloader
try:
    bootloader = IntelHex()
    bootloader.fromfile("build/bootloader.hex", format="hex")
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
    firmware.tofile("../.build/firmware_with_bootloader.hex", format="hex")
except:
    print("Merge error")
    quit()
