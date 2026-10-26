/* Loads SYS:prog.info through icon.library and writes what it found to SYS:icon.txt (see suite.py, icon test). */
#include <exec/types.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <workbench/workbench.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <string.h>

struct Library *IconBase;

static BPTR out;

static void put(const char *s)
{
	Write(out, (APTR)s, strlen(s));
}

static void num(const char *label, LONG v)
{
	char b[16], t[16];
	int n = 0, i;

	put(label);
	if (v < 0) {
		put("-");
		v = -v;
	}
	do
		t[n++] = '0' + v % 10;
	while (v /= 10);
	for (i = 0; i < n; i++)
		b[i] = t[n - 1 - i];
	b[n] = 0;
	put(b);
	put("\n");
}

int main(void)
{
	struct DiskObject *d;
	char **tt;
	STRPTR v;

	out = Open("SYS:icon.txt", MODE_NEWFILE);
	IconBase = OpenLibrary("icon.library", 33);
	if (!IconBase) {
		put("no icon.library\n");
		Close(out);
		return 20;
	}
	d = GetDiskObject("SYS:prog");
	if (!d) {
		put("GetDiskObject failed\n");
		Close(out);
		return 20;
	}
	num("type ", d->do_Type);
	num("stack ", d->do_StackSize);
	num("width ", d->do_Gadget.Width);
	num("height ", d->do_Gadget.Height);
	if (d->do_Gadget.GadgetRender) {
		struct Image *im = (struct Image *)d->do_Gadget.GadgetRender;

		num("image width ", im->Width);
		num("image height ", im->Height);
		num("image depth ", im->Depth);
		num("image first word ", ((UWORD *)im->ImageData)[0]);
	}
	for (tt = (char **)d->do_ToolTypes; tt && *tt; tt++) {
		put("tooltype ");
		put(*tt);
		put("\n");
	}
	v = FindToolType((UBYTE **)d->do_ToolTypes, "ITER");
	put("ITER = ");
	put(v ? (char *)v : "(none)");
	put("\n");
	v = FindToolType((UBYTE **)d->do_ToolTypes, "EXACT");
	put(v ? "EXACT is on\n" : "EXACT is off\n");
	FreeDiskObject(d);
	put("end\n");
	Close(out);
	return 0;
}
