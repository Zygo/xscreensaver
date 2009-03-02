$ save_verify='f$verify(0)
$! set ver 
$!
$!      VMS compile and link for xlockmore
$!
$! USAGE:
$! @make [debug clobber clean]
$!       debug : compile with degugger switch
$!       clean : clean all except executable
$!       clobber : clean all
$!
$! If you have
$!              XPM library
$!              XVMSUTILS library (VMS6.2 or lower)
$!              Mesa GL library
$! insert the correct directory instead of X11 or GL:
$ xvmsutilsf="X11:XVMSUTILS.OLB"
$ xpmf="X11:LIBXPM.OLB"
$ glf="GL:LIBMESAGL.OLB"
$ gluf="GL:LIBMESAGLU.OLB"
$!
$! Assume C.
$ deccxx=0
$! Assume C++ (but may not link on VMS6.2 or lower)
$! deccxx=1
$! test on C++.
$! deccxx=f$search("SYS$SYSTEM:CXX$COMPILER.EXE") .nes. ""
$!
$! Already assumes DEC C on Alpha.
$! Assume VAX C on VAX.
$! decc=0
$! Assume DEC C on VAX.
$! decc=1
$! test on DEC C.
$ decc=f$search("SYS$SYSTEM:DECC$COMPILER.EXE") .nes. ""
$!
$! if vroot<>0 the use of the root window is enabled
$ vroot=1
$! vroot=0
$!
$! if bomb<>0 the use bomb mode is included (does not come up in random mode)
$ bomb=1
$! bomb=0
$!
$! if sound<>0 sound capability is included (only available on Alpha)
$! from vms_amd.c and vms_amd.h
$ sound=1
$! sound=0
$!
$! Memory Check stuff.  Very experimental!
$ check=0
$! check=1
$!
$!
$! NOTHING SHOULD BE MODIFIED BELOW
$!
$ if p1 .eqs. "CLEAN" then goto Clean
$ if p1 .eqs. "CLOBBER" then goto Clobber
$!
$ defs=="VMS"
$ dtsaver=f$search("SYS$LIBRARY:CDE$LIBDTSVC.EXE") .nes. ""
$ xpm=f$search("''xpmf'") .nes. ""
$ gl=f$search("''glf'") .nes. ""
$ glu=f$search("''gluf'") .nes. ""
$ axp=f$getsyi("HW_MODEL") .ge. 1024
$ sys_ver=f$edit(f$getsyi("version"),"compress")
$ if f$extract(0,1,sys_ver) .nes. "V"
$ then
$   type sys$input
This script will assume that the operating system version is at least V7.0. 
$!
$   sys_ver="V7.0"
$ endif
$ sys_maj=0+f$extract(1,1,sys_ver)
$ if sys_maj .lt. 7
$ then
$   xvmsutils=f$search("''xvmsutilsf'") .nes. ""
$ endif
$!
$! Create .opt file
$ close/nolog optf
$ open/write optf xlock.opt
$!
$ if xpm then defs=="''defs',USE_XPM"
$ if gl then defs=="''defs',USE_GL"
$ if dtsaver then defs=="''defs',USE_DTSAVER"
$ if axp .and. sound then defs=="''defs',USE_VMSPLAY"
$ if sys_maj .lt. 7
$ then
$   if xvmsutils then defs=="''defs',USE_XVMSUTILS"
$ endif
$ if vroot then defs=="''defs',USE_VROOT"
$ if bomb then defs=="''defs',USE_BOMB"
$ if check then defs=="''defs',DEBUG"
$! The next must be the last one.
$ if sys_maj .ge. 7
$ then
$   defs=="''defs',SRAND=""""srand48"""",LRAND=""""lrand48"""",MAXRAND=2147483648.0"
$ endif
$!
$! Establish the Compiling Environment
$!
$! Set compiler command
$! Put in /include=[] for local include file like a pwd.h ...
$!   not normally required.
$ if deccxx
$ then
$   cc=="cxx/include=([],[.xlock])/define=(''defs')"
$ else
$   if axp
$   then
$!     cc=="cc/standard=vaxc/include=([],[.xlock])/define=(''defs')"
$     cc=="cc/include=([],[.xlock])/define=(''defs')"
$   else
$     if decc
$     then
$!       cc=="cc/decc/standard=vaxc/include=([],[.xlock])/define=(''defs')"
$       cc=="cc/decc/include=([],[.xlock])/define=(''defs')"
$     else ! VAX C
$       cc=="cc/include=([],[.xlock])/define=(''defs')"
$     endif
$   endif
$ endif
$ if p1 .eqs. "DEBUG" .or. p2 .eqs. "DEBUG" .or. p3 .eqs. "DEBUG"
$ then
$   if deccxx
$   then
$     cc=="cxx/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$   else
$     if axp
$     then
$!       cc=="cc/deb/noopt/standard=vaxc/include=([],[.xlock])/define=(''defs')/list"
$       cc=="cc/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$     else
$       if decc
$       then
$!         cc=="cc/deb/noopt/decc/standard=vaxc/include=([],[.xlock])/define=(''defs')/list"
$         cc=="cc/deb/noopt/decc/include=([],[.xlock])/define=(''defs')/list"
$       else ! VAX C
$         cc=="cc/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$       endif
$     endif
$   endif
$   link=="link/deb"
$ endif
$!
$ if axp .or. .not. decc
$ then
$   define/nolog sys sys$library
$ endif
$!
$ write sys$output "Linking Include Files"
$! call make eyes.xbm   "set file/enter=[]eyes.xbm [.bitmaps]m-dec.xbm"    [.bitmaps]m-dec.xbm
$ call make eyes.xbm   "set file/enter=[]eyes.xbm [.bitmaps]m-grelb.xbm"  [.bitmaps]m-grelb.xbm
$ call make flag.xbm   "set file/enter=[]flag.xbm [.bitmaps]m-dec.xbm"    [.bitmaps]m-dec.xbm
$! call make ghost.xbm  "set file/enter=[]ghost.xbm [.bitmaps]m-dec.xbm"   [.bitmaps]m-dec.xbm
$ call make ghost.xbm  "set file/enter=[]ghost.xbm [.bitmaps]m-ghost.xbm" [.bitmaps]m-ghost.xbm
$ call make image.xbm  "set file/enter=[]image.xbm [.bitmaps]m-dec.xbm"   [.bitmaps]m-dec.xbm
$ call make life.xbm   "set file/enter=[]life.xbm [.bitmaps]s-grelb.xbm"  [.bitmaps]s-grelb.xbm
$! call make life.xbm   "set file/enter=[]life.xbm [.bitmaps]s-dec.xbm"    [.bitmaps]s-dec.xbm
$ call make life1d.xbm "set file/enter=[]life1d.xbm [.bitmaps]t-x11.xbm"  [.bitmaps]t-x11.xbm
$ call make maze.xbm   "set file/enter=[]maze.xbm [.bitmaps]l-dec.xbm"    [.bitmaps]l-dec.xbm
$ call make puzzle.xbm "set file/enter=[]puzzle.xbm [.bitmaps]l-xlock.xbm"  [.bitmaps]l-xlock.xbm
$ if xpm
$ then
$   call make flag.xpm   "set file/enter=[]flag.xpm [.pixmaps]m-dec.xpm"    [.pixaps]m-dec.xpm
$   call make image.xpm  "set file/enter=[]image.xpm [.pixmaps]m-dec.xpm"   [.pixmaps]m-dec.xpm
$   call make maze.xpm   "set file/enter=[]maze.xpm [.pixmaps]m-dec.xpm"    [.pixmaps]m-dec.xpm
$   call make puzzle.xpm "set file/enter=[]puzzle.xpm [.pixmaps]l-xlock.xpm"  [.pixmaps]l-xlock.xpm
$!   call make puzzle.xpm "set file/enter=[]puzzle.xpm [.pixmaps]m-dec.xpm"  [.pixmaps]m-dec.xpm
$ endif
$!
$ write sys$output "Compiling XLock where cc = ''cc'"
$ call make [.xlock]xlock.obj     "cc /object=[.xlock] [.xlock]xlock.c"     [.xlock]xlock.c [.xlock]xlock.h [.xlock]mode.h [.xlock]vroot.h
$ call make [.xlock]passwd.obj    "cc /object=[.xlock] [.xlock]passwd.c"    [.xlock]passwd.c [.xlock]xlock.h
$ call make [.xlock]resource.obj  "cc /object=[.xlock] [.xlock]resource.c"  [.xlock]resource.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.xlock]utils.obj     "cc /object=[.xlock] [.xlock]utils.c"     [.xlock]utils.c [.xlock]xlock.h
$ call make [.xlock]logout.obj    "cc /object=[.xlock] [.xlock]logout.c"    [.xlock]logout.c [.xlock]xlock.h
$ call make [.xlock]mode.obj      "cc /object=[.xlock] [.xlock]mode.c"      [.xlock]mode.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.xlock]ras.obj       "cc /object=[.xlock] [.xlock]ras.c"       [.xlock]ras.c [.xlock]xlock.h
$ call make [.xlock]xbm.obj       "cc /object=[.xlock] [.xlock]xbm.c"       [.xlock]xbm.c [.xlock]xlock.h
$ call make [.xlock]color.obj     "cc /object=[.xlock] [.xlock]color.c"     [.xlock]color.c [.xlock]xlock.h
$ call make [.xlock]visual.obj    "cc /object=[.xlock] [.xlock]visual.c"    [.xlock]visual.c [.xlock]xlock.h
$ if check
$ then
$   write sys$output "Compiling XLock Memory Check Caution: Experimental!"
$   call make [.xlock]memcheck.obj     "cc /object=[.xlock] [.xlock]memcheck.c"     [.xlock]memcheck.c [.xlock]xlock.h
$ endif
$ if axp .and. sound
$ then
$   call make [.xlock]sound.obj     "cc /object=[.xlock] [.xlock]sound.c"     [.xlock]sound.c [.xlock]xlock.h [.xlock]vms_amd.h
$   call make [.xlock]vms_amd.obj   "cc /object=[.xlock] [.xlock]vms_amd.c"   [.xlock]vms_amd.c [.xlock]vms_amd.h
$ else
$   call make [.xlock]sound.obj     "cc /object=[.xlock] [.xlock]sound.c"     [.xlock]sound.c [.xlock]xlock.h
$ endif
$ call make [.modes]ant.obj       "cc /object=[.modes] [.modes]ant.c"       [.modes]ant.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ball.obj      "cc /object=[.modes] [.modes]ball.c"      [.modes]ball.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bat.obj       "cc /object=[.modes] [.modes]bat.c"       [.modes]bat.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]blot.obj      "cc /object=[.modes] [.modes]blot.c"      [.modes]blot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bouboule.obj  "cc /object=[.modes] [.modes]bouboule.c"  [.modes]bouboule.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bounce.obj    "cc /object=[.modes] [.modes]bounce.c"    [.modes]bounce.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]braid.obj     "cc /object=[.modes] [.modes]braid.c"     [.modes]braid.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bubble.obj    "cc /object=[.modes] [.modes]bubble.c"    [.modes]bubble.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bug.obj       "cc /object=[.modes] [.modes]bug.c"       [.modes]bug.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]clock.obj     "cc /object=[.modes] [.modes]clock.c"     [.modes]clock.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]coral.obj     "cc /object=[.modes] [.modes]coral.c"     [.modes]coral.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]crystal.obj   "cc /object=[.modes] [.modes]crystal.c"   [.modes]crystal.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]daisy.obj     "cc /object=[.modes] [.modes]daisy.c"     [.modes]daisy.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]dclock.obj    "cc /object=[.modes] [.modes]dclock.c"    [.modes]dclock.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]deco.obj      "cc /object=[.modes] [.modes]deco.c"      [.modes]deco.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]demon.obj     "cc /object=[.modes] [.modes]demon.c"     [.modes]demon.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]dilemma.obj     "cc /object=[.modes] [.modes]dilemma.c"     [.modes]dilemma.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]drift.obj     "cc /object=[.modes] [.modes]drift.c"     [.modes]drift.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]eyes.obj      "cc /object=[.modes] [.modes]eyes.c"      [.modes]eyes.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]fadeplot.obj  "cc /object=[.modes] [.modes]fadeplot.c"  [.modes]fadeplot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]flag.obj      "cc /object=[.modes] [.modes]flag.c"      [.modes]flag.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]flame.obj     "cc /object=[.modes] [.modes]flame.c"     [.modes]flame.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]forest.obj    "cc /object=[.modes] [.modes]forest.c"    [.modes]forest.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]galaxy.obj    "cc /object=[.modes] [.modes]galaxy.c"    [.modes]galaxy.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]grav.obj      "cc /object=[.modes] [.modes]grav.c"      [.modes]grav.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]helix.obj     "cc /object=[.modes] [.modes]helix.c"     [.modes]helix.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]hop.obj       "cc /object=[.modes] [.modes]hop.c"       [.modes]hop.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]hyper.obj     "cc /object=[.modes] [.modes]hyper.c"     [.modes]hyper.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ico.obj       "cc /object=[.modes] [.modes]ico.c"       [.modes]ico.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ifs.obj       "cc /object=[.modes] [.modes]ifs.c"       [.modes]ifs.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]image.obj     "cc /object=[.modes] [.modes]image.c"     [.modes]image.c [.xlock]xlock.h [.xlock]mode.h ras.h
$ call make [.modes]julia.obj     "cc /object=[.modes] [.modes]julia.c"     [.modes]julia.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]kaleid.obj    "cc /object=[.modes] [.modes]kaleid.c"    [.modes]kaleid.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]laser.obj     "cc /object=[.modes] [.modes]laser.c"     [.modes]laser.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life.obj      "cc /object=[.modes] [.modes]life.c"      [.modes]life.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life1d.obj    "cc /object=[.modes] [.modes]life1d.c"    [.modes]life1d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life3d.obj    "cc /object=[.modes] [.modes]life3d.c"    [.modes]life3d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lightning.obj "cc /object=[.modes] [.modes]lightning.c" [.modes]lightning.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lisa.obj      "cc /object=[.modes] [.modes]lisa.c"      [.modes]lisa.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lissie.obj    "cc /object=[.modes] [.modes]lissie.c"    [.modes]lissie.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]loop.obj      "cc /object=[.modes] [.modes]loop.c"      [.modes]loop.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]mandelbrot.obj "cc /object=[.modes] [.modes]mandelbrot.c" [.modes]mandelbrot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]marquee.obj   "cc /object=[.modes] [.modes]marquee.c"   [.modes]marquee.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]maze.obj      "cc /object=[.modes] [.modes]maze.c"      [.modes]maze.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]mountain.obj  "cc /object=[.modes] [.modes]mountain.c"  [.modes]mountain.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]munch.obj     "cc /object=[.modes] [.modes]munch.c"     [.modes]munch.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]nose.obj      "cc /object=[.modes] [.modes]nose.c"      [.modes]nose.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]qix.obj       "cc /object=[.modes] [.modes]qix.c"       [.modes]qix.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]pacman.obj    "cc /object=[.modes] [.modes]pacman.c"    [.modes]pacman.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]penrose.obj   "cc /object=[.modes] [.modes]penrose.c"   [.modes]penrose.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]petal.obj     "cc /object=[.modes] [.modes]petal.c"     [.modes]petal.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]puzzle.obj    "cc /object=[.modes] [.modes]puzzle.c"    [.modes]puzzle.c [.xlock]xlock.h [.xlock]mode.h ras.h
$ call make [.modes]pyro.obj      "cc /object=[.modes] [.modes]pyro.c"      [.modes]pyro.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]roll.obj      "cc /object=[.modes] [.modes]roll.c"      [.modes]roll.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]rotor.obj     "cc /object=[.modes] [.modes]rotor.c"     [.modes]rotor.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]shape.obj     "cc /object=[.modes] [.modes]shape.c"     [.modes]shape.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]sierpinski.obj "cc /object=[.modes] [.modes]sierpinski.c" [.modes]sierpinski.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]slip.obj      "cc /object=[.modes] [.modes]slip.c"      [.modes]slip.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]sphere.obj    "cc /object=[.modes] [.modes]sphere.c"    [.modes]sphere.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]spiral.obj    "cc /object=[.modes] [.modes]spiral.c"    [.modes]spiral.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]spline.obj    "cc /object=[.modes] [.modes]spline.c"    [.modes]spline.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]star.obj      "cc /object=[.modes] [.modes]star.c"      [.modes]star.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]strange.obj   "cc /object=[.modes] [.modes]strange.c"   [.modes]strange.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]swarm.obj     "cc /object=[.modes] [.modes]swarm.c"     [.modes]swarm.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]swirl.obj     "cc /object=[.modes] [.modes]swirl.c"     [.modes]swirl.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]triangle.obj  "cc /object=[.modes] [.modes]triangle.c"  [.modes]triangle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]tube.obj      "cc /object=[.modes] [.modes]tube.c"      [.modes]tube.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]turtle.obj    "cc /object=[.modes] [.modes]turtle.c"    [.modes]turtle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]vines.obj     "cc /object=[.modes] [.modes]vines.c"     [.modes]vines.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]voters.obj    "cc /object=[.modes] [.modes]voters.c"    [.modes]voters.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]wator.obj     "cc /object=[.modes] [.modes]wator.c"     [.modes]wator.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]wire.obj      "cc /object=[.modes] [.modes]wire.c"      [.modes]wire.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]world.obj     "cc /object=[.modes] [.modes]world.c"     [.modes]world.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]worm.obj      "cc /object=[.modes] [.modes]worm.c"      [.modes]worm.c [.xlock]xlock.h [.xlock]mode.h
$ if xpm 
$ then
$   call make [.modes]cartoon.obj   "cc /object=[.modes] [.modes]cartoon.c"   [.modes]cartoon.c [.xlock]xlock.h [.xlock]mode.h
$ endif
$ if gl
$ then
$   call make [.modes.glx]cage.obj    "cc /object=[.modes.glx] [.modes.glx]cage.c"    [.modes.glx]cage.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]gears.obj     "cc /object=[.modes.glx] [.modes.glx]gears.c"     [.modes.glx]gears.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]moebius.obj    "cc /object=[.modes.glx] [.modes.glx]moebius.c"    [.modes.glx]moebius.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]morph3d.obj   "cc /object=[.modes.glx] [.modes.glx]morph3d.c"   [.modes.glx]morph3d.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]rubik.obj     "cc /object=[.modes.glx] [.modes.glx]rubik.c"     [.modes.glx]rubik.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]stairs.obj    "cc /object=[.modes.glx] [.modes.glx]stairs.c" [.modes.glx]stairs.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]superquadrics.obj "cc /object=[.modes.glx] [.modes.glx]superquadrics.c" [.modes.glx]superquadrics.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]buildlwo.obj  "cc /object=[.modes.glx] [.modes.glx]buildlwo.c"  [.modes.glx]buildlwo.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]pipes.obj     "cc /object=[.modes.glx] [.modes.glx]pipes.c"     [.modes.glx]pipes.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]pipeobjs.obj  "cc /object=[.modes.glx] [.modes.glx]pipeobjs.c"  [.modes.glx]pipeobjs.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sproingies.obj "cc /object=[.modes.glx] [.modes.glx]sproingies.c" [.modes.glx]sproingies.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sproingiewrap.obj "cc /object=[.modes.glx] [.modes.glx]sproingiewrap.c" [.modes.glx]sproingiewrap.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_b.obj      "cc /object=[.modes.glx] [.modes.glx]s1_b.c"      [.modes.glx]s1_b.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_1.obj      "cc /object=[.modes.glx] [.modes.glx]s1_1.c"      [.modes.glx]s1_1.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_2.obj      "cc /object=[.modes.glx] [.modes.glx]s1_2.c"      [.modes.glx]s1_2.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_3.obj      "cc /object=[.modes.glx] [.modes.glx]s1_3.c"      [.modes.glx]s1_3.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_4.obj      "cc /object=[.modes.glx] [.modes.glx]s1_4.c"      [.modes.glx]s1_4.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_5.obj      "cc /object=[.modes.glx] [.modes.glx]s1_5.c"      [.modes.glx]s1_5.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_6.obj      "cc /object=[.modes.glx] [.modes.glx]s1_6.c"      [.modes.glx]s1_6.c [.xlock]xlock.h [.xlock]mode.h
$ endif
$ if bomb
$ then
$   call make [.modes]bomb.obj      "cc /object=[.modes] [.modes]bomb.c"      [.modes]bomb.c [.xlock]xlock.h [.xlock]mode.h
$ endif
$ call make [.modes]blank.obj     "cc /object=[.modes] [.modes]blank.c"     [.modes][.modes]blank.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]random.obj    "cc /object=[.modes] [.modes]random.c"    [.modes]random.c [.xlock]xlock.h [.xlock]mode.h
$!
$! Get libraries
$ if xpm then write optf "''xpmf'/lib"
$ if gl then write optf "''glf'/lib"
$ if glu then write optf "''gluf'/lib"
$ if sys_maj .lt. 7
$ then
$   if xvmsutils then write optf "''xvmsutilsf'/lib"
$ endif
$! if .not. axp then write optf "sys$library:vaxcrtl/lib"
$ write optf "sys$library:vaxcrtl/lib"
$ if dtsaver then write optf "sys$library:cde$libdtsvc.exe/share"
$ if axp then write optf "sys$library:ucx$ipc_shr/share"
$ if axp then write optf "sys$share:decw$xextlibshr/share"
$ if axp then write optf "sys$share:decw$xtlibshrr5/share"
$ if .not. axp then write optf "sys$library:ucx$ipc/lib"
$ write optf "sys$share:decw$dxmlibshr/share"
$ write optf "sys$share:decw$xlibshr/share"
$ close optf
$!
$! LINK
$ write sys$output "Linking XLock"
$ link/map/exec=[.xlock] xlock/opt
$!
$! Create .opt file
$ open/write optf xmlock.opt
$ write sys$output "Compiling XmLock where cc = ''cc'"
$ call make [.xmlock]option.obj "cc /object=[.xmlock] [.xmlock]option.c" option.c
$ call make [.xmlock]xmlock.obj "cc /object=[.xmlock] [.xmlock]xmlock.c" xmlock.c
$! Get libraries
$! if .not. axp then write optf "sys$library:vaxcrtl/lib"
$ write optf "sys$library:vaxcrtl/lib"
$ if axp then write optf "sys$library:ucx$ipc_shr/share"
$ if axp then write optf "sys$share:decw$xextlibshr/share"
$ if axp then write optf "sys$share:decw$xtlibshrr5/share"
$ if .not. axp then write optf "sys$library:ucx$ipc/lib"
$! write optf "sys$share:decw$dxmlibshr/share"
$ write optf "sys$share:decw$xmlibshr12/share"
$ write optf "sys$share:decw$xlibshr/share"
$ close optf
$!
$! LINK
$ write sys$output "Linking XmLock"
$ link/map/exec=[.xmlock] xmlock/opt
$!
$ set noverify
$ exit
$!
$Clobber:      ! Delete executables, Purge directory and clean up object files
$!                and listings
$ delete/noconfirm [.xlock]xlock.exe;*
$ delete/noconfirm [.xmlock]xmlock.exe;*
$!
$Clean:        ! Purge directory, clean up object files and listings
$ close/nolog optf
$ purge [...]
$ delete/noconfirm [...]*.lis;*
$ delete/noconfirm [...]*.obj;*
$ delete/noconfirm [...]*.opt;*
$ delete/noconfirm [...]*.map;*
$ set file/remove eyes.xbm;*
$ set file/remove flag.xbm;*
$ set file/remove image.xbm;*
$ set file/remove ghost.xbm;*
$ set file/remove life.xbm;*
$ set file/remove life1d.xbm;*
$ set file/remove maze.xbm;*
$ set file/remove puzzle.xbm;*
$ set file/remove flag.xpm;*
$ set file/remove image.xpm;*
$ set file/remove maze.xpm;*
$ set file/remove puzzle.xpm;*
$!
$ exit
$!
! SUBROUTINE TO CHECK DEPENDENCIES
$ make: subroutine
$   v='f$verify(0)
$!   p1       What we are trying to make
$!   p2       Command to make it
$!   p3 - p8  What it depends on
$
$   if (f$extract(0,3,p2) .eqs. "cc ") then write optf "''p1'"
$
$   if f$search(p1) .eqs. "" then goto MakeIt
$   time=f$cvtime(f$file(p1,"RDT"))
$   arg=3
$Loop:
$   argument=p'arg
$   if argument .eqs. "" then goto Exit
$   el=0
$Loop2:
$   file=f$element(el," ",argument)
$   if file .eqs. " " then goto Endl
$   afile=""
$Loop3:
$   ofile=afile
$   afile=f$search(file)
$   if afile .eqs. "" .or. afile .eqs. ofile then goto NextEl
$   if f$cvtime(f$file(afile,"RDT")) .gts. time then goto MakeIt
$   goto Loop3
$NextEL:
$   el=el+1
$   goto Loop2
$EndL:
$   arg=arg+1
$   if arg .le. 8 then goto Loop
$   goto Exit
$
$MakeIt:
$   set verify
$   'p2
$   vv='f$verify(0)
$Exit:
$   if v then set verify
$ endsubroutine
