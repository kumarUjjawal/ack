#include "sys.h"
.globl	_lock
.globl	_errno

_lock:
	mov	2(sp),0f+2
	sys	indir; 0f
	bec	1f
	mov	r0,_errno
	mov	$-1,r0
1:
	rts	pc
.data
0:
	sys	lock; ..
