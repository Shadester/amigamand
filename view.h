/* The view (centre, width, Julia constant) and the escape-time loops.
   num is Q8.24 fixed point, or double when built with -DUSE_FPU. */
#ifndef VIEW_H
#define VIEW_H
#include <exec/types.h>

#ifdef USE_FPU
typedef double num;
#else
typedef LONG num;
#endif

#define MAXIT_LIMIT 255 /* an iteration count has to fit in a UBYTE */

struct view {
	num cx, cy, span;  /* centre, and width of the view in the complex plane */
	num jr, ji;        /* Julia constant */
	WORD julia;        /* 0 Mandelbrot, 1 Julia */
	WORD maxit;
	WORD w, h, ax, ay; /* picture size; a pixel is ax wide and ay high (relative) */
	num x0, y0, dx, dy; /* derived: complex value of pixel (0,0) and the step per pixel */
};
extern struct view V;

void view_reset(void);
void view_mode(WORD w, WORD h, WORD ax, WORD ay); /* new picture size, same view */
void view_set(num cx, num cy, num span);
void view_zoom_at(WORD px, WORD py, WORD boxw);   /* centre on a pixel, span *= boxw / w */
void view_scale(LONG mul, LONG div);              /* span *= mul / div about the centre */
void view_pan(WORD fx, WORD fy);                  /* in quarters of the picture */
void view_point(WORD px, WORD py, num *re, num *im);
void view_julia(num re, num im);                  /* switch to the Julia set of re+im*i */
void view_mandel(void);
int view_zoomed(void);                            /* the span is under 0.5: big regions of one colour are likely and costly */
int view_symmetric(void);                         /* 0 none, 1 rows y and h-1-y are equal, 2 turned half way round */
void view_row(WORD y, UBYTE *dst, const UBYTE *lut);   /* the pens of row y; lut maps an iteration count to a pen */
void view_run(WORD x, WORD y, WORD n, UBYTE *dst, const UBYTE *lut); /* the pens of n pixels along row y */
UBYTE view_pen(WORD x, WORD y, const UBYTE *lut);   /* the pen of one pixel */

const char *num_parse(const char *s, num *out);   /* returns the end, or NULL if no number */
void num_fmt(num v, char *buf);
char *put_uint(char *p, ULONG v);                 /* returns the end */
#endif
