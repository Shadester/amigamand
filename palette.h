/* Palettes: pen 0 is black (inside the set), the pens after the reserved ones follow a gradient. */
#ifndef PALETTE_H
#define PALETTE_H
#include <exec/types.h>

#define NPAL 5
/* The first pens of a screen belong to Intuition (requesters, menus): black, white, grey, blue. With 16 colours or more the
   fractal starts after them; with fewer there is no room and only pen 0 (black) is kept. */
WORD pal_reserved(WORD ncol);
const char *pal_name(WORD i);
/* rgb gets 3 bytes (0..255) for each of ncol pens; ncol is 2..256 */
void pal_make(WORD i, WORD ncol, UBYTE *rgb);
#endif
