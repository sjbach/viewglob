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

# Make sure the user doesn't run viewglob on top of a viewglob shell
# The naming is purposely ugly to ensure there's no clobbering.
export VG_VIEWGLOB_ACTIVE=yep

# Source the user's run-control file.
[ -f ~/.zshrc ] && . ~/.zshrc

if [ "$VG_SANDBOX" = yep ]; then
	# This is all for the sandbox shell.

	# We have to disable this stuff because...
	# History expansion is a no-go.
	# The NOMATCH error message prevents glob-expand from running.
	# Global aliases could be dangerous to the filesystem, and they can't be
	#   disabled without disabling all aliases (AFAIK).
	# No spell check!
	# BAD_PATTERN can prevent glob-expand from running.
	# The sandbox should NEVER modify the file system, but turn off clobbering
	#   and star_silent just in case.
	# Don't save history in the sandbox.
	# The user could put setopt zle in their .zshrc, overriding seer's +Z.
	setopt \
		NO_BANG_HIST      \
		NO_NOMATCH        \
		NO_ALIASES        \
		NO_CORRECT        \
		NO_CORRECT_ALL    \
		NO_BAD_PATTERN    \
		NO_CLOBBER        \
		NO_RM_STAR_SILENT \
		NO_RCS            \
		NO_ZLE

	# And we don't want these functions in the sandbox.
	unfunction chpwd periodic precmd preexec        2>/dev/null
	unfunction TRAPHUP TRAPDEBUG TRAPEXIT TRAPZERR  2>/dev/null

	# Seriously, don't save the history.
	unset HISTFILE

	# Only viewglob programs (glob-expand) in the path.
	PATH="$VG_DIR"

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

		if [ "$TMPDIR" ]; then
			VG_TEMP_FILE="$TMPDIR/vg$$"
		else
			VG_TEMP_FILE=/tmp/vg$$
		fi

		# Try to create a temporary file.
		if ! touch "$VG_TEMP_FILE"; then
			print "viewglob: Could not make a temporary file" >&2
			exit 9   # Nothing to do but give up.
		fi

		# Rename the user's precmd to vg_precmd (sad hack).
		typeset -f precmd | sed '1s/precmd/vg_precmd/' >> "$VG_TEMP_FILE"
		chmod u+x "$VG_TEMP_FILE"
		. "$VG_TEMP_FILE"
		rm -f "$VG_TEMP_FILE"

		# Now make a new one, which first calls the user's.
		precmd() {
			vg_precmd
			printf "\033P${PWD}\033\\"
		}

	else
		# The user doesn't have a precmd, so we just make one.
		precmd() {
			printf "\033P${PWD}\033\\"
		}
	fi

	# If viewglob is to exit correctly, zsh shouldn't handle SIGHUP.
	unfunction TRAPHUP  2>/dev/null
fi

# Re-set this just in case.
export VG_VIEWGLOB_ACTIVE=yep

# Don't want to clutter the environment.
unset VG_ASTERISK
unset VG_SANDBOX
unset VG_TEMP_FILE
unset VG_DIR

