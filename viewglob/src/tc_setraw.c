/*
	Raw terminal I/O
	AUP2, Sec. 4.05.9, 4.05.10

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
#include "tc_setraw.h"
#include <termios.h>

/*[tc_keystroke]*/
static struct termios tbufsave;
static bool have_attr = false;

int tc_keystroke(void)
{
	static unsigned char buf[10];
	static ssize_t total = 0, next = 0;
	static bool first = true;
	struct termios tbuf;

	if (first) {
		first = false;
		ec_neg1( tcgetattr(STDIN_FILENO, &tbuf) )
		have_attr = true;
		tbufsave = tbuf;
		tbuf.c_lflag &= ~ICANON;
		tbuf.c_cc[VMIN] = sizeof(buf);
		tbuf.c_cc[VTIME] = 2;
		ec_neg1( tcsetattr(STDIN_FILENO, TCSAFLUSH, &tbuf) )
	}
	if (next >= total)
		switch (total = read(0, buf, sizeof(buf))) {
		case -1:
			syserr("read");
		case 0:
			fprintf(stderr, "Mysterious EOF\n");
			exit(EXIT_FAILURE);
		default:
			next = 0;
		}
	return buf[next++];

EC_CLEANUP_BGN
	return -1;
EC_CLEANUP_END
}
/*[tc_setraw]*/
bool tc_setraw(void)
{
	struct termios tbuf;
	long disable;
	int i;

#ifdef _POSIX_VDISABLE
	disable = _POSIX_VDISABLE;
#else
	/* treat undefined as error with errno = 0 */
	ec_neg1( (errno = 0, disable = fpathconf(STDIN_FILENO, _PC_VDISABLE)) )
#endif
	ec_neg1( tcgetattr(STDIN_FILENO, &tbuf) )
	have_attr = true;
	tbufsave = tbuf;
	tbuf.c_cflag &= ~(CSIZE | PARENB);
	tbuf.c_cflag |= CS8;
	tbuf.c_iflag &= ~(INLCR | ICRNL | ISTRIP | INPCK | IXON | BRKINT);
	tbuf.c_oflag &= ~OPOST;
	tbuf.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
	for (i = 0; i < NCCS; i++)
		tbuf.c_cc[i] = (cc_t)disable;
	tbuf.c_cc[VMIN] = 5;
	tbuf.c_cc[VTIME] = 2;
	ec_neg1( tcsetattr(STDIN_FILENO, TCSAFLUSH, &tbuf) )
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[tc_restore]*/
bool tc_restore(void)
{
	if (have_attr)
		ec_neg1( tcsetattr(STDIN_FILENO, TCSAFLUSH, &tbufsave) )
	return true;

EC_CLEANUP_BGN
	return false;
EC_CLEANUP_END
}
/*[]*/
