/* Config file for xlockmore
 * Many "ideas" taken from xscreensaver-1.34 by Jamie Zawinski.
 *
 * This file is included by the various Imakefiles.
 * After editing this file, you need to execute the commands
 *
 *         xmkmf
 *         make Makefiles
 *
 * Substitute #undef with #define to activate option
 */

XCOMM Define these now or down further below, see below for explaination.
#define XpmLibrary
XCOMM  #define XmLibrary
XCOMM  #define XawLibrary
#define GLLibrary
XCOMM  #define DtSaverLibrary
XCOMM  #define RplayLibrary
XCOMM  #define NasLibrary
XCOMM  #define Check

N =
O = .o
XCOMM  O = .obj
C=.c
XCOMM  C = .cc
S=$(N) $(N)
XCOMM  S = ,

XCOMM please define
XCOMM C as the C source code extension
XCOMM O as the object extension
XCOMM S as the separator for object code

XCOMM  CC = cc
XCOMM  CC = acc
XCOMM  CC = CC
XCOMM  CC = gcc -Wall
XCOMM  CC = g++ -Wall

LN_S = $(LN)

XCOMM   *** BEGIN XPM CONFIG SECTION ***

XCOMM Only the image.c and bat.c modes use this.
XCOMM If your system has libXpm, remove the 'XCOMM  ' from the next line.
XCOMM  #define XpmLibrary

#ifdef XpmLibrary
XPMDEF = -DUSE_XPM
XCOMM Use the following if your xpm.h file is not in an X11 directory
XCOMM  XPMDEF = -DUSE_XPMINC

XCOMM If you get an error "Cannot find xpm.h" while compiling, set
XCOMM XPMINC to the directory X11/xpm.h is in.  Below is a guess.
XPMINC = -I/usr/local/include
XCOMM SGI's ViewKit (use with -DUSE_XPMINC)
XCOMM  XPMINC = -I/usr/include/Vk

XCOMM If you get an error "Cannot find libXpm" while linking, set XPMLIBPATH
XCOMM to the directory libXpm.* is in.  Below is a guess.
XPMLIB = -L/usr/local/lib -lXpm

#endif

XCOMM      *** END XPM CONFIG SECTION ***

XCOMM      *** BEGIN XM CONFIG SECTION ***
 
XCOMM Only options.c and xmlock.c uses Motif.
XCOMM If your system has libXm, remove the 'XCOMM  ' from the next line.
XCOMM  #define XmLibrary
 
#ifdef XmLibrary
XCOMM If its not with your X11 stuff you should set the following
XCOMM  MOTIFHOME = /usr/local
XCOMM  MOTIFHOME = /usr/dt
 
XCOMM If you get an error "Cannot find Xm/PanedW.h" while compiling, set
XCOMM XMINC to the directory Xm/PanedW.h is in.  Below is a guess.
XCOMM  XMINC = -I$(MOTIFHOME)/include
 
XCOMM If you get an error "Cannot find libXm" while linking, set XMLIBPATH
XCOMM to the directory libXm.* is in.  Below is a guess.
XCOMM  XMLIB = -L$(MOTIFHOME)/lib -lXm
XMLIB = -lXm
 
XCOMM Debugging with editres
XCOMM  EDITRESDEF = -DUSE_XMU
XCOMM  EDITRESLIB = -lXmu
#endif
 
XCOMM   *** END XM CONFIG SECTION ***

XCOMM      *** BEGIN XAW CONFIG SECTION ***
 
XCOMM Only options.c and xalock.c uses Athena.
XCOMM If your system has libsx, libXaw, and libXmu,
XCOMM remove the 'XCOMM  ' from the next line.
XCOMM  #define XawLibrary
 
#ifdef XawLibrary
XCOMM If its not with your X11 stuff you should set the following
XCOMM  ATHENAHOME = /usr/local
 
XCOMM Below is a guess.
XCOMM  XAWINC = -I$(ATHENAHOME)/include
 
XCOMM Below is a guess.
XCOMM  XAWLIB = -L$(ATHENAHOME)/lib -lsx -lXaw -lXmu
XAWLIB = -lsx -lXaw -lXmu
 
#endif
 
XCOMM   *** END XAW CONFIG SECTION ***

XCOMM   *** BEGIN MESAGL AND OPENGL CONFIG SECTION ***

XCOMM escher, gears, morph3d, pipes, superquadrics, sproingies modes use this.
XCOMM If your system has libMesaGL & widgets,
XCOMM remove the 'XCOMM  ' from the next line.
XCOMM  #define GLLibrary

#ifdef GLLibrary
GLDEF = -DUSE_GL

XCOMM If you get an error "Cannot find GL/gl.h" while compiling, set
XCOMM GLINC to the directory GL/gl.h is in.
GLINC = -I/usr/local/include

XCOMM If you get an error "Cannot find libMesaGL" while linking, set GLLIBPATH
XCOMM to the directory libMesaGL.* is in.  Below is a guess.
GLLIB = -L/usr/local/lib -lMesaGL -lMesaGLU

XCOMM On SGI
XCOMM  GLLIB = -lGL -lGLU -lgl
XCOMM On Sun with OGL1.1
XCOMM  GLDEF += -DSUN_OGL_NO_VERTEX_MACROS
#endif

XCOMM   *** END MESAGL AND OPENGL CONFIG SECTION ***

XCOMM   *** BEGIN CDE DT CONFIG SECTION ***
 
XCOMM COMMON DESKTOP ENVIRONMENT
XCOMM if your system has libDtSvc, remove the 'XCOMM  ' from the next line
XCOMM  #define DtSaverLibrary

#ifdef DtSaverLibrary
DTSAVERDEF = -DUSE_DTSAVER
DTSAVERINC = -I/usr/dt/include
DTSAVERLIB = -L/usr/dt/lib -lDtSvc
#endif

XCOMM   *** END CDE DT CONFIG SECTION ***

XCOMM   *** BEGIN SOUND CONFIG SECTION ***
 
XCOMM Only xlock.c and resource.c use this
XCOMM if your system has these sound libraries, remove the 'XCOMM  '
XCOMM  #define RplayLibrary
XCOMM  #define NasLibrary

#ifdef RplayLibrary
SOUNDDEF = -DUSE_RPLAY
SOUNDINC = -I/usr/local/include
XCOMM SOUNDLIB = -L/share/lib -lrplay
SOUNDLIB = -L/usr/local/lib -lrplay
#endif

#ifdef NasLibrary
SOUNDDEF = -DUSE_NAS
SOUNDINC = -I/usr/X11R6/include
SOUNDLIB = -L/usr/X11R6/lib -laudio
#endif

XCOMM system play (optional software)
XCOMM SUNOS 4.1.3
XCOMM  DEF_PLAY = "/usr/demo/SOUND/play /usr/local/share/sounds/xlock/"
XCOMM IRIX 5.3
XCOMM  DEF_PLAY = "/usr/sbin/sfplay /usr/local/share/sounds/xlock/"
XCOMM Digital Unix with Multimedia Services installed:
XCOMM  DEF_PLAY = "/usr/bin/mme/decsound -play /usr/local/share/sounds/xlock/"
XCOMM LINUX and others (see config directory)
XCOMM  DEF_PLAY = "/usr/local/bin/play.sh /usr/local/share/sounds/xlock/"
XCOMM Uncomment line below if you have one of the above
XCOMM  SOUNDDEF = -DDEF_PLAY=\"$(DEF_PLAY)\"

XCOMM      *** END SOUND CONFIG SECTION ***

XCOMM      *** BEGIN RNG CONFIG SECTION ***

XCOMM Uncomment to use your system's Random Number Generator
XCOMM They usually come in 3 types

XCOMM Uncomment to use high-precision (but expensive) RNG
XCOMM  SRANDDEF = -DSRAND=srand48
XCOMM  LRANDDEF = -DLRAND=lrand48

XCOMM  SRANDDEF = -DSRAND=srandom
XCOMM  LRANDDEF = -DLRAND=random

XCOMM Least desirable RNG
XCOMM  SRANDDEF = -DSRAND=srand
XCOMM  LRANDDEF = -DLRAND=rand

XCOMM Normally use the above with (default)
XCOMM  MAXRANDDEF = -DMAXRAND=2147483648.0
XCOMM Use the following if using srand/rand and NOT AIXV3
XCOMM  MAXRANDDEF = -DMAXRAND=32767.0
RANDDEF = $(SRANDDEF) $(LRANDDEF) $(MAXRANDDEF)

XCOMM      *** END RNG CONFIG SECTION ***

XCOMM      *** BEGIN DEBUG CHECK SECTION ***
 
XCOMM #define Check
 
#ifdef Check
XCOMM Very esperimental
CHECKDEF = -DDEBUG
#endif
 
XCOMM      *** END DEBUG CHECK SECTION ***

#ifndef __QNX__
#ifndef MathLibrary
#define MathLibrary -lm
#endif
#else
#define MathLibrary
PASSWDDEF = -DHAVE_SHADOW -Dlinux
PASSWDLIB = -l/src/util/Lib/util
#endif

XCOMM KERBEROS Ver. 4
XCOMM  PASSWDDEF = -DHAVE_KRB4
XCOMM  PASSWDINC = -I/usr/athena/include
XCOMM  PASSWDLIB = -L/usr/athena/lib -lkrb -ldes -lresolv
XCOMM
XCOMM KERBEROS Ver. 5
XCOMM  PASSWDDEF = -DHAVE_KRB5
XCOMM  PASSWDINC = -I/opt/krb5/include
XCOMM  PASSWDLIB = -L/opt/krb5/lib -lkrb5 -lcrypto -lcom_err

XCOMM DCE authentication (tested only on HP's)
XCOMM  PASSWDDEF = -DDCE_PASSWD
XCOMM  PASSWDINC = -I/usr/include/reentrant
XCOMM  PASSWDLIB = -ldce -lc_r

#ifdef UltrixArchitecture
EXTRA_LIBRARIES = -lauth
#endif

#ifdef SCOArchitecture
PASSWDDEF = -DHAVE_SHADOW -DSVR4
#endif

#ifdef SVR4ix86Architecture
PASSWDDEF = -DHAVE_SHADOW
#endif

#ifdef SunArchitecture
#if SystemV4
SYSTEMDEF = -DSOLARIS2
XCOMM imake is usually not set up right here.
XCOMM Assume shadowing... usually more correct.
XCOMM  #if HasShadowPasswd
XCOMM  PASSWDDEF = -DHAVE_SHADOW
XCOMM  #endif
PASSWDDEF = -DHAVE_SHADOW
XCOMM  SLEEPDEF = -DHAVE_NAONOSLEEP

XCOMM Problems finding libXext.so.0 when sticky bit is set
EXTRA_LDOPTIONS = -R/usr/lib:/usr/openwin/lib:/usr/dt/lib:/usr/local/lib

PIXMAPTYPE = sol
#else
SYSTEMDEF = -DSUNOS4 -DUSE_MATHERR
SLEEPDEF = -DHAVE_USLEEP
#if HasShadowPasswd
XCOMM  PASSWDDEF = -DSUNOS_ADJUNCT_PASSWD
PASSWDDEF = -DHAVE_SHADOW
#endif
PIXMAPTYPE = sun
#endif
BITMAPTYPE = sun
#else 
#if HasShadowPasswd
PASSWDDEF = -DHAVE_SHADOW
XCOMM  PASSWDLIB = -lshadow
#endif
#if defined(HPArchitecture) || defined(ApolloArchitecture)
#ifdef HPArchitecture
XCOMM If your site is using Secured Passwords,
XCOMM  PASSWDDEF = -DHPUX_SECURE_PASSWD
XCOMM If your site is using Passwd Etc,
XCOMM  PASSWDDEF = -DHP_PASSWDETC
XCOMM  PASSWDLIB = -lrgy -lnck -lndbm
XCOMM  PASSWDLIB = -lrgy -lnck -lndbm -lsec
CCOPTIONS = -Aa
SYSTEMDEF = -D_HPUX_SOURCE -DSYSV -DUSE_MATHERR
#else
SLEEPDEF = -DHAVE_USLEEP
#endif
EXTRA_LIBRARIES = -lXhp11
BITMAPTYPE = hp
PIXMAPTYPE = hp
#else
#ifdef i386SVR4Architecture
EXTRA_LIBRARIES = -lsocket -lnsl -lgen
PASSWDDEF = -DHAVE_SHADOW
BITMAPTYPE = x11
PIXMAPTYPE = x11
#else
#if defined(FreeBSDArchitecture) || defined(NetBSDArchitecture) || defined(i386BsdArchitecture)
SLEEPDEF = -DHAVE_USLEEP
BITMAPTYPE = bsd
PIXMAPTYPE = bsd
INSTPGMFLAGS = -s -o root -m 4111
#else
#ifdef LinuxArchitecture
SLEEPDEF = -DHAVE_USLEEP
BITMAPTYPE = linux
PIXMAPTYPE = linux
#if HasShadowPasswd && !UseElfFormat
EXTRA_LIBRARIES = -lgdbm
#endif
#else
#ifdef SGIArchitecture
BITMAPTYPE = sgi
PIXMAPTYPE = sgi
#else
#ifdef AIXArchitecture
BITMAPTYPE = x11
PIXMAPTYPE = x11
XCOMM If AIX 3.1 or less, do not have struct passwd and other things
#if OSMajorVersion < 3 || (OSMajorVersion == 3 && OSMinorVersion < 2)
SYSTEMDEF = -DLESS_THAN_AIX3_2
#endif
XCOMM Use this if your site is using AFS:
XCOMM  PASSWDDEF = -DAFS
XCOMM  PASSWDINC = -I/usr/afsws/include
XCOMM  PASSWDLIB = -L/usr/afsws/lib -L/usr/afsws/lib/afs -lkauth -lubik -lprot -lrxkad -lrx -llwp -lauth -lsys -ldes -lcmd -lcom_err /usr/afsws/lib/afs/util.a
XCOMM You may need this one too.
XCOMM  EXTRA_LIBRARIES = -laudit
#else
XCOMM Use this if your site is using OSF Enhanced Security:
XCOMM  PASSWDDEF = -DOSF1_ENH_SEC
XCOMM  PASSWDLIB = -lsecurity
XCOMM  INSTPGMFLAGS = -s -g auth -m 2111
BITMAPTYPE = x11
PIXMAPTYPE = x11
#endif
#endif
#endif
#endif
#endif
#endif
#endif

XLOCKINC = -I$(top_srcdir) -I. -I$(UTILSDIR)

XCOMM -DUSE_VROOT          Allows xlock to run in root window (some
XCOMM                      window managers have problems)
XCOMM -DALWAYS_ALLOW_ROOT  Users can not turn off allowroot
XCOMM -DUSE_SYSLOG         Paranoid administrator option (a check is also
XCOMM                      done to see if you have it)
XCOMM -DUSE_MULTIPLE_ROOT  Multiple root users ... security?
XCOMM -DUSE_MOUSE_MOTION   Password screen displayed with mouse motion
XCOMM -DUSE_OLD_EVENT_LOOP Some machines may still need this (fd_set
XCOMM                      errors may be a sign)
XCOMM -DUSE_VMSUTILS       This patches up old __VMS_VER < 70000000
XCOMM
XCOMM For personal use you may want to consider:
XCOMM -DUSE_XLOCKRC        paranoid admin or unknown shadow passwd alg
XCOMM
XCOMM For labs you may want to consider:
XCOMM -DUSE_AUTO_LOGOUT=240  Enable auto-logout and set deadline (minutes)
XCOMM -DDEF_AUTO_LOGOUT=\"120\" Set default auto-logout deadline (minutes)
XCOMM -DUSE_BUTTON_LOGOUT=10 Enable logout button and set when to appear (min) 
XCOMM -DDEF_BUTTON_LOGOUT=\"5\" Set default logout button (min) 
XCOMM -DUSE_BOMB           Enable automatic logout mode (does not come up
XCOMM                      in random mode)
XCOMM -DCLOSEDOWN_LOGOUT   Use with USE_AUTO_LOGOUT, USE_BUTTON_LOGOUT,
XCOMM                      USE_BOMB if using xdm
XCOMM -DSESSION_LOGOUT     Alternate of above
XCOMM -DSTAFF_FILE=\"/etc/xlock.staff\"  File of staff who are exempt
XCOMM -DSTAFF_NETGROUP=\"/etc/xlock.netgroup\"  Netgroup that is exempt

XCOMM May have to combine in one long line if "+=" does not work
OPTDEF = -DUSE_VROOT -DALWAYS_ALLOW_ROOT -DUSE_BOMB
XCOMM  OPTDEF += -DUSE_SYSLOG -DSYSLOG_FACILITY=LOG_AUTH
XCOMM  OPTDEF += -DSYSLOG_WARNING=LOG_WARNING
XCOMM  OPTDEF += -DSYSLOG_NOTICE=LOG_NOTICE -DSYSLOG_INFO=LOG_INFO
XCOMM  OPTDEF += -DUSE_MOUSE_MOTION
XCOMM  OPTDEF += -DUSE_MULTIPLE_ROOT
XCOMM  OPTDEF += -DUSE_OLD_EVENT_LOOP
XCOMM  OPTDEF += -DUSE_XLOCKRC
XCOMM  OPTDEF += -DUSE_AUTO_LOGOUT=240
XCOMM  OPTDEF += -DDEF_AUTO_LOGOUT=\"120\"
XCOMM  OPTDEF += -DUSE_BUTTON_LOGOUT=10
XCOMM  OPTDEF += -DDEF_BUTTON_LOGOUT=\"5\"
XCOMM  OPTDEF += -DCLOSEDOWN_LOGOUT
XCOMM  OPTDEF += -DSESSION_LOGOUT
XCOMM  OPTDEF += -DSTAFF_FILE=\"/etc/xlock.staff\"
XCOMM  OPTDEF += -DSTAFF_NETGROUP=\"/etc/xlock.netgroup\"

DEFINES = -DDEF_FILESEARCHPATH=\"$(LIBDIR)/%T/%N%S\" \
$(SYSTEMDEF) $(EDITRESDEF) $(SLEEPDEF) $(OPTDEF) $(RANDDEF) \
$(PASSWDDEF) $(XMINC) $(XAWINC) $(XPMDEF) $(GLDEF) $(DTSAVERDEF) $(SOUNDDEF) \
$(PASSWDINC) $(XPMINC) $(GLINC) $(DTSAVERINC) $(SOUNDINC) $(XLOCKINC)

DEPLIBS = $(DEPXLIB)
LOCAL_LIBRARIES = $(XLIB) $(XPMLIB) $(GLLIB) $(DTSAVERLIB) $(SOUNDLIB)
MLIBS = $(XMLIB) $(EDITRESLIB) -lXt $(XLIB) $(SMLIB) $(ICELIB)
ALIBS = $(XAWLIB) -lXt $(XLIB) $(SMLIB) $(ICELIB)
LINTLIBS = $(LINTXLIB)
#if HasLibCrypt
       CRYPTLIB = -lcrypt
#endif
SYS_LIBRARIES = $(CRYPTLIB) $(PASSWDLIB) MathLibrary

VER = xlockmore
DISTVER = xlockmore-4.05
