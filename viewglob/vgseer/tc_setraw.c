/*
	Raw terminal I/O
	AUP2, Sec. 4.05.9, 4.05.10

	Copyright 2003 by Marc J. Rochkind. All rights reserved.
	May be copied only for purposes and under conditions described
	on the Web page www.basepath.com/aup/copyright.htm.

	2004, 2005 Modified by Stephen Bach for Viewglob's purposes.

*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "tc_setraw.h"
#include <termios.h>

static struct termios tbufsave;
static gboolean have_attr = FALSE;

gboolean tc_setraw(void) {
	struct termios tbuf;
	glong disable;
	gint i;

#ifdef _POSIX_VDISABLE
	disable = _POSIX_VDISABLE;
#else
	/* treat undefined as error with errno = 0 */
	errno = 0;
	if ( (disable = fpathconf(STDIN_FILENO, _PC_VDISABLE)) == -1)
		return FALSE;
#endif
	if (tcgetattr(STDIN_FILENO, &tbuf) == -1)
		return FALSE;
	have_attr = TRUE;
	tbufsave = tbuf;
	tbuf.c_cflag &= ~(CSIZE | PARENB);
	tbuf.c_cflag |= CS8;
	tbuf.c_iflag &= ~(INLCR | ICRNL | ISTRIP | INPCK | IXON | BRKINT);
	tbuf.c_oflag &= ~OPOST;
	tbuf.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
	for (i = 0; i < NCCS; i++)
		tbuf.c_cc[i] = (cc_t)disable;
	tbuf.c_cc[VMIN] = 1;
	tbuf.c_cc[VTIME] = 0;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tbuf) == -1)
		return FALSE;

	return TRUE;
}


gboolean tc_restore(void) {
	if (have_attr) {
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tbufsave) == -1)
			return FALSE;
	}
	return TRUE;
}

