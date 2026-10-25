/* amigamand: Mandelbrot for AmigaOS 3.1. Build with -DUSE_FFP for the FFP variant. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <graphics/modeid.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#ifdef USE_FFP
#include <proto/mathffp.h>
struct Library *MathBase;
#endif

#define DEPTH 8
#define MAXIT 128 /* must be <= (1 << DEPTH) - 1 */

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

/* The view is always x -2.5..1, y -1.4..1.4: every mode below has the same aspect on a PAL monitor. */
static const struct {
	UWORD w, h;
	ULONG id;
} modes[] = {
	{ 320, 256, PAL_MONITOR_ID | LORES_KEY },
	{ 640, 256, PAL_MONITOR_ID | HIRES_KEY },
	{ 640, 512, PAL_MONITOR_ID | HIRESLACE_KEY },
};
#define DEFMODE 1

enum { CMD_REDRAW = 1, CMD_QUIT, CMD_MODE }; /* CMD_MODE + index into modes[] */

static struct NewMenu newmenu[] = {
	{ NM_TITLE, "Project", NULL, 0, 0, NULL },
	{ NM_ITEM, "Redraw", "R", 0, 0, (APTR)CMD_REDRAW },
	{ NM_ITEM, "Quit", "Q", 0, 0, (APTR)CMD_QUIT },
	{ NM_TITLE, "Mode", NULL, 0, 0, NULL },
	{ NM_ITEM, "Lores 320x256", NULL, 0, 0, (APTR)(CMD_MODE + 0) },
	{ NM_ITEM, "Hires 640x256", NULL, 0, 0, (APTR)(CMD_MODE + 1) },
	{ NM_ITEM, "Hires interlace 640x512", NULL, 0, 0, (APTR)(CMD_MODE + 2) },
	{ NM_END, NULL, NULL, 0, 0, NULL },
};

#ifdef USE_FFP
/* FFP argument order is reversed: SPSub(a,b) = b-a, SPDiv(a,b) = b/a. These give a-b and a/b. SPCmp(a,b) is +1 if a > b. */
#define FSUB(a, b) SPSub(b, a)
#define FDIV(a, b) SPDiv(b, a)

static FLOAT x0, y0, dx, dy, two, four;

static void view_init(LONG w, LONG h)
{
	x0 = FDIV(SPFlt(-5), SPFlt(2));
	y0 = FDIV(SPFlt(-7), SPFlt(5));
	dx = FDIV(SPFlt(7), SPFlt(2 * w));
	dy = FDIV(SPFlt(14), SPFlt(5 * h));
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
/* (a*b) >> 24 from one muls.l: hi<<8 | lo>>24. vbcc would call 64-bit helpers for long long. */
LONG fxmul(__reg("d0") LONG, __reg("d1") LONG) =
	"\tmuls.l\td1,d1:d0\n"
	"\tlsl.l\t#8,d1\n"
	"\trol.l\t#8,d0\n"
	"\tand.l\t#255,d0\n"
	"\tor.l\td1,d0\n";
#define MUL(a, b) fxmul(a, b)

static LONG x0, y0, dx, dy;

static void view_init(LONG w, LONG h)
{
	x0 = -(5L << (FB - 1));
	y0 = -(7L << FB) / 5;
	dx = (7L << (FB - 1)) / w;
	dy = (14L << FB) / (5 * h);
}

static LONG mandel(LONG px, LONG py)
{
	LONG cr = x0 + px * dx, ci = y0 + py * dy;
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

static struct Screen *scr;
static struct Window *win;
static struct Menu *menu;
static APTR vi;

static void close_display(void)
{
	if (win) {
		ClearMenuStrip(win);
		CloseWindow(win);
		win = NULL;
	}
	if (menu) {
		FreeMenus(menu);
		menu = NULL;
	}
	if (vi) {
		FreeVisualInfo(vi);
		vi = NULL;
	}
	if (scr) {
		CloseScreen(scr);
		scr = NULL;
	}
}

/* Returns 0, else an exit code: 100 + SA_ErrorCode if the screen fails (7 = too deep, no AGA), 24/25 window/menu. */
static LONG open_display(LONG m)
{
	ULONG err = 0;

	close_display();
	scr = OpenScreenTags(NULL, SA_Width, modes[m].w, SA_Height, modes[m].h, SA_Depth, DEPTH,
			     SA_DisplayID, modes[m].id, SA_Quiet, TRUE, SA_ErrorCode, &err, TAG_END);
	if (!scr)
		return 100 + err;
	win = OpenWindowTags(NULL, WA_CustomScreen, scr, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
			     WA_IDCMP, IDCMP_MENUPICK | IDCMP_VANILLAKEY, TAG_END);
	if (!win)
		return 24;
	vi = GetVisualInfo(scr, TAG_END);
	menu = CreateMenus(newmenu, TAG_END);
	if (!vi || !menu || !LayoutMenus(menu, vi, GTMN_NewLookMenus, TRUE, TAG_END))
		return 25;
	SetMenuStrip(win, menu);
	set_palette(scr);
	view_init(modes[m].w, modes[m].h);
	return 0;
}

int main(void)
{
	struct IntuiMessage *msg;
	LONG mode = DEFMODE, x, y = 0, it, quit = 0, rc = 20;

	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
	GadToolsBase = OpenLibrary("gadtools.library", 39);
#ifdef USE_FFP
	MathBase = OpenLibrary("mathffp.library", 33);
	if (!MathBase) {
		rc = 21;
		goto out;
	}
#endif
	if (!IntuitionBase || !GfxBase || !GadToolsBase) {
		rc = 22;
		goto out;
	}
	SetChipRev(SETCHIPREV_BEST); /* enable AGA modes if the machine has them */

	rc = open_display(mode);
	if (rc)
		goto out;

	while (!quit) {
		if (y < modes[mode].h) { /* one row, then look at the messages */
			struct RastPort *rp = win->RPort;

			for (x = 0; x < modes[mode].w; x++) {
				it = mandel(x, y);
				SetAPen(rp, it >= MAXIT ? 0 : it);
				WritePixel(rp, x, y);
			}
			y++;
		} else
			WaitPort(win->UserPort);

		while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
			ULONG cls = msg->Class;
			UWORD code = msg->Code;

			ReplyMsg((struct Message *)msg);
			if (cls == IDCMP_VANILLAKEY && code == 27) /* Esc */
				quit = 1;
			while (cls == IDCMP_MENUPICK && code != MENUNULL && !quit) {
				struct MenuItem *item = ItemAddress(menu, code);
				LONG cmd = (LONG)GTMENUITEM_USERDATA(item);

				if (cmd == CMD_QUIT) {
					quit = 1;
				} else if (cmd == CMD_REDRAW) {
					y = 0;
				} else if (cmd >= CMD_MODE) {
					mode = cmd - CMD_MODE;
					rc = open_display(mode);
					if (rc)
						goto out;
					y = 0;
					break; /* win and menu are new */
				}
				code = item->NextSelect;
			}
		}
	}
	rc = 0;
out:
	close_display();
#ifdef USE_FFP
	if (MathBase)
		CloseLibrary(MathBase);
#endif
	if (GadToolsBase)
		CloseLibrary(GadToolsBase);
	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);
	return rc;
}
