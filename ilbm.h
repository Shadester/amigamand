/* IFF ILBM writer: BMHD, CAMG, CMAP, BODY (ByteRun1). */
#ifndef ILBM_H
#define ILBM_H
#include <exec/types.h>

/* img is w*h pens; rgb has 3 bytes for each of 1<<depth pens. Returns 0, else a DOS error code. */
LONG ilbm_save(const char *name, const UBYTE *img, WORD w, WORD h, WORD depth, ULONG camg, UBYTE xasp, UBYTE yasp,
	       const UBYTE *rgb);
#endif
