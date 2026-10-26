/* spawn <command line>: starts the command as a separate process and returns at once (the test boot has no Run). */
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <string.h>

int main(int argc, char **argv)
{
	char cmd[256];
	int i;

	cmd[0] = 0;
	for (i = 1; i < argc; i++) {
		strcat(cmd, argv[i]);
		strcat(cmd, " ");
	}
	SystemTags(cmd, SYS_Asynch, TRUE, SYS_Input, Open("NIL:", MODE_OLDFILE), SYS_Output, Open("NIL:", MODE_NEWFILE),
		   TAG_END);
	return 0;
}
