############################################################
# Make file to makexlock.exe on a VMS system.
#
# Created By J.Jansen         12 February 1996
#
# Usage : type MMS in the directory containing the source
#
# It automatically detects the existance of the XPM-library and the XVMSUTILS
# library to use the new (better) features of the program. It is assumed that
# both libraries if exist are in the directory with logical symbol X11.
#
# This is no longer regularly tested so it may not work.
#
############################################################

# default target

C = .c
#C++
#C = .cc

#VMS
O = .obj
S = ,
E = .exe
A = ;*

all : macro xmlock$(E)
	@ $(ECHO) ""

macro :
	@ xvms = f$search("X11:XVMSUTILS.OLB").nes.""
	@ xpm = f$search("X11:LIBXPM.OLB").nes.""
	@ axp = f$getsyi("HW_MODEL").ge.1024
	@ macro = ""
	@ if axp.or.xvms.or.xpm then macro = "/MACRO=("
	@ if axp then macro = macro + "__ALPHA__=1,"
	@ if xpm then macro = macro + "__XPM__=1,"
	@ if xvms then macro = macro + "__XVMSUTILS__=1,"
	@ if macro.nes."" then macro = f$extract(0,f$length(macro)-1,macro)+ ")"
	$(MMS)$(MMSQUALIFIERS)'macro' xlock$(E)

BITMAPDIR = [.bitmaps]
PIXMAPDIR = [.pixmaps]
CONFIGDIR = [.config]
GLDIR = [.glx]
HACKERDIR = [.hackers]

#CC = gcc -g -Wall -ansi -pedantic
#CC = gcc -g -Wall
#CC = g++ -g -Wall
CC = cc
#CC = cxx

BITMAPTYPE = x11
PIXMAPTYPE = x11

# Here is your chance to override the default icon:
TINYBITMAP = $(BITMAPTYPE)
TINYBITMAP = t-x11
SMALLBITMAP = s-$(BITMAPTYPE)
MEDIUMBITMAP = m-$(BITMAPTYPE)
LARGEBITMAP = l-$(BITMAPTYPE)
MEDIUMPIXMAP = m-$(PIXMAPTYPE)
LARGEPIXMAP = l-$(PIXMAPTYPE)

#EYESBITMAP = $(MEDIUMBITMAP)
EYESBITMAP = m-grelb
IMAGEBITMAP = $(MEDIUMBITMAP)
#IMAGEBITMAP = m-xlock
#LIFEBITMAP = $(SMALLBITMAP)
LIFEBITMAP = s-grelb
LIFE1DBITMAP = $(TINYBITMAP)
MAZEBITMAP = $(LARGEBITMAP)
#MAZEBITMAP = m-xlock
#PACMANBITMAP = $(MEDIUMBITMAP)
PACMANBITMAP = m-ghost
#PUZZLEBITMAP = $(LARGEBITMAP)
PUZZLEBITMAP = l-xlock
IMAGEPIXMAP = $(MEDIUMPIXMAP)
#IMAGEPIXMAP = m-xlock
#PUZZLEPIXMAP = $(LARGEPIXMAP)
PUZZLEPIXMAP = l-xlock

RM = delete/noconfirm
RM_S = set file/remove
ECHO = write sys$output
BLN_S = set file/enter=[]

.IFDEF __ALPHA__
#SYSTEMDEF = /standard=vaxc
SOUNDOBJS = $(S)amd$(O)
SOUNDSRCS = amd$(C)
SOUNDDEF = ,VMS_PLAY
.ENDIF
.IFDEF __XPM__
XPMDEF = ,HAVE_XPM
.ENDIF
.IFDEF __XVMSUTILS__
XVMSUTILSDEF = ,HAVE_XVMSUTILS
.ENDIF

# Add hackers modes as you like.  It may make your xlock unstable.
# Please do not uncomment for precompiled distributions.
#HACKERDEF = ,USE_HACKERS
#XLOCKHACKEROBJS = $(S)fadeplot$(O)


CFLAGS = /define = (VMS,USE_VROOT,USE_BOMB$(SOUNDDEF)$(XPMDEF)$(XVMSUTILSDEF)$(GLDEF)$(HACKERDEF))$(SYSTEMDEF)
LDFLAGS =

VER = xlockmore
DISTVER = xlockmore-4.00


####################################################################
# List of object files


XLOCKCOREOBJS = xlock$(O)$(S)passwd$(O)$(S)resource$(O)$(S)\
utils$(O)$(S)logout$(O)$(S)mode$(O)$(S)ras$(O)$(S)xbm$(O)$(S)\
color$(O)$(S)sound$(O)$(SOUNDOBJS)
XLOCKMODEOBJS = ant$(O)$(S)ball$(O)$(S)bat$(O)$(S)blot$(O)$(S)\
bouboule$(O)$(S)bounce$(O)$(S)braid$(O)$(S)bug$(O)$(S)\
clock$(O)$(S)crystal$(O)$(S)\
daisy$(O)$(S)dclock$(O)$(S)demon$(O)$(S)drift$(O)$(S)eyes$(O)$(S)\
flag$(O)$(S)flame$(O)$(S)forest$(O)$(S)fract$(O)$(S)\
galaxy$(O)$(S)geometry$(O)$(S)grav$(O)$(S)\
helix$(O)$(S)hop$(O)$(S)hyper$(O)$(S)\
ico$(O)$(S)ifs$(O)$(S)image$(O)$(S)julia$(O)$(S)kaleid$(O)$(S)\
laser$(O)$(S)life$(O)$(S)life1d$(O)$(S)life3d$(O)$(S)\
lightning$(O)$(S)lisa$(O)$(S)lissie$(O)$(S)loop$(O)$(S)\
marquee$(O)$(S)maze$(O)$(S)mountain$(O)$(S)nose$(O)$(S)\
pacman$(O)$(S)penrose$(O)$(S)petal$(O)$(S)puzzle$(O)$(S)pyro$(O)$(S)\
qix$(O)$(S)roll$(O)$(S)rotor$(O)$(S)\
shape$(O)$(S)sierpinski$(O)$(S)slip$(O)$(S)\
sphere$(O)$(S)spiral$(O)$(S)spline$(O)$(S)\
star$(O)$(S)strange$(O)$(S)swarm$(O)$(S)swirl$(O)$(S)\
triangle$(O)$(S)tube$(O)$(S)turtle$(O)$(S)voters$(O)$(S)\
wator$(O)$(S)wire$(O)$(S)world$(O)$(S)worm$(O)$(S)\
cartoon$(O)
XLOCKGLOBJS = escher$(O)$(S)gears$(O)$(S)morph3d$(O)$(S)superquadrics$(O)$(S)\
buildlwo$(O)$(S)pipes$(O)$(S)pipeobjs$(O)$(S)\
sproingies$(O)$(S)sproingiewrap$(O)$(S)s1_b$(O)$(S)\
s1_1$(O)$(S)s1_2$(O)$(S)s1_3$(O)$(S)s1_4$(O)$(S)s1_5$(O)$(S)s1_6$(O)
XLOCKCOREMODEOBJS = blank$(O)$(S)bomb$(O)$(S)random$(O)
XLOCKOBJS = $(XLOCKCOREOBJS)$(S)$(XLOCKMODEOBJS)$(S)\
$(XLOCKGLOBJS)$(XLOCKHACKEROBJS)$(S)$(XLOCKCOREMODEOBJS)
XMLOCKOBJS = option$(O)$(S)xmlock$(O)

####################################################################
# List of source files
# Used for lint, and some dependencies.

BITMAPS = eyes.xbm ghost.xbm image.xbm life.xbm life1d.xbm \
maze.xbm puzzle.xbm
PIXMAPS = image.xpm puzzle.xpm
XLOCKHDRS = xlock.h mode.h vroot.h ras.h version.h config.h
XLOCKCORESRCS = xlock$(C) passwd$(C) resource$(C) \
utils$(C) logout$(C) mode$(C) ras$(C) xbm$(C) \
color$(C) sound$(C)$(SOUNDSRCS)
XLOCKMODESRCS = ant$(C) ball$(C) bat$(C) blot$(C) \
bouboule$(C) bounce$(C) braid$(C) bug$(C) \
clock$(C) crystal$(C) \
daisy$(C) dclock$(C) demon$(C) drift$(C) eyes$(C) \
flag$(C) flame$(C) forest$(C) fract$(C) \
galaxy$(C) geometry$(C) grav$(C) \
helix$(C) hop$(C) hyper$(C) \
ifs$(C) image$(C) julia$(C) kaleid$(C) \
laser$(C) life$(C) life1d$(C) life3d$(C) \
lightning$(C) lisa$(C) lissie$(C) loop$(C) \
marquee$(C) maze$(C) mountain$(C) nose$(C) \
pacman$(C) penrose$(C) petal$(C) puzzle$(C) pyro$(C) \
qix$(C) roll$(C) rotor$(C) \
shape$(C) sierpinski$(C) slip$(C) \
sphere$(C) spiral$(C) spline$(C) \
star$(C) strange$(C) swarm$(C) swirl$(C) \
triangle$(C) tube$(C) turtle$(C) voters$(C)\
wator$(C) wire$(C) world$(C) worm$(C) \
cartoon$(C)
XLOCKGLSRCS = escher$(C) gears$(C) morph3d$(C) superquadrics$(C) \
buildlwo$(C) pipes$(C) pipeobjs$(C) \
sproingies$(C) sproingiewrap$(C) buildlwo$(C) s1_b$(C) \
s1_1$(C) s1_2$(C) s1_3$(C) s1_4$(C) s1_5$(C) s1_6$(C)
XLOCKCOREMODESRCS = blank$(C) bomb$(C) random$(C)
XLOCKHACKERSRCS = fadeplot$(C)
XLOCKSRCS = $(XLOCKCORESRCS) $(XLOCKMODESRCS) $(XLOCKCOREMODESRCS) \
$(XLOCKGLSRCS) $(XLOCKHACKERSRCS)
XMLOCKSRCS = option$(C) xmlock$(C)


#########################################################################

xlock$(E) : $(XLOCKOBJS)$(S)xlock.opt
	link/map $(XLOCKOBJS)$(S)xlock.opt/opt
	@ $(ECHO) "$@ BUILD COMPLETE"
	@ $(ECHO) ""

xmlock$(E) : $(XMLOCKOBJS)$(S)xmlock.opt
	link/map $(XMLOCKOBJS)$(S)xmlock.opt/opt
	@ $(ECHO) "$@ BUILD COMPLETE"
	@ $(ECHO) ""

xlock.opt :
	@ close/nolog optf
	@ open/write optf xlock.opt
#	@ if .not. axp then write optf "sys$library:vaxcrtl/lib"
	@ write optf "sys$library:vaxcrtl/lib"
	@ if axp then write optf "sys$library:ucx$ipc_shr/share"
	@ if axp then write optf "sys$share:decw$xextlibshr/share"
	@ if axp then write optf "sys$share:decw$xtlibshrr5/share"
	@ if .not. axp then write optf "sys$library:ucx$ipc/lib"
#	@ write optf "sys$share:decw$dxmlibshr/share"
	@ write optf "sys$share:decw$xmlibshr12/share"
	@ write optf "sys$share:decw$xlibshr/share"
	@ close optf

xmlock.opt :
	@ close/nolog optf
	@ open/write optf xmlock.opt
#	@ if .not. axp then write optf "sys$library:vaxcrtl/lib"
	@ write optf "sys$library:vaxcrtl/lib"
	@ if axp then write optf "sys$library:ucx$ipc_shr/share"
	@ if axp then write optf "sys$share:decw$xextlibshr/share"
	@ if axp then write optf "sys$share:decw$xtlibshrr5/share"
	@ if .not. axp then write optf "sys$library:ucx$ipc/lib"
#	@ write optf "sys$share:decw$dxmlibshr/share"
	@ write optf "sys$share:decw$xmlibshr12/share"
	@ write optf "sys$share:decw$xlibshr/share"
	@ close optf

eyes$(O) : eyes$(C) eyes.xbm
flag$(O) : flag$(C) flag.xbm
life$(O) : life$(C) life.xbm
life1d$(O) : life1d$(C) life1d.xbm
maze$(O) : maze$(C) maze.xbm
pacman$(O) : pacman$(C) ghost.xbm
# Do not need xpm files if not using them but not worth figuring out
image$(O) : image$(C) image.xbm image.xpm
puzzle$(O) : puzzle$(C) puzzle.xbm puzzle.xpm

eyes.xbm : $(BITMAPDIR)$(EYESBITMAP).xbm
	$(BLN_S)eyes.xbm $(BITMAPDIR)$(EYESBITMAP).xbm

flag.xbm : $(BITMAPDIR)$(FLAGBITMAP).xbm
	$(BLN_S)flag.xbm $(FLAGDIR)$(FLAGBITMAP).xbm
 
image.xbm : $(BITMAPDIR)$(IMAGEBITMAP).xbm
	$(BLN_S)image.xbm $(BITMAPDIR)$(IMAGEBITMAP).xbm

ghost.xbm : $(BITMAPDIR)$(PACMANBITMAP).xbm
	$(BLN_S)ghost.xbm $(BITMAPDIR)$(PACMANBITMAP).xbm

life.xbm : $(BITMAPDIR)$(LIFEBITMAP).xbm
	$(BLN_S)life.xbm $(BITMAPDIR)$(LIFEBITMAP).xbm

life1d.xbm : $(BITMAPDIR)$(LIFE1DBITMAP).xbm
	$(BLN_S)life1d.xbm $(BITMAPDIR)$(LIFE1DBITMAP).xbm

maze.xbm : $(BITMAPDIR)$(MAZEBITMAP).xbm
	$(BLN_S)maze.xbm $(BITMAPDIR)$(MAZEBITMAP).xbm

puzzle.xbm : $(BITMAPDIR)$(IMAGEBITMAP).xbm
	$(BLN_S)puzzle.xbm $(BITMAPDIR)$(IMAGEBITMAP).xbm

image.xpm : $(PIXMAPDIR)$(IMAGEPIXMAP).xpm
	$(BLN_S)image.xpm $(PIXMAPDIR)$(IMAGEPIXMAP).xpm

puzzle.xpm : $(PIXMAPDIR)$(PUZZLEPIXMAP).xpm
	$(BLN_S)puzzle.xpm $(PIXMAPDIR)$(PUZZLEPIXMAP).xpm

amd$(C) : $(CONFIGDIR)amd$(C)
	$(BLN_S)amd$(C) $(CONFIGDIR)amd$(C)

amd.h : $(CONFIGDIR)amd.h
	$(BLN_S)amd.h $(CONFIGDIR)amd.h

amd$(O) : amd$(C) amd.h

escher$(C) : $(GLDIR)escher$(C)
	$(BLN_S) escher$(C) $(GLDIR)escher$(C)

gears$(C) : $(GLDIR)gears$(C)
	$(BLN_S) gears$(C) $(GLDIR)gears$(C)

morph3d$(C) : $(GLDIR)morph3d$(C)
	$(BLN_S) morph3d$(C) $(GLDIR)morph3d$(C)

superquadrics$(C) : $(GLDIR)superquadrics$(C)
	$(BLN_S) superquadrics$(C) $(GLDIR)superquadrics$(C)

buildlwo$(C) : $(GLDIR)buildlwo$(C)
	$(BLN_S) buildlwo$(C) $(GLDIR)buildlwo$(C)

pipes$(C) : $(GLDIR)pipes$(C)
	$(BLN_S) pipes$(C) $(GLDIR)pipes$(C)

pipeobjs$(C) : $(GLDIR)pipeobjs$(C)
	$(BLN_S) pipeobjs$(C) $(GLDIR)pipeobjs$(C)

sproingies$(C) : $(GLDIR)sproingies$(C)
	$(BLN_S)sproingies$(C) $(GLDIR)sproingies$(C)

sproingiewrap$(C) : $(GLDIR)sproingiewrap$(C)
	$(BLN_S)sproingiewrap$(C) $(GLDIR)sproingiewrap$(C)

s1_b$(C) : $(GLDIR)s1_b$(C)
	$(BLN_S)s1_b$(C) $(GLDIR)s1_b$(C)

s1_1$(C) : $(GLDIR)s1_1$(C)
	$(BLN_S)s1_1$(C) $(GLDIR)s1_1$(C)

s1_2$(C) : $(GLDIR)s1_2$(C)
	$(BLN_S)s1_2$(C) $(GLDIR)s1_2$(C)

s1_3$(C) : $(GLDIR)s1_3$(C)
	$(BLN_S)s1_3$(C) $(GLDIR)s1_3$(C)

s1_4$(C) : $(GLDIR)s1_4$(C)
	$(BLN_S)s1_4$(C) $(GLDIR)s1_4$(C)

s1_5$(C) : $(GLDIR)s1_5$(C)
	$(BLN_S)s1_5$(C) $(GLDIR)s1_6$(C)

s1_6$(C) : $(GLDIR)s1_6$(C)
	$(BLN_S)s1_6$(C) $(GLDIR)s1_6$(C)

fadeplot$(C) : $(HACKERDIR)fadeplot$(C)
	$(BLN_S)fadeplot$(C) $(HACKERDIR)fadeplot$(C)

################################################################

install :

uninstall :

################################################################
# Miscellaneous targets

clean :
	@ close/nolog optf
	@ purge
	@ $(RM) *.lis$(A)
	@ $(RM) *.obj$(A)
	@ $(RM) *.opt$(A)
	@ $(RM) *.map$(A)
	@ $(RM_S) eyes.xbm$(A)
	@ $(RM_S) flag.xbm$(A)
	@ $(RM_S) image.xbm$(A)
	@ $(RM_S) ghost.xbm$(A)
	@ $(RM_S) life.xbm$(A)
	@ $(RM_S) life1d.xbm$(A)
	@ $(RM_S) maze.xbm$(A)
	@ $(RM_S) puzzle.xbm$(A)
	@ $(RM_S) image.xpm$(A)
	@ $(RM_S) puzzle.xpm$(A)
	@ $(RM_S) amd.h$(A)
	@ $(RM_S) amd.c$(A)

distclean : clean
	@ $(RM_S) fadeplot$(C)$(A)
	@ $(RM) xlock$(E)$(A)
	@ $(RM) xmlock$(E)$(A)

read :
	more README
