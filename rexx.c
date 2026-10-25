#include <exec/types.h>
#include <exec/ports.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/exec.h>
#include <proto/rexxsyslib.h>
#include <string.h>
#include "rexx.h"

struct RxsLib *RexxSysBase; /* optional: only needed to hand back a result string */
static struct MsgPort *port;
static struct RexxMsg *pending;

static void reply(struct RexxMsg *rm, LONG rc, const char *res)
{
	rm->rm_Result1 = rc;
	rm->rm_Result2 = 0;
	if (rc == 0 && res[0] && RexxSysBase && (rm->rm_Action & RXFF_RESULT))
		rm->rm_Result2 = (LONG)CreateArgstring((STRPTR)res, strlen(res));
	ReplyMsg((struct Message *)rm);
}

BOOL rexx_open(void)
{
	port = CreateMsgPort();
	if (!port)
		return FALSE;
	port->mp_Node.ln_Name = "AMIGAMAND";
	port->mp_Node.ln_Pri = 0;
	Forbid();
	if (FindPort("AMIGAMAND")) { /* a second copy: no port for it */
		Permit();
		DeleteMsgPort(port);
		port = NULL;
		return FALSE;
	}
	AddPort(port);
	Permit();
	RexxSysBase = (struct RxsLib *)OpenLibrary("rexxsyslib.library", 0);
	return TRUE;
}

ULONG rexx_sigmask(void)
{
	return port ? 1UL << port->mp_SigBit : 0;
}

void rexx_service(rexx_cb cb)
{
	struct Message *m;

	while (port && (m = GetMsg(port))) {
		struct RexxMsg *rm = (struct RexxMsg *)m;
		char res[96];
		LONG rc;

		res[0] = 0;
		if ((rm->rm_Action & RXCODEMASK) == RXCOMM && ARG0(rm))
			rc = cb((const char *)ARG0(rm), res, sizeof res);
		else
			rc = 20;
		if (rc == RC_DEFER) {
			if (pending)
				reply(rm, 10, ""); /* one WAIT at a time */
			else
				pending = rm;
		} else
			reply(rm, rc, res);
	}
}

void rexx_wake(void)
{
	if (pending) {
		reply(pending, 0, "");
		pending = NULL;
	}
}

void rexx_close(void)
{
	struct Message *m;

	if (!port)
		return;
	RemPort(port);
	rexx_wake();
	while ((m = GetMsg(port)))
		reply((struct RexxMsg *)m, 20, "");
	DeleteMsgPort(port);
	port = NULL;
	if (RexxSysBase)
		CloseLibrary((struct Library *)RexxSysBase);
	RexxSysBase = NULL;
}
