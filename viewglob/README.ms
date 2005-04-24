.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
Viewglob 2.0 README
.br
April 25, 2005
.AU
Stephen Bach <sjbach@users.sourceforge.net>
.
.Sh INTRODUCTION
.LP
\fIViewglob\fP is a filesystem visualization add-on for Bash and Zsh when used in windowing environments.  It has three communicating parts:
.RS
.IP vgseer:
Supervises an interactive shell and keeps track of command line and environment changes.
.IP vgd:
Mediates information exchange between any number of vgseer processes (local or remote) and a Viewglob display.
.IP "the display:"
Lists the contents of directories relevant to the currently active shell, highlighting file selections and potential name completions.
.RE
.LP
The website is here:
.IP
http://viewglob.sourceforge.net
.LP
The sourceforge listing is here:
.IP
http://sourceforge.net/projects/viewglob/
.LP
Feedback is welcome!
.LP
.
.Sh NEW IN THIS RELEASE
.LP
The Viewglob display was sometimes forgetting to convert filenames from their locale encoding to the encoding which GLib uses, resulting in frequent annoying error messages for people not using UTF-8.  This has been fixed and some information about encodings has been added to the manual.
.
.Sh INSTALLATION
.LP
For a full install, it's a simple ./configure && make && sudo make install.  If you only wish to install the shell client portion, say for a server without X which you only access through ssh, use the following flag:
.IP
 ./configure --enable-vgseer-only
.LP
See the file \fBINSTALL\fP for generic installation instructions.
.
.Sh PORTABILITY
.LP
Tested on:
.Vs
FreeBSD 4.10 with GTK+ 2.4.3
GNU/Linux (various) with GTK+ 2.4+
Cygwin with GTK+ 2.4.6
.Ve
Previous versions of Viewglob were said to run on Mac OS X 10.3.5 (with fink unstable providing GTK+ 2.4), but it has not been tested for this release.
.LP
The program has been built with portability in mind, so it should (ha) run on most POSIX-compliant systems with minimal fuss.  I happily accept patches for portability issues.
.
.Sh REQUIREMENTS
.LP
Viewglob requires a version of Bash (any, as far as I know) and GTK+ 2.4.  A version of Zsh is also obviously required if you wish to use Zsh under Viewglob.

