#!/bin/sh
# Copyright (C) 2004, 2005 Stephen Bach
#
# This script convert a Viewglob configuration file into a string that looks
# like a list of command line arguments.
#
# Viewglob is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Viewglob is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Viewglob; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

[ -z "$1" ] && exit 0
[ -f "$1" ] || exit 0

ARGS=

while read opt arg; do
	case "$opt" in
		\#*)
			# Ignore comments.
			;;
		"")
			# Ignore blank lines.
			;;
		*)
			ARGS="${ARGS} --$opt"
			if [ ! -z "$arg" ]
				then ARGS="${ARGS} $arg"
			fi
			;;
	esac

done < "$1"

if [ ! -z "$ARGS" ]
	then echo program $ARGS
fi

