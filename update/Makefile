BUILD_DIR=build
RES_DIR=resources
SRC_FILES=$(wildcard *.asm *.inc)
PRGFILE=$(shell python scripts/prgname.py)

$(BUILD_DIR)/$(PRGFILE): $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/default
	@mkdir -p $(BUILD_DIR)/community
	@mkdir -p $(RES_DIR)
	python scripts/version.py
	python scripts/convert_hex2bin.py
	cl65 -o $(BUILD_DIR)/default/$(PRGFILE) -u __EXEHDR__ -t cx16 -C cx16-asm.cfg --asm-args -Dtarget_board=1 main.asm
	cl65 -o $(BUILD_DIR)/community/$(PRGFILE) -u __EXEHDR__ -t cx16 -C cx16-asm.cfg --asm-args -Dtarget_board=2 main.asm
	rm -f main.o

# Clean
clean:
	rm -f -r $(BUILD_DIR)/*
