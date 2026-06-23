CC       ?= cc
RGBASM   ?= rgbasm
RGBLINK  ?= rgblink
RGBFIX   ?= rgbfix
SAMEBOY  := third_party/SameBoy
SB_LIB   := $(SAMEBOY)/build/lib/libsameboy.a
SB_BOOT  := $(SAMEBOY)/build/bin/BootROMs

CFLAGS   := -std=gnu11 -Wall -Wextra -I$(SAMEBOY) -Isrc
LDFLAGS  := -lm

SRC      := src/screenshot.c src/script.c src/runner.c src/main.c
OBJ      := $(patsubst src/%.c,build/obj/%.o,$(SRC))

BOOTROMS := dmg_boot.bin cgb_boot.bin sgb2_boot.bin
BOOT_OUT := $(patsubst %,build/bin/%,$(BOOTROMS))

.PHONY: all clean test testroms integration
all: build/bin/boyless $(BOOT_OUT)

# Assembly test ROMs (RGBDS).
testroms: build/testroms/fill.gb build/testroms/input.gb build/testroms/mem.gb

build/testroms/fill.gb: testroms/fill.asm
	@mkdir -p $(dir $@)
	$(RGBASM) -o build/testroms/fill.o $<
	$(RGBLINK) -o $@ build/testroms/fill.o
	$(RGBFIX) -v -p 0xFF $@

build/testroms/input.gb: testroms/input.asm
	@mkdir -p $(dir $@)
	$(RGBASM) -o build/testroms/input.o $<
	$(RGBLINK) -o $@ build/testroms/input.o
	$(RGBFIX) -v -C -p 0xFF $@

build/testroms/mem.gb: testroms/mem.asm
	@mkdir -p $(dir $@)
	$(RGBASM) -o build/testroms/mem.o $<
	$(RGBLINK) -o $@ build/testroms/mem.o
	$(RGBFIX) -v -p 0xFF $@

integration: all testroms
	./tests/integration.sh

# Build SameBoy's static library and boot ROMs in a single serialized sub-make.
# Under `make -jN` the four would otherwise race into the shared build tree.
SB_STAMP := $(SAMEBOY)/build/.boyless-sameboy-stamp
$(SB_STAMP):
	$(MAKE) -C $(SAMEBOY) lib bootroms
	@touch $@

# The library and boot ROMs are produced by the single stamp sub-make.
# .PRECIOUS prevents make from deleting these as intermediate files.
$(SB_LIB): $(SB_STAMP) ;
$(SB_BOOT)/%.bin: $(SB_STAMP) ;
.PRECIOUS: $(SB_BOOT)/%.bin

build/obj/%.o: src/%.c | $(SB_STAMP)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/bin/boyless: $(OBJ) $(SB_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(OBJ) $(SB_LIB) -o $@ $(LDFLAGS)

build/bin/%.bin: $(SB_BOOT)/%.bin
	@mkdir -p $(dir $@)
	cp -f $< $@

clean:
	rm -rf build

test: build/test_screenshot build/test_script build/test_runner
	./build/test_screenshot
	./build/test_script
	./build/test_runner

build/test_screenshot: tests/test_screenshot.c src/screenshot.c | $(SB_STAMP)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/test_script: tests/test_script.c src/script.c | $(SB_STAMP)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

build/test_runner: tests/test_runner.c src/runner.c src/screenshot.c | $(SB_STAMP)
	@mkdir -p build
	$(CC) $(CFLAGS) $^ $(SB_LIB) -o $@ $(LDFLAGS)
