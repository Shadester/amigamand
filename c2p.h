/* Chunky to planar, straight into a screen's bitmap. graphics.library's WriteChunkyPixels is a generic software
   conversion on AGA and costs hundreds of cycles per pixel. */
#ifndef C2P_H
#define C2P_H
#include <exec/types.h>
#include <graphics/gfx.h>

/* x and w must be multiples of 8. src is the first pixel (one pen per byte), srcbpr the bytes per source row.
   Nothing else may draw into the same area at the same time (no menu or requester over it). */
void c2p_rect(const UBYTE *src, LONG srcbpr, struct BitMap *bm, WORD x, WORD y, WORD w, WORD h);
#endif
