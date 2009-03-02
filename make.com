$ save_verify='f$verify(0)
$! set ver 
$!
$!      Compile and link for xlockmore
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
$! amd.c and amd.h are linked in from the config directory
$ sound=1
$! sound=0
$!
$! Hackers locks.  Experimental!
$ hacker=0
$! hacker=1
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
$ if hacker then defs=="''defs',USE_HACKERS"
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
$   cc=="cxx/define=(''defs')"
$ else
$   if axp
$   then
$!     cc=="cc/standard=vaxc/define=(''defs')"
$     cc=="cc/define=(''defs')"
$   else
$     if decc
$     then
$!       cc=="cc/decc/standard=vaxc/define=(''defs')"
$       cc=="cc/decc/define=(''defs')"
$     else ! VAX C
$       cc=="cc/define=(''defs')"
$     endif
$   endif
$ endif
$ if p1 .eqs. "DEBUG" .or. p2 .eqs. "DEBUG" .or. p3 .eqs. "DEBUG"
$ then
$   if deccxx
$   then
$     cc=="cxx/deb/noopt/define=(''defs')/list"
$   else
$     if axp
$     then
$!       cc=="cc/deb/noopt/standard=vaxc/define=(''defs')/list"
$       cc=="cc/deb/noopt/define=(''defs')/list"
$     else
$       if decc
$       then
$!         cc=="cc/deb/noopt/decc/standard=vaxc/define=(''defs')/list"
$         cc=="cc/deb/noopt/decc/define=(''defs')/list"
$       else ! VAX C
$         cc=="cc/deb/noopt/define=(''defs')/list"
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
$! call make eyes.xbm   "set file/enter=[]eyes.xbm [.bitmaps]m-x11.xbm"    [.bitmaps]m-x11.xbm
$ call make eyes.xbm   "set file/enter=[]eyes.xbm [.bitmaps]m-grelb.xbm"  [.bitmaps]m-grelb.xbm
$ call make flag.xbm   "set file/enter=[]flag.xbm [.bitmaps]m-x11.xbm"    [.bitmaps]m-x11.xbm
$! call make ghost.xbm  "set file/enter=[]ghost.xbm [.bitmaps]m-x11.xbm"   [.bitmaps]m-x11.xbm
$ call make ghost.xbm  "set file/enter=[]ghost.xbm [.bitmaps]m-ghost.xbm" [.bitmaps]m-ghost.xbm
$ call make image.xbm  "set file/enter=[]image.xbm [.bitmaps]m-x11.xbm"   [.bitmaps]m-x11.xbm
$ call make life.xbm   "set file/enter=[]life.xbm [.bitmaps]s-grelb.xbm"  [.bitmaps]s-grelb.xbm
$! call make life.xbm   "set file/enter=[]life.xbm [.bitmaps]s-x11.xbm"    [.bitmaps]s-x11.xbm
$ call make life1d.xbm "set file/enter=[]life1d.xbm [.bitmaps]t-x11.xbm"  [.bitmaps]t-x11.xbm
$ call make maze.xbm   "set file/enter=[]maze.xbm [.bitmaps]l-x11.xbm"    [.bitmaps]l-x11.xbm
$ call make puzzle.xbm "set file/enter=[]puzzle.xbm [.bitmaps]l-xlock.xbm"  [.bitmaps]l-xlock.xbm
$ if xpm
$ then
$   call make image.xpm  "set file/enter=[]image.xpm [.pixmaps]m-x11.xpm"   [.pixmaps]m-x11.xpm
$   call make puzzle.xpm "set file/enter=[]puzzle.xpm [.pixmaps]l-xlock.xpm"  [.pixmaps]l-xlock.xpm
$ endif
$ if axp .and. sound
$ then
$   call make amd.h      "set file/enter=[]amd.h [.config]amd.h"            [.config]amd.h
$   call make amd.c      "set file/enter=[]amd.c [.config]amd.c"            [.config]amd.c
$ endif
$ call make escher.c   "set file/enter=[]escher.c [.glx]escher.c"         [.glx]escher.c
$ call make gears.c    "set file/enter=[]gears.c [.glx]gears.c"           [.glx]gears.c
$ call make morph3d.c  "set file/enter=[]morph3d.c [.glx]morph3d.c"       [.glx]morph3d.c
$ call make superquadrics.c "set file/enter=[]superquadrics.c [.glx]superquadrics.c" [.glx]superquadrics.c
$ call make buildlwo.c "set file/enter=[]buildlwo.c [.glx]buildlwo.c"     [.glx]buildlwo.c
$ call make pipes.c    "set file/enter=[]pipes.c [.glx]pipes.c"           [.glx]pipes.c
$ call make pipeobjs.c "set file/enter=[]pipeobjs.c [.glx]pipeobjs.c"     [.glx]pipeobjs.c
$ call make sproingies.c "set file/enter=[]sproingies.c [.glx]sproingies.c" [.glx]sproingies.c
$ call make sproingiewrap.c "set file/enter=[]sproingiewrap.c [.glx]sproingiewrap.c" [.glx]sproingiewrap.c
$ call make s1_b.c     "set file/enter=[]s1_b.c [.glx]s1_b.c"             [.glx]s1_b.c
$ call make s1_1.c     "set file/enter=[]s1_1.c [.glx]s1_1.c"             [.glx]s1_1.c
$ call make s1_2.c     "set file/enter=[]s1_2.c [.glx]s1_2.c"             [.glx]s1_2.c
$ call make s1_3.c     "set file/enter=[]s1_3.c [.glx]s1_3.c"             [.glx]s1_3.c
$ call make s1_4.c     "set file/enter=[]s1_4.c [.glx]s1_4.c"             [.glx]s1_4.c
$ call make s1_5.c     "set file/enter=[]s1_5.c [.glx]s1_5.c"             [.glx]s1_5.c
$ call make s1_6.c     "set file/enter=[]s1_6.c [.glx]s1_6.c"             [.glx]s1_6.c
$ if hacker
$ then
$   call make fadeplot.c "set file/enter=[]fadeplot.c [.hackers]fadeplot.c" [.hackers]fadeplot.c
$ endif
$!
$ write sys$output "Compiling XLock where cc = ''cc'"
$ call make xlock.obj     "cc xlock.c"     xlock.c xlock.h mode.h vroot.h
$ call make passwd.obj    "cc passwd.c"    passwd.c xlock.h
$ call make resource.obj  "cc resource.c"  resource.c xlock.h mode.h
$ call make utils.obj     "cc utils.c"     utils.c xlock.h
$ call make logout.obj    "cc logout.c"    logout.c xlock.h
$ call make mode.obj      "cc mode.c"      mode.c xlock.h mode.h
$ call make ras.obj       "cc ras.c"       ras.c xlock.h
$ call make xbm.obj       "cc xbm.c"       xbm.c xlock.h
$ call make color.obj     "cc color.c"     color.c xlock.h
$ if axp .and. sound
$ then
$   call make sound.obj     "cc sound.c"     sound.c xlock.h amd.h
$ else
$   call make sound.obj     "cc sound.c"     sound.c xlock.h
$ endif
$ call make ant.obj       "cc ant.c"       ant.c xlock.h mode.h
$ call make ball.obj      "cc ball.c"      ball.c xlock.h mode.h
$ call make bat.obj       "cc bat.c"       bat.c xlock.h mode.h
$ call make blot.obj      "cc blot.c"      blot.c xlock.h mode.h
$ call make bouboule.obj  "cc bouboule.c"  bouboule.c xlock.h mode.h
$ call make bounce.obj    "cc bounce.c"    bounce.c xlock.h mode.h
$ call make braid.obj     "cc braid.c"     braid.c xlock.h mode.h
$ call make bug.obj       "cc bug.c"       bug.c xlock.h mode.h
$ call make clock.obj     "cc clock.c"     clock.c xlock.h mode.h
$ call make crystal.obj   "cc crystal.c"   crystal.c xlock.h mode.h
$ call make daisy.obj     "cc daisy.c"     daisy.c xlock.h mode.h
$ call make dclock.obj    "cc dclock.c"    dclock.c xlock.h mode.h
$ call make demon.obj     "cc demon.c"     demon.c xlock.h mode.h
$ call make drift.obj     "cc drift.c"     drift.c xlock.h mode.h
$ call make eyes.obj      "cc eyes.c"      eyes.c xlock.h mode.h
$ call make flag.obj      "cc flag.c"      flag.c xlock.h mode.h
$ call make flame.obj     "cc flame.c"     flame.c xlock.h mode.h
$ call make forest.obj    "cc forest.c"    forest.c xlock.h mode.h
$ call make fract.obj     "cc fract.c"     fract.c xlock.h mode.h
$ call make galaxy.obj    "cc galaxy.c"    galaxy.c xlock.h mode.h
$ call make geometry.obj  "cc geometry.c"  geometry.c xlock.h mode.h
$ call make grav.obj      "cc grav.c"      grav.c xlock.h mode.h
$ call make helix.obj     "cc helix.c"     helix.c xlock.h mode.h
$ call make hop.obj       "cc hop.c"       hop.c xlock.h mode.h
$ call make hyper.obj     "cc hyper.c"     hyper.c xlock.h mode.h
$ call make ico.obj       "cc ico.c"       ico.c xlock.h mode.h
$ call make ifs.obj       "cc ifs.c"       ifs.c xlock.h mode.h
$ call make image.obj     "cc image.c"     image.c xlock.h mode.h ras.h
$ call make julia.obj     "cc julia.c"     julia.c xlock.h mode.h
$ call make kaleid.obj    "cc kaleid.c"    kaleid.c xlock.h mode.h
$ call make laser.obj     "cc laser.c"     laser.c xlock.h mode.h
$ call make life.obj      "cc life.c"      life.c xlock.h mode.h
$ call make life1d.obj    "cc life1d.c"    life1d.c xlock.h mode.h
$ call make life3d.obj    "cc life3d.c"    life3d.c xlock.h mode.h
$ call make lightning.obj "cc lightning.c" lightning.c xlock.h mode.h
$ call make lisa.obj      "cc lisa.c"      lisa.c xlock.h mode.h
$ call make lissie.obj    "cc lissie.c"    lissie.c xlock.h mode.h
$ call make loop.obj      "cc loop.c"      loop.c xlock.h mode.h
$ call make marquee.obj   "cc marquee.c"   marquee.c xlock.h mode.h
$ call make maze.obj      "cc maze.c"      maze.c xlock.h mode.h
$ call make mountain.obj  "cc mountain.c"  mountain.c xlock.h mode.h
$ call make nose.obj      "cc nose.c"      nose.c xlock.h mode.h
$ call make qix.obj       "cc qix.c"       qix.c xlock.h mode.h
$ call make pacman.obj    "cc pacman.c"    pacman.c xlock.h mode.h
$ call make penrose.obj   "cc penrose.c"   penrose.c xlock.h mode.h
$ call make petal.obj     "cc petal.c"     petal.c xlock.h mode.h
$ call make puzzle.obj    "cc puzzle.c"    puzzle.c xlock.h mode.h ras.h
$ call make pyro.obj      "cc pyro.c"      pyro.c xlock.h mode.h
$ call make roll.obj      "cc roll.c"      roll.c xlock.h mode.h
$ call make rotor.obj     "cc rotor.c"     rotor.c xlock.h mode.h
$ call make shape.obj     "cc shape.c"     shape.c xlock.h mode.h
$ call make sierpinski.obj "cc sierpinski.c" sierpinski.c xlock.h mode.h
$ call make slip.obj      "cc slip.c"      slip.c xlock.h mode.h
$ call make sphere.obj    "cc sphere.c"    sphere.c xlock.h mode.h
$ call make spiral.obj    "cc spiral.c"    spiral.c xlock.h mode.h
$ call make spline.obj    "cc spline.c"    spline.c xlock.h mode.h
$ call make star.obj      "cc star.c"      star.c xlock.h mode.h
$ call make strange.obj   "cc strange.c"   strange.c xlock.h mode.h
$ call make swarm.obj     "cc swarm.c"     swarm.c xlock.h mode.h
$ call make swirl.obj     "cc swirl.c"     swirl.c xlock.h mode.h
$ call make triangle.obj  "cc triangle.c"  triangle.c xlock.h mode.h
$ call make tube.obj      "cc tube.c"      tube.c xlock.h mode.h
$ call make turtle.obj    "cc turtle.c"    turtle.c xlock.h mode.h
$ call make voters.obj    "cc voters.c"    voters.c xlock.h mode.h
$ call make wator.obj     "cc wator.c"     wator.c xlock.h mode.h
$ call make wire.obj      "cc wire.c"      wire.c xlock.h mode.h
$ call make world.obj     "cc world.c"     world.c xlock.h mode.h
$ call make worm.obj      "cc worm.c"      worm.c xlock.h mode.h
$ call make cartoon.obj   "cc cartoon.c"   cartoon.c xlock.h mode.h
$ call make escher.obj    "cc escher.c"    escher.c xlock.h mode.h
$ call make gears.obj     "cc gears.c"     gears.c xlock.h mode.h
$ call make morph3d.obj   "cc morph3d.c"   morph3d.c xlock.h mode.h
$ call make superquadrics.obj "cc superquadrics.c" superquadrics.c xlock.h mode.h
$ call make buildlwo.obj  "cc buildlwo.c"  buildlwo.c xlock.h mode.h
$ call make pipes.obj     "cc pipes.c"     pipes.c xlock.h mode.h
$ call make pipeobjs.obj  "cc pipeobjs.c"  pipeobjs.c xlock.h mode.h
$ call make sproingies.obj "cc sproingies.c" sproingies.c xlock.h mode.h
$ call make sproingiewrap.obj "cc sproingiewrap.c" sproingiewrap.c xlock.h mode.h
$ call make s1_b.obj      "cc s1_b.c"      s1_b.c xlock.h mode.h
$ call make s1_1.obj      "cc s1_1.c"      s1_1.c xlock.h mode.h
$ call make s1_2.obj      "cc s1_2.c"      s1_2.c xlock.h mode.h
$ call make s1_3.obj      "cc s1_3.c"      s1_3.c xlock.h mode.h
$ call make s1_4.obj      "cc s1_4.c"      s1_4.c xlock.h mode.h
$ call make s1_5.obj      "cc s1_5.c"      s1_5.c xlock.h mode.h
$ call make s1_6.obj      "cc s1_6.c"      s1_6.c xlock.h mode.h
$ call make blank.obj     "cc blank.c"     blank.c xlock.h mode.h
$ call make bomb.obj      "cc bomb.c"      bomb.c xlock.h mode.h
$ call make random.obj    "cc random.c"    random.c xlock.h mode.h
$! AMD and USE_VMSPLAY for SOUND
$ if axp .and. sound
$ then
$   call make amd.obj       "cc amd.c"       amd.c amd.h
$ endif
$ if hacker
$ then
$   write sys$output "Compiling XLock Hackers modes, Caution: Experimental!"
$   call make fadeplot.obj  "cc fadeplot.c"  fadeplot.c xlock.h mode.h
$ endif
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
$ link/map xlock/opt
$!
$! Create .opt file
$ open/write optf xmlock.opt
$ write sys$output "Compiling XmLock where cc = ''cc'"
$ call make option.obj "cc option.c" option.c
$ call make xmlock.obj "cc xmlock.c" xmlock.c
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
$ link/map xmlock/opt
$!
$ set noverify
$ exit
$!
$Clobber:      ! Delete executables, Purge directory and clean up object files
$!                and listings
$ delete/noconfirm xlock.exe;*
$ delete/noconfirm xmlock.exe;*
$ set file/remove fadeplot.c;*
$!
$Clean:        ! Purge directory, clean up object files and listings
$ close/nolog optf
$ purge
$ delete/noconfirm *.lis;*
$ delete/noconfirm *.obj;*
$ delete/noconfirm *.opt;*
$ delete/noconfirm *.map;*
$ set file/remove eyes.xbm;*
$ set file/remove flag.xbm;*
$ set file/remove image.xbm;*
$ set file/remove ghost.xbm;*
$ set file/remove life.xbm;*
$ set file/remove life1d.xbm;*
$ set file/remove maze.xbm;*
$ set file/remove puzzle.xbm;*
$ set file/remove image.xpm;*
$ set file/remove puzzle.xpm;*
$ set file/remove amd.h;*
$ set file/remove amd.c;*
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
$   if (f$extract(0,3,p2) .eqs. "CC ") then write optf "''p1'"
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
