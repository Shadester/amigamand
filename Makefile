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
amigamand: $(SRC:%.c=build/fix/%.o)
	$(CC) +aos68k -cpu=68020 -o $@ $^

# FPU: needs a 68881/68882/68040+
amigamand-fpu: $(SRC:%.c=build/fpu/%.o)
	$(CC) +aos68k -cpu=68020 -fpu=68881 -o $@ $^

build/fix/%.o: %.c $(HDR)
	@mkdir -p build/fix
	$(CC) $(CFLAGS) -c -o $@ $<

build/fpu/%.o: %.c $(HDR)
	@mkdir -p build/fpu
	$(CC) $(CFLAGS) -DUSE_FPU -fpu=68881 -c -o $@ $<

clean:
	rm -rf amigamand amigamand-fpu build/fix build/fpu

.PHONY: all clean
