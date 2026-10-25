/* ARexx port "AMIGAMAND". The commands themselves are handled by the callback. */
#ifndef REXX_H
#define REXX_H
#include <exec/types.h>

#define RC_DEFER (-1) /* the callback keeps the message: answer it later with rexx_wake() */

/* Fills res (size n) with an optional result string; returns the ARexx return code (0, 5, 10, 20) or RC_DEFER. */
typedef LONG (*rexx_cb)(const char *cmd, char *res, LONG n);

BOOL rexx_open(void);
ULONG rexx_sigmask(void);
void rexx_service(rexx_cb cb); /* handles every waiting message */
void rexx_wake(void);          /* answers the deferred message with return code 0 */
void rexx_close(void);
#endif
