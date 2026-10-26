#!/usr/bin/env python3
"""Writes the Workbench icons amigamand.info and amigamand-fpu.info (a DiskObject: a tool with a 4 colour picture).

    python3 tools/make_icon.py [outdir]        (default: the repo root)

The picture is a small Mandelbrot set in the four colours of the Workbench palette (grey, black, white, blue).
The tool types are the start options of main.c; the ones in parentheses are switched off (remove the brackets).
"""
import struct
import sys

W, H, DEPTH = 48, 32, 2

TOOLTYPES = ['MODE=1', 'ITER=128', 'PALETTE=Rainbow',
             '(CX=-0.75)', '(CY=0)', '(SPAN=3.5)', '(JULIA)', '(JR=-0.8)', '(JI=0.156)',
             '(DEPTH=8)', '(EXACT)', '(SAVE=RAM:amigamand.iff)', '(QUIT)']
FPU_TOOLTYPES = TOOLTYPES + ['(needs a 68881, 68882 or 68040 or better)']


def picture():
    """Pens 0..3 for W x H pixels: inside the set black (1), the rim white (2), near it blue (3), far grey (0)."""
    rows = []
    for py in range(H):
        row = []
        for px in range(W):
            cr = -2.2 + px * 3.2 / W
            ci = -1.2 + py * 2.4 / H
            zr = zi = 0.0
            i = 0
            while i < 40 and zr * zr + zi * zi <= 4:
                zr, zi = zr * zr - zi * zi + cr, 2 * zr * zi + ci
                i += 1
            row.append(1 if i == 40 else 2 if i > 12 else 3 if i > 5 else 0)
        rows.append(row)
    return rows


def planes(rows):
    """Planar data as in an Image structure: plane by plane, each row padded to whole words."""
    rowbytes = ((W + 15) // 16) * 2
    out = bytearray()
    for p in range(DEPTH):
        for row in rows:
            b = bytearray(rowbytes)
            for x, pen in enumerate(row):
                if (pen >> p) & 1:
                    b[x >> 3] |= 0x80 >> (x & 7)
            out += b
    return bytes(out)


def string(s):
    b = s.encode('latin1') + b'\0'
    return struct.pack('>I', len(b)) + b


def diskobject(tooltypes, stack=8192):
    img = planes(picture())
    # struct Gadget: next, left, top, width, height, flags, activation, type, render, select, text, exclude, special, id, user
    gadget = struct.pack('>IhhhhHHHIIIIIHI', 0, 0, 0, W, H,
                         0x0005,  # GFLG_GADGBACKFILL | GFLG_GADGIMAGE: the picture, inverted when selected
                         0x0003,  # GACT_RELVERIFY | GACT_IMMEDIATE
                         0x0001,  # GTYP_BOOLGADGET
                         1, 0, 0, 0, 0, 0,
                         1)       # the icon revision (WB_DISKREVISION)
    assert len(gadget) == 44
    NO_POS = 0x80000000
    head = struct.pack('>HH', 0xE310, 1) + gadget
    head += struct.pack('>BBIIIIIII', 3, 0,   # WBTOOL
                        0,                    # no default tool
                        1,                    # tool types follow
                        NO_POS, NO_POS,       # no fixed position
                        0, 0, stack)
    assert len(head) == 78, len(head)
    image = struct.pack('>hhhhhIBBI', 0, 0, W, H, DEPTH, 1, 3, 0, 0) + img  # struct Image, then its data
    tt = struct.pack('>I', (len(tooltypes) + 1) * 4) + b''.join(string(t) for t in tooltypes)
    return head + image + tt


if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '.'
    for name, tt in (('amigamand.info', TOOLTYPES), ('amigamand-fpu.info', FPU_TOOLTYPES)):
        data = diskobject(tt)
        open(f'{out}/{name}', 'wb').write(data)
        print(name, len(data), 'bytes')
