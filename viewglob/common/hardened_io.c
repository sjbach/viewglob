/*
	Copyright (C) 2004, 2005 Stephen Bach
	This file is part of the viewglob package.

	viewglob is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	viewglob is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with viewglob; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common.h"
#include "vgseer.h"
#include "viewglob-error.h"
#include "hardened_io.h"

#if HAVE_TERMIOS_H
# include <termios.h>
#endif

#if GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

#include <sys/stat.h>
#include <fcntl.h>

extern struct user_shell u;


/* Attempt to open the given file with the given flags and mode.
   Emit warning if it doesn't work out. */
gint open_warning(gchar* file_name, gint flags, mode_t mode) {
	gint fd = -1;
	if (file_name) {
		if ( (fd = open(file_name, flags, mode)) == -1) {
			viewglob_warning("Could not open file");
			viewglob_warning(file_name);
		}
	}
	return fd;
}

/* Attempt to close the given file.  Emit warning on failure. */
void close_warning(gint fd, gchar* file_name) {
	if ( fd != -1 && close(fd) == -1) {
		viewglob_warning("Could not close file");
		if (file_name)
			viewglob_warning(file_name);
	}
}

/* If read is interrupted by a signal, try again.  Emit error on failure. */
gboolean hardened_read(gint fd, void* buf, size_t count, ssize_t* nread) {
	gboolean ok = TRUE;

	while (TRUE) {
		errno = 0;
		if ( (*(nread) = read(fd, buf, count)) == -1 ) {
			if (errno == EINTR) {
				if (u.term_size_changed) { 
					/* Received SIGWINCH. */
					if (send_term_size(u.s.fd))
						u.term_size_changed = FALSE;
					else {
						viewglob_error("Resizing term failed");
						ok = FALSE;
						break;
					}
				}
				continue;
			}
			else {
				ok = FALSE;
				break;
			}
		}
		else
			break;
	}

	return ok;
}


/* Write all length bytes of buff to fd, even if it requires several tries.
   Retry after signal interrupts.  Emit error on failure. */
gboolean hardened_write(gint fd, gchar* buff, size_t length) {
	ssize_t nwritten;
	size_t offset = 0;

	while (length > 0) {
		while (TRUE) {
			errno = 0;
			if ( (nwritten = write(fd, buff + offset, length)) == -1 ) {
				if (errno == EINTR) {
					if (u.term_size_changed) { 
						/* Received SIGWINCH. */
						if (send_term_size(u.s.fd))
							u.term_size_changed = FALSE;
						else {
							viewglob_error("Resizing term failed");
							return FALSE;
						}
					}
					continue;
				}
				else
					return FALSE;
			}
			else
				break;
		}
		length -= nwritten;
		offset += nwritten;
	}

	return TRUE;
}


/* If select is interrupted by a signal, try again. */
gboolean hardened_select(gint n, fd_set* readfds, fd_set* writefds) {

	while (TRUE) {
		if (select(n, readfds, writefds, NULL, NULL) == -1) {
			if (errno == EINTR) {
				if (u.term_size_changed) { 
					/* Received SIGWINCH. */
					if (send_term_size(u.s.fd))
						u.term_size_changed = FALSE;
					else {
						viewglob_error("Resizing term failed");
						return FALSE;
					}
				}
				continue;
			}
			else {
				viewglob_error("Select failed in hardened_select");
				return FALSE;
			}
		}
		else
			break;
	}
	return TRUE;
}


/* Send the terminal size to the given terminal. */
/* This really shouldn't be here, but it's convenient. */
gboolean send_term_size(gint shell_fd) {
	struct winsize size;
	DEBUG((df, "in send_term_size\n"));
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1)
		return FALSE;
	else if (ioctl(shell_fd, TIOCSWINSZ, &size) == -1)
		return FALSE;
	else
		return TRUE;
}

