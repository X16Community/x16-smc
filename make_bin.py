# Usage:
# make_bin.py path1 path2
#  path1: Path to SMC firmware in Intel hex file format
#  path2: Path to output folder

import sys
import os
from intelhex import IntelHex

# Table of compatible Kernal versions (negative value is pre-release)
kernal_versions = {-44, -45, 45, -46}

# Path to firmware hex file
firmware_hex = sys.argv[1]

# Init header output buffer
buf = [0] * 32

# Get SMC firmware version from C++ header file
version_major = 0
version_minor = 0
version_patch = 0

smc_version = open("version.h", "r")

lines = smc_version.readlines()
for l in lines:
    parts = l.split()
    if parts[0] == "#define":
        if parts[1] == "version_major":
            version_major = int(parts[2])
            buf[0] = version_major
        elif parts[1] == "version_minor":
            version_minor = int(parts[2])
            buf[1] = version_minor
        elif parts[1] == "version_patch":
            version_patch = int(parts[2])
            buf[2] = version_patch

smc_version.close()

# Add supported Kernal versions to output buffer
i = 32 - len(kernal_versions)
for v in kernal_versions:
    buf[i] = v
    i = i + 1

# Read firmware file
ih = IntelHex()
ih.loadfile(firmware_hex, format="hex")
firmware = ih.tobinarray()

# Create output folder, if not exist
try:
    os.mkdir(".build")
except:
    pass

# Create SMC.BIN file
smc = open(sys.argv[2] + "/SMC-" + str(version_major) + "." + str(version_minor) + "." + str(version_patch) + ".BIN", "wb")
i = 0
for v in buf:
    if i < 3:
        is_signed = False
    else:
        is_signed = True
    smc.write(v.to_bytes(1, byteorder=sys.byteorder, signed=is_signed))
    i = i + 1
smc.write(firmware)
smc.close()