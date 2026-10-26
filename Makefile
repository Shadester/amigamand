VBCC   ?= $(CURDIR)/toolchain/vbcc
export VBCC
# vc spawns vbccm68k/vasm/vlink via PATH
export PATH := $(VBCC)/bin:$(PATH)

CC      = $(VBCC)/bin/vc
CFLAGS  = +aos68k -cpu=68020 -O2 -c99
SRC     = main.c view.c palette.c ilbm.c rexx.c c2p.c
HDR     = $(wildcard *.h)

all: amigamand amigamand-fpu

# fixed point: no FPU needed
amigamand: $(SRC:%.c=build/fix/%.o) build/fix/iter020.o
	$(CC) +aos68k -cpu=68020 -o $@ $^

# FPU: needs a 68881/68882/68040+
amigamand-fpu: $(SRC:%.c=build/fpu/%.o)
	$(CC) +aos68k -cpu=68020 -fpu=68881 -o $@ $^

build/fix/%.o: %.c $(HDR)
	@mkdir -p build/fix
	$(CC) $(CFLAGS) -c -o $@ $<

build/fix/iter020.o: iter020.s
	@mkdir -p build/fix
	$(CC) +aos68k -cpu=68020 -c -o $@ $<

build/fpu/%.o: %.c $(HDR)
	@mkdir -p build/fpu
	$(CC) $(CFLAGS) -DUSE_FPU -fpu=68881 -c -o $@ $<

# test builds (tools/test): the first render ends with a mouse gesture, see TEST_MOUSE in main.c
amigamand-tm%: build/tm%/main.o $(filter-out build/fix/main.o,$(SRC:%.c=build/fix/%.o)) build/fix/iter020.o
	$(CC) +aos68k -cpu=68020 -o $@ $^

build/tm%/main.o: main.c $(HDR)
	@mkdir -p build/tm$*
	$(CC) $(CFLAGS) -DTEST_MOUSE=$* -c -o $@ $<

# test builds for tools/test: a start from Workbench (tool types of SYS:prog.info), and the icon check
amigamand-tw: $(SRC:%.c=build/tw/%.o) build/fix/iter020.o
	$(CC) +aos68k -cpu=68020 -o $@ $^

build/tw/%.o: %.c $(HDR)
	@mkdir -p build/tw
	$(CC) $(CFLAGS) -DTEST_WB -c -o $@ $<

build/spawn: tools/test/spawn.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

build/icontest: tools/test/icontest.c
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf amigamand amigamand-fpu amigamand-tm1 amigamand-tm2 amigamand-tw build/fix build/fpu build/tm1 build/tm2 build/tw build/icontest build/spawn

.PHONY: all clean
