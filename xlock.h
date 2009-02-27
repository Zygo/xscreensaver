/*-
 * @(#)xlock.h	1.9 91/05/24 XLOCK
 *
 * xlock.h - external interfaces for new modes and SYSV OS defines.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *
 * Changes of David Bagley <bagleyd@source.asset.com>
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
 * 01-Dec-93: added patch for AIXV3 from
 *            (Tom McConnell, tmcconne@sedona.intel.com) also added a patch
 *            for HP-UX 8.0.
 *
 */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#define MAXSCREENS 3
#define NUMCOLORS 64

#ifdef VMS
typedef unsigned long u_long;
typedef unsigned short u_short;
typedef unsigned char u_char;
#define M_PI    3.14159265358979323846
#define M_PI_2  1.57079632679489661923
#define M_PI_4  0.78539816339744830962
#endif
 
typedef struct {
    GC          gc;		/* graphics context for animation */
    int         npixels;	/* number of valid entries in pixels */
    u_long      pixels[NUMCOLORS];	/* pixel values in the colormap */
}           perscreen;

extern perscreen Scr[MAXSCREENS];
extern Display *dsp;
extern int  screen;

extern char *ProgramName;
extern char *displayname;
extern char *mode;
extern char *fontname;
extern char *background;
extern char *foreground;
extern char *text_name;
extern char *text_pass;
extern char *text_info;
extern char *text_valid;
extern char *text_invalid;
extern float saturation;
extern int  nicelevel;
extern int delay;
extern int  batchcount;
extern int  reinittime;
extern int  timeout;
extern Bool usefirst;
extern Bool mono;
extern Bool nolock;
extern Bool allowroot;
extern Bool enablesaver;
extern Bool allowaccess;
extern Bool echokeys;
extern Bool verbose;
extern Bool inwindow;
extern void (*callback) ();
extern void (*init) ();

extern void GetResources();
extern void hsbramp();
extern void error();
extern long seconds();
extern void usage();


/* System V Release 4 redefinitions of BSD functions and structures */
#if defined(SYSV) || defined(SVR4)

#if defined(HAS_SHADOW) || defined(SOLARIS_SHADOW)
#define passwd spwd
#define pw_passwd sp_pwdp
#include <shadow.h>
#define getpwnam getspnam
#ifdef SOLARIS_SHADOW
#define pw_name sp_namp
#endif
#endif

#include <sys/time.h>
#ifndef __hpux
#ifdef AIXV3
#undef NULL
#define NULL 0
#include <sys/poll.h>
struct passwd {
        char    *pw_name;
        char    *pw_passwd;
        uid_t   pw_uid;
        gid_t   pw_gid;
        char    *pw_gecos;
        char    *pw_dir;
        char    *pw_shell;
};
#else
#include <poll.h>
#endif
#endif
#define srandom(a) srand(a)
#define random() rand()
#define MAXRAND (32767.0)

#else

#if defined(HAS_SHADOW)
#include <shadow.h>
#define passwd spwd
#define pw_passwd sp_pwdp
#define pw_name sp_namp
#endif

#define MAXRAND (2147483648.0)

#endif
