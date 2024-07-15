BUILD_DIR=build
SRC_FILES=$(wildcard *.asm *.inc)

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
 
# Clean
clean:
	rm -f $(BUILD_DIR)/*
