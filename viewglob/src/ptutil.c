/*
	Pseudo-Terminal Library
	AUP2, Sec. 4.10.1

	Copyright 2003 by Marc J. Rochkind. All rights reserved.
	May be copied only for purposes and under conditions described
	on the Web page www.basepath.com/aup/copyright.htm.

	The Example Files are provided "as is," without any warranty;
	without even the implied warranty of merchantability or fitness
	for a particular purpose. The author and his publisher are not
	responsible for any damages, direct or incidental, resulting
	from the use or non-use of these Example Files.

	The Example Files may contain defects, and some contain deliberate
	coding mistakes that were included for educational reasons.
	You are responsible for determining if and how the Example Files
	are to be used.

*/
#include "defs.h"
#include "ptutil.h"
/*[NONPORTABILITY_SYMBOLS]*/
#if defined(SOLARIS) /* add to this as necessary */
#define NEED_STREAM_SETUP
#endif

#if defined(FREEBSD) /* add to this as necessary */
#define NEED_TIOCSCTTY
#endif

#ifndef _XOPEN_UNIX
#define MASTER_NAME_SEARCH
#endif
/*[incl]*/
#ifdef _XOPEN_UNIX
#include <stropts.h> /* for STREAMS */
#endif
#ifdef NEED_TIOCSCTTY
#include <sys/ttycom.h> /* for TIOCSCTTY */
#endif
/*[]*/
	/*
		Masters are of the form /dev/ptyXY. According to the FreeBSD
		documentation, X is in the range [p-sP-S] and Y is in the
		range [0-9a-v]. But, this is unlikely to be true of every
		FREEBSD-type system, so we will search X in the range [0-9A-Za-z]
		and	Y in the same range. However, our algorithm assumes that for
		any X, the first valid Y is 0. That is, there is no need to waste
		time checking values of Y if there is no master with a Y of 0.
	*/
/*[find_and_open_master]*/
#if defined(MASTER_NAME_SEARCH)
#define PTY_RANGE \
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
#define PTY_PROTO	"/dev/ptyXY"
#define PTY_X		8
#define PTY_Y		9
#define PTY_MS		5 /* replace with 't' to get slave name */
#endif /* MASTER_NAME_SEARCH */

static bool find_and_open_master(PTINFO *p)
{
#if defined(_XOPEN_UNIX)
#if _XOPEN_VERSION >= 600
	p->pt_name_m[0] = '\0'; /* don't know or need name */
	ec_neg1( p->pt_fd_m = posix_openpt(O_RDWR | O_NOCTTY) )
#else
	strcpy(p->pt_name_m, "/dev/ptmx"); /* clone device */
	ec_neg1( p->pt_fd_m = open(p->pt_name_m, O_RDWR) )
#endif
#elif defined(MASTER_NAME_SEARCH)
	int i, j;
	char proto[] = PTY_PROTO;

	if (p->pt_fd_m != -1) {
		(void)close(p->pt_fd_m);
		p->pt_fd_m = -1;
	}
	for (i = 0; i < sizeof(PTY_RANGE) - 1; i++) {
		proto[PTY_X] = PTY_RANGE[i];
		proto[PTY_Y] = PTY_RANGE[0];
		if (access(proto, F_OK) == -1) {
			if (errno == ENOENT)
				continue;
			EC_FAIL
		}
		for (j = 0; j < sizeof(PTY_RANGE) - 1; j++) {
			proto[PTY_Y] = PTY_RANGE[j];
			if ((p->pt_fd_m = open(proto, O_RDWR)) == -1) {
				if (errno == ENOENT)
					break;
			}
			else {
				strcpy(p->pt_name_m, proto);
				break;
			}
		}
		if (p->pt_fd_m != -1)
			break;
	}
	if (p->pt_fd_m == -1) {
		errno = EAGAIN;
		EC_FAIL
	}
#else
	errno = ENOSYS;
	EC_FAIL
#endif
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[pt_open_master]*/
PTINFO *pt_open_master(void)
{
	PTINFO *p = NULL;
#ifdef _XOPEN_UNIX
	char *s;
#endif

	ec_null( p = calloc(1, sizeof(PTINFO)) )
	p->pt_fd_m = -1;
	p->pt_fd_s = -1;
	ec_false( find_and_open_master(p) )
#ifdef _XOPEN_UNIX
	ec_neg1( grantpt(p->pt_fd_m) )
	ec_neg1( unlockpt(p->pt_fd_m) )
	ec_null( s = ptsname(p->pt_fd_m) )
	if (strlen(s) >= PT_MAX_NAME) {
		errno = ENAMETOOLONG;
		EC_FAIL
	}
	strcpy(p->pt_name_s, s);
#elif defined(MASTER_NAME_SEARCH)
	strcpy(p->pt_name_s, p->pt_name_m);
	p->pt_name_s[PTY_MS] = 't';
#else
	errno = ENOSYS;
	EC_FAIL
#endif
	return p;

EC_CLEANUP_BGN
	if (p != NULL) {
		(void)close(p->pt_fd_m);
		(void)close(p->pt_fd_s);
		free(p);
	}
	return NULL;
EC_CLEANUP_END
}
/*[]*/
/*
	Following function must be opened in child process,
	as it will set the controlling terminal.
*/
/*[pt_open_slave]*/
bool pt_open_slave(PTINFO *p)
{
	ec_neg1( setsid() )
	if (p->pt_fd_s != -1)
		ec_neg1( close(p->pt_fd_s) )
	ec_neg1( p->pt_fd_s = open(p->pt_name_s, O_RDWR) )
#if defined(NEED_TIOCSCTTY)
	ec_neg1( ioctl(p->pt_fd_s, TIOCSCTTY, 0) )
#endif
#if defined(NEED_STREAM_SETUP)
	ec_neg1( ioctl(p->pt_fd_s, I_PUSH, "ptem") )
	ec_neg1( ioctl(p->pt_fd_s, I_PUSH, "ldterm") )
#endif
	/*
		Changing mode not that important, so don't fail if it doesn't
		work only because we're not superuser.
	*/
	if (fchmod(p->pt_fd_s, PERM_FILE) == -1 && errno != EPERM)
		EC_FAIL
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[pt_wait_master]*/
bool pt_wait_master(PTINFO *p)
{
	fd_set fd_set_write;

	FD_ZERO(&fd_set_write);
	FD_SET(PT_GET_MASTER_FD(p), &fd_set_write);
	ec_neg1( select(PT_GET_MASTER_FD(p) + 1, NULL, &fd_set_write, NULL,
	  NULL) )
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[pt_close_master]*/
bool pt_close_master(PTINFO *p)
{
	ec_neg1( close(p->pt_fd_m) )
	free(p);
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[pt_close_slave]*/
bool pt_close_slave(PTINFO *p)
{
	(void)close(p->pt_fd_s); /* probably already closed */
	free(p);
	return true;
}
/*[]*/
