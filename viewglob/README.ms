.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
viewglob 1.0 README
.br
September 28, 2004
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
The display reveals the results of file globs and expansions as they are typed (hence the name), highlighting selected files and potential name completions.
.LP
It can also be used as a surrogate terminal, where keystrokes typed in the display are passed to the shell.  Files and directories can be double-clicked to insert their names and/or paths into the terminal.
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
The biggest change is the addition of file name insert functionality.  If you double click a file in the display (or the directory header), its name will be passed as text to the terminal.  Holding shift will send the full path to the file.
.LP
By default viewglob is in "smart insert" mode.  There's more info about that in the man page.
.LP
This release also allows you to type into the display and have the keystrokes passed to the terminal.  This works well in my testing, but let me know if you're finding that the display doesn't pass something along correctly.  Note that navigation keys are captured by the display instead of being passed on.
.LP
Two large issues with FreeBSD (and assumedly the other BSDs as well) are also fixed.  I've been unable to track down the problem with Ctrl-C and SIGINT, though; if you are a terminal I/O savvy BSD user willing to help, please get in touch with me.
.LP
See NEWS for the full list of important changes.
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

