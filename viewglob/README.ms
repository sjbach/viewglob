.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
viewglob 0.8.4 README
.br
August 31, 2004
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
This is a bug fix release.  See NEWS and ChangeLog for more information.  See the manpage (man viewglob) for notes and usage information.
.LP
The website is here:
.IP
http://viewglob.sourceforge.net
.LP
The sourceforge project is here:
.IP
http://sourceforge.net/projects/viewglob/
.LP
Feedback is welcome!
.
.Sh INSTALLATION
.LP
See the file \fBINSTALL\fP for generic instructions.  This program follows the ./configure && make && make install convention.
.
.Sh PORTABILITY
.LP
Tested on FreeBSD 4.10 and various GNU/Linux distributions.  I don't have access to OS X or Solaris, so I can only hope for the best -- it would be awesome if someone could try it for me.
.
.Sh REQUIREMENTS
.LP
viewglob requires a version of bash (any, as far as I know) and GTK+ 2.4.

