/* amigamand: Mandelbrot and Julia sets for AmigaOS 3.1 (AGA, or any chip set with fewer colours).
   Build with -DUSE_FPU -fpu=68881 for the FPU variant (it needs a 68881/68882/68040+). */
#include <exec/types.h>
#include <exec/execbase.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <intuition/intuition.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <graphics/displayinfo.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/asl.h>
#include <proto/icon.h>
#include <string.h>
#include "view.h"
#include "palette.h"
#include "ilbm.h"
#include "rexx.h"
#include "c2p.h"

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase, *AslBase, *IconBase;

/* ---- screen modes ---- */

struct mode {
	UWORD w, h;
	ULONG id;
	WORD depth, ax, ay; /* a pixel is ax wide and ay high (relative) */
};
#define NPRESET 3
#define OTHER_MODE 3
static struct mode modes[4]; /* the presets, then the one picked with the ASL requester */
static WORD mode = 1;

/* PAL or NTSC, as the machine is: the same three modes, 256/512 or 200/400 lines. */
static void init_modes(void)
{
	BOOL pal = (GfxBase->DisplayFlags & PAL) != 0;
	ULONG mon = pal ? PAL_MONITOR_ID : NTSC_MONITOR_ID;
	UWORD h = pal ? 256 : 200;

	modes[0].w = 320, modes[0].h = h, modes[0].id = mon | LORES_KEY, modes[0].ax = 1, modes[0].ay = 1;
	modes[1].w = 640, modes[1].h = h, modes[1].id = mon | HIRES_KEY, modes[1].ax = 1, modes[1].ay = 2;
	modes[2].w = 640, modes[2].h = 2 * h, modes[2].id = mon | HIRESLACE_KEY, modes[2].ax = 1, modes[2].ay = 1;
	modes[0].depth = modes[1].depth = modes[2].depth = 8;
}

/* ---- commands: one number for every menu item, key and ARexx action ---- */

enum {
	ACT_REDRAW = 1, ACT_ABORT, ACT_SAVE, ACT_QUIT, ACT_ZIN, ACT_ZOUT, ACT_RESET, ACT_MANDEL, ACT_JULIA, ACT_OTHER,
	ACT_CYCLE, ACT_FAST,
	ACT_MODE = 20,  /* + 0..2 */
	ACT_PAL = 30,   /* + 0..NPAL-1 */
	ACT_ITER = 40,  /* + 0..2 */
};
static const WORD iters[3] = { 64, 128, 255 };

#define ITEM(label, key, cmd) { NM_ITEM, label, key, 0, 0, (APTR)(cmd) }
#define RADIO(label, cmd, excl) { NM_ITEM, label, NULL, CHECKIT, excl, (APTR)(cmd) }
static struct NewMenu newmenu[] = {
	{ NM_TITLE, "Project", NULL, 0, 0, NULL },
	ITEM("Redraw", "R", ACT_REDRAW),
	ITEM("Abort", "A", ACT_ABORT),
	ITEM("Save ILBM...", "S", ACT_SAVE),
	ITEM("Quit", "Q", ACT_QUIT),
	{ NM_TITLE, "View", NULL, 0, 0, NULL },
	ITEM("Zoom in", "+", ACT_ZIN),
	ITEM("Zoom out", "-", ACT_ZOUT),
	ITEM("Reset view", "0", ACT_RESET),
	ITEM("Mandelbrot set", "M", ACT_MANDEL),
	ITEM("Julia set at last click", "J", ACT_JULIA),
	{ NM_TITLE, "Mode", NULL, 0, 0, NULL },
	RADIO("Lores", ACT_MODE + 0, 6),
	RADIO("Hires", ACT_MODE + 1, 5),
	RADIO("Hires interlace", ACT_MODE + 2, 3),
	ITEM("Other...", "O", ACT_OTHER),
	{ NM_TITLE, "Palette", NULL, 0, 0, NULL },
	RADIO("Rainbow", ACT_PAL + 0, 0x1E),
	RADIO("Fire", ACT_PAL + 1, 0x1D),
	RADIO("Ocean", ACT_PAL + 2, 0x1B),
	RADIO("Gray", ACT_PAL + 3, 0x17),
	RADIO("Electric", ACT_PAL + 4, 0x0F),
	{ NM_ITEM, "Cycle colours", "C", CHECKIT | MENUTOGGLE, 0, (APTR)ACT_CYCLE },
	{ NM_TITLE, "Detail", NULL, 0, 0, NULL },
	RADIO("64 iterations", ACT_ITER + 0, 6),
	RADIO("128 iterations", ACT_ITER + 1, 5),
	RADIO("255 iterations", ACT_ITER + 2, 3),
	{ NM_ITEM, "Fast fill", "F", CHECKIT | MENUTOGGLE, 0, (APTR)ACT_FAST },
	{ NM_END, NULL, NULL, 0, 0, NULL },
};

static void mark(LONG cmd, BOOL on)
{
	struct NewMenu *p;

	for (p = newmenu; p->nm_Type != NM_END; p++)
		if (p->nm_UserData == (APTR)cmd)
			p->nm_Flags = on ? p->nm_Flags | CHECKED : p->nm_Flags & ~CHECKED;
}

/* ---- state ---- */

static struct Screen *scr;
static struct Window *win;
static struct Menu *menu;
static APTR vi;
static UBYTE *img; /* the picture as pens */
static WORD cdepth, ncol;
static UBYTE lut[256];       /* iteration count -> pen */
static UBYTE rgb[768], cur_rgb[768];
static WORD pal_idx, cyc_off;
static BOOL cycle;
static BOOL rendering, dragging, quitting, have_last, menu_open;
static WORD sym; /* view_symmetric(): 0 none, 1 mirrored about the real axis, 2 turned half way round */
static WORD pass, rk, hh, rows_done;
/* Mariani-Silver: tiles, each a stack of rectangles whose border is tested for one colour */
#define TILE 32
#define UNSET 255 /* a pen that is never used, marks a pixel not computed yet */
struct rect {
	WORD x0, y0, x1, y1;
};
static struct rect stk[48];
static WORD sp, tile_x, tile_y, tx0, ty0, tx1, ty1, tiles_done, tiles_total;
static BOOL fast = TRUE, use_ms, preview_pending, tile_open, tile_solid;
static UBYTE solid_pen;
static LONG t_start, t_elapsed, exit_rc;
static num last_re, last_im;
static WORD sx, sy, bx, by, bw; /* mouse drag: the start, and the box shown (corner, width) */
static BOOL band_on;
static char status[48];

static struct {
	WORD mode, iter, julia, quit, exact, depth;
	char cx[40], cy[40], span[40], jr[40], ji[40], pal[24], save[100];
} opt = { -1, 0 };

static LONG ticks(void)
{
	struct DateStamp d;

	DateStamp(&d);
	return d.ds_Minute * 3000L + d.ds_Tick; /* 1/50 s */
}

static char *cat(char *p, const char *s)
{
	while ((*p = *s++))
		p++;
	return p;
}

static char *put_tenths(char *p, LONG t)
{
	p = put_uint(p, t / 10);
	*p++ = '.';
	*p++ = '0' + t % 10;
	return p;
}

/* The screen title is only visible while the menu is open, but it is the one place that needs no space. */
static void show_title(void)
{
	char b[160], *p = b;

	if (!win)
		return;
	p = cat(p, V.julia ? "Julia " : "Mandelbrot ");
	p = put_uint(p, V.w), *p++ = 'x', p = put_uint(p, V.h), *p++ = 'x', p = put_uint(p, cdepth);
	p = cat(p, " span ");
	num_fmt(V.span, p);
	p += strlen(p);
	*p++ = ' ', p = put_uint(p, V.maxit), p = cat(p, " it  ");
	if (rendering) {
		p = put_uint(p, use_ms ? (tiles_total ? tiles_done * 100 / tiles_total : 0) : (hh ? rows_done * 100 / hh : 0));
		p = cat(p, "%  ");
		p = put_tenths(p, (ticks() - t_start) / 5);
	} else
		p = put_tenths(p, t_elapsed / 5);
	p = cat(p, "s  ");
	p = cat(p, status);
	SetWindowTitles(win, (UBYTE *)-1L, (UBYTE *)b);
}

/* ---- drawing ---- */

static void build_lut(void)
{
	WORD it, nres = pal_reserved(ncol);

	for (it = 0; it < V.maxit; it++)
		lut[it] = nres + it * (ncol - nres - 1) / V.maxit;
	lut[V.maxit] = 0; /* inside the set */
}

static void start_render(void)
{
	sym = view_symmetric();
	hh = sym ? (V.h + 1) / 2 : V.h;
	pass = rk = rows_done = 0;
	rendering = TRUE;
	status[0] = 0;
	t_start = ticks();
	build_lut();
	/* The fill pays off for the Mandelbrot set, zoomed in, where whole regions are one colour and each pixel is dear.
	   In an overview the pixels are cheap and the row loop wins; a Julia set is half the work by its symmetry. */
	use_ms = fast && !V.julia && view_zoomed();
	if (use_ms) {
		memset(img, UNSET, (LONG)V.w * V.h);
		preview_pending = TRUE;
		tile_open = FALSE;
		tile_x = tile_y = tiles_done = 0;
		tiles_total = ((V.w + TILE - 1) / TILE) * ((hh + TILE - 1) / TILE);
	}
	show_title();
}

/* Passes of rows 8k, 8k+4, 4k+2, 2k+1: a coarse picture first, each row repeated over the rows not yet drawn. */
static BOOL next_row(WORD *y, WORD *rep)
{
	static const WORD mul[4] = { 8, 8, 4, 2 }, add[4] = { 0, 4, 2, 1 }, reps[4] = { 8, 4, 2, 1 };

	while (pass < 4) {
		LONG yy = (LONG)rk * mul[pass] + add[pass];

		if (yy < hh) {
			*y = yy;
			*rep = reps[pass];
			if (yy + *rep > hh)
				*rep = hh - yy;
			rk++;
			return TRUE;
		}
		pass++, rk = 0;
	}
	return FALSE;
}

static LONG do_save(const char *name);
#ifdef TEST_MOUSE
static void mouse_move(WORD mx, WORD my);
static void mouse_up(WORD mx, WORD my);
#endif

static void render_done(void)
{
	rendering = FALSE;
	t_elapsed = ticks() - t_start;
	show_title();
	rexx_wake();
	if (opt.quit) { /* batch run: report the time on stdout, so a script can read it */
		LONG a[1];

		a[0] = t_elapsed;
		VPrintf("render %ld ticks\n", a);
	}
	if (opt.save[0]) {
		do_save(opt.save);
		opt.save[0] = 0;
	}
	if (opt.quit)
		quitting = TRUE;
#ifdef TEST_MOUSE /* a test build: the first render ends with a mouse gesture, through the handlers the mouse uses */
	{
		static WORD gestures;

		if (gestures++ == 0) {
			dragging = TRUE;
#if TEST_MOUSE == 1 /* drag a box from (60,40) with a corner at (140,60) */
			sx = 60, sy = 40;
			view_point(sx, sy, &last_re, &last_im);
			have_last = TRUE;
			mouse_move(100, 50);
			mouse_move(140, 60);
			mouse_up(140, 60);
#else /* click at (200,100) */
			sx = 200, sy = 100;
			mouse_up(201, 100);
#endif
		}
	}
#endif
}

/* A block of pens to the screen: our own conversion when it is 8-pixel aligned, graphics.library otherwise. */
static void put_chunky(WORD x, WORD y, WORD w, WORD h, const UBYTE *src)
{
	if (((x | w) & 7) == 0)
		c2p_rect(src, V.w, scr->RastPort.BitMap, x, y, w, h);
	else
		WriteChunkyPixels(win->RPort, x, y, x + w - 1, y + h - 1, (UBYTE *)src, V.w);
}

static void row_step(void)
{
	WORD y, rep, r, x, w = V.w;
	UBYTE *dst;

	if (!next_row(&y, &rep)) {
		render_done();
		return;
	}
	dst = img + (LONG)y * w;
	view_row(y, dst, lut);
	for (r = 1; r < rep; r++)
		CopyMem(dst, dst + (LONG)r * w, w);
	put_chunky(0, y, w, rep, dst);
	if (sym) { /* the same rows again, mirrored about the real axis */
		WORD my = V.h - y - rep;
		UBYTE *m = img + (LONG)my * w;

		for (r = 0; r < rep; r++) {
			UBYTE *q = m + (LONG)r * w;

			if (sym == 1)
				CopyMem(dst, q, w);
			else
				for (x = 0; x < w; x++)
					q[x] = dst[w - 1 - x];
		}
		put_chunky(0, my, w, rep, m);
	}
	if ((++rows_done & 15) == 0)
		show_title();
}

/* the pen of a pixel, computed the first time it is asked for */
static UBYTE pix(WORD x, WORD y)
{
	UBYTE *p = img + (LONG)y * V.w + x;

	if (*p == UNSET)
		*p = view_pen(x, y, lut);
	return *p;
}

/* One coarse sample for each 16x16 block, drawn at once: something to look at while the tiles are computed. */
static void preview(void)
{
	struct RastPort *rp = win->RPort;
	WORD bx, by, x1, y1;

	for (by = 0; by < hh; by += 16)
		for (bx = 0; bx < V.w; bx += 16) {
			x1 = bx + 15 < V.w ? bx + 15 : V.w - 1;
			y1 = by + 15 < hh ? by + 15 : hh - 1;
			SetAPen(rp, pix((bx + x1) / 2, (by + y1) / 2));
			RectFill(rp, bx, by, x1, y1);
			if (sym)
				RectFill(rp, bx, V.h - 1 - y1, x1, V.h - 1 - by);
		}
}

static void push(WORD x0, WORD y0, WORD x1, WORD y1)
{
	stk[sp].x0 = x0, stk[sp].y0 = y0, stk[sp].x1 = x1, stk[sp].y1 = y1;
	sp++;
}

/* One rectangle: if its whole border has one pen so has the inside (the set is connected), else cut it in four. */
static void ms_rect(WORD x0, WORD y0, WORD x1, WORD y1, BOOL root)
{
	WORD x, y, xm, ym;
	UBYTE p;
	BOOL same = TRUE;

	if (x1 - x0 < 3 || y1 - y0 < 3) { /* too small to cut: the few pixels left */
		for (y = y0; y <= y1; y++)
			for (x = x0; x <= x1; x++)
				pix(x, y);
		return;
	}
	p = pix(x0, y0);
	for (x = x0; x <= x1 && same; x++)
		if (pix(x, y0) != p || pix(x, y1) != p)
			same = FALSE;
	for (y = y0 + 1; y < y1 && same; y++)
		if (pix(x0, y) != p || pix(x1, y) != p)
			same = FALSE;
	if (same) {
		for (y = y0 + 1; y < y1; y++)
			memset(img + (LONG)y * V.w + x0 + 1, p, x1 - x0 - 1);
		if (root)
			tile_solid = TRUE, solid_pen = p;
		return;
	}
	xm = (x0 + x1) / 2, ym = (y0 + y1) / 2;
	push(x0, y0, xm, ym), push(xm, y0, x1, ym), push(x0, ym, xm, y1), push(xm, ym, x1, y1);
}

/* A finished tile goes to the screen in one call (or one blitter fill), and its mirror image with it. */
static void flush_tile(void)
{
	struct RastPort *rp = win->RPort;
	WORD y, w = V.w, tw = tx1 - tx0 + 1, my0 = V.h - 1 - ty1, my1 = V.h - 1 - ty0;

	if (sym)
		for (y = ty0; y <= ty1; y++)
			CopyMem(img + (LONG)y * w + tx0, img + (LONG)(V.h - 1 - y) * w + tx0, tw);
	if (tile_solid) {
		SetAPen(rp, solid_pen);
		RectFill(rp, tx0, ty0, tx1, ty1);
		if (sym)
			RectFill(rp, tx0, my0, tx1, my1);
	} else {
		put_chunky(tx0, ty0, tw, ty1 - ty0 + 1, img + (LONG)ty0 * w + tx0);
		if (sym)
			put_chunky(tx0, my0, tw, ty1 - ty0 + 1, img + (LONG)my0 * w + tx0);
	}
}

static void ms_step(void)
{
	if (preview_pending) {
		preview();
		preview_pending = FALSE;
		return;
	}
	if (!tile_open) {
		if (tile_y >= hh) {
			render_done();
			return;
		}
		tx0 = tile_x, ty0 = tile_y;
		tx1 = (tx0 + TILE < V.w ? tx0 + TILE : V.w) - 1;
		ty1 = (ty0 + TILE < hh ? ty0 + TILE : hh) - 1;
		tile_x += TILE;
		if (tile_x >= V.w)
			tile_x = 0, tile_y += TILE;
		tile_open = TRUE, tile_solid = FALSE;
		sp = 0;
		ms_rect(tx0, ty0, tx1, ty1, TRUE); /* the root test decides if the tile is one colour */
	} else {
		sp--;
		ms_rect(stk[sp].x0, stk[sp].y0, stk[sp].x1, stk[sp].y1, FALSE);
	}
	if (sp == 0) {
		flush_tile();
		tile_open = FALSE;
		if ((++tiles_done & 3) == 0)
			show_title();
	}
}

static void render_step(void)
{
	if (use_ms)
		ms_step();
	else
		row_step();
}

static void pal_apply(void)
{
	static ULONG t[3 * 256 + 2];
	WORD i, src, nres = pal_reserved(ncol);

	for (i = 0; i < ncol; i++) {
		src = i < nres ? i : nres + (i - nres + cyc_off) % (ncol - nres); /* only the gradient turns */
		cur_rgb[i * 3] = rgb[src * 3], cur_rgb[i * 3 + 1] = rgb[src * 3 + 1], cur_rgb[i * 3 + 2] = rgb[src * 3 + 2];
		t[1 + i * 3] = cur_rgb[i * 3] * 0x01010101UL;
		t[2 + i * 3] = cur_rgb[i * 3 + 1] * 0x01010101UL;
		t[3 + i * 3] = cur_rgb[i * 3 + 2] * 0x01010101UL;
	}
	t[0] = ((ULONG)ncol << 16) | 0;
	t[1 + ncol * 3] = 0;
	LoadRGB32(&scr->ViewPort, t);
}

static void set_palette(WORD i)
{
	pal_idx = i;
	pal_make(i, ncol, rgb);
	pal_apply();
}

/* ---- display ---- */

static BOOL menu_build(void)
{
	WORD i;

	if (win)
		ClearMenuStrip(win);
	if (menu) {
		FreeMenus(menu);
		menu = NULL;
	}
	for (i = 0; i < NPRESET; i++)
		mark(ACT_MODE + i, mode == i);
	for (i = 0; i < NPAL; i++)
		mark(ACT_PAL + i, pal_idx == i);
	for (i = 0; i < 3; i++)
		mark(ACT_ITER + i, V.maxit == iters[i]);
	mark(ACT_CYCLE, cycle);
	mark(ACT_FAST, fast);
	menu = CreateMenus(newmenu, TAG_END);
	if (!menu || !LayoutMenus(menu, vi, GTMN_NewLookMenus, TRUE, TAG_END))
		return FALSE;
	SetMenuStrip(win, menu);
	return TRUE;
}

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

/* Returns 0, else an exit code: 100 + SA_ErrorCode if the screen fails, 24 window, 25 menu, 27 memory. */
static LONG open_display(WORD m)
{
	struct mode *md = &modes[m];
	ULONG err = 0;

	close_display();
	cdepth = md->depth;
	for (;;) { /* a chip set that cannot do this many planes gets fewer colours */
		scr = OpenScreenTags(NULL, SA_Width, md->w, SA_Height, md->h, SA_Depth, cdepth, SA_DisplayID, md->id,
				     SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_ErrorCode, &err, TAG_END);
		if (scr || err != OSERR_TOODEEP || cdepth <= 2)
			break;
		cdepth--;
	}
	if (!scr)
		return 100 + err;
	win = OpenWindowTags(NULL, WA_CustomScreen, scr, WA_Borderless, TRUE, WA_Backdrop, TRUE, WA_Activate, TRUE,
			     WA_IDCMP, IDCMP_MENUPICK | IDCMP_VANILLAKEY | IDCMP_RAWKEY | IDCMP_MOUSEBUTTONS |
			     IDCMP_MOUSEMOVE | IDCMP_INTUITICKS | IDCMP_MENUVERIFY, TAG_END);
	if (!win)
		return 24;
	vi = GetVisualInfo(scr, TAG_END);
	if (img)
		FreeVec(img);
	img = AllocVec((LONG)md->w * md->h, MEMF_PUBLIC | MEMF_CLEAR);
	if (!img)
		return 27;
	ncol = 1 << cdepth;
	mode = m;
	view_mode(md->w, md->h, md->ax, md->ay);
	set_palette(pal_idx);
	rendering = FALSE;
	if (!vi || !menu_build())
		return 25;
	return 0;
}

/* Switch screen mode, keeping the view. If it fails the old mode comes back. */
static LONG act_mode(WORD m)
{
	WORD old = mode;
	LONG rc = open_display(m);

	if (rc) {
		mode = old;
		if (open_display(old)) {
			exit_rc = rc;
			quitting = TRUE;
		}
		strcpy(status, "mode not available");
		show_title();
		return 10;
	}
	status[0] = 0;
	start_render();
	return 0;
}

/* ---- saving and requesters ---- */

static LONG do_save(const char *name)
{
	struct mode *md = &modes[mode];
	LONG e = ilbm_save(name, img, V.w, V.h, cdepth, md->id, md->ax, md->ay, cur_rgb);

	strcpy(status, e ? "save failed" : "saved");
	show_title();
	return e;
}

static void act_save_ui(void)
{
	struct FileRequester *fr;
	char path[200];

	if (!AslBase)
		AslBase = OpenLibrary("asl.library", 37);
	if (AslBase && (fr = AllocAslRequestTags(ASL_FileRequest, ASLFR_Screen, scr, ASLFR_Activate, TRUE, ASLFR_TitleText, "Save ILBM",
						 ASLFR_DoSaveMode, TRUE, ASLFR_InitialFile, "amigamand.iff",
						 ASLFR_InitialDrawer, "RAM:", TAG_END))) {
		if (AslRequest(fr, NULL)) {
			strcpy(path, fr->fr_Drawer);
			AddPart(path, fr->fr_File, sizeof path);
			do_save(path);
		}
		FreeAslRequest(fr);
	} else
		do_save("RAM:amigamand.iff");
}

static LONG act_other_mode(void)
{
	struct ScreenModeRequester *smr;
	struct DisplayInfo di;
	LONG rc = 10;

	memset(&di, 0, sizeof di);
	if (!AslBase)
		AslBase = OpenLibrary("asl.library", 37);
	if (!AslBase) {
		strcpy(status, "no asl.library");
		show_title();
		return rc;
	}
	smr = AllocAslRequestTags(ASL_ScreenModeRequest, ASLSM_Screen, scr, ASLSM_Activate, TRUE, ASLSM_TitleText, "Screen mode",
				  ASLSM_DoWidth, TRUE, ASLSM_DoHeight, TRUE, ASLSM_DoDepth, TRUE, ASLSM_MinDepth, 2,
				  ASLSM_MaxDepth, 8, ASLSM_InitialDisplayID, modes[mode].id,
				  ASLSM_InitialDisplayWidth, modes[mode].w, ASLSM_InitialDisplayHeight, modes[mode].h,
				  ASLSM_InitialDisplayDepth, cdepth, TAG_END);
	if (smr && AslRequest(smr, NULL)) {
		struct mode *c = &modes[OTHER_MODE];

		c->w = smr->sm_DisplayWidth, c->h = smr->sm_DisplayHeight, c->id = smr->sm_DisplayID;
		c->depth = smr->sm_DisplayDepth, c->ax = c->ay = 1;
		if (GetDisplayInfoData(NULL, (UBYTE *)&di, sizeof di, DTAG_DISP, c->id) > 0 && di.Resolution.x &&
		    di.Resolution.y)
			c->ax = di.Resolution.x, c->ay = di.Resolution.y; /* ticks per pixel: a pixel is x wide, y high */
		FreeAslRequest(smr);
		return act_mode(OTHER_MODE);
	}
	if (smr)
		FreeAslRequest(smr);
	return 0;
}

/* ---- actions ---- */

static void act_palette(WORD i)
{
	set_palette(i);
	menu_build();
}

static void act_iter(WORD n)
{
	V.maxit = n;
	menu_build();
	start_render();
}

static void act_julia_last(void)
{
	if (have_last)
		view_julia(last_re, last_im);
	else
		view_julia(V.jr, V.ji);
	start_render();
}

static void act_cycle(BOOL on)
{
	cycle = on;
	menu_build();
}

static void do_menu_cmd(LONG cmd)
{
	if (cmd >= ACT_ITER)
		act_iter(iters[cmd - ACT_ITER]);
	else if (cmd >= ACT_PAL)
		act_palette(cmd - ACT_PAL);
	else if (cmd >= ACT_MODE)
		act_mode(cmd - ACT_MODE);
	else
		switch (cmd) {
		case ACT_REDRAW: start_render(); break;
		case ACT_ABORT: rendering = FALSE; strcpy(status, "aborted"); show_title(); rexx_wake(); break;
		case ACT_SAVE: act_save_ui(); break;
		case ACT_QUIT: quitting = TRUE; break;
		case ACT_ZIN: view_scale(1, 2); start_render(); break;
		case ACT_ZOUT: view_scale(2, 1); start_render(); break;
		case ACT_RESET: view_reset(); start_render(); break;
		case ACT_MANDEL: view_mandel(); start_render(); break;
		case ACT_JULIA: act_julia_last(); break;
		case ACT_OTHER: act_other_mode(); break;
		case ACT_CYCLE: act_cycle(!cycle); break;
		case ACT_FAST: fast = !fast; menu_build(); start_render(); break;
		}
}

/* ---- ARexx ---- */

static char up(char c)
{
	return c >= 'a' && c <= 'z' ? c - 32 : c;
}

static BOOL eq(const char *a, const char *b) /* the word at a is b, ignoring case */
{
	for (; *b; a++, b++)
		if (up(*a) != up(*b))
			return FALSE;
	return *a == 0 || *a == ' ' || *a == '\t';
}

static LONG to_long(const char *s)
{
	LONG v = 0;

	while (*s >= '0' && *s <= '9')
		v = v * 10 + *s++ - '0';
	return v;
}

static const char *skip(const char *s)
{
	while (*s == ' ' || *s == '\t')
		s++;
	return s;
}

static const char *word(const char *s) /* past the first word */
{
	while (*s && *s != ' ' && *s != '\t')
		s++;
	return skip(s);
}

static LONG do_command(const char *line, char *res, LONG n)
{
	const char *a = word(line);
	num x, y, s;

	line = skip(line);
	if (eq(line, "RENDER"))
		start_render();
	else if (eq(line, "ABORT"))
		do_menu_cmd(ACT_ABORT);
	else if (eq(line, "WAIT"))
		return rendering ? RC_DEFER : 0;
	else if (eq(line, "RESET"))
		do_menu_cmd(ACT_RESET);
	else if (eq(line, "ZOOM")) {
		if (!(a = num_parse(a, &x)) || !(a = num_parse(a, &y)) || !num_parse(a, &s))
			return 10;
		view_set(x, y, s);
		start_render();
	} else if (eq(line, "CENTER")) {
		if (!(a = num_parse(a, &x)) || !num_parse(a, &y))
			return 10;
		view_set(x, y, V.span);
		start_render();
	} else if (eq(line, "SPAN")) {
		if (!num_parse(a, &s))
			return 10;
		view_set(V.cx, V.cy, s);
		start_render();
	} else if (eq(line, "MODE")) {
		LONG m = *a - '0';

		if (m < 0 || m > 3 || (m == OTHER_MODE && !modes[OTHER_MODE].w))
			return 10;
		return act_mode(m);
	} else if (eq(line, "PALETTE")) {
		WORD i;

		if (*a >= '0' && *a < '0' + NPAL)
			i = *a - '0';
		else
			for (i = 0; i < NPAL && !eq(a, pal_name(i)); i++)
				;
		if (i == NPAL)
			return 10;
		act_palette(i);
	} else if (eq(line, "CYCLE"))
		act_cycle(!eq(a, "OFF"));
	else if (eq(line, "FAST")) {
		fast = !eq(a, "OFF");
		menu_build();
		start_render();
	}
	else if (eq(line, "ITER")) {
		LONG v = to_long(a);

		if (v < 1 || v > MAXIT_LIMIT)
			return 10;
		act_iter(v);
	} else if (eq(line, "JULIA")) {
		if (*a) {
			if (!(a = num_parse(a, &x)) || !num_parse(a, &y))
				return 10;
			view_julia(x, y);
			start_render();
		} else
			act_julia_last();
	} else if (eq(line, "MANDELBROT")) {
		view_mandel();
		start_render();
	} else if (eq(line, "SAVE")) {
		char name[200];
		WORD k = 0;

		if (*a == '"')
			a++;
		while (*a && *a != '"' && k < 199)
			name[k++] = *a++;
		name[k] = 0;
		return k && !do_save(name) ? 0 : 10;
	} else if (eq(line, "QUERY")) {
		char *p = res;

		p = cat(p, V.julia ? "JULIA " : "MANDELBROT ");
		num_fmt(V.cx, p), p += strlen(p), *p++ = ' ';
		num_fmt(V.cy, p), p += strlen(p), *p++ = ' ';
		num_fmt(V.span, p), p += strlen(p), *p++ = ' ';
		p = put_uint(p, V.maxit), *p++ = ' ';
		p = put_uint(p, V.w), *p++ = 'x', p = put_uint(p, V.h), *p++ = 'x', p = put_uint(p, cdepth), *p++ = ' ';
		p = cat(p, pal_name(pal_idx)), *p++ = ' ';
		p = cat(p, rendering ? "RENDERING" : "DONE");
		*p++ = ' ';
		p = put_uint(p, t_elapsed); /* ticks of the last finished render, 1/50 s */
		*p = 0;
	} else if (eq(line, "QUIT"))
		quitting = TRUE;
	else {
		strcpy(res, "unknown command");
		return 10;
	}
	return 0;
}

/* ---- mouse, keys ---- */


static void band(void) /* draws or erases the zoom box */
{
	struct RastPort *rp = win->RPort;
	WORD bh = (LONG)bw * V.h / V.w;

	SetDrMd(rp, COMPLEMENT);
	Move(rp, bx, by);
	Draw(rp, bx + bw, by), Draw(rp, bx + bw, by + bh), Draw(rp, bx, by + bh), Draw(rp, bx, by);
	SetDrMd(rp, JAM1);
	band_on = !band_on;
}

static void mouse_move(WORD mx, WORD my)
{
	WORD w = mx - sx;

	if (band_on)
		band();
	if (w < 0)
		w = -w;
	bw = w;
	bx = mx >= sx ? sx : sx - bw;
	by = my >= sy ? sy : sy - (LONG)bw * V.h / V.w;
	if (bw >= 6)
		band();
}

static void mouse_up(WORD mx, WORD my)
{
	WORD w = mx - sx;

	if (band_on)
		band();
	ReportMouse(FALSE, win);
	dragging = FALSE;
	if (w < 0)
		w = -w;
	if (w < 6)
		view_zoom_at(sx, sy, V.w); /* a click centres the view there */
	else
		view_zoom_at(bx + bw / 2, by + (WORD)((LONG)bw * V.h / V.w / 2), bw);
	start_render();
}

static void key(UWORD code)
{
	switch (code) {
	case 27: quitting = TRUE; break;
	case '+': case '=': do_menu_cmd(ACT_ZIN); break;
	case '-': do_menu_cmd(ACT_ZOUT); break;
	case '0': do_menu_cmd(ACT_RESET); break;
	case 'j': do_menu_cmd(ACT_JULIA); break;
	case 'm': do_menu_cmd(ACT_MANDEL); break;
	case 'r': do_menu_cmd(ACT_REDRAW); break;
	case 's': do_menu_cmd(ACT_SAVE); break;
	case 'c': do_menu_cmd(ACT_CYCLE); break;
	case ' ': do_menu_cmd(ACT_ABORT); break;
	}
}

static void handle(struct IntuiMessage *msg)
{
	ULONG cls = msg->Class;
	UWORD code = msg->Code;
	WORD mx = msg->MouseX, my = msg->MouseY, n = 0, i;
	LONG picks[8];

	if (cls == IDCMP_MENUPICK) /* take the whole chain first: acting on a pick rebuilds the menu */
		while (code != MENUNULL && n < 8) {
			struct MenuItem *it = ItemAddress(menu, code);

			picks[n++] = (LONG)GTMENUITEM_USERDATA(it);
			code = it->NextSelect;
		}
	ReplyMsg((struct Message *)msg);
	switch (cls) {
	case IDCMP_MENUVERIFY: /* a menu is about to open on the screen: stop drawing into it */
		menu_open = TRUE;
		break;
	case IDCMP_MENUPICK:
		menu_open = FALSE;
		for (i = 0; i < n && !quitting; i++)
			do_menu_cmd(picks[i]);
		break;
	case IDCMP_VANILLAKEY:
		key(code);
		break;
	case IDCMP_RAWKEY:
		if (!(code & 0x80))
			switch (code) {
			case 0x4C: view_pan(0, -1); start_render(); break;
			case 0x4D: view_pan(0, 1); start_render(); break;
			case 0x4E: view_pan(1, 0); start_render(); break;
			case 0x4F: view_pan(-1, 0); start_render(); break;
			}
		break;
	case IDCMP_MOUSEBUTTONS:
		if (code == SELECTDOWN) {
			dragging = TRUE;
			sx = mx, sy = my;
			view_point(mx, my, &last_re, &last_im);
			have_last = TRUE;
			ReportMouse(TRUE, win);
		} else if (code == SELECTUP && dragging)
			mouse_up(mx, my);
		break;
	case IDCMP_MOUSEMOVE:
		if (dragging)
			mouse_move(mx, my);
		break;
	case IDCMP_INTUITICKS:
		if (cycle && ncol - pal_reserved(ncol) > 1) {
			cyc_off = (cyc_off + 1) % (ncol - pal_reserved(ncol));
			pal_apply();
		}
		break;
	}
}

/* ---- start-up options: command line (ReadArgs) or Workbench tooltypes ---- */

static void setopt(char *dst, const char *src, WORD n)
{
	if (src) {
		strncpy(dst, src, n - 1);
		dst[n - 1] = 0;
	}
}

static void cli_opts(void)
{
	LONG a[13] = { 0 };
	struct RDArgs *rda = ReadArgs("MODE/K/N,CX/K,CY/K,SPAN/K,ITER/K/N,PALETTE/K,JULIA/S,JR/K,JI/K,SAVE/K,QUIT/S,EXACT/S,DEPTH/K/N", a, NULL);

	if (!rda) {
		PrintFault(IoErr(), "amigamand");
		exit_rc = 10;
		quitting = TRUE;
		return;
	}
	if (a[0])
		opt.mode = *(LONG *)a[0];
	setopt(opt.cx, (char *)a[1], sizeof opt.cx), setopt(opt.cy, (char *)a[2], sizeof opt.cy);
	setopt(opt.span, (char *)a[3], sizeof opt.span);
	if (a[4])
		opt.iter = *(LONG *)a[4];
	setopt(opt.pal, (char *)a[5], sizeof opt.pal);
	opt.julia = a[6] != 0;
	setopt(opt.jr, (char *)a[7], sizeof opt.jr), setopt(opt.ji, (char *)a[8], sizeof opt.ji);
	setopt(opt.save, (char *)a[9], sizeof opt.save);
	opt.quit = a[10] != 0;
	opt.exact = a[11] != 0;
	if (a[12])
		opt.depth = *(LONG *)a[12];
	FreeArgs(rda);
}

static void wb_opts(struct WBStartup *wbs)
{
	struct DiskObject *dob;
	BPTR old;
	char **tt, *v;

	IconBase = OpenLibrary("icon.library", 33);
	if (!IconBase || wbs->sm_NumArgs < 1)
		return;
	old = CurrentDir(wbs->sm_ArgList[0].wa_Lock);
	dob = GetDiskObject(wbs->sm_ArgList[0].wa_Name);
	CurrentDir(old);
	if (!dob)
		return;
	tt = (char **)dob->do_ToolTypes;
	if ((v = (char *)FindToolType((UBYTE **)tt, "MODE")))
		opt.mode = *v - '0';
	if ((v = (char *)FindToolType((UBYTE **)tt, "ITER")))
		opt.iter = to_long(v);
	setopt(opt.cx, (char *)FindToolType((UBYTE **)tt, "CX"), sizeof opt.cx);
	setopt(opt.cy, (char *)FindToolType((UBYTE **)tt, "CY"), sizeof opt.cy);
	setopt(opt.span, (char *)FindToolType((UBYTE **)tt, "SPAN"), sizeof opt.span);
	setopt(opt.pal, (char *)FindToolType((UBYTE **)tt, "PALETTE"), sizeof opt.pal);
	setopt(opt.jr, (char *)FindToolType((UBYTE **)tt, "JR"), sizeof opt.jr);
	setopt(opt.ji, (char *)FindToolType((UBYTE **)tt, "JI"), sizeof opt.ji);
	setopt(opt.save, (char *)FindToolType((UBYTE **)tt, "SAVE"), sizeof opt.save);
	opt.julia = FindToolType((UBYTE **)tt, "JULIA") != NULL;
	opt.quit = FindToolType((UBYTE **)tt, "QUIT") != NULL;
	opt.exact = FindToolType((UBYTE **)tt, "EXACT") != NULL;
	if ((v = (char *)FindToolType((UBYTE **)tt, "DEPTH")))
		opt.depth = to_long(v);
	FreeDiskObject(dob);
}

/* The options that change the view, once the screen is open. */
static void apply_opts(void)
{
	num x = V.cx, y = V.cy, s = V.span, jr = V.jr, ji = V.ji;

	if (opt.iter >= 1 && opt.iter <= MAXIT_LIMIT)
		V.maxit = opt.iter;
	if (opt.cx[0])
		num_parse(opt.cx, &x);
	if (opt.cy[0])
		num_parse(opt.cy, &y);
	if (opt.span[0])
		num_parse(opt.span, &s);
	if (opt.jr[0])
		num_parse(opt.jr, &jr);
	if (opt.ji[0])
		num_parse(opt.ji, &ji);
	if (opt.julia)
		view_julia(jr, ji);
	if (opt.cx[0] || opt.cy[0] || opt.span[0])
		view_set(x, y, s);
	menu_build();
}

int main(int argc, char **argv)
{
	LONG rc;
	WORD i;

#ifdef USE_FPU
	if (!(SysBase->AttnFlags & (AFF_68881 | AFF_68882 | AFF_FPU40)))
		return 26; /* an FPU instruction without an FPU is a guru */
#endif
	IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
	GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 40);
	GadToolsBase = OpenLibrary("gadtools.library", 39);
	if (!IntuitionBase || !GfxBase || !GadToolsBase) {
		exit_rc = 22;
		goto out;
	}
	init_modes();
	view_reset();
#ifdef TEST_WB /* a test build: read the tool types of SYS:prog.info as if Workbench had started the program */
	{
		static struct WBArg arg;
		static struct WBStartup wbs;

		arg.wa_Lock = Lock("SYS:", ACCESS_READ);
		arg.wa_Name = "prog";
		wbs.sm_NumArgs = 1;
		wbs.sm_ArgList = &arg;
		wb_opts(&wbs);
	}
#else
	if (argc == 0)
		wb_opts((struct WBStartup *)argv);
	else
		cli_opts();
#endif
	if (quitting)
		goto out;
	if (opt.mode >= 0 && opt.mode < NPRESET)
		mode = opt.mode;
	if (opt.pal[0]) {
		if (opt.pal[0] >= '0' && opt.pal[0] < '0' + NPAL)
			pal_idx = opt.pal[0] - '0';
		else
			for (i = 0; i < NPAL; i++)
				if (eq(opt.pal, pal_name(i)))
					pal_idx = i;
	}
	SetChipRev(SETCHIPREV_BEST); /* enable AGA modes if the machine has them */
	if (opt.depth >= 1 && opt.depth <= 8) /* fewer bitplanes: less video DMA, fewer colours */
		for (i = 0; i < NPRESET; i++)
			modes[i].depth = opt.depth;
	if (opt.exact)
		fast = FALSE;
	rexx_open();
	rc = open_display(mode);
	if (rc) {
		exit_rc = rc;
		goto out;
	}
	apply_opts();
	start_render();

	while (!quitting) {
		ULONG wsig = 1UL << win->UserPort->mp_SigBit, rsig = rexx_sigmask(), got;
		struct IntuiMessage *msg;

		if (rendering && !dragging && !menu_open) {
			render_step();
			got = SetSignal(0, 0) & (wsig | rsig | SIGBREAKF_CTRL_C);
		} else
			got = Wait(wsig | rsig | SIGBREAKF_CTRL_C);
		if (got & SIGBREAKF_CTRL_C)
			break;
		if (got & rsig)
			rexx_service(do_command);
		if (got & wsig)
			while (!quitting && win && (msg = (struct IntuiMessage *)GetMsg(win->UserPort)))
				handle(msg);
	}
out:
	rexx_close();
	close_display();
	if (img)
		FreeVec(img);
	if (IconBase)
		CloseLibrary(IconBase);
	if (AslBase)
		CloseLibrary(AslBase);
	if (GadToolsBase)
		CloseLibrary(GadToolsBase);
	if (GfxBase)
		CloseLibrary((struct Library *)GfxBase);
	if (IntuitionBase)
		CloseLibrary((struct Library *)IntuitionBase);
	return exit_rc;
}
