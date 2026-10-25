VBCC   ?= $(CURDIR)/toolchain/vbcc
export VBCC
# vc spawns vbccm68k/vasm/vlink via PATH
export PATH := $(VBCC)/bin:$(PATH)

CC      = $(VBCC)/bin/vc
CFLAGS  = +aos68k -cpu=68020 -O2 -c99

all: amigamand amigamand-fpu

amigamand: main.c
	$(CC) $(CFLAGS) -o $@ $<

amigamand-fpu: main.c
	$(CC) $(CFLAGS) -DUSE_FPU -fpu=68881 -o $@ $<

clean:
	rm -f amigamand amigamand-fpu

.PHONY: all clean
