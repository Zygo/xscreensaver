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
$ xpmf="SYS$LIBRARY:LIBXPM.OLB"
$ glf="GL:LIBMESAGL.OLB"
$ glf_share="GL:LIBMESAGL.EXE"
$ gluf="GL:LIBMESAGLU.OLB"
$ gluf_share="GL:LIBMESAGLU.EXE"
$ ttff="SYS$LIBRARY:LIBTTF.OLB"
$ glttf="SYS$LIBRARY:LIBGLTT.OLB"
$ freetypef="SYS$LIBRARY:FREETYPE.OLB"
$ ftglf="SYS$LIBRARY:LIBFTGL.OLB"
$ zlibf="SYS$LIBRARY:LIBZ.OLB"
$ mmovf="SYS$LIBRARY:MMOV.OLB"
$ mmovf2="SYS$SHARE:MMOV.EXE"
$ magickf="SYS$LIBRARY:MAGICKSHR.OLB"
$!
$! Default for some commands
$ xl_link=="link"
$ xl_cc=="cc/name=(as_is,short)/float=ieee"
$ xl_cxx=="cxx/name=(as_is,short)/float=ieee/assume=(nostdnew,noglobal_array_new"
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
$! if disable_interactive<>0 then interactive modes are disabled (useful for
$! production environments)
$ disable_interactive=0
$! disable_interactive=1
$!
$! if unstable<>0 some of these mode(s) included could be a little buggy
$! unstable=0
$ unstable=1
$!
$! if sound<>0 sound capability is included (only available on Alpha)
$! from vms_amd.c and vms_amd.h
$ sound=1
$! sound=0
$!
$! Memory Check stuff.  Very experimental!
$ check=0
$! check=1
$
$! Compliant colour map if <>1
$ complmap=0
$! complmap=1
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
$ gl_share=f$search("''glf_share'") .nes. ""
$ glu=f$search("''gluf'") .nes. ""
$ glu_share=f$search("''gluf_share'") .nes. ""
$ gltt=f$search("''glttf'") .nes. ""
$ ttf=f$search("''ttff'") .nes. ""
$ ftgl=f$search("''ftglf'") .nes. ""
$ zlib=f$search("''zlibf'") .nes. ""
$ ft=f$search("''freetypef'") .nes. ""
$ mmov=f$search("''mmovf'") .nes. ""
$ mmov2=f$search("''mmovf2'") .nes. ""
$ use_magick=f$search("''magickf'") .nes. ""
$ iscxx=f$search("SYS$SYSTEM:CXX$COMPILER.EXE") .nes. ""
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
$ if iscxx then defs=="''defs',HAVE_CXX"
$ if ttf then defs=="''defs',HAVE_TTF"
$ if gltt then defs=="''defs',HAVE_GLTT"
$ if ft then defs=="''defs',HAVE_FREETYPE"
$ if ftgl .and. zlib then defs=="''defs',HAVE_FTGL"
$ if use_magick then defs=="''defs',USE_MAGICK"
$ if complmap then defs=="''defs',COMPLIANT_COLORMAP"
$ if xpm then defs=="''defs',HAVE_XPM"
$ if gl .or. gl_share then defs=="''defs',USE_GL,HAVE_GLBINDTEXTURE"
$ if dtsaver then defs=="''defs',USE_DTSAVER"
$ if mmov .and. sound
$ then
$   defs=="''defs',HAS_MMOV"
$   if f$search("MMOV.DIR") .eqs. ""
$   then
$     create/dir [.mmov]
$     set def [.mmov]
$     copy SYS$COMMON:[SYSHLP.EXAMPLES.MMOV.COMMON]*.* []
$! spawn included to avoid unwanted redefinition of logicals
$     spawn @build_common
$     copy SYS$COMMON:[SYSHLP.EXAMPLES.MMOV.VIDEO]readavi.c []
$     spaw @[-]mmov
$     set def [-]
$     endif
$   endif
$ if axp .and. sound then defs=="''defs',USE_VMSPLAY"
$ if sys_maj .lt. 7
$ then
$   if xvmsutils then defs=="''defs',USE_XVMSUTILS"
$ endif
$ if vroot then defs=="''defs',USE_VROOT"
$ if bomb then defs=="''defs',USE_BOMB"
$ if disable_interactive then defs=="''defs',DISABLE_INTERACTIVE"
$ if unstable then defs=="''defs',USE_UNSTABLE"
$ if check then defs=="''defs',DEBUG"
$! The next must be the last one.
$ if sys_maj .ge. 7
$ then
$   defs=="''defs',HAVE_USLEEP"
$   defs=="''defs',SRAND=""""srand48"""",LRAND=""""lrand48"""",MAXRAND=2147483648.0"
$ endif
$!
$! Establish the Compiling Environment
$!
$! Set compiler command
$! Put in /include=[] for local include file like a pwd.h ...
$!   not normally required.
$     xl_cxx=="cxx/name=(as_is,short)/float=ieee/assume=(nostdnew,noglobal_array_new)/include=([],[.xlock])/define=(''defs')"
$ if deccxx
$ then
$   xl_cc=="cxx/name=(as_is,short)/float=ieee/assume=(nostdnew,noglobal_array_new)/include=([],[.xlock])/define=(''defs')"
$ else
$   if axp
$   then
$     xl_cc=="cc/name=(as_is,short)/float=ieee/include=([],[.xlock])/define=(''defs')"
$   else
$     if decc
$     then
$!       xl_cc=="cc/decc/standard=vaxc/include=([],[.xlock])/define=(''defs')"
$       xl_cc=="cc/decc/include=([],[.xlock])/define=(''defs')"
$     else ! VAX C
$       xl_cc=="cc/include=([],[.xlock])/define=(''defs')"
$     endif
$   endif
$ endif
$ if p1 .eqs. "DEBUG" .or. p2 .eqs. "DEBUG" .or. p3 .eqs. "DEBUG"
$ then
$   if deccxx
$   then
$     xl_cc=="cxx/name=(as_is,short)/float=ieee/assume=(nostdnew,noglobal_array_new)/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$   else
$     if axp
$     then
$       xl_cc=="cc/name=(as_is,short)/float=ieee/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$     else
$       if decc
$       then
$!         xl_cc=="cc/deb/noopt/decc/standard=vaxc/include=([],[.xlock])/define=(''defs')/list"
$         xl_cc=="cc/deb/noopt/decc/include=([],[.xlock])/define=(''defs')/list"
$       else ! VAX C
$         xl_cc=="cc/deb/noopt/include=([],[.xlock])/define=(''defs')/list"
$       endif
$     endif
$   endif
$   if iscxx
$   then
$     xl_link=="cxxlink/deb"
$   else
$     xl_link=="link/deb"
$   endif
$ else
$   if iscxx then xl_link=="cxxlink/trace"
$ endif
$!
$ if axp .or. .not. decc
$ then
$   define/nolog sys sys$library
$ endif
$!
$ write sys$output "Linking Include Files"
$ call make bat.xbm    "set file/enter=[]bat.xbm [.bitmaps]l-xlock.xbm"   [.bitmaps]l-xlock.xbm
$ call make bounce.xbm  "set file/enter=[]bounce.xbm [.bitmaps]l-xlock.xbm" [.bitmaps]l-xlock.xbm
$ call make decay.xbm  "set file/enter=[]decay.xbm [.bitmaps]l-xlock.xbm" [.bitmaps]l-xlock.xbm
$! call make eyes.xbm  "set file/enter=[]eyes.xbm [.bitmaps]m-dec.xbm"    [.bitmaps]m-dec.xbm
$ call make eyes.xbm   "set file/enter=[]eyes.xbm [.bitmaps]m-grelb.xbm"  [.bitmaps]m-grelb.xbm
$ call make eyes2.xbm  "set file/enter=[]eyes2.xbm [.bitmaps]m-grelb-2.xbm" [.bitmaps]m-grelb-2.xbm
$ call make flag.xbm   "set file/enter=[]flag.xbm [.bitmaps]m-dec.xbm"    [.bitmaps]m-dec.xbm
$ call make image.xbm  "set file/enter=[]image.xbm [.bitmaps]m-dec.xbm"   [.bitmaps]m-dec.xbm
$ call make life.xbm   "set file/enter=[]life.xbm [.bitmaps]s-grelb.xbm"  [.bitmaps]s-grelb.xbm
$! call make life.xbm  "set file/enter=[]life.xbm [.bitmaps]s-dec.xbm"    [.bitmaps]s-dec.xbm
$ call make life2.xbm  "set file/enter=[]life2.xbm [.bitmaps]s-grelb-2.xbm" [.bitmaps]s-grelb-2.xbm
$ call make life1d.xbm "set file/enter=[]life1d.xbm [.bitmaps]t-x11.xbm"  [.bitmaps]t-x11.xbm
$ call make maze.xbm   "set file/enter=[]maze.xbm [.bitmaps]l-dec.xbm"    [.bitmaps]l-dec.xbm
$ call make puzzle.xbm "set file/enter=[]puzzle.xbm [.bitmaps]l-xlock.xbm" [.bitmaps]l-xlock.xbm
$ if xpm
$ then
$   call make bat.xpm    "set file/enter=[]bat.xpm [.pixmaps]l-xlock.xpm"   [.pixmaps]l-xlock.xpm
$   call make bounce.xpm  "set file/enter=[]bounce.xpm [.pixmaps]l-xlock.xpm" [.pixmaps]l-xlock.xpm
$   call make decay.xpm  "set file/enter=[]decay.xpm [.pixmaps]l-xlock.xpm" [.pixmaps]m-dec.xpm
$!   call make decay.xpm  "set file/enter=[]decay.xpm [.pixmaps]m-dec.xpm"   [.pixmaps]m-dec.xpm
$   call make flag.xpm   "set file/enter=[]flag.xpm [.pixmaps]m-dec.xpm"    [.pixaps]m-dec.xpm
$   call make image.xpm  "set file/enter=[]image.xpm [.pixmaps]m-dec.xpm"   [.pixmaps]m-dec.xpm
$   call make maze.xpm   "set file/enter=[]maze.xpm [.pixmaps]m-dec.xpm"    [.pixmaps]m-dec.xpm
$   call make puzzle.xpm  "set file/enter=[]puzzle.xpm [.pixmaps]l-xlock.xpm" [.pixmaps]l-xlock.xpm
$!   call make puzzle.xpm "set file/enter=[]puzzle.xpm [.pixmaps]m-dec.xpm" [.pixmaps]m-dec.xpm
$ endif
$!
$ write sys$output "Compiling XLock whith the folowing defines ="
$ write sys$output "''defs'"
$ call make [.xlock]xlock.obj     "xl_cc /object=[.xlock] [.xlock]xlock.c"     [.xlock]xlock.c [.xlock]xlock.h [.xlock]mode.h [.xlock]vroot.h
$ call make [.xlock]passwd.obj    "xl_cc /object=[.xlock] [.xlock]passwd.c"    [.xlock]passwd.c [.xlock]xlock.h
$ call make [.xlock]resource.obj  "xl_cc /object=[.xlock] [.xlock]resource.c"  [.xlock]resource.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.xlock]parsecmd.obj  "xl_cc /object=[.xlock] [.xlock]parsecmd.c"  [.xlock]parsecmd.c
$ call make [.xlock]util.obj      "xl_cc /object=[.xlock] [.xlock]util.c"      [.xlock]util.c [.xlock]xlock.h [.xlock]util.h
$ call make [.xlock]logout.obj    "xl_cc /object=[.xlock] [.xlock]logout.c"    [.xlock]logout.c [.xlock]xlock.h
$ call make [.xlock]mode.obj      "xl_cc /object=[.xlock] [.xlock]mode.c"      [.xlock]mode.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.xlock]xlockimage.obj "xl_cc /object=[.xlock] [.xlock]xlockimage.c"       [.xlock]xlockimage.c [.xlock]xlock.h [.xlock]xlockimage.c
$ if use_magick
$ then
$   call make [.xlock]magick.obj     "xl_cc /object=[.xlock] [.xlock]magick.c"       [.xlock]magick.c [.xlock]xlock.h [.xlock]magick.h [.xlock]xlockimage.c magick:api.h
$   call make [.xlock]iostuff.obj   "xl_cc /object=[.xlock] [.xlock]iostuff.c"   [.xlock]iostuff.c [.xlock]xlock.h [.xlock]iostuff.h [.xlock]magick.h magick:api.h
$ else
$   call make [.xlock]ras.obj       "xl_cc /object=[.xlock] [.xlock]ras.c"       [.xlock]ras.c [.xlock]xlock.h [.xlock]ras.h [.xlock]xlockimage.c
$   call make [.xlock]xbm.obj       "xl_cc /object=[.xlock] [.xlock]xbm.c"       [.xlock]xbm.c [.xlock]xlock.h
$   call make [.xlock]iostuff.obj   "xl_cc /object=[.xlock] [.xlock]iostuff.c"   [.xlock]iostuff.c [.xlock]xlock.h [.xlock]iostuff.h
$ endif
$ call make [.xlock]vis.obj       "xl_cc /object=[.xlock] [.xlock]vis.c"       [.xlock]vis.c [.xlock]xlock.h [.xlock]vis.h
$ call make [.xlock]visgl.obj     "xl_cc /object=[.xlock] [.xlock]visgl.c"     [.xlock]visgl.c [.xlock]xlock.h [.xlock]visgl.h
$ call make [.xlock]color.obj     "xl_cc /object=[.xlock] [.xlock]color.c"     [.xlock]color.c [.xlock]xlock.h [.xlock]color.h
$ call make [.xlock]random.obj    "xl_cc /object=[.xlock] [.xlock]random.c"    [.xlock]random.c [.xlock]xlock.h [.xlock]random.h
$ call make [.xlock]automata.obj  "xl_cc /object=[.xlock] [.xlock]automata.c"  [.xlock]automata.c [.xlock]xlock.h [.xlock]automata.h
$ call make [.xlock]spline.obj    "xl_cc /object=[.xlock] [.xlock]spline.c"    [.xlock]spline.c [.xlock]xlock.h [.xlock]spline.h
$ call make [.xlock]erase.obj     "xl_cc /object=[.xlock] [.xlock]erase.c"     [.xlock]erase.c [.xlock]xlock.h [.xlock]erase.h [.xlock]erase_debug.h [.xlock]erase_init.h
$ if check
$ then
$   write sys$output "Compiling XLock Memory Check Caution: Experimental!"
$   call make [.xlock]memcheck.obj     "xl_cc /object=[.xlock] [.xlock]memcheck.c"     [.xlock]memcheck.c [.xlock]xlock.h
$ endif
$ if axp .and. sound
$ then
$   call make [.xlock]sound.obj     "xl_cc /object=[.xlock] [.xlock]sound.c"     [.xlock]sound.c [.xlock]xlock.h [.xlock]vms_amd.h
$   call make [.xlock]vms_amd.obj   "xl_cc /object=[.xlock] [.xlock]vms_amd.c"   [.xlock]vms_amd.c [.xlock]vms_amd.h
$ else
$   call make [.xlock]sound.obj     "xl_cc /object=[.xlock] [.xlock]sound.c"     [.xlock]sound.c [.xlock]xlock.h
$ endif
$ call make [.modes]ant.obj       "xl_cc /object=[.modes] [.modes]ant.c"       [.modes]ant.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ant3d.obj       "xl_cc /object=[.modes] [.modes]ant3d.c"       [.modes]ant3d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]apollonian.obj "xl_cc /object=[.modes] [.modes]apollonian.c" [.modes]apollonian.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ball.obj      "xl_cc /object=[.modes] [.modes]ball.c"      [.modes]ball.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bat.obj       "xl_cc /object=[.modes] [.modes]bat.c"       [.modes]bat.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]blot.obj      "xl_cc /object=[.modes] [.modes]blot.c"      [.modes]blot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bouboule.obj  "xl_cc /object=[.modes] [.modes]bouboule.c"  [.modes]bouboule.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bounce.obj    "xl_cc /object=[.modes] [.modes]bounce.c"    [.modes]bounce.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]braid.obj     "xl_cc /object=[.modes] [.modes]braid.c"     [.modes]braid.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bubble.obj    "xl_cc /object=[.modes] [.modes]bubble.c"    [.modes]bubble.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]bug.obj       "xl_cc /object=[.modes] [.modes]bug.c"       [.modes]bug.c [.xlock]xlock.h [.xlock]mode.h
$! call make [.modes]cartoon.obj     "xl_cc /object=[.modes] [.modes]cartoon.c"    [.modes]cartoon.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]clock.obj     "xl_cc /object=[.modes] [.modes]clock.c"     [.modes]clock.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]coral.obj     "xl_cc /object=[.modes] [.modes]coral.c"     [.modes]coral.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]crystal.obj   "xl_cc /object=[.modes] [.modes]crystal.c"   [.modes]crystal.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]daisy.obj     "xl_cc /object=[.modes] [.modes]daisy.c"     [.modes]daisy.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]dclock.obj    "xl_cc /object=[.modes] [.modes]dclock.c"    [.modes]dclock.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]decay.obj     "xl_cc /object=[.modes] [.modes]decay.c"     [.modes]decay.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]deco.obj      "xl_cc /object=[.modes] [.modes]deco.c"      [.modes]deco.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]demon.obj     "xl_cc /object=[.modes] [.modes]demon.c"     [.modes]demon.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]dilemma.obj   "xl_cc /object=[.modes] [.modes]dilemma.c"   [.modes]dilemma.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]discrete.obj  "xl_cc /object=[.modes] [.modes]discrete.c"  [.modes]discrete.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]dragon.obj    "xl_cc /object=[.modes] [.modes]dragon.c"    [.modes]dragon.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]drift.obj     "xl_cc /object=[.modes] [.modes]drift.c"     [.modes]drift.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]euler2d.obj   "xl_cc /object=[.modes] [.modes]euler2d.c"   [.modes]euler2d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]eyes.obj      "xl_cc /object=[.modes] [.modes]eyes.c"      [.modes]eyes.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]fadeplot.obj  "xl_cc /object=[.modes] [.modes]fadeplot.c"  [.modes]fadeplot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]fiberlamp.obj "xl_cc /object=[.modes] [.modes]fiberlamp.c" [.modes]fiberlamp.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]flag.obj      "xl_cc /object=[.modes] [.modes]flag.c"      [.modes]flag.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]flame.obj     "xl_cc /object=[.modes] [.modes]flame.c"     [.modes]flame.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]flow.obj      "xl_cc /object=[.modes] [.modes]flow.c"      [.modes]flow.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]forest.obj    "xl_cc /object=[.modes] [.modes]forest.c"    [.modes]forest.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]fzort.obj    "xl_cc /object=[.modes] [.modes]fzort.c"    [.modes]fzort.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]galaxy.obj    "xl_cc /object=[.modes] [.modes]galaxy.c"    [.modes]galaxy.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]goop.obj      "xl_cc /object=[.modes] [.modes]goop.c"      [.modes]goop.c [.xlock]xlock.h [.xlock]mode.h [.xlock]spline.h
$ call make [.modes]grav.obj      "xl_cc /object=[.modes] [.modes]grav.c"      [.modes]grav.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]helix.obj     "xl_cc /object=[.modes] [.modes]helix.c"     [.modes]helix.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]hop.obj       "xl_cc /object=[.modes] [.modes]hop.c"       [.modes]hop.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]hyper.obj     "xl_cc /object=[.modes] [.modes]hyper.c"     [.modes]hyper.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ico.obj       "xl_cc /object=[.modes] [.modes]ico.c"       [.modes]ico.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]ifs.obj       "xl_cc /object=[.modes] [.modes]ifs.c"       [.modes]ifs.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]image.obj     "xl_cc /object=[.modes] [.modes]image.c"     [.modes]image.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]juggle.obj    "xl_cc /object=[.modes] [.modes]juggle.c"    [.modes]juggle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]julia.obj     "xl_cc /object=[.modes] [.modes]julia.c"     [.modes]julia.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]kaleid.obj    "xl_cc /object=[.modes] [.modes]kaleid.c"    [.modes]kaleid.c [.xlock]xlock.h [.xlock]mode.h
$! call make [.modes]kaleid2.obj     "xl_cc /object=[.modes] [.modes]kaleid2.c"     [.modes]kaleid2.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]kumppa.obj    "xl_cc /object=[.modes] [.modes]kumppa.c"    [.modes]kumppa.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]laser.obj     "xl_cc /object=[.modes] [.modes]laser.c"     [.modes]laser.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life.obj      "xl_cc /object=[.modes] [.modes]life.c"      [.modes]life.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life1d.obj    "xl_cc /object=[.modes] [.modes]life1d.c"    [.modes]life1d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]life3d.obj    "xl_cc /object=[.modes] [.modes]life3d.c"    [.modes]life3d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lightning.obj "xl_cc /object=[.modes] [.modes]lightning.c" [.modes]lightning.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lisa.obj      "xl_cc /object=[.modes] [.modes]lisa.c"      [.modes]lisa.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lissie.obj    "xl_cc /object=[.modes] [.modes]lissie.c"    [.modes]lissie.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]loop.obj      "xl_cc /object=[.modes] [.modes]loop.c"      [.modes]loop.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]lyapunov.obj  "xl_cc /object=[.modes] [.modes]lyapunov.c"  [.modes]lyapunov.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]mandelbrot.obj "xl_cc /object=[.modes] [.modes]mandelbrot.c" [.modes]mandelbrot.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]marquee.obj   "xl_cc /object=[.modes] [.modes]marquee.c"   [.modes]marquee.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]matrix.obj    "xl_cc /object=[.modes] [.modes]matrix.c"    [.modes]matrix.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]maze.obj      "xl_cc /object=[.modes] [.modes]maze.c"      [.modes]maze.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]mountain.obj  "xl_cc /object=[.modes] [.modes]mountain.c"  [.modes]mountain.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]munch.obj     "xl_cc /object=[.modes] [.modes]munch.c"     [.modes]munch.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]nose.obj      "xl_cc /object=[.modes] [.modes]nose.c"      [.modes]nose.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]pacman.obj    "xl_cc /object=[.modes] [.modes]pacman.c"    [.modes]pacman.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]penrose.obj   "xl_cc /object=[.modes] [.modes]penrose.c"   [.modes]penrose.c [.xlock]xlock.h [.xlock]mode.h [.modes]pacman.h [.modes]pacman_level.h [.modes]pacman_ai.h
$ call make [.modes]petal.obj     "xl_cc /object=[.modes] [.modes]petal.c"     [.modes]petal.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]petri.obj     "xl_cc /object=[.modes] [.modes]petri.c"     [.modes]petri.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]polyominoes.obj   "xl_cc /object=[.modes] [.modes]polyominoes.c"   [.modes]polyominoes.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]puzzle.obj    "xl_cc /object=[.modes] [.modes]puzzle.c"    [.modes]puzzle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]pyro.obj      "xl_cc /object=[.modes] [.modes]pyro.c"      [.modes]pyro.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]qix.obj       "xl_cc /object=[.modes] [.modes]qix.c"       [.modes]qix.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]roll.obj      "xl_cc /object=[.modes] [.modes]roll.c"      [.modes]roll.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]rotor.obj     "xl_cc /object=[.modes] [.modes]rotor.c"     [.modes]rotor.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]scooter.obj   "xl_cc /object=[.modes] [.modes]scooter.c"   [.modes]scooter.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]shape.obj     "xl_cc /object=[.modes] [.modes]shape.c"     [.modes]shape.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]sierpinski.obj "xl_cc /object=[.modes] [.modes]sierpinski.c" [.modes]sierpinski.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]slip.obj      "xl_cc /object=[.modes] [.modes]slip.c"      [.modes]slip.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]space.obj     "xl_cc /object=[.modes] [.modes]space.c"     [.modes]space.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]sphere.obj    "xl_cc /object=[.modes] [.modes]sphere.c"    [.modes]sphere.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]spiral.obj    "xl_cc /object=[.modes] [.modes]spiral.c"    [.modes]spiral.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]spline.obj    "xl_cc /object=[.modes] [.modes]spline.c"    [.modes]spline.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]star.obj      "xl_cc /object=[.modes] [.modes]star.c"      [.modes]star.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]starfish.obj  "xl_cc /object=[.modes] [.modes]starfish.c"  [.modes]starfish.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]strange.obj   "xl_cc /object=[.modes] [.modes]strange.c"   [.modes]strange.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]swarm.obj     "xl_cc /object=[.modes] [.modes]swarm.c"     [.modes]swarm.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]swirl.obj     "xl_cc /object=[.modes] [.modes]swirl.c"     [.modes]swirl.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]t3d.obj       "xl_cc /object=[.modes] [.modes]t3d.c"       [.modes]t3d.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]tetris.obj    "xl_cc /object=[.modes] [.modes]tetris.c"    [.modes]tetris.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]thornbird.obj "xl_cc /object=[.modes] [.modes]thornbird.c" [.modes]thornbird.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]tik_tak.obj   "xl_cc /object=[.modes] [.modes]tik_tak.c"   [.modes]tik_tak.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]toneclock.obj "xl_cc /object=[.modes] [.modes]toneclock.c" [.modes]toneclock.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]triangle.obj  "xl_cc /object=[.modes] [.modes]triangle.c"  [.modes]triangle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]tube.obj      "xl_cc /object=[.modes] [.modes]tube.c"      [.modes]tube.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]turtle.obj    "xl_cc /object=[.modes] [.modes]turtle.c"    [.modes]turtle.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]vines.obj     "xl_cc /object=[.modes] [.modes]vines.c"     [.modes]vines.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]voters.obj    "xl_cc /object=[.modes] [.modes]voters.c"    [.modes]voters.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]wator.obj     "xl_cc /object=[.modes] [.modes]wator.c"     [.modes]wator.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]wire.obj      "xl_cc /object=[.modes] [.modes]wire.c"      [.modes]wire.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]world.obj     "xl_cc /object=[.modes] [.modes]world.c"     [.modes]world.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]worm.obj      "xl_cc /object=[.modes] [.modes]worm.c"      [.modes]worm.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]xcl.obj       "xl_cc /object=[.modes] [.modes]xcl.c"       [.modes]xcl.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]xjack.obj     "xl_cc /object=[.modes] [.modes]xjack.c"     [.modes]xjack.c [.xlock]xlock.h [.xlock]mode.h
$ if unstable
$ then
$   call make [.modes]run.obj       "xl_cc /object=[.modes] [.modes]run.c"       [.modes]run.c [.xlock]xlock.h [.xlock]mode.h
$ endif
$ if iscxx
$ then
$   call make [.modes]solitare.obj     "xl_cxx /object=[.modes] [.modes]solitare.cc"     [.modes]solitare.cc [.xlock]xlock.h [.xlock]mode.h
$ endif
$ if gl .or. gl_share
$ then
$   call make [.modes.glx]biof.obj      "xl_cc /object=[.modes.glx] [.modes.glx]biof.c"     [.modes.glx]biof.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]cage.obj      "xl_cc /object=[.modes.glx] [.modes.glx]cage.c"     [.modes.glx]cage.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]fire.obj      "xl_cc /object=[.modes.glx] [.modes.glx]fire.c"     [.modes.glx]fire.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]gears.obj     "xl_cc /object=[.modes.glx] [.modes.glx]gears.c"    [.modes.glx]gears.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]tube.obj      "xl_cc /object=[.modes.glx] [.modes.glx]tube.c" [.modes.glx]tube.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sphere.obj    "xl_cc /object=[.modes.glx] [.modes.glx]sphere.c" [.modes.glx]sphere.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]glplanet.obj  "xl_cc /object=[.modes.glx] [.modes.glx]glplanet.c" [.modes.glx]glplanet.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]lament.obj    "xl_cc /object=[.modes.glx] [.modes.glx]lament.c"   [.modes.glx]lament.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]moebius.obj   "xl_cc /object=[.modes.glx] [.modes.glx]moebius.c"  [.modes.glx]moebius.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]molecule.obj  "xl_cc /object=[.modes.glx] [.modes.glx]molecule.c"    [.modes.glx]molecule.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]morph3d.obj   "xl_cc /object=[.modes.glx] [.modes.glx]morph3d.c"   [.modes.glx]morph3d.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]noof.obj      "xl_cc /object=[.modes.glx] [.modes.glx]noof.c"      [.modes.glx]noof.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]rubik.obj     "xl_cc /object=[.modes.glx] [.modes.glx]rubik.c"     [.modes.glx]rubik.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sballs.obj    "xl_cc /object=[.modes.glx] [.modes.glx]sballs.c"     [.modes.glx]sballs.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sierpinski3d.obj     "xl_cc /object=[.modes.glx] [.modes.glx]sierpinski3d.c"     [.modes.glx]sierpinski3d.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]stairs.obj    "xl_cc /object=[.modes.glx] [.modes.glx]stairs.c" [.modes.glx]stairs.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]superquadrics.obj "xl_cc /object=[.modes.glx] [.modes.glx]superquadrics.c" [.modes.glx]superquadrics.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]xpm-ximage.obj  "xl_cc /object=[.modes.glx] [.modes.glx]xpm-ximage.c"  [.modes.glx]xpm-ximage.c [.xlock]xlock.h [.xlock]xpm-ximage.h
$   call make [.modes.glx]buildlwo.obj  "xl_cc /object=[.modes.glx] [.modes.glx]buildlwo.c"  [.modes.glx]buildlwo.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]pipes.obj     "xl_cc /object=[.modes.glx] [.modes.glx]pipes.c"     [.modes.glx]pipes.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]pipeobjs.obj  "xl_cc /object=[.modes.glx] [.modes.glx]pipeobjs.c"  [.modes.glx]pipeobjs.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sproingies.obj "xl_cc /object=[.modes.glx] [.modes.glx]sproingies.c" [.modes.glx]sproingies.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]sproingiewrap.obj "xl_cc /object=[.modes.glx] [.modes.glx]sproingiewrap.c" [.modes.glx]sproingiewrap.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_b.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_b.c"      [.modes.glx]s1_b.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_1.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_1.c"      [.modes.glx]s1_1.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_2.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_2.c"      [.modes.glx]s1_2.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_3.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_3.c"      [.modes.glx]s1_3.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_4.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_4.c"      [.modes.glx]s1_4.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_5.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_5.c"      [.modes.glx]s1_5.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]s1_6.obj      "xl_cc /object=[.modes.glx] [.modes.glx]s1_6.c"      [.modes.glx]s1_6.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]atlantis.obj  "xl_cc /object=[.modes.glx] [.modes.glx]atlantis.c"  [.modes.glx]atlantis.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]dolphin.obj   "xl_cc /object=[.modes.glx] [.modes.glx]dolphin.c"   [.modes.glx]dolphin.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]shark.obj     "xl_cc /object=[.modes.glx] [.modes.glx]shark.c"     [.modes.glx]shark.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]swim.obj      "xl_cc /object=[.modes.glx] [.modes.glx]swim.c"      [.modes.glx]swim.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]whale.obj     "xl_cc /object=[.modes.glx] [.modes.glx]whale.c"     [.modes.glx]whale.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]atunnels.obj  "xl_cc /object=[.modes.glx] [.modes.glx]atunnels.c"  [.modes.glx]atunnels.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]tunnel_draw.obj     "xl_cc /object=[.modes.glx] [.modes.glx]tunnel_draw.c"     [.modes.glx]tunnel_draw.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]bubble3d.obj  "xl_cc /object=[.modes.glx] [.modes.glx]bubble3d.c"     [.modes.glx]bubble3d.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]b_draw.obj    "xl_cc /object=[.modes.glx] [.modes.glx]b_draw.c"     [.modes.glx]b_draw.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]b_lockglue.obj "xl_cc /object=[.modes.glx] [.modes.glx]b_lockglue.c"     [.modes.glx]b_lockglue.c [.xlock]xlock.h [.xlock]mode.h
$   call make [.modes.glx]b_sphere.obj  "xl_cc /object=[.modes.glx] [.modes.glx]b_sphere.c"     [.modes.glx]b_sphere.c [.xlock]xlock.h [.xlock]mode.h
$   if unstable
$   then
$     call make [.modes.glx]skewb.obj     "xl_cc /object=[.modes.glx] [.modes.glx]skewb.c"     [.modes.glx]skewb.c [.xlock]xlock.h [.xlock]mode.h
$   endif
$   if iscxx
$   then
$     call make [.modes.glx]invert.obj   "xl_cc /object=[.modes.glx] [.modes.glx]invert.c"  [.modes.glx]invert.c [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_figureeight.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_figureeight.cc"  [.modes.glx]i_figureeight.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_linkage.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_linkage.cc"  [.modes.glx]i_linkage.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_sphere.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_sphere.cc"  [.modes.glx]i_sphere.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_spline.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_spline.cc"  [.modes.glx]i_spline.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_threejet.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_threejet.cc"  [.modes.glx]i_threejet.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_threejetvec.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_threejetvec.cc"  [.modes.glx]i_threejetvec.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_twojet.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_twojet.cc"  [.modes.glx]i_twojet.cc [.xlock]xlock.h [.xlock]mode.h
$     call make [.modes.glx]i_twojetvec.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]i_twojetvec.cc"  [.modes.glx]i_twojetvec.cc [.xlock]xlock.h [.xlock]mode.h
$     if ttf .and. gltt
$     then
$       call make [.modes.glx]text3d.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]text3d.cc"  [.modes.glx]text3d.cc [.xlock]xlock.h [.xlock]mode.h
$     endif
$     if ft .and. ftgl .and. zlib
$     then
$       call make [.modes.glx]text3d2.obj  "xl_cxx /object=[.modes.glx] [.modes.glx]text3d2.cc"  [.modes.glx]text3d2.cc [.xlock]xlock.h [.xlock]mode.h
$       call make [.modes.glx]rotator.obj  "xl_cc /object=[.modes.glx] [.modes.glx]rotator.c"  [.modes.glx]rotator.c [.xlock]xlock.h [.xlock]mode.h
$     endif
$   endif
$ endif
$ if bomb
$ then
$   call make [.modes]bomb.obj      "xl_cc /object=[.modes] [.modes]bomb.c"      [.modes]bomb.c [.xlock]xlock.h [.xlock]mode.h
$ endif
$ call make [.modes]blank.obj     "xl_cc /object=[.modes] [.modes]blank.c"     [.modes]blank.c [.xlock]xlock.h [.xlock]mode.h
$ call make [.modes]random.obj    "xl_cc /object=[.modes] [.modes]random.c"    [.modes]random.c [.xlock]xlock.h [.xlock]mode.h
$!
$! Get libraries
$ if use_magick then write optf "''magickf'/lib"
$ if ftgl then write optf "''ftglf'/lib"
$ if ft then write optf "''freetypef'/lib"
$ if zlib then write optf "''zlibf'/lib"
$ if gltt then write optf "''glttf'/lib"
$ if ttf then write optf "''ttff'/lib"
$ if xpm then write optf "''xpmf'/lib"
$ if gl then write optf "''glf'/lib"
$ if gl_share then write optf "''glf_share'/share"
$ if glu then write optf "''gluf'/lib"
$ if glu_share then write optf "''gluf_share'/share"
$ if sys_maj .lt. 7
$ then
$   if xvmsutils then write optf "''xvmsutilsf'/lib"
$ endif
$! if .not. axp then write optf "sys$library:vaxcrtl/lib"
$ write optf "sys$library:vaxcrtl/lib"
$ if dtsaver then write optf "sys$library:cde$libdtsvc.exe/share"
$ if axp then write optf "sys$library:ucx$ipc_shr/share"
$ if mmov
$ then
$   write optf "[.mmov]vms_mmov.obj"
$   write optf "[.mmov]readavi.obj"
$   write optf "[.mmov]commonlib.olb/lib"
$   if mmov2
$   then
$     write optf "sys$share:mmov.exe/share
$   else
$     when:==on
$     when error then continue
$     open/write optf1 mmov_link.opt
$     write optf1 "sys$library:vaxcrtl/lib"
$     write optf1 "sys$share:cma$open_rtl.exe/share"
$     close optf1
$     lib/extract=* sys$library:mmov.olb/lib
$     link/exec=mmov.exe/share mmov/opt,MMOV_VMS.OPT/opt,mmov_link/opt
$     write optf "mmov.exe/share"
$     copy mmov.exe sys$share:mmov.exe
$     write sys$output "If the copy failed you have to place the file mmov.exe"
$     write sys$output "in the directory SYS$SHARE manually before running xlock"
$     noon
$     endif
$   endif
$ if axp then write optf "sys$share:decw$xextlibshr/share"
$ if axp then write optf "sys$share:decw$xtlibshrr5/share"
$ if .not. axp then write optf "sys$library:ucx$ipc/lib"
$ write optf "sys$share:decw$dxmlibshr/share"
$ write optf "sys$share:decw$xlibshr/share"
$ close optf
$!
$! LINK
$ write sys$output "Linking XLock"
$ xl_link/map/exec=[.xlock] xlock/opt
$!
$! Create .opt file
$ open/write optf xmlock.opt
$ write sys$output "Compiling XMLock whith the folowing defines ="
$ write sys$output "''defs'"
$ call make [.xmlock]option.obj "xl_cc /object=[.xmlock] [.xmlock]option.c" [.xmlock]option.c
$ call make [.xmlock]xmlock.obj "xl_cc /object=[.xmlock] [.xmlock]xmlock.c" [.xmlock]xmlock.c
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
$ xl_link/map/exec=[.xmlock] xmlock/opt
$!
$ if mmov
$ then
$   write sys$output "NOTE:"
$   write sys$output "Sound files are played only when the user has the SYSNAM"
$   write sys$output "as an authorized privilege"
$ endif
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
$ set file/remove bat.xbm;*
$ set file/remove bounce.xbm;*
$ set file/remove decay.xbm;*
$ set file/remove eyes.xbm;*
$ set file/remove eyes2.xbm;*
$ set file/remove flag.xbm;*
$ set file/remove image.xbm;*
$ set file/remove life.xbm;*
$ set file/remove life2.xbm;*
$ set file/remove life1d.xbm;*
$ set file/remove maze.xbm;*
$ set file/remove puzzle.xbm;*
$ set file/remove bat.xpm;*
$ set file/remove bounce.xpm;*
$ set file/remove decay.xpm;*
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
$   if (f$extract(0,6,p2) .eqs. "xl_cc ") then write optf "''p1'"
$   if (f$extract(0,7,p2) .eqs. "xl_cxx ") then write optf "''p1'"
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
