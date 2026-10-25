#include <exec/types.h>
#include <graphics/gfx.h>
#include <proto/graphics.h>
#include "c2p.h"

/* Eight pixels are a 8x8 bit matrix (a byte per pixel, a bit per plane); three masked swaps transpose it. */
void c2p_rect(const UBYTE *src, LONG srcbpr, struct BitMap *bm, WORD x, WORD y, WORD w, WORD h)
{
	WORD depth = bm->Depth, groups = w >> 3, p;

	WaitBlit(); /* a RectFill may still be writing */
	for (; h > 0; h--, y++, src += srcbpr) {
		const ULONG *s = (const ULONG *)src;
		UBYTE *d[8];
		WORD g;

		for (p = 0; p < 8; p++)
			d[p] = p < depth ? bm->Planes[p] + (LONG)y * bm->BytesPerRow + (x >> 3) : 0;
		for (g = 0; g < groups; g++) {
			ULONG hi = *s++, lo = *s++, t;

			t = (hi ^ (hi >> 7)) & 0x00AA00AA, hi ^= t ^ (t << 7);
			t = (lo ^ (lo >> 7)) & 0x00AA00AA, lo ^= t ^ (t << 7);
			t = (hi ^ (hi >> 14)) & 0x0000CCCC, hi ^= t ^ (t << 14);
			t = (lo ^ (lo >> 14)) & 0x0000CCCC, lo ^= t ^ (t << 14);
			t = (hi ^ (lo >> 4)) & 0x0F0F0F0F, hi ^= t, lo ^= t << 4;
			/* now plane 7 is the top byte of hi ... plane 0 the bottom byte of lo */
			if (depth == 8) {
				*d[7]++ = hi >> 24, *d[6]++ = hi >> 16, *d[5]++ = hi >> 8, *d[4]++ = hi;
				*d[3]++ = lo >> 24, *d[2]++ = lo >> 16, *d[1]++ = lo >> 8, *d[0]++ = lo;
			} else {
				if (depth > 7) *d[7]++ = hi >> 24;
				if (depth > 6) *d[6]++ = hi >> 16;
				if (depth > 5) *d[5]++ = hi >> 8;
				if (depth > 4) *d[4]++ = hi;
				if (depth > 3) *d[3]++ = lo >> 24;
				if (depth > 2) *d[2]++ = lo >> 16;
				if (depth > 1) *d[1]++ = lo >> 8;
				*d[0]++ = lo;
			}
		}
	}
}
