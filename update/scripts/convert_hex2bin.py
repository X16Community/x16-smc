from intelhex import IntelHex

fw1 = IntelHex()
fw1.loadfile("../build/default/x16-smc.ino.hex", format="hex")
fw1.tofile("resources/x16-smc-default.bin", format="bin")

fw2 = IntelHex()
fw2.loadfile("../build/community/x16-smc.ino.hex", format="hex")
fw2.tofile("resources/x16-smc-community.bin", format="bin")
