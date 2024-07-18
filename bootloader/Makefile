BUILD_DIR=build
SRC_FILES=$(wildcard *.asm *.inc)

# Bootloader
$(BUILD_DIR)/bootloader.hex: $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	avra -o $@ -W NoRegDef main.asm
	rm -f main.eep.hex	
	rm -f main.obj
	python merge.py
 
# Clean
clean:
	rm -f $(BUILD_DIR)/*
