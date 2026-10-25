#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "ilbm.h"

static BPTR fh;
static UBYTE obuf[4096];
static LONG on, err;
static ULONG pos; /* bytes put so far */

static void flush(void)
{
	if (on && !err && Write(fh, obuf, on) != on)
		err = IoErr() ? IoErr() : ERROR_DISK_FULL;
	on = 0;
}

static void put(const UBYTE *p, LONG n)
{
	pos += n;
	while (n > 0) {
		LONG c = sizeof obuf - on;

		if (c > n)
			c = n;
		CopyMem((APTR)p, obuf + on, c);
		on += c, p += c, n -= c;
		if (on == sizeof obuf)
			flush();
	}
}

static void put32(ULONG v)
{
	UBYTE b[4];

	b[0] = v >> 24, b[1] = v >> 16, b[2] = v >> 8, b[3] = v;
	put(b, 4);
}

static void chunk(const char *id, ULONG len)
{
	put((const UBYTE *)id, 4);
	put32(len);
}

/* ByteRun1: runs of 3 or more equal bytes are repeated, the rest is copied. Returns the packed length. */
static LONG pack(const UBYTE *s, LONG n, UBYTE *d)
{
	LONG i = 0, o = 0;

	while (i < n) {
		LONG run = 1;

		while (i + run < n && run < 128 && s[i + run] == s[i])
			run++;
		if (run >= 3) {
			d[o++] = (UBYTE)(257 - run);
			d[o++] = s[i];
			i += run;
		} else {
			LONG start = i, len = 0;

			while (i < n && len < 128 && !(i + 2 < n && s[i] == s[i + 1] && s[i] == s[i + 2]))
				i++, len++;
			d[o++] = (UBYTE)(len - 1);
			while (len--)
				d[o++] = s[start++];
		}
	}
	return o;
}

LONG ilbm_save(const char *name, const UBYTE *img, WORD w, WORD h, WORD depth, ULONG camg, UBYTE xasp, UBYTE yasp,
	       const UBYTE *rgb)
{
	LONG rowbytes = ((w + 15) >> 4) << 1, ncol = 1L << depth, x, y, p, body_at, n;
	UBYTE *planes, *packed, hdr[20];
	ULONG body_len;

	planes = AllocVec(depth * rowbytes + rowbytes + rowbytes / 128 + 4, MEMF_PUBLIC);
	if (!planes)
		return ERROR_NO_FREE_STORE;
	packed = planes + depth * rowbytes;
	fh = Open((STRPTR)name, MODE_NEWFILE);
	if (!fh) {
		FreeVec(planes);
		return IoErr() ? IoErr() : ERROR_OBJECT_NOT_FOUND;
	}
	on = err = 0, pos = 0;

	chunk("FORM", 0); /* size patched below */
	put((const UBYTE *)"ILBM", 4);

	chunk("BMHD", 20);
	hdr[0] = w >> 8, hdr[1] = w, hdr[2] = h >> 8, hdr[3] = h;
	hdr[4] = hdr[5] = hdr[6] = hdr[7] = 0; /* x, y */
	hdr[8] = depth, hdr[9] = 0, hdr[10] = 1, hdr[11] = 0; /* planes, no mask, ByteRun1 */
	hdr[12] = hdr[13] = 0;                                  /* transparent colour */
	hdr[14] = xasp, hdr[15] = yasp;
	hdr[16] = w >> 8, hdr[17] = w, hdr[18] = h >> 8, hdr[19] = h; /* page size */
	put(hdr, 20);

	chunk("CAMG", 4);
	put32(camg);

	chunk("CMAP", ncol * 3);
	put(rgb, ncol * 3);
	if ((ncol * 3) & 1)
		put((const UBYTE *)"", 1);

	chunk("BODY", 0);
	body_at = pos - 4;
	for (y = 0; y < h; y++) {
		const UBYTE *row = img + (LONG)y * w;

		for (n = 0; n < depth * rowbytes; n++)
			planes[n] = 0;
		for (x = 0; x < w; x++) {
			UBYTE pix = row[x], m = 0x80 >> (x & 7);

			for (p = 0; p < depth; p++)
				if (pix & (1 << p))
					planes[p * rowbytes + (x >> 3)] |= m;
		}
		for (p = 0; p < depth; p++) {
			n = pack(planes + p * rowbytes, rowbytes, packed);
			put(packed, n);
		}
	}
	body_len = pos - body_at - 4;
	if (body_len & 1)
		put((const UBYTE *)"", 1);
	flush();
	if (!err) { /* patch the two sizes */
		UBYTE b[4];

		b[0] = body_len >> 24, b[1] = body_len >> 16, b[2] = body_len >> 8, b[3] = body_len;
		if (Seek(fh, body_at, OFFSET_BEGINNING) < 0 || Write(fh, b, 4) != 4)
			err = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
		n = pos - 8;
		b[0] = n >> 24, b[1] = n >> 16, b[2] = n >> 8, b[3] = n;
		if (!err && (Seek(fh, 4, OFFSET_BEGINNING) < 0 || Write(fh, b, 4) != 4))
			err = IoErr() ? IoErr() : ERROR_SEEK_ERROR;
	}
	Close(fh);
	if (err)
		DeleteFile((STRPTR)name);
	FreeVec(planes);
	return err;
}
