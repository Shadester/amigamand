#include <exec/types.h>
#include "palette.h"

struct stop {
	UBYTE pos, r, g, b;
};

static const struct stop rainbow[] = { { 0, 255, 0, 0 }, { 43, 255, 255, 0 }, { 85, 0, 255, 0 }, { 128, 0, 255, 255 },
	{ 171, 0, 0, 255 }, { 213, 255, 0, 255 }, { 255, 255, 0, 0 } };
static const struct stop fire[] = { { 0, 30, 0, 0 }, { 64, 160, 0, 0 }, { 128, 255, 128, 0 }, { 192, 255, 255, 0 },
	{ 255, 255, 255, 255 } };
static const struct stop ocean[] = { { 0, 0, 0, 60 }, { 85, 0, 70, 170 }, { 170, 0, 190, 210 }, { 255, 220, 250, 255 } };
static const struct stop gray[] = { { 0, 20, 20, 20 }, { 255, 255, 255, 255 } };
static const struct stop electric[] = { { 0, 90, 0, 160 }, { 64, 0, 120, 255 }, { 128, 255, 255, 255 }, { 192, 255, 60, 140 },
	{ 255, 90, 0, 160 } };

static const struct pal {
	const char *name;
	const struct stop *s;
	WORD n;
} pals[NPAL] = {
	{ "Rainbow", rainbow, 7 }, { "Fire", fire, 5 }, { "Ocean", ocean, 4 }, { "Gray", gray, 2 }, { "Electric", electric, 5 },
};

const char *pal_name(WORD i)
{
	return pals[i].name;
}

WORD pal_reserved(WORD ncol)
{
	return ncol >= 16 ? 4 : 1;
}

void pal_make(WORD i, WORD ncol, UBYTE *rgb)
{
	static const UBYTE ui[4][3] = { { 0, 0, 0 }, { 255, 255, 255 }, { 170, 170, 170 }, { 0, 90, 200 } };
	const struct pal *p = &pals[i];
	WORD pen, k = 0, nres = pal_reserved(ncol);

	for (pen = 0; pen < nres; pen++)
		rgb[pen * 3] = ui[pen][0], rgb[pen * 3 + 1] = ui[pen][1], rgb[pen * 3 + 2] = ui[pen][2];
	for (pen = nres; pen < ncol; pen++) {
		LONG pos = ncol - nres > 1 ? (LONG)(pen - nres) * 255 / (ncol - nres - 1) : 0, span, t;
		const struct stop *a, *b;

		while (k < p->n - 2 && pos >= p->s[k + 1].pos)
			k++;
		a = &p->s[k], b = &p->s[k + 1];
		span = b->pos - a->pos;
		t = pos - a->pos;
		rgb[pen * 3] = a->r + ((LONG)b->r - a->r) * t / span;
		rgb[pen * 3 + 1] = a->g + ((LONG)b->g - a->g) * t / span;
		rgb[pen * 3 + 2] = a->b + ((LONG)b->b - a->b) * t / span;
	}
}
