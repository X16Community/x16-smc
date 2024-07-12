BUILD_DIR=build
SRC_FILES=$(wildcard *.asm *.inc)

# Commands
bootloader: $(BUILD_DIR)/bootloader.hex $(BUILD_DIR)/firmware+bootloader.hex
merge: $(BUILD_DIR)/firmware+bootloader.hex

# Merge firmware and bootloader
$(BUILD_DIR)/firmware+bootloader.hex: $(BUILD_DIR)/bootloader.hex
	@mkdir -p $(BUILD_DIR)
	./merge.py

# Bootloader
$(BUILD_DIR)/bootloader.hex: $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	avra -o $@ -W NoRegDef main.asm
	rm -f main.eep.hex	
	rm -f main.obj

# Set path to firmware hex file - Path specified in command line arg "firmware"
setpath:
	ln -sf $(firmware) firmware.hex
 
# Clean
clean:
	rm -f $(BUILD_DIR)/*
