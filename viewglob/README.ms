.ss 12 0
.ie n .nr LL 75m
.el   .nr LL 6.5i
.so giftcurs.tmac
.
.TL
Viewglob 1.0.3 README
.br
March 6, 2005
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
New releases are announced through Freshmeat, so if you'd like to stay up-to-date you may subscribe to the Viewglob project there:
.IP
http://freshmeat.net/projects/viewglob/
.LP
Feedback is welcome!
.LP
.
.Sh NEW IN THIS RELEASE
.LP
My biggest issue with Viewglob has always been the awkwardness of dealing with separate windows.  As you shuffle around, the display gets separated from the terminal, and soon you're just using the shell and none of the Viewglob.  I considered creating a terminal with an integrated display, but that feels like giving up.
.LP
In my first attempt at combatting this problem I've added a key sequence, Cg-<TAB> or Cg-<ENTER> (Cg = Ctrl-G), which will ask the window manager to refocus the terminal and the display, bringing them both to the top layer.  So when you've got one but not the other, type this sequence and both will be visible.
.LP
This has necessitated a new reliance on Xlib (previously only implied with GTK), which may cause compatibility issues.  If so, please let me know.  Also, whether this feature works or not depends on your window manager.  If it has implemented a good portion of the EWMH specification you'll probably be okay.
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
Mac OS X 10.3.5 with fink unstable providing GTK+ 2.4.9 (Viewglob 0.9)
.Ve
The program has been built with portability in mind, so it should (ha) run on most POSIX-compliant systems with minimal fuss.  I happily accept patches for portability issues.
.
.Sh REQUIREMENTS
.LP
Viewglob requires a version of Bash (any, as far as I know) and GTK+ 2.4.  A version of Zsh is also obviously required if you wish to use Zsh under Viewglob.
.
.Sh NEW IN PREVIOUS RELEASE 1.0.2
.LP
If you find the default font sizes in the display are too big / too small in your window manager, you can change them with the --font-size-modifier flag.  Passing +2 will increase all fonts by two, passing -2 will decrease all by two (duh).  Hardcores can now keep the font size so small that they can fit all of /usr/bin in the display at once, if they so choose.
.LP
Also, the code managing pseudo terminals has been replaced with code from rxvt, which has been shown to improve compatibility.  BSD users may now Ctrl-C to their heart's content.
.
.Sh NEW IN PREVIOUS RELEASE 1.0.1
.LP
It was sometimes inconvenient to be in directories with long listings because you wouldn't be able to see the listings of other directories without scrolling.  There is now an option to list directories in reverse order, i.e. the directory you reference last appears at the top of the display.  Actually, there are now three ordering modes (used with the flag --dir-order):
.RS
.IP ascending
Directories appear in reverse order.
.IP ascending-pwd-first
Directories appear in reverse order, but the current directory is always at the top.
.IP descending
Classic ordering and the default mode: directories appear in the order you reference them (and with the current directory at the top).
.RE
.
.Sh NEW IN PREVIOUS RELEASE 1.0
.LP
The biggest change is the addition of file name insert functionality.  If you double click a file in the display (or the directory header), its name will be passed as text to the terminal.  Holding shift will send the full path to the file.
.LP
By default Viewglob is in "smart insert" mode.  There's more info about that in the man page.
.LP
This release also allows you to type into the display and have the keystrokes passed to the terminal.  This works well in my testing, but let me know if you're finding that the display doesn't pass something along correctly.  Note that navigation keys are captured by the display instead of being passed on.
.LP
Two large issues with FreeBSD (and assumedly the other BSDs as well) are also fixed.  I've been unable to track down the problem with Ctrl-C and SIGINT, though; if you are a terminal I/O savvy BSD user willing to help, please get in touch with me.
.
.Sh NEW IN PREVIOUS RELEASE 0.9.1
.LP
First of all, if you've got a previous installation of Viewglob that's not from a package manager (i.e. you just ran make && make install), you should remove it before installing this version.  I was incorrectly installing some binaries to .../share/viewglob instead of .../lib/viewglob, which goes against the Filesystem Hierarchy Standard.  If you've still got the sources to the old install, a make uninstall should do the job.  If not, the directory which you want to remove is, by default, /usr/local/share/viewglob.
.LP
The display can now be toggled on and off with the sequence Ctrl-G <SPACE> (or Ctrl-G Ctrl-<SPACE>).  If the display is closed manually, Viewglob now interprets it as toggling off, and doesn't disable itself.  A toggled-off display still follows the state of the shell (i.e. when it's toggled on it'll be at the right place).
.LP
To disable Viewglob (which you used to be able to do by closing the display), you can use the sequence Ctrl-G Q (but not Ctrl-G Ctrl-Q, because that's too easy to type accidentally).  Disabling a Viewglob shell is non-recoverable.
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
When you might want to disable Viewglob (C-g q):
.RS
.IP - 3n
You just want a regular shell, but you want to keep the current environment.
.IP -
Maybe you're compiling in the background, and your Viewglob shell has begun to stutter.
.RE
.
.Sh NEW IN PREVIOUS RELEASE 0.9
.LP
Preliminary support for zsh has been added.  From my testing Viewglob works well with the advanced completion features of zsh, but there is a lot of ground to cover.  If you use zsh and find that Viewglob isn't working well at all, please let me know - I'd be mostly interested in seeing your .zshrc file.
.LP
This release also adds the capability to navigate the display from the terminal.  This is done in a way similar to GNU screen's Ctrl-A commands - you type the control key followed by a direction character.
.LP
I've chosen Ctrl-G as Viewglob's control key, because a. the letter makes sense, and b. Ctrl-G is by default just a beeping (i.e. more annoying) version of Ctrl-C in most shell configurations, and I don't think people use it.  But I could be wrong.
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
The Ctrl counterparts also work.

