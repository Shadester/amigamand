/* Palettes: pen 0 is black (inside the set), pens 1..ncol-1 follow a gradient. */
#ifndef PALETTE_H
#define PALETTE_H
#include <exec/types.h>

#define NPAL 5
const char *pal_name(WORD i);
/* rgb gets 3 bytes (0..255) for each of ncol pens; ncol is 2..256 */
void pal_make(WORD i, WORD ncol, UBYTE *rgb);
#endif
