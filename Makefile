BUILD_DIR=build
RES_DIR=resources
SRC_FILES=$(wildcard *.asm *.inc)
PRGFILE=$(shell python scripts/prgname.py)

$(BUILD_DIR)/$(PRGFILE): $(SRC_FILES)
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(RES_DIR)
	python scripts/converthex2bin.py
	cl65 -o $(BUILD_DIR)/$(PRGFILE) -u __EXEHDR__ -t cx16 -C cx16-asm.cfg main.asm
	rm -f main.o

# Clean
clean:
	rm -f $(BUILD_DIR)/*
