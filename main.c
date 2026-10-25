/* amigamand: Mandelbrot for AmigaOS 3.1. Build with -DUSE_FFP for the FFP variant. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#ifdef USE_FFP
#include <proto/mathffp.h>
struct Library *MathBase;
#endif

#define W     320
#define H     256
#define DEPTH 8
#define MAXIT 128 /* must be <= (1 << DEPTH) - 1 */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

#ifdef USE_FFP
/* FFP argument order is reversed: SPSub(a,b) = b-a, SPDiv(a,b) = b/a. */
#define FSUB(a, b) SPSub(b, a)
#define FDIV(a, b) SPDiv(b, a)

static FLOAT x0, y0, dx, dy, two, four;

static void view_init(void)
{
	x0 = FDIV(SPFlt(2), SPFlt(-5));
	y0 = FDIV(SPFlt(4), SPFlt(-5));
	dx = FDIV(SPFlt(2 * W), SPFlt(7));
	dy = FDIV(SPFlt(2 * H), SPFlt(5));
	two = SPFlt(2);
	four = SPFlt(4);
}

static LONG mandel(LONG px, LONG py)
{
	FLOAT cr = SPAdd(x0, SPMul(SPFlt(px), dx));
	FLOAT ci = SPAdd(y0, SPMul(SPFlt(py), dy));
	FLOAT zr = 0, zi = 0, zr2 = 0, zi2 = 0;
	LONG i = 0;

	while (i < MAXIT && SPCmp(four, SPAdd(zr2, zi2)) >= 0) { /* sum <= 4 */
		zi = SPAdd(ci, SPMul(two, SPMul(zr, zi)));
		zr = SPAdd(cr, FSUB(zr2, zi2));
		zr2 = SPMul(zr, zr);
		zi2 = SPMul(zi, zi);
		i++;
	}
	return i;
}
#else
/* Q8.24: |z| can reach ~6.8 before the escape test, squares ~46 < 128. */
#define FB 24
#define MUL(a, b) ((LONG)(((long long)(a) * (b)) >> FB))
#define X0 (-(5L << FB) / 2)
#define Y0 (-(5L << FB) / 4)
#define DX ((7L << FB) / (2 * W))
#define DY ((5L << FB) / (2 * H))

static void view_init(void) {}

static LONG mandel(LONG px, LONG py)
{
	LONG cr = X0 + px * DX, ci = Y0 + py * DY;
	LONG zr = 0, zi = 0, zr2 = 0, zi2 = 0, i = 0;

	while (i < MAXIT && zr2 + zi2 <= (4L << FB)) {
		zi = MUL(zr, zi) * 2 + ci;
		zr = zr2 - zi2 + cr;
		zr2 = MUL(zr, zr);
		zi2 = MUL(zi, zi);
		i++;
	}
	return i;
}
#endif

/* Pen 0 stays black (inside set); the rest is a simple ramp. */
static void set_palette(struct Screen *scr)
{
	static ULONG pal[2 + 3 * (1 << DEPTH) + 1];
	ULONG n = 1 << DEPTH, i;

	pal[0] = (n << 16) | 0;
	for (i = 1; i < n; i++) {
		pal[1 + i * 3] = (i * 3 & 0xFF) * 0x01010101UL;
		pal[2 + i * 3] = (i * 5 & 0xFF) * 0x01010101UL;
		pal[3 + i * 3] = (i * 7 & 0xFF) * 0x01010101UL;
	}
	pal[1 + n * 3] = 0;
	LoadRGB32(&scr->ViewPort, pal);
}

int main(void)
{
	struct Screen *scr = NULL;
	struct Window *win = NULL;
	struct RastPort *rp;
	LONG x, y, it, done = 0, rc = 20;

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
#ifdef USE_FFP
	MathBase = OpenLibrary("mathffp.library", 33);
	if (!MathBase)
		goto out;
#endif
	if (!IntuitionBase || !GfxBase)
		goto out;
	view_init();

	scr = OpenScreenTags(NULL, SA_Width, W, SA_Height, H, SA_Depth, DEPTH,
			     SA_Quiet, TRUE, TAG_END);
	if (!scr)
		goto out;
	win = OpenWindowTags(NULL, WA_CustomScreen, scr, WA_Borderless, TRUE,
			     WA_Backdrop, TRUE, WA_Activate, TRUE,
			     WA_IDCMP, IDCMP_MOUSEBUTTONS | IDCMP_VANILLAKEY,
			     TAG_END);
	if (!win)
		goto out;
	set_palette(scr);
	rp = win->RPort;

	for (y = 0; y < H && !done; y++) {
		for (x = 0; x < W; x++) {
			it = mandel(x, y);
			SetAPen(rp, it >= MAXIT ? 0 : it);
			WritePixel(rp, x, y);
		}
		if (GetMsg(win->UserPort)) /* ponytail: leaks one msg, we exit anyway */
			done = 1;
	}
	while (!done) {
		WaitPort(win->UserPort);
		done = GetMsg(win->UserPort) != NULL;
	}
	rc = 0;
out:
	if (win)
		CloseWindow(win);
	if (scr)
		CloseScreen(scr);
#ifdef USE_FFP
	if (MathBase)
		CloseLibrary(MathBase);
#endif
	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);
	return rc;
}
