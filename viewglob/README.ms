.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
viewglob 0.9 README
.br
September 9, 2004
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
The sourceforge project is here:
.IP
http://sourceforge.net/projects/viewglob/
.LP
Feedback is welcome!
.
.Sh NEW IN THIS RELEASE
.LP
Preliminary support for zsh has been added.  From my testing viewglob works well with the advanced completion features of zsh, but there is a lot of ground to cover.  If you use zsh and find that viewglob isn't working well at all, please let me know - I'd be mostly interested in seeing your .zshrc file.
.LP
This release also adds the capability to navigate the display from the terminal.  This is done in a way similar to GNU screen's Ctrl-A commands - you type the control key followed by a direction character.
.LP
I've chosen Ctrl-G as viewglob's control key, because a. the letter makes sense, and b. Ctrl-G is by default just a beeping (i.e. more annoying) version of Ctrl-C in most shell configurations, and I don't think people use it.  But I could be wrong.
.LP
If you think this is a bad move, let me know.  Making the control key configurable would complicate things, but I'd be willing to go the distance if there's a reason to do so.
.LP
I've tried to accomodate both emacs and vi users for key bindings:
.RS
.IP Down: 12n
j, n, <Down Arrow>
.IP Up:
k, p, <Up Arrow>
.IP Page\ Down:
f, d, <PgDown>
.IP Page\ Up:
b, u, <PgUp>
.RE
.LP
The Ctrl counterparts also work.  Read NEWS and ChangeLog for more information.  See the manpage (man viewglob) for notes and usage information.
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
viewglob requires a version of bash (any, as far as I know) and GTK+ 2.4.  A version of zsh is also obviously required if you wish to use zsh under viewglob.

