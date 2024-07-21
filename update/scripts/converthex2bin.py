from intelhex import IntelHex

fw = IntelHex()
fw.loadfile("../build/x16-smc.ino.hex", format="hex")
fw.tofile("../resources/x16-smc.ino.bin", format="bin")
