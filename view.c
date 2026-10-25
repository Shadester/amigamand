/* The view and the escape-time loops. Fixed point (Q8.24) by default, double with -DUSE_FPU. */
#include <exec/types.h>
#include "view.h"

struct view V;
static num saved_cx, saved_cy, saved_span; /* the Mandelbrot view while a Julia set is shown */

#ifdef USE_FPU
#define NMUL(a, b) ((a) * (b))
#define NI(i) ((double)(i))
#define NFRAC(a, b) ((double)(a) / (b))
#define NQ(a) ((a) * 0.25)
#define MINDX 2e-13 /* below this the doubles run out of digits at |x| ~ 1 */
#else
#define FB 24
#define ONE (1L << FB)
/* (a*b) >> 24 from one muls.l: hi<<8 | lo>>24. vbcc would call 64-bit helpers for long long. */
LONG fxmul(__reg("d0") LONG, __reg("d1") LONG) =
	"\tmuls.l\td1,d1:d0\n"
	"\tlsl.l\t#8,d1\n"
	"\trol.l\t#8,d0\n"
	"\tand.l\t#255,d0\n"
	"\tor.l\td1,d0\n";
#define NMUL(a, b) fxmul(a, b)
#define NI(i) ((LONG)(i) << FB)
#define NFRAC(a, b) ((LONG)(a) * ONE / (b)) /* |a| <= 127 */
#define NQ(a) ((a) >> 2)
#define MINDX 16 /* units of 2^-24: about 1e-6 per pixel */
#endif

/* Keep the view where the arithmetic (and the loop's range, |z|^2 < 128 in Q8.24) still works. */
static void prep(void)
{
	num lim = NI(3), minspan = V.w * MINDX;

	if (V.cx > lim) V.cx = lim;
	if (V.cx < -lim) V.cx = -lim;
	if (V.cy > lim) V.cy = lim;
	if (V.cy < -lim) V.cy = -lim;
	if (V.span > NI(6)) V.span = NI(6);
	if (V.span < minspan) V.span = minspan;
	V.dx = V.span / V.w;
	V.dy = V.dx * V.ay / V.ax;
	V.x0 = V.cx - V.dx * (V.w - 1) / 2;
	V.y0 = V.cy - V.dy * (V.h - 1) / 2;
}

void view_reset(void)
{
	if (!V.maxit) {
		V.maxit = 128;
		V.jr = NFRAC(-4, 5);
		V.ji = NFRAC(39, 250);
		V.w = 320, V.h = 256, V.ax = V.ay = 1;
	}
	V.cx = V.julia ? 0 : NFRAC(-3, 4);
	V.cy = 0;
	V.span = NFRAC(7, 2);
	prep();
}

void view_mode(WORD w, WORD h, WORD ax, WORD ay)
{
	V.w = w, V.h = h, V.ax = ax, V.ay = ay;
	prep();
}

void view_set(num cx, num cy, num span)
{
	V.cx = cx, V.cy = cy, V.span = span;
	prep();
}

void view_point(WORD px, WORD py, num *re, num *im)
{
	*re = V.x0 + px * V.dx;
	*im = V.y0 + py * V.dy;
}

void view_zoom_at(WORD px, WORD py, WORD boxw)
{
	num re, im;

	if (boxw < 1)
		boxw = 1;
	view_point(px, py, &re, &im);
	V.cx = re, V.cy = im;
#ifdef USE_FPU
	V.span = V.span * boxw / V.w;
#else
	V.span = (num)(((long long)V.span * boxw) / V.w);
#endif
	prep();
}

void view_scale(LONG mul, LONG div)
{
#ifdef USE_FPU
	V.span = V.span * mul / div;
#else
	V.span = (num)(((long long)V.span * mul) / div);
#endif
	prep();
}

void view_pan(WORD fx, WORD fy)
{
	V.cx += V.dx * (V.w / 4) * fx;
	V.cy += V.dy * (V.h / 4) * fy;
	prep();
}

void view_julia(num re, num im)
{
	if (!V.julia) {
		saved_cx = V.cx, saved_cy = V.cy, saved_span = V.span;
		V.julia = 1;
		V.cx = V.cy = 0;
		V.span = NFRAC(7, 2);
	}
	V.jr = re, V.ji = im;
	prep();
}

void view_mandel(void)
{
	if (V.julia) {
		V.julia = 0;
		V.cx = saved_cx, V.cy = saved_cy, V.span = saved_span;
		prep();
	}
}

int view_symmetric(void)
{
	return !V.julia && V.cy == 0;
}

static WORD iter(num pr, num pi)
{
	num cr, ci, zr, zi, zr2, zi2;
	WORD i = 0, maxit = V.maxit;

	if (V.julia) {
		cr = V.jr, ci = V.ji;
		zr = pr, zi = pi;
	} else {
		/* the main cardioid and the period-2 bulb are inside; only near them, so the products stay small */
		if (pr > -NFRAC(3, 2) && pr < NFRAC(1, 2) && pi > -NI(1) && pi < NI(1)) {
			num xm = pr - NFRAC(1, 4), yy = NMUL(pi, pi), q = NMUL(xm, xm) + yy, xb = pr + NI(1);

			if (NMUL(q, q + xm) < NQ(yy) || NMUL(xb, xb) + yy < NFRAC(1, 16))
				return maxit;
		}
		cr = pr, ci = pi;
		zr = zi = 0;
	}
	zr2 = NMUL(zr, zr);
	zi2 = NMUL(zi, zi);
	while (i < maxit && zr2 + zi2 <= NI(4)) {
		zi = NMUL(zr, zi) * 2 + ci;
		zr = zr2 - zi2 + cr;
		zr2 = NMUL(zr, zr);
		zi2 = NMUL(zi, zi);
		i++;
	}
	return i;
}

WORD view_pixel(WORD x, WORD y)
{
	return iter(V.x0 + x * V.dx, V.y0 + y * V.dy);
}

int view_c_inside(void)
{
	WORD julia = V.julia, r;

	V.julia = 0;
	r = iter(V.jr, V.ji);
	V.julia = julia;
	return r >= V.maxit;
}

void view_row(WORD y, UBYTE *dst)
{
	num ci = V.y0 + y * V.dy, cr = V.x0;
	WORD x;

	for (x = 0; x < V.w; x++) {
		dst[x] = (UBYTE)iter(cr, ci);
		cr += V.dx;
	}
}

char *put_uint(char *p, ULONG v)
{
	char t[12];
	WORD n = 0;

	do
		t[n++] = '0' + v % 10;
	while (v /= 10);
	while (n)
		*p++ = t[--n];
	return p;
}

#ifdef USE_FPU
const char *num_parse(const char *s, num *out)
{
	double m = 0, scale = 1;
	int neg = 0, digits = 0, dot = 0, e = 0, eneg = 0;

	while (*s == ' ')
		s++;
	if (*s == '-')
		neg = 1, s++;
	else if (*s == '+')
		s++;
	for (;; s++) {
		if (*s >= '0' && *s <= '9') {
			m = m * 10 + (*s - '0');
			digits++;
			if (dot)
				scale *= 10;
		} else if (*s == '.' && !dot)
			dot = 1;
		else
			break;
	}
	if (!digits)
		return NULL;
	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '-')
			eneg = 1, s++;
		else if (*s == '+')
			s++;
		while (*s >= '0' && *s <= '9' && e < 400)
			e = e * 10 + (*s++ - '0');
	}
	m /= scale;
	while (e-- > 0)
		m = eneg ? m / 10 : m * 10;
	*out = neg ? -m : m;
	return s;
}

void num_fmt(num v, char *b)
{
	ULONG ip;
	WORD i;

	if (v < 0)
		*b++ = '-', v = -v;
	ip = (ULONG)v;
	v -= ip;
	b = put_uint(b, ip);
	*b++ = '.';
	for (i = 0; i < 15; i++) {
		v *= 10;
		ip = (ULONG)v;
		*b++ = '0' + ip;
		v -= ip;
	}
	*b = 0;
}
#else
const char *num_parse(const char *s, num *out)
{
	unsigned long long m = 0, d = 1;
	int neg = 0, digits = 0, dot = 0, k = 0, e = 0, eneg = 0;

	while (*s == ' ')
		s++;
	if (*s == '-')
		neg = 1, s++;
	else if (*s == '+')
		s++;
	for (;; s++) {
		if (*s >= '0' && *s <= '9') {
			digits++;
			if (m < 100000000000ULL) { /* 11 digits: m << 24 still fits in 64 bits */
				m = m * 10 + (*s - '0');
				if (dot)
					k++;
			}
		} else if (*s == '.' && !dot)
			dot = 1;
		else
			break;
	}
	if (!digits)
		return NULL;
	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '-')
			eneg = 1, s++;
		else if (*s == '+')
			s++;
		while (*s >= '0' && *s <= '9' && e < 100)
			e = e * 10 + (*s++ - '0');
	}
	k += eneg ? e : -e; /* value = m / 10^k */
	m <<= FB;
	if (k > 0) {
		if (k > 18)
			m = 0;
		else {
			while (k--)
				d *= 10;
			m /= d;
		}
	} else
		while (k++ < 0 && m < 0x7FFFFFFFULL)
			m *= 10;
	if (m > 0x7FFFFFFFULL)
		m = 0x7FFFFFFFULL;
	*out = neg ? -(LONG)m : (LONG)m;
	return s;
}

void num_fmt(num v, char *b)
{
	ULONG a;
	unsigned long long f;
	WORD i;

	if (v < 0)
		*b++ = '-', a = -v;
	else
		a = v;
	b = put_uint(b, a >> FB);
	*b++ = '.';
	f = a & (ONE - 1);
	for (i = 0; i < 7; i++) {
		f *= 10;
		*b++ = '0' + (char)(f >> FB);
		f &= ONE - 1;
	}
	*b = 0;
}
#endif
