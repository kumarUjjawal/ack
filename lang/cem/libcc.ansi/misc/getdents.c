/*
	getdents -- get directory entries in a file system independent format
			(SVR3 system call emulation)

	last edit:	06-Jul-1987	D A Gwyn

	This single source file supports several different methods of
	getting directory entries from the operating system.  Define
	whichever one of the following describes your system:

	UFS	original UNIX filesystem (14-character name limit)
	BFS	4.2BSD (also 4.3BSD) native filesystem (long names)
	NFS	getdirentries() system call

	Also define any of the following that are pertinent:

	ATT_SPEC	check user buffer address for longword alignment
	BSD_SYSV	BRL UNIX System V emulation environment on 4.nBSD
	UNK		have _getdents() system call, but kernel may not
			support it

	If your C library has a getdents() system call interface, but you
	can't count on all kernels on which your application binaries may
	run to support it, change the system call interface name to
	_getdents() and define "UNK" to enable the system-call validity
	test in this "wrapper" around _getdents().

	If your system has a getdents() system call that is guaranteed 
	to always work, you shouldn't be using this source file at all.
*/

#include	<errno.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/errno.h>
#include	<sys/types.h>
#ifdef BSD_SYSV
#include	<sys/_dir.h>		/* BSD flavor, not System V */
#else
#if	defined(UFS)
#define	DIRSIZ		14		/* 14 char filename in Version 7 */
#endif
#define	MAXNAMLEN	255
struct  direct {
	off_t	d_off;			/* offset of next disk directory entry */
	u_long	d_fileno;		/* file number of entry */
	u_short	d_reclen;		/* length of this record */
	u_short	d_namlen;		/* length of string in d_name */
	char	d_name[MAXNAMLEN + 1];	/* name (up to MAXNAMLEN + 1) */
};
#undef	MAXNAMLEN			/* avoid conflict with SVR3 */

#define	d_ino	d_fileno		/* compatability */

#ifdef d_ino				/* 4.3BSD/NFS using d_fileno */
#undef	d_ino				/* (not absolutely necessary) */
#else
#define	d_fileno	d_ino		/* (struct direct) member */
#endif
#endif
#include	<sys/dirent.h>
#include	<sys/stat.h>
#ifdef UNK
#ifndef UFS
#error	UNK applies only to UFS
/* One could do something similar for getdirentries(), but I didn't bother. */
#endif
#include	<signal.h>
#endif

#if	defined(UFS) + defined(BFS) + defined(NFS) != 1	/* sanity check */
#error	exactly one of UFS, BFS, or NFS must be defined
#endif

#ifdef UFS
#define	RecLen( dp )	(sizeof(struct direct))	/* fixed-length entries */
#else	/* BFS || NFS */
#define	RecLen( dp )	((dp)->d_reclen)	/* variable-length entries */
#endif

#ifdef NFS
#ifdef BSD_SYSV
#define	getdirentries	_getdirentries	/* package hides this system call */
#endif
extern int	getdirentries(int fd, char *buf, int nbytes, long *basep);
static long	dummy;			/* getdirentries() needs basep */
#define	GetBlock( fd, buf, n )	getdirentries( fd, buf, (unsigned)n, &dummy )
#else	/* UFS || BFS */
#ifdef BSD_SYSV
#define read	_read			/* avoid emulation overhead */
#endif
extern int	read();
#define	GetBlock( fd, buf, n )	read( fd, buf, (unsigned)n )
#endif

#ifdef UNK
extern int	_getdents();		/* actual system call */
#endif

extern int	_fstat(int fd, struct stat *buf);
extern off_t	_lseek(int d, int offset, int whence);

#ifndef DIRBLKSIZ
#define	DIRBLKSIZ	4096		/* directory file read buffer size */
#endif

#ifndef NULL
#define	NULL	0
#endif

#ifndef SEEK_CUR
#define	SEEK_CUR	1
#endif

#ifndef S_ISDIR				/* macro to test for directory file */
#define	S_ISDIR( mode )		(((mode) & S_IFMT) == S_IFDIR)
#endif

#ifdef UFS

/*
	The following routine is necessary to handle DIRSIZ-long entry names.
	Thanks to Richard Todd for pointing this out.
*/

static int
NameLen( char name[] )				/* return # chars in embedded name */
		/* -> name embedded in struct direct */
{
	register char	*s;		/* -> name[.] */
	register char	*stop = &name[DIRSIZ];	/* -> past end of name field */

	for ( s = &name[1];		/* (empty names are impossible) */
	      *s != '\0'		/* not NUL terminator */
	   && ++s < stop;		/* < DIRSIZ characters scanned */
	    )
		;

	return s - name;		/* # valid characters in name */
}

#else	/* BFS || NFS */

#define	NameLen( name )	strlen( name )	/* names are always NUL-terminated */

#endif

#ifdef UNK
static enum	{ maybe, no, yes }	state = maybe;
					/* does _getdents() work? */

/*ARGSUSED*/
static void
sig_catch(int sig)			/* sig must be SIGSYS */
{
	state = no;			/* attempted _getdents() faulted */
}
#endif

int
getdents(int fildes, char *buf, unsigned nbyte)	/* returns # bytes read;
						    0 on EOF, -1 on error */
/* fildes == directory file descriptor */
/* *buf == where to put the (struct dirent)s */
/* nbyte == size of buf[] */
{
	int			serrno;	/* entry errno */
	off_t			offset;	/* initial directory file offset */
	struct stat		statb;	/* fstat() info */
	union	{
		char		dblk[DIRBLKSIZ];
					/* directory file block buffer */
		struct direct	dummy;	/* just for alignment */
		}	u;		/* (avoids having to malloc()) */
	register struct direct	*dp;	/* -> u.dblk[.] */
	register struct dirent	*bp;	/* -> buf[.] */

#ifdef UNK
	switch ( state )
		{
		void		(*shdlr)();	/* entry SIGSYS handler */
		register int	retval;	/* return from _getdents() if any */

	case yes:			/* _getdents() is known to work */
		return _getdents( fildes, buf, nbyte );

	case maybe:			/* first time only */
		shdlr = signal( SIGSYS, sig_catch );
		retval = _getdents( fildes, buf, nbyte );	/* try it */
		(void)signal( SIGSYS, shdlr );

		if ( state == maybe )	/* SIGSYS did not occur */
			{
			state = yes;	/* so _getdents() must have worked */
			return retval;
			}
		/* else fall through into emulation */

/*	case no:*/	/* fall through into emulation */
		}
#endif

	if ( buf == NULL
#ifdef ATT_SPEC
	  || (unsigned long)buf % sizeof(long) != 0	/* ugh */
#endif
	   )	{
		errno = EFAULT;		/* invalid pointer */
		return -1;
		}

	if ( _fstat( fildes, &statb ) != 0 )
		return -1;		/* errno set by fstat() */

	if ( !S_ISDIR( statb.st_mode ) )
		{
		errno = ENOTDIR;	/* not a directory */
		return -1;
		}

	if ( (offset = _lseek( fildes, (off_t)0, SEEK_CUR )) < 0 )
		return -1;		/* errno set by lseek() */

#ifdef BFS				/* no telling what remote hosts do */
	if ( (unsigned long)offset % DIRBLKSIZ != 0 )
		{
		errno = ENOENT;		/* file pointer probably misaligned */
		return -1;
		}
#endif

	serrno = errno;			/* save entry errno */

	for ( bp = (struct dirent *)buf; bp == (struct dirent *)buf; )
		{			/* convert next directory block */
		int	size;

		do	size = GetBlock( fildes, u.dblk, DIRBLKSIZ );
		while ( size == -1 && errno == EINTR );

		if ( size <= 0 )
			return size;	/* EOF or error (EBADF) */

		for ( dp = (struct direct *)u.dblk;
		      (char *)dp < &u.dblk[size];
		      dp = (struct direct *)((char *)dp + RecLen( dp ))
		    )	{
#ifndef UFS
			if ( dp->d_reclen <= 0 )
				{
				errno = EIO;	/* corrupted directory */
				return -1;
				}
#endif

			if ( dp->d_fileno != 0 )
				{	/* non-empty; copy to user buffer */
				register int	reclen =
					DIRENTSIZ( NameLen( dp->d_name ) );

				if ( (char *)bp + reclen > &buf[nbyte] )
					{
					errno = EINVAL;
					return -1;	/* buf too small */
					}

				bp->d_ino = dp->d_fileno;
				bp->d_off = offset + ((char *)dp - u.dblk);
				bp->d_reclen = reclen;
				(void)strncpy( bp->d_name, dp->d_name,
					       reclen - DIRENTBASESIZ
					     );	/* adds NUL padding */

				bp = (struct dirent *)((char *)bp + reclen);
				}
			}

#ifndef BFS	/* 4.2BSD screwed up; fixed in 4.3BSD */
		if ( (char *)dp > &u.dblk[size] )
			{
			errno = EIO;	/* corrupted directory */
			return -1;
			}
#endif
		}

	errno = serrno;			/* restore entry errno */
	return (char *)bp - buf;	/* return # bytes read */
}
