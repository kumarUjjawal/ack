#include "sys.h"
.globl	_dup
.globl	_dup2
.globl	_errno

_dup2:
	mov	2(sp),r0
	bis	$100,r0
	br	1f

_dup:
	mov	2(sp),r0
1:
	mov	4(sp),r1
	sys	dup
	bec	1f
	mov	r0,_errno
	mov	$-1,r0
1:
	rts	pc
