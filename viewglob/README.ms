.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
Viewglob 2.0.1 README
.br
May 5, 2005
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
The sourceforge listing is here:
.IP
http://sourceforge.net/projects/viewglob/
.LP
Feedback is welcome!
.LP
.
.Sh NEW IN THIS RELEASE
.LP
Unix-domain sockets are now used by default instead of Internet-domain when using Viewglob locally.  This is to prevent a security issue where an attacker could see an inattentive user's Viewglob display by being sneaky with X forwarding over ssh.
.LP
The refocus/raise command has been tweaked to be much less broken.
.
.Sh INSTALLATION
.LP
For a full install, it's a simple ./configure && make && sudo make install.  If you only wish to install the shell client portion, say for a server without X or GTK+ which you only access through ssh, use the following flag:
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
.Ve
Previous versions of Viewglob were said to run on Mac OS X 10.3.5 (with fink unstable providing GTK+ 2.4), but it has not been tested for this release.
.LP
The program has been built with portability in mind, so it should (ha) run on most POSIX-compliant systems with minimal fuss.  I happily accept patches for portability issues.
.
.Sh REQUIREMENTS
.LP
Viewglob requires a version of Bash (any, as far as I know) and GTK+ 2.4.  A version of Zsh is also obviously required if you wish to use Zsh under Viewglob.

