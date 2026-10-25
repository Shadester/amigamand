CC     ?= vc
CFLAGS  = +aos68k -cpu=68020 -O2 -c99

all: amigamand amigamand-ffp

amigamand: main.c
	$(CC) $(CFLAGS) -o $@ $<

amigamand-ffp: main.c
	$(CC) $(CFLAGS) -DUSE_FFP -o $@ $<

clean:
	rm -f amigamand amigamand-ffp

.PHONY: all clean
