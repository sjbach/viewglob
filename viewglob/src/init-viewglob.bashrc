# Copyright (C) 2004 Stephen Bach
# This file is part of the viewglob package.
#
# viewglob is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# viewglob is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with viewglob; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Source the user's run-control file.
[ -f ~/.bashrc ] && . ~/.bashrc

if [ "$VG_SANDBOX" = yep ]; then
	# This is all for the sandbox shell.

	# Disable history expansion.
	set +o history

	# Don't save the history.
	unset HISTFILE

	# Only viewglob programs (glob-expand) in the path.
	PATH="$VG_DIR"

else
	# This is all for the user's shell.

	if [ "$VG_ASTERISK" = yep ]; then
		# Put a little asterisk on the front as an indicator that
		# this is a viewglob-controlled shell.
		export PS1="\[\033[44;1;33m\]*\[\033[0m\]${PS1}"
	fi

	# If the user has a PROMPT_COMMAND, delimit it with ;.
	if [ "x${PROMPT_COMMAND}" != x ]; then
		PROMPT_COMMAND="${PROMPT_COMMAND};"
	fi

	# Adding semaphores to the ends of these variables.
	export PS1="${PS1}\[\033[0;30m\]\[\033[0m\]\[\033[1;37m\]\[\033[0m\]"
	export PROMPT_COMMAND="${PROMPT_COMMAND}"'printf "\033P${PWD}\033\\"'

	# Used to make sure the user doesn't run viewglob on top of a viewglob
	# shell.  The naming is purposely ugly to ensure there's no clobbering.
	export VG_VIEWGLOB_ACTIVE=yep
fi

# Don't want to clutter the environment.
unset VG_ASTERISK
unset VG_SANDBOX
unset VG_DIR

