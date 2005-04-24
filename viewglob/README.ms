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
\fIViewglob\fP is a utility designed to add a kind of visualization to Bash and Zsh when used in windowing environments.  It has two parts:
.RS
.IP 1.
A tool that sits inconspicuously as a layer between a shell and its X terminal, keeping track of the user's current directory and command line.
.IP 2. 
A GTK+ display which shows the layouts of pwd and other directories as they are referenced.
.RE
.LP
The display updates dynamically to highlight selected files and potential name completions, including from shell expansions and globs - hence the name.
.LP
It can also be used as a surrogate terminal, where keystrokes typed into the display are passed to the shell.  Likewise, files and directories can be double-clicked to insert their names and/or paths into the terminal.
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
For a full install, it's a simple ./configure && make && sudo make install.  If you only wish to install the shell client portion, say for a server without X which you only access through ssh, add the following flag:
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
Mac OS X 10.3.5 with fink unstable providing GTK+ 2.4.9 (Viewglob 0.9)
.Ve
The program has been built with portability in mind, so it should (ha) run on most POSIX-compliant systems with minimal fuss.  I happily accept patches for portability issues.
.
.Sh REQUIREMENTS
.LP
Viewglob requires a version of Bash (any, as far as I know) and GTK+ 2.4.  A version of Zsh is also obviously required if you wish to use Zsh under Viewglob.

