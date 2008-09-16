.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
Viewglob 2.0.4 README
.br
April 26, 2006
.AU
Stephen Bach <sjbach@users.sourceforge.net>
.
.Sh INTRODUCTION
.LP
Viewglob is a filesystem visualization add-on for Bash and Zsh. It tracks the command line and environment of any number of interactive shells (local and remote). A graphical display follows the currently active terminal, listing the contents of directories relevant to its shell and highlighting file selections and potential name completions dynamically.
.LP
The package has three communicating parts:
.RS
.IP "- vgseer, client:"
Supervises an interactive shell and keeps track of command line and environment changes.
.IP "- vgd, server daemon:"
Mediates information exchange between any number of vgseer processes (local or remote) and a Viewglob display.
.IP "- vgmini and vgclassic, graphical displays:"
List the contents of directories relevant to the currently active shell, highlighting file selections and potential name completions.
.RE
.LP
A convenience script "viewglob" is provided as a startup shortcut.
.LP
You can learn more about using this package in the manpages for viewglob(1), vgseer(1), and vgd(1).
.LP
The website for Viewglob is here:
.IP
http://viewglob.sourceforge.net
.LP
Mailing list signup is here:
.IP
http://lists.sourceforge.net/lists/listinfo/viewglob-general
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
Mostly just fixing OS X compatibility.  See NEWS for more information.
.
.Sh INSTALLATION
.LP
For a full install, it's a simple ./configure && make && sudo make install.  If you only wish to install the shell client portion, say for a server without X or GTK+ which you only access through ssh, use the following flag:
.IP
 ./configure --enable-vgseer-only
.LP
See the file \fBINSTALL\fP for generic installation instructions.
.
.Sh REQUIREMENTS
.LP
All installations of Viewglob require a version of Bash (any, as far as I know).  A client-only install requires GLib 2.2, and a full install requires GTK+ 2.4.  A version of Zsh is also obviously required if you wish to use Zsh under Viewglob.
.
.Sh PORTABILITY
.LP
Tested on:
.Vs
FreeBSD 4.10 with GTK+ 2.4.3
GNU/Linux (various) with GTK+ 2.4+
.Ve
This version of Viewglob is also said to run on Mac OS X 10.3.9 (with fink providing GTK+ 2.4).
.LP
The program has been built with portability in mind, so it should (ha) run on most POSIX-compliant systems with minimal fuss.  I happily accept patches for portability issues.

