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
[ -f ~/.zshrc ] && . ~/.zshrc

if [ "$VG_SANDBOX" = yep ]; then
	# This is all for the sandbox shell.

	# Just in case the user put setopt zle in their .zshrc.
	setopt \
		NO_ZLE \
		NO_BANGHIST
else
	# This is all for the user's shell.

	if [ "$VG_ASTERISK" = yep ]; then
		# Put a little asterisk on the front as an indicator that
		# this is a viewglob-controlled shell.
		export PROMPT="%{"$'\e[44;1;33m'"%}*%{"$'\e[0m'"%}${PROMPT}"
	fi

	# Adding semaphores.
	export PROMPT="${PROMPT}%{"$'\e[0;30m'"%}%{"$'\e[0m'"%}%{"$'\e[1;37m'"%}%{"$'\e[0m'"%}"

	# RPROMPT has semaphores on both ends.
	RPROMPT="%{"$'\e[0;34m'"%}%{"$'\e[0m'"%}%{"$'\e[0;31m'"%}%{"$'\e[0m'"%}${RPROMPT}"
	export RPROMPT="${RPROMPT}%{"$'\e[0;34m'"%}%{"$'\e[0m'"%}%{"$'\e[0;31m'"%}%{"$'\e[0m'"%}"

	if typeset -f precmd >/dev/null; then
		# The user already has a precmd function.  Gotta do some finagling.

		VG_TEMP_FILE=/tmp/vg$$

		# Rename the user's precmd to vg_precmd (sad hack).
		typeset -f precmd | sed '1s/precmd/vg_precmd/' > $VG_TEMP_FILE
		chmod u+x $VG_TEMP_FILE
		. $VG_TEMP_FILE
		rm -f $VG_TEMP_FILE

		# Now make a new one, which first calls the user's.
		precmd() {
			vg_precmd
			printf "\033P${PWD}\033\\"
		}

	else
		# The user doesn't have a precmd, so we make one.
		precmd() {
			printf "\033P${PWD}\033\\"
		}
	fi

	# Used to make sure the user doesn't run viewglob on top of a viewglob
	# shell.  The naming is purposely ugly to ensure there's no clobbering.
	export VG_VIEWGLOB_ACTIVE=yep
fi

# Don't want to clutter the environment.
unset VG_ASTERISK
unset VG_SANDBOX
unset VG_TEMP_FILE

