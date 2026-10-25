CC     ?= m68k-amigaos-gcc
CFLAGS  = -m68020 -noixemul -O2 -Wall

all: amigamand amigamand-ffp

amigamand: main.c
	$(CC) $(CFLAGS) -o $@ $<

amigamand-ffp: main.c
	$(CC) $(CFLAGS) -DUSE_FFP -o $@ $<

clean:
	rm -f amigamand amigamand-ffp

.PHONY: all clean
