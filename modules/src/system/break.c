/* $Header$ */

#include <system.h>

char *sbrk();

char *
sys_break(incr)
	int incr;
{
	char *sbrk();
	char *brk = sbrk(incr);

	if (brk == (char *) 0 || brk == (char *)-1)
		return ILL_BREAK;
	return brk;
}
