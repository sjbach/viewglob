/*
	Note from Stephen Bach:
	The following are modified versions of functions originally
	written by Marc J. Rochkind.  His copyright notice follows:

	Minimal defensive signal handling
	AUP2, Sec. 9.01.8

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

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "seer.h"
#include <signal.h>

bool handle_signals(void) {
	sigset_t set;
	struct sigaction act;

	if (sigfillset(&set) == -1)
		return false;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		return false;
	memset(&act, 0, sizeof(act));
	if (sigfillset(&act.sa_mask) == -1)
		return false;
	act.sa_handler = SIG_IGN;
	if (sigaction(SIGHUP, &act, NULL) == -1)
		return false;
	if (sigaction(SIGINT, &act, NULL) == -1)
		return false;
	if (sigaction(SIGQUIT, &act, NULL) == -1)
		return false;
	if (sigaction(SIGPIPE, &act, NULL) == -1)
		return false;
	act.sa_handler = handler;
	if (sigaction(SIGTERM, &act, NULL) == -1)
		return false;
	if (sigaction(SIGBUS, &act, NULL) == -1)
		return false;
	if (sigaction(SIGFPE, &act, NULL) == -1)
		return false;
	if (sigaction(SIGILL, &act, NULL) == -1)
		return false;
	if (sigaction(SIGSEGV, &act, NULL) == -1)
		return false;
	if (sigaction(SIGSYS, &act, NULL) == -1)
		return false;
	if (sigaction(SIGXCPU, &act, NULL) == -1)
		return false;
	if (sigaction(SIGXFSZ, &act, NULL) == -1)
		return false;
	act.sa_handler = sigwinch_handler;
	if (sigaction(SIGWINCH, &act, NULL) == -1)
		return false;
	if (sigemptyset(&set) == -1)
		return false;
	if (sigprocmask(SIG_SETMASK, &set, NULL) == -1)
		return false;
	return true;
}


void handler(int signum) {
	int i;
	struct {
		int signum;
		char* msg;
	} sigmsg[] = {
		{ SIGTERM, "Termination signal" },
		{ SIGBUS, "Access to undefined portion of a memory object" },
		{ SIGFPE, "Erroneous arithmetic operation" },
		{ SIGILL, "Illegal instruction" },
		{ SIGSEGV, "Invalid memory reference" },
		{ SIGSYS, "Bad system call" },
		{ SIGXCPU, "CPU-time limit exceeded" },
		{ SIGXFSZ, "File-size limit exceeded" },
		{ 0, NULL }
	};

	/* clean_up(); */
	for (i = 0; sigmsg[i].signum > 0; i++) {
		if (sigmsg[i].signum = signum) {
			(void)write(STDERR_FILENO, sigmsg[i].msg, strlen_safe(sigmsg[i].msg));
			(void)write(STDERR_FILENO, "\n", 1);
			break;
		}
	}
	_exit(EXIT_FAILURE);
}


size_t strlen_safe(const char* string) {
	size_t n = 0;
	while (*string++ != '\0')
		n++;
	return n;
}


