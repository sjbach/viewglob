.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
viewglob 0.8.0 README
.AU
Stephen Bach <sjbach@users.sourceforge.net>
.
.Sh INTRODUCTION
.LP
This is the initial release.  \fIviewglob\fP is a utility designed to complement the Unix shell in graphical environments. It has two parts:
.RS
.IP 1.
A tool that sits as a layer between the shell and X terminal, keeping track of the user's current directory and command-line.
.IP 2. 
A graphical display which shows the layouts of directories referenced on the command-line (including pwd).
.RE
.LP
The display also shows the results of file globs and expansions as they are typed (hence the name), highlighting selected files and potential name completions.
.LP
See the manpage (man viewglob) for notes and usage information.
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
It runs for sure on FreeBSD 4.10 and GNU/Linux 2.6. I don't have access to OS X or Solaris, so I can only hope for the best -- it would be awesome if someone could try it for me. I looked into supporting Cygwin, but ran into a wall with its lack of a mkfifo call.
.
.Sh REQUIREMENTS
.LP
viewglob requires a version of bash, and at least GTK+ 2.4.
.
.Sh SAFETY
.LP
viewglob obtains its expansion information in a hacky manner: it forks a "sandbox" shell, which makes repeated calls to the glob-expand program (included in this package) when the user's command line changes.  The advantage of this method is that viewglob isn't tied to any particular version or configuration of bash.  The commands are of this form:

cd <user's pwd> && glob-expand <sanitized command line> >> <a fifo> ; cd / 

If viewglob were perfect, the sanitize function would always ensure <sanitized
command line> never contains any unescaped control operators such as these: ||
& && ; ;; |.  Hopefully this is the case... but _I'm_ not perfect, so I can't
promise that.  If you don't see the problem, imagine an unlikely scenario
where somehow <sanitized command line> happened to be: "; rm *".  Now what the
sandbox shell is actually running is equivalent to the following series of
commands:

if cd <user's pwd>
.br
	then glob-expand
.br
fi
.br
rm * >> <a fifo>
.br
cd /

Ouch.  Now, I can't see any way that this could happen.  The code which generates <sanitized command line> is pretty solid in my opinion, and errs on the side of caution.  I use viewglob, and I believe viewglob is safe for you to use too.  But I would definitely not feel comfortable keeping this possibility to myself.  If viewglob _does_ harm your data, I'll feel terrible about it.


