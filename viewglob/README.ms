.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
viewglob 0.9.1 README
.br
September 20, 2004
.AU
Stephen Bach <sjbach@users.sourceforge.net>
.
.Sh INTRODUCTION
.LP
\fIviewglob\fP is a utility designed to complement the Unix shell in graphical environments. It has two parts:
.RS
.IP 1.
A tool that sits as a layer between the shell and X terminal, keeping track of the user's current directory and command line.
.IP 2. 
A graphical display which shows the layouts of directories referenced on the command line (including pwd).
.RE
.LP
The display also shows the results of file globs and expansions as they are typed (hence the name), highlighting selected files and potential name completions.
.LP
The website is here:
.IP
http://viewglob.sourceforge.net
.LP
The sourceforge listing is here:
.IP
http://sourceforge.net/projects/viewglob/
.LP
New releases are announced through Freshmeat, so if you'd like to stay up-to-date you may subscribe to the viewglob project there:
.IP
http://freshmeat.net/projects/viewglob/
.LP
Feedback is welcome!
.LP
.
.Sh NEW IN THIS RELEASE
.LP
First of all, if you've got a previous installation of viewglob that's not from a package manager (i.e. you just ran make && make install), you should remove it before installing this version.  I was incorrectly installing some binaries to .../share/viewglob instead of .../lib/viewglob, which goes against the Filesystem Hierarchy Standard.  If you've still got the sources to the old install, a make uninstall should do the job.  If not, the directory which you want to remove is, by default, /usr/local/share/viewglob.
.LP
The display can now be toggled on and off with the sequence Ctrl-G <SPACE> (or Ctrl-G Ctrl-<SPACE>).  If the display is closed manually, viewglob now interprets it as toggling off, and doesn't disable itself.  A toggled-off display still follows the state of the shell (i.e. when it's toggled on it'll be at the right place).
.LP
To disable viewglob (which you used to be able to do by closing the display), you can use the sequence Ctrl-G Q (but not Ctrl-G Ctrl-Q, because that's too easy to type accidentally).  Disabling a viewglob shell is non-recoverable.
.LP
When you might want to toggle off the display (C-g <SPACE>):
.RS
.IP - 3n
You're dealing with a directory or a series of directories that have a lot of files, and the display's refresh time is slowing you down.
.IP -
You need the extra memory (the display's memory footprint is sometimes not as small as it probably should be); when the display is "toggled off", all of its memory is freed.
.IP -
It's just in the way.
.RE
.LP
When you might want to disable viewglob (C-g q):
.RS
.IP - 3n
You just want a regular shell, but you want to keep the current environment.
.IP -
Maybe you're compiling in the background, and your viewglob shell has begun to stutter.
.RE
.LP
See NEWS for the full list of changes.
.
.Sh INSTALLATION
.LP
See the file \fBINSTALL\fP for generic instructions.  This program follows the ./configure && make && make install convention.
.
.Sh PORTABILITY
.LP
Tested on:
.Vs
FreeBSD 4.10 with GTK+ 2.4.3
GNU/Linux (various) with GTK+ 2.4+
Mac OS X 10.3.5 with fink unstable providing GTK+ 2.4.9 (viewglob 0.9)
.Ve
I don't have access to Solaris, so I can only hope for the best -- it would be awesome if someone could try it for me.
.
.Sh REQUIREMENTS
.LP
viewglob requires a version of bash (any, as far as I know) and GTK+ 2.4.  A version of zsh is also obviously required if you wish to use zsh under viewglob.

