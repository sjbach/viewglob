
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

	# Adding semaphores to the ends of these variables.
	export PROMPT="${PROMPT}%{"$'\e[0;30m'"%}%{"$'\e[0m'"%}%{"$'\e[1;37m'"%}%{"$'\e[0m'"%}"

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

fi

# Don't want to clutter the environment.
unset VG_ASTERISK
unset VG_SANDBOX
unset VG_TEMP_FILE
