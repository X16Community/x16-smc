from intelhex import IntelHex

bootloader = IntelHex()
bootloader.fromfile("build/bootloader.hex", format="hex")
bootloader.tofile("build/bootloader.bin", format="bin")
