Xlock95 Readme.txt file.

Version 0.11


Requirements.
-------------

Windows 95, 98, ME or Windows NT, 2000, XP running 256 colors or greater.
Don't know the minimum spec. machine, but I have run it on a 
Pentium 166 (Win95), Pentium Pro 200 (Win NT), Pentium II 233 (Win95),
Pentium III 1000 (Win98) and Pentium 4 3Ghz (XP Pro).

Building.
---------
Cygwin - www.cygwin.com
   (Gnu gcc compiler for windows)

I no longer use MS VC++ to compile. It took about a day to
convert over to cygwin (and that was mainly setting up the
Makefiles). To compile under VC++ would take a little work
but it should be fairly painless.

At top level
make -f Makefile.win32

Note: for other compilers things may be different. For VC++ there
      were many more steps to set up.


Note2: When you use Debug configuration, you will only get one mode
       working. When you want to debug a mode you will have to
       change the mode you want. I set this up so when I debugged
       any new mode that I have just added, the only mode displayed
       was the one I set. To set debug mode, modify the Makefile in
           the xlock sub-directory to define _DEBUG (ie -D_DEBUG)


Installation.
-------------

Place the file "xlock95.scr" in either Windows system or Windows
directory ("C:\Windows\System\", "C:\Windows\", "C:\WINNT
which ever works, `cygpath -W` should tell you what this is)

make -f Makefile.win32 install

Set "xlock95" to be your screen saver. When you enter the
Screen Saver tab in the Display Properties it may take a little
while for control to come back to you if xlock95 is set.
If it does not show up the scr file is probably in the wrong directory.

Alternatively you can just double click the .scr from
explorer and it should run fine (but thats not the
same as setting it up as a screen saver).

Windows Screen Saver commandline arguments:

xlock95.scr               Runs screensaver's configuration window.
xlock95.scr /s            Runs the screensaver on the full screen.
xlock95.scr /p 1234567    Runs the screensaver in windowid 1234567.

The Scrnsave.lib clone provided by Cygwin/MinGW
(libscrnsave.a) doesn't appear to support multiple screens.
The source is available from Cygwin in the w32api package.
According to the MS screensaver docs, linking with the latest
scrnsaver.lib should handle multiple screens automatically.
See
http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/monitor_2eur.asp
xlock95 is also not yet coded to handle multiple screens.

Notes.
------

If you have used an older version than 0.4, it was called 
"xlock.scr". This is not needed anymore and can be deleted from
Windows system directory. Also timings from the older versions are
different than the versions 0.4 and above, so modes will be either 
faster or slower.

If you have used a version between 0.4 and 0.6, it was called
"xlockmore.scr". This is not needed anymore and can be deleted from
Windows system directory.

When the screen saver is started, it randomly chooses a mode. At this
time, there are 86 modes. These are:

Ant, Ant3D, Apollonian, Ball, Bat, Blot, Bouboule, Bounce, Braid,
Bug, Clock, Coral, Crystal, Daisy, Deco, Demon, Dilemma, Discrete,
Dragon, Drift, Euler2D, Eyes, Fadeplot, Fiberlamp, Flame, Flow, 
Forest, Galaxy, Grav, Helix, Hop, Hyper, Ico, Image, Juggle, Julia,
Kaleid, Kumppa, Laser, Life, Life1D, Life3D, Lightning, Lisa, Lissie,
Loop, Lyapunov, Mandelbrot, Matrix, Maze, Mountain, Munch, Nose,
Pacman, Petal, Petri, Polyominoes, Pyro, Qix, Roll, Rotor, Scooter,
Shape, Sierpinski, Slip, Space, Sphere, Spiral, Spline, Star,
Starfish, Swarm, Tetris, Thornbird, Tik_tak, Toneclock, Triangle,
Tube, Turtle, Vines, Voters, Wator, Wire, World, Worm, Xjack

In the X-Windows version it is possible to set variables that affect
how long certain things are done (eg how many vines or blots to be
drawn before clearing the screen). These variables are cycles, size and
batch counts, and every mode is affected by them. The Win95/NT version
currently isn't able to set these variables and therefore they use the
defaults. Also some modes read in settings from files (eg life3d reads
from file .life3d.rule3d). At the moment this isn't being done either.

Running it from the commandline yields "Hello World!" and "Okie dokie".
These are the config dialogs. Eventually if
proper configuration is added, this is where it should
go. You'll get the same dialogs when you press the
settings button on the screensaver tab when
xlock95 is the active screensaver.


Future of Xlockmore95.
----------------------

At this stage I have done intermediate releases of Xlockmore95. I
know there are more modes I can add, but as to adding all the modes 
that exist in Xlockmore, I'm not sure it is possible. There are modes 
that use XPM images (X PixMap images), opengl, and there may be 
X11 functions that just won't map easily to win32 functions (I've been
adding them on an as needed basis).

This is a list of what I would like to do (but I'm not promising)...
 . add more modes
 . allowing users to set the cycles and batch count variables, probably
   in the settings window.
 . allowing users to set specific options to modes that require it,
   probably in the settings window.
 . allowing modes to read settings from files
 . handle colors as X11 does
 . add all modes (even opengl modes)


Differences with X11 & Bugs.
----------------------------

ball, crystal, munch, pyro, tik_tak: 
XSetFunction() isn't drawing correctly.

ant: truchet drawing messed up
ant3d and toneclock: double buffering ok but now label flashes
bouboule & some others: flickers.
crystal & tic_toc: xor errors in beginning
julia: red square dot
nose: text messed up
pacman: bad graphics red
petri: only red germs
scooter: red rects
tube: needs closure for stars and triangles (does not handle sharp angles)
xjack: messed up when text gets to bottom

There are small differences in Line Attributes between X11 and WIN32

Color model is totally different, so no color cycling occurs.


Change Log.
-----------

version 0.1
. initial release, Vines, Fadeplot, Sierpinski Triangles, Blot

version 0.2 (not a public release)
. added Coral mode

version 0.3
. wasn't initialising some pointer variables, which could cause an 
  error if circumstances allowed. This is now fixed.

. added the ability to redefine the color palette in a mode class.
  This means it is possible to have modes that don't have a palette
  saturation of 1.0 to recalculate the palette to how they require.
  For example Blot mode now has a palette saturation of 0.4

. added Deco & Forest modes

version 0.4 (debug build)
. added code for XDrawArc(), XFillArc() & XFillPolygon()

. changed from using my own base source code to using the source code
  for xlockmore 4.07. I still have to use some of my own code for 
  start-up and initialisation of the screen saver (cannot get around 
  this due to differences between X11 and Win32), and of course all
  the X11 functions and structures. But the rest is xlockmore 4.07 
  code. At some point I will see if David will integrate the Win32
  source code into the official distrubution.

. As a result a lot more modes are now added. These are:
  Braid, Clock, Daisy, Drift, Flame, Galaxy, Geometry, Grav, Helix, 
  Hop, Lightning, Lisa, Lissie, Petal, Qix, Roll, Rotor, Sphere, 
  Spiral, Spline, Triangle, Turtle

version 0.5
. determined that the problem with the Galaxy mode in the release 
  build was to do with VC++. When VC++ optimizes for speed, Galaxy 
  does not work. I now optimize for size.

. life3d: added an extra check to life3dfile, to see if it was NULL
  before doing the strlen() on it. This has fixed a crash I was 
  getting with it. This is the only mode I have changed.

. added code for XCreateGC(), XChangeGC(), XDrawLines(), XFillArcs(),
  XFillRectangles(), XSetFunction() so the following modes now work:
  Ant, Ball, Bouboule, Bug, Crystal, Demon, Laser, Life3d, Loop, 
  Mountain, Munch, Pyro, Wire

version 0.6
. added code for XDrawSegments(), XClearArea() so the following modes
  now work: Hyper, Ico, Kaleid, Worm

. added code to set each modes default options (if there are any). 
  This affects every mode, but only boolean and string types are being 
  set at the moment. Ico refused to work until either edges or faces 
  were defined as true. This also allowed me to remove the code I
  added to life3d.

version 0.7
. changed my build environment. Now using cygwin as my compiler. As a
  result some small changes were needed to xlock.c. I am building it
  with the -mno-cygwin option (mingw mode ???) so the cywin1.dll is
  not used. Created Makefiles to build the screensaver.

. no longer use the ScrPlus library, but the scrnsave library that
  comes with cygwin. Initially used SetTimer(), but this was too slow, so
  I created a thread, and use SendMessage() WM_TIMER from there.

. modified XFillArc() so it now draws the Arc with a thin line before
  it fills it. This fixes the minute and hour hands in clock. Other
  modes may be affected (ball, bouboule, daisy, galaxy, grav), though
  they looked OK.

. added displaying the mode and its description in the top left hand
  corner.

. filled in code for XDrawString(), though it doesn't affect anything
  (yet)

. added code for XDrawRectangle(), XStoreColors() so the following 
  modes now works: mandelbrot, tube

. added empty function calls to XPutImage(), XCreateBitmapFromData(), 
  XSetBackground(), XSetFillStyle(), XSetStipple() so Maze now works
  (minus the operating system graphic)

. fixed up XSetFunction so now it should utilise different drawing 
  functions. It does not seem to be 100% correct, but much better than
  before.

version 0.8 (8-Oct-2004)
. added code for XPutImage() so now the operating system graphic is
  drawn in Maze.
  
. added code for XCreateBitmapFromData(), XFreePixmap(), XCreateGC(),
  XFreeGC()

version 0.9 (15-May-2005)
. fixed XCreateGC() and XSetStipple() so the bad paths of Maze are
  set properly (still a problem setting the stipple (brush) the right
  color). Other modes are broken for now (such as Galaxy and Grav) as
  they leave trails.

version 0.10 (26-Jul-2005)
. fixed XDrawArc(), XFillArc() & XFillArcs so they draw fully the
  complete angles. This was causing a graphical error in some modes,
  for example clock.
  
. fixed setting the stipple brush color on an XSetForeground. Now the
  bad paths of Maze are the correct color.

. fixed most of the broken modes. I think this had to do with 
  recreating the pen for a filled arc in XFillArc(). It wasn't always
  creating the new pen. I create it now as null, so the border is not
  drawn, and to make up for this I make the arc a little larger in the
  function. This fixes Galaxy and Grav. Ball is still broken.

. fixed ball. The problem had to do with XCreateGC(). The way I had
  implemented this function meant only about 5 GCs could be created
  before problems were occuring. This was fine for all other modes,
  apart from ball which needs about 20 GCs. I now have totally
  reorganised how GCs are created and are related back to a device
  context (DC), which is what Windows uses. I now only have 1 DC, and
  reset it when I use a different GC. This means that modes will run
  slower. (31-Aug-2005)

version 0.11 (29-Sep-2005)
. updated to use xlockmore-5.19. This is the first step of getting my
  source code integrated into the offical source. A number of changes
  had to be made to my code, but it was mostly OK. The following modes
  are removed in this new version (removed in the official code): 
  geometry (14-Sep-2005)

. many modes have changed, but some have now a few issues (maze - OS
  graphic missing again, ico - not moving and artifacts left,
  galaxy - awful flickering as there is no double buffering).
  (29-Sep-2005)

. there are many new modes but I have not attempted to implement any
  just yet. (29-Sep-2005)

[Now distributed with xlockmore... David changed back xlockmore95 back
to xlock95... David hopes this is OK with all concerned.]

Download.
---------

Please don't email David with questions about Xlockmore95.
[Well actually David does not mind if you cc him  :)  ]
They can be directed to me: petey_leinonen@yahoo.com.au

You can find the latest information about xlockmore from 
David A. Bagley's site at http://www.tux.org/~bagleyd/xlockmore.html


Copyright.
----------

Copyright (c) 1988-91 by Patrick J. Naughton
Copyright (c) 1993-2005 by David A. Bagley
Copyright (c) 1998-2005 by Petey Leinonen (win95/NT version)

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation. 

Please note that I do not provide any warranties, and that you use this
executable and/or source code at your own risk. I will not be held 
responsible for any damage that is incurred by using the executable 
and/or source code.


Petey Leinonen,   14 September 2005
