#include "sys.h"
.globl	_acct
.globl	_errno

_acct:
	mov	2(sp),0f+2
	sys	indir; 0f
	bec	1f
	mov	r0, _errno
	mov	$-1,r0
	rts	pc
1:
	clr	r0
	rts	pc
.data
0:
	sys	acct; ..
