#ifndef __XLOCK_XLOCK_H__
#define __XLOCK_XLOCK_H__

#if !defined( lint ) && !defined( SABER )
/* #ident	"@(#)xlock.h	4.14 99/06/17 xlockmore" */

#endif

/*-
 * @(#)xlock.h	4.00 97/01/01 xlockmore
 *
 * External interfaces for new modes and SYSV OS defines.
 *
 * xscreensaver code, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.  The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 * Revision History:
 *
 * Changes maintained by David Bagley <bagleyd@tux.org>
 * 23-Apr-98: Merged in xlockmore.h from xscreensaver, not much in common yet.
 * 12-May-95: Added defines for SunOS's Adjunct password file
 *            Dale A. Harris <rodmur@ecst.csuchico.edu>
 * 18-Nov-94: Modified for QNX 4.2 w/ Metrolink X server from Brian Campbell
 *            <brianc@qnx.com>.
 * 11-Jul-94: added Bool flag: inwindow, which tells xlock to run in a
 *            window from Greg Bowering <greg@cs.adelaide.edu.au>
 * 11-Jul-94: patch for Solaris SYR4 from Chris P. Ross <cross@eng.umd.edu>
 * 28-Jun-94: Reorganized shadow stuff
 * 24-Jun-94: Reorganized
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 17-Jun-94: patched shadow passwords and bcopy and bzero for SYSV from
 *            <reggers@julian.uwo.ca>
 * 21-Mar-94: patched the patch for AIXV3 and HP from
 *            <R.K.Lloyd@csc.liv.ac.uk>.
 * 01-Dec-93: added patch for AIXV3 from Tom McConnell
 *            <tmcconne@sedona.intel.com> also added a patch for HP-UX 8.0.
 *
 */

#ifdef STANDALONE
/* xscreensaver compatibility layer for xlockmore modules. */

/*-
 * xscreensaver, Copyright (c) 1997, 1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * The definitions in this file make it possible to compile an xlockmore
 * module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@jwz.org> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#if !defined(PROGCLASS) || !defined(HACK_INIT) || !defined(HACK_DRAW)
ERROR ! Define PROGCLASS, HACK_INIT, and HACK_DRAW before including xlockmore.h
#endif

#include "config.h"

#include <stdio.h>
#include <math.h>
#include "mode.h"

#ifdef USE_GL
#include "visgl.h"
#define FreeAllGL(dpy)		/* */
#endif

/*-
 * Sun version of OpenGL needs to have the constant
 * SUN_OGL_NO_VERTEX_MACROS defined in order to compile modes that
 * use glNormal3f (the number of arguments to the macro changes...)
 */
#ifndef HAVE_MESA_GL
#if defined(__sun) && defined(__SVR4)	/* Solaris */
#define SUN_OGL_NO_VERTEX_MACROS 1
#endif /* Solaris */
#endif /* !HAVE_MESA_GL */


/* Some other utility macros.
 */
#define SINF(n)			((float)sin((double)(n)))
#define COSF(n)			((float)cos((double)(n)))
#define FABSF(n)		((float)fabs((double)(n)))

#undef MAX
#undef MIN
#undef ABS
#define MAX(a,b)((a)>(b)?(a):(b))
#define MIN(a,b)((a)<(b)?(a):(b))
#define ABS(a)((a)<0 ? -(a) : (a))

/* Maximum possible number of colors (*not* default number of colors.) */
#ifndef WIN32
#define NUMCOLORS 256
#endif /* !WIN32 */

/* The globals that screenhack.c expects (initialized by xlockmore.c).
 */
#ifdef __cplusplus
  extern "C" {
#endif
char       *defaults[100];
XrmOptionDescRec options[100];
#ifdef __cplusplus
  }
#endif

#define XSCREENSAVER_PREF 1	/* Disagreements handled with this  :) */

#else /* !STANDALONE */

#ifdef HAVE_CONFIG_H
#include "config.h"
#ifdef USE_MB
#ifdef __linux__
/*#define X_LOCALE*/
#endif
#include <X11/Xlocale.h>
#endif
#else /* HAVE_CONFIG_H */

#ifdef WIN32
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_SYSLOG_H 1
#define HAVE_MEMORY_H 1
#define HAVE_STRDUP 1
#else /* !WIN32 */

/* THIS MAY SOON BE DEFUNCT, SHOULD WORK NOW THOUGH FOR IMAKE */
#define HAVE_GLBINDTEXTURE 1
#if !defined(__cplusplus) && !defined(c_plusplus)
#ifdef inline
#undef inline
#endif
#define inline
#endif
#ifdef AIXV3
#define HAVE_SYS_SELECT_H 1
#else
#define HAVE_SYS_TIME_H 1
#endif
#if !defined( __hpux ) && !defined( apollo ) && !defined( VMS )
#define HAVE_SETEUID
#endif
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_SYSLOG_H 1
#define HAVE_GETHOSTNAME 1
#define HAVE_UNISTD_H 1
#ifdef VMS
#if ( __VMS_VER < 70000000 )
#ifdef USE_XVMSUTILS
#define HAVE_DIRENT_H 1
#define HAVE_GETTIMEOFDAY 1
#define GETTIMEOFDAY_TWO_ARGS 1
#else /* !USE_XVMSUTILS */
#ifndef USE_OLD_EVENT_LOOP
#define USE_OLD_EVENT_LOOP
#endif /* USE_OLD_EVENT_LOOP */
#define HAVE_DIRENT_H 0
#define HAVE_GETTIMEOFDAY 0
#endif /* !USE_XVMSUTILS */
#define HAVE_STRDUP 0
#else /* __VMS_VER >= 70000000 */
#define HAVE_DIRENT_H 1
#define HAVE_GETTIMEOFDAY 1
#define GETTIMEOFDAY_TWO_ARGS 1
#define HAVE_STRDUP 1
#endif /* __VMS_VER >= 70000000 */
#else /* !VMS */
#define HAVE_DIRENT_H 1
#define HAVE_MEMORY_H 1
#define HAVE_GETTIMEOFDAY 1
#define GETTIMEOFDAY_TWO_ARGS 1
#ifdef ultrix
#define HAVE_STRDUP 0
#else /* !ultrix */
#define HAVE_STRDUP 1
#endif /* !ultrix */
#endif /* !VMS */
#endif /* !WIN32 */
#endif /* !HAVE_CONF_H */

#include <sys/types.h>
#ifndef WIN32
#ifdef __NetBSD__
#include <signal.h>
#else /* !__NetBSD__ */
#include <sys/signal.h>
#endif /* __NetBSD__ */
#endif /* !WIN32 */
#if defined( VMS ) && defined( __cplusplus )
/* Xlib.h for VMS is not (yet) compatible with C++ *
 * The resulting warnings are switched off here    */
#pragma message disable nosimpint
#endif
#ifdef VMS
#include "vms_x_fix.h"
#endif
#ifndef WIN32
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xresource.h>
#else
#include "Xapi.h"
/* wingdi.h also defines NUMCOLORS but to 24 */
#undef NUMCOLORS
#define NUMCOLORS 256
#endif /* WIN32 */
#if defined( VMS ) && defined( __cplusplus )
#pragma message enable nosimpint
#endif
#include <math.h>

/* I use this for testing SunCplusplus
   I still get this in the link:
   Undefined                       first referenced
   symbol                             in file
   gethostbyname(const char*)               ../xlock/resource.o
   kill(long, int)                         ../xlock/logout.o
   signal(int, void (*)(int))                   ../xlock/logout.o

 */
#ifdef SunCplusplus
#ifndef HAVE_USLEEP
#define HAVE_USLEEP 1
#endif
#ifndef SUN_OGL_NO_VERTEX_MACROS
#define SUN_OGL_NO_VERTEX_MACROS 1
#endif
#endif
#ifndef TEXTLINES
#define TEXTLINES      40
#endif
#define PASSLENGTH        120

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef DEF_NONE3D
#define DEF_NONE3D "Black"
#endif
#ifndef DEF_RIGHT3D
#define DEF_RIGHT3D "Blue"
#endif
#ifndef DEF_LEFT3D
#define DEF_LEFT3D "Red"
#endif
#ifndef DEF_BOTH3D
#define DEF_BOTH3D "Magenta"
#endif

#ifndef DEF_ICONW
#define DEF_ICONW         64	/* Age old default */
#endif
#ifndef DEF_ICONH
#define DEF_ICONH         64
#endif

#ifndef DEF_GLW
#define DEF_GLW         640
#endif
#ifndef DEF_GLH
#define DEF_GLH         480
#endif

#define MINICONW          1	/* Will not see much */
#define MINICONH          1

#ifndef MAXICONW
#define MAXICONW          256	/* Want users to know the screen is locked */
#endif
#ifndef MAXICONH
#define MAXICONH          256	/* by a particular user */
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#ifndef ABS
#define ABS(a)  ((a<0)?(-(a)):(a))
#endif

#ifndef M_E
#define M_E         2.7182818284590452354	/* e */
#endif
#ifndef M_LOG2E
#define M_LOG2E     1.4426950408889634074       /* */
#endif
#ifndef M_LN2
#define M_LN2       0.69314718055994530942	/* log e2 */
#endif
#ifndef M_PI
#define M_PI        3.14159265358979323846	/* pi */
#endif
#ifndef M_PI_2
#define M_PI_2      1.57079632679489661923	/* pi/2 */
#endif
#ifdef MATHF
#define SINF(n) sinf(n)
#define COSF(n) cosf(n)
#define FABSF(n) fabsf(n)
#else
#define SINF(n) ((float)sin((double)(n)))
#define COSF(n) ((float)cos((double)(n)))
#define FABSF(n) ((float)fabs((double)(n)))
#endif

#if VMS
#include <unixlib.h>
#endif
#if 0
#ifndef uid_t
#define uid_t int
#endif
#ifndef gid_t
#define gid_t int
#endif
#ifndef pid_t
#define pid_t int
#endif
#ifndef size_t
#define size_t unsigned
#endif
#ifndef caddr_t
#define caddr_t char *
#endif
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if (defined( SYSV ) || defined( SVR4 )) && defined( SOLARIS2 ) && !defined( LESS_THAN_SOLARIS2_5 )
#ifdef __cplusplus
  extern "C" {
#endif
struct hostent {
	char       *h_name;	/* official name of host */
	char      **h_aliases;	/* alias list */
	int         h_addrtype;	/* host address type */
	int         h_length;	/* length of address */
	char      **h_addr_list;	/* list of addresses from name server */
};
#ifdef __cplusplus
  }
#endif

#else
#ifndef WIN32
#include <netdb.h>		/* Gives problems on Solaris 2.6 with gcc */
#endif /* !WIN32 */
#endif
#include <ctype.h>
#if HAVE_DIRENT_H
#ifdef USE_XVMSUTILS
#if 0
#include "../xvmsutils/unix_types.h"
#include "../xvmsutils/dirent.h"
#else
#include <X11/unix_types.h>
#include <X11/dirent.h>
#endif
#else /* !USE_XVMSUTILS */
#include <dirent.h>
#endif /* !USE_XVMSUTILS */
#else
#define dirent direct
#define NAMELEN(dirent) (dirent)->d_namelen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif
#if defined(_SYSTYPE_SVR4) && !defined(SVR4)	/* For SGI */
#define SVR4
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64	/* SunOS 3.5 does not define this */
#endif

#ifndef MAXNAMLEN
#define MAXNAMLEN       256	/* maximum filename length */
#endif
#ifndef DIRBUF
#define DIRBUF          512	/* buffer size for fs-indep. dirs */
#endif

#if defined(__cplusplus) || defined(c_plusplus)
#define CLASS c_class
#else
#define CLASS class
#endif

#if (!defined( AFS ) && !defined( SIA ) && defined( HAVE_SHADOW ) && !defined( OSF1_ENH_SEC ) && !defined( HP_PASSWDETC ) && !defined( VMS ) && !defined( USE_PAM ) && !defined( PASSWD_HELPER_PROGRAM ))
#define FALLBACK_XLOCKRC
#endif

#define t_String        0
#define t_Float         1
#define t_Int           2
#define t_Bool          3

#ifdef __cplusplus
  extern "C" {
#endif
typedef struct {
	void       *var;
	char       *name;
	char       *classname;
	char       *def;
	int         type;
} argtype;

typedef struct {
	char       *opt;
	char       *desc;
} OptionStruct;

typedef struct {
	int         numopts;
	XrmOptionDescRec *opts;
	int         numvarsdesc;
	argtype    *vars;
	OptionStruct *desc;
} ModeSpecOpt;

#ifdef USE_MODULES
typedef struct {
	const char       *cmdline_arg;	/* mode name */
	const char       *init_name;	/* name of init a mode */
	const char       *callback_name;	/* name of run (tick) a mode */
	const char       *release_name;	/* name of shutdown of a mode */
	const char       *refresh_name;	/* name of mode to repaint */
	const char       *change_name;	/* name of mode to change */
	const char       *unused_name;	/* name for future expansion */
	ModeSpecOpt *msopt;	/* this mode's def resources */
	int         def_delay;	/* default delay for mode */
	int         def_count;
	int         def_cycles;
	int         def_size;
	int         def_ncolors;
	float       def_saturation;
	const char       *def_bitmap;
	const char       *desc;	/* text description of mode */
	unsigned int flags;	/* state flags for this mode */
	void       *userdata;	/* for use by the mode */
} ModStruct;

#endif
#ifdef __cplusplus
  }
#endif

/* this must follow definition of ModeSpecOpt */
#include "mode.h"

#define CHECK_OLD_ARGS		/* check deprecated args */

#ifdef OPENGL_MESA_INCLUDES
/* Allow OPEN GL using MESA includes */
#undef MESA
#endif

#ifdef __cplusplus
  extern "C" {
#endif
extern char * ProgramName;
extern void getResources(Display ** displayp, int argc, char **argv);
extern void finish(Display * display, Bool closeDisplay);
#ifdef HAS_MMOV
#define error xlock_error
#endif
extern void error(const char *buf);
#ifndef DECLARED_GETENV
extern char * getenv(const char *);
#endif
#ifdef __cplusplus
  }
#endif

#ifdef VMS
#define FORK vfork
#else
#define FORK fork
#endif

#ifdef LESS_THAN_AIX3_2
#undef NULL
#define NULL 0
#endif /* LESS_THAN_AIX3_2 */

#ifndef SYSLOG_WARNING
#define SYSLOG_WARNING LOG_WARNING
#endif
#ifndef SYSLOG_NOTICE
#define SYSLOG_NOTICE LOG_NOTICE
#endif
#ifndef SYSLOG_INFO
#define SYSLOG_INFO LOG_INFO
#endif

#if (defined( USE_RPLAY ) || defined( USE_NAS ) || defined( USE_VMSPLAY ) || \
     defined( HAS_MMOV ) || defined( DEF_PLAY ) || defined( USE_ESOUND ))
#define USE_SOUND
#endif

#ifdef USE_MB
#ifdef __cplusplus
  extern "C" {
#endif
extern XFontSet fontset;
#ifdef __cplusplus
  }
#endif

#define XTextWidth(font,string,length) \
		XmbTextEscapement(fontset,string,length)
#define XDrawString(display,window,gc,x,y,string,length) \
		XmbDrawString(display,window,fontset,gc,x,y,string,length)
#define XDrawImageString(display,window,gc,x,y,string,length) \
		XmbDrawImageString(display,window,fontset,gc,x,y,string,length)
#endif

#endif /* STANDALONE */

/* COMMON STUFF */
#if HAVE_GETTIMEOFDAY
#ifdef GETTIMEOFDAY_TWO_ARGS
#ifdef SunCplusplus
#define GETTIMEOFDAY(t) (void)gettimeofday(t,(struct timezone *) NULL);
#else
#define GETTIMEOFDAY(t) (void)gettimeofday(t,NULL);
#endif
#else
#define GETTIMEOFDAY(t) (void)gettimeofday(t);
#endif
#endif

#ifndef CLOCKS_PER_SEC
#define	CLOCKS_PER_SEC		1000000
#endif


/* There is some overlap so it can be made more efficient */
#define ERASE_IMAGE(d,w,g,x,y,xl,yl,xs,ys) \
if (yl<y) \
(y<(yl+ys))?XFillRectangle(d,w,g,xl,yl,xs,y-(yl)): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
else if (yl>y) \
(y>(yl-(ys)))?XFillRectangle(d,w,g,xl,y+ys,xs,yl-(y)): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
if (xl<x) \
(x<(xl+xs))?XFillRectangle(d,w,g,xl,yl,x-(xl),ys): \
XFillRectangle(d,w,g,xl,yl,xs,ys); \
else if (xl>x) \
(x>(xl-(xs)))?XFillRectangle(d,w,g,x+xs,yl,xl-(x),ys): \
XFillRectangle(d,w,g,xl,yl,xs,ys)

#include "random.h"

#ifdef WIN32
void xlockmore_win32_text(int xloc, int yloc, char *text);
void xlockmore_set_debug(char *text);
#endif

#endif /* __XLOCK_XLOCK_H__ */
