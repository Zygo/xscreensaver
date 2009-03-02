#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xlock.c	4.03 97/06/16 xlockmore";

#endif

/*-
 * xlock.c - X11 client to lock a display and show a screen saver.
 *
 * Copyright (c) 1988-91 by Patrick J. Naughton.
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
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 01-May-97: Scott Carter <scarter@sdsc.edu>
 *            Added code to stat .xlockmessage, .plan, and .signature files
 *            before reading the message;  only regular files are read (no
 *            pipes or special files).
 *            Added code to replace tabs with 8 spaces in the message buffer.
 * 01-Sep-96: Ron Hitchens <ron@idiom.com>
 *            Updated xlock so it would refresh more reliably and
 *            handle window resizing.
 * 18-Mar-96: Ron Hitchens <ron@idiom.com>
 *            Implemented new ModeInfo hook calling scheme.
 *            Created mode_info() to gather and pass info to hooks.
 *            Watch for and use MI_PAUSE value if mode hook sets it.
 *            Catch SIGSEGV, SIGBUS and SIGFPE signals.  Other should
 *            be caught.  Eliminate some globals.
 * 23-Dec-95: Ron Hitchens <ron@idiom.com>
 *            Rewrote event loop so as not to use signals.
 * 01-Sep-95: initPasswd function, more stuff removed to passwd.c
 * 24-Jun-95: Cut out passwd stuff to passwd.c-> getPasswd & checkPasswd
 * 17-Jun-95: Added xlockrc password compile time option.
 * 12-May-95: Added defines for SunOS's Adjunct password file from
 *            Dale A. Harris <rodmur@ecst.csuchico.edu>
 * 21-Feb-95: MANY patches from Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 * 24-Jan-95: time_displayed fixed from Chris Ross <cross@va.pubnix.com>.
 * 18-Jan-95: Ultrix systems (at least DECstations) running enhanced
 *            security from Chris Fuhrman <cfuhrman@vt.edu> and friend.
 * 26-Oct-94: In order to use extra-long passwords with the Linux changed
 *            PASSLENGTH to 64 <slouken@virtbrew.water.ca.gov>
 * 11-Jul-94: added -inwindow option from Greg Bowering
 *            <greg@cs.adelaide.edu.au>
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 10-Jun-94: patch for BSD from Victor Langeveld <vic@mbfys.kun.nl>
 * 02-May-94: patched to work on Linux from Darren Senn's
 *            <sinster@scintilla.capitola.ca.us> xlock for Linux.
 *            Took out "bounce" since it was too buggy (maybe I will put
 *            it back later).
 * 21-Mar-94: patch to to trap Shift-Ctrl-Reset courtesy of Jamie Zawinski
 *            <jwz@netscape.com>, patched the patch (my mistake) for AIXV3
 *            and HP from <R.K.Lloyd@csc.liv.ac.uk>.
 * 01-Dec-93: added patch for AIXV3 from Tom McConnell
 *            <tmcconne@sedona.intel.com> also added a patch for HP-UX 8.0.
 * 29-Jul-93: "hyper", "helix", "rock", and "blot" (also tips on "maze") I
 *            got courtesy of Jamie Zawinski <jwz@netscape.com>;
 *            at the time I could not get his stuff to work for the hpux 8.0,
 *            so I scrapped it but threw his stuff in xlock.
 *            "maze" and "sphere" I got courtesy of Sun Microsystems.
 *            "spline" I got courtesy of Jef Poskanzer <jef@netcom.com or
 *            jef@well.sf.ca.us>.
 *
 * Changes of Patrick J. Naughton
 * 24-Jun-91: make foreground and background color get used on mono.
 * 24-May-91: added -usefirst.
 * 16-May-91: added pyro and random modes.
 *	      ripped big comment block out of all other files.
 * 08-Jan-91: fix some problems with password entry.
 *	      removed renicing code.
 * 29-Oct-90: added cast to XFree() arg.
 *	      added volume arg to call to XBell().
 * 28-Oct-90: center prompt screen.
 *	      make sure Xlib input buffer does not use up all of swap.
 *	      make displayed text come from resource file for better I18N.
 *	      add backward compatible signal handlers for pre 4.1 machines.
 * 31-Aug-90: added blank mode.
 *	      added swarm mode.
 *	      moved usleep() and seconds() out to usleep.c.
 *	      added SVR4 defines to xlock.h
 * 29-Jul-90: added support for multiple screens to be locked by one xlock.
 *	      moved global defines to xlock.h
 *	      removed use of allowsig().
 * 07-Jul-90: reworked commandline args and resources to use Xrm.
 *	      moved resource processing out to resource.c
 * 02-Jul-90: reworked colors to not use dynamic colormap.
 * 23-May-90: added autoraise when obscured.
 * 15-Apr-90: added hostent alias searching for host authentication.
 * 18-Feb-90: added SunOS3.5 fix.
 *	      changed -mono -> -color, and -saver -> -lock.
 *	      allow non-locking screensavers to display on remote machine.
 *	      added -echokeys to disable echoing of '?'s on input.
 *	      cleaned up all of the parameters and defaults.
 * 20-Dec-89: added -xhost to allow access control list to be left alone.
 *	      added -screensaver (do not disable screen saver) for the paranoid.
 *	      Moved seconds() here from all of the display mode source files.
 *	      Fixed bug with calling XUngrabHosts() in finish().
 * 19-Dec-89: Fixed bug in GrabPointer.
 *	      Changed fontname to XLFD style.
 * 23-Sep-89: Added fix to allow local hostname:0 as a display.
 *	      Put empty case for Enter/Leave events.
 *	      Moved colormap installation later in startup.
 * 20-Sep-89: Linted and made -saver mode grab the keyboard and mouse.
 *	      Replaced SunView code for life mode with Jim Graham's version,
 *		so I could contrib it without legal problems.
 *	      Sent to expo for X11R4 contrib.
 * 19-Sep-89: Added '?'s on input.
 * 27-Mar-89: Added -qix mode.
 *	      Fixed GContext->GC.
 * 20-Mar-89: Added backup font (fixed) if XQueryLoadFont() fails.
 *	      Changed default font to lucida-sans-24.
 * 08-Mar-89: Added -nice, -mode and -display, built vector for life and hop.
 * 24-Feb-89: Replaced hopalong display with life display from SunView1.
 * 22-Feb-89: Added fix for color servers with n < 8 planes.
 * 16-Feb-89: Updated calling conventions for XCreateHsbColormap();
 *	      Added -count for number of iterations per color.
 *	      Fixed defaulting mechanism.
 *	      Ripped out VMS hacks.
 *	      Sent to expo for X11R3 contrib.
 * 15-Feb-89: Changed default font to pellucida-sans-18.
 * 20-Jan-89: Added -verbose and fixed usage message.
 * 19-Jan-89: Fixed monochrome gc bug.
 * 16-Dec-88: Added SunView style password prompting.
 * 19-Sep-88: Changed -color to -mono. (default is color on color displays).
 *	      Added -saver option. (just do display... do not lock.)
 * 31-Aug-88: Added -time option.
 *	      Removed code for fractals to separate file for modularity.
 *	      Added signal handler to restore host access.
 *	      Installs dynamic colormap with a Hue Ramp.
 *	      If grabs fail then exit.
 *	      Added VMS Hacks. (password 'iwiwuu').
 *	      Sent to expo for X11R2 contrib.
 * 08-Jun-88: Fixed root password pointer problem and changed PASSLENGTH to 20.
 * 20-May-88: Added -root to allow root to unlock.
 * 12-Apr-88: Added root password override.
 *	      Added screen saver override.
 *	      Removed XGrabServer/XUngrabServer.
 *	      Added access control handling instead.
 * 01-Apr-88: Added XGrabServer/XUngrabServer for more security.
 * 30-Mar-88: Removed startup password requirement.
 *	      Removed cursor to avoid phosphor burn.
 * 27-Mar-88: Rotate fractal by 45 degrees clockwise.
 * 24-Mar-88: Added color support. [-color]
 *	      wrote the man page.
 * 23-Mar-88: Added HOPALONG routines from Scientific American Sept. 86 p. 14.
 *	      added password requirement for invokation
 *	      removed option for command line password
 *	      added requirement for display to be "unix:0".
 * 22-Mar-88: Recieved Walter Milliken's comp.windows.x posting.
 *
 */

#include "xlock.h"
#include <sys/stat.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H && defined(AIXV3)
#include <sys/select.h>
#endif
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#ifdef USE_VROOT
#include "vroot.h"
#endif
#if USE_XVMSUTILS
#if 0
#include "../xvmsutils/unix_types.h"
#include "../xvmsutils/unix_time.h"
#else
#include <X11/unix_types.h>
#include <X11/unix_time.h>
#endif
#endif /* USE_XVMSUTILS */
#ifdef USE_DTSAVER
#include <X11/Intrinsic.h>
#include <Dt/Saver.h>
#endif
#if defined( __hpux ) || defined( __apollo )
#include <X11/XHPlib.h>
extern int  XHPEnableReset(Display * dsp);

#endif

/*-
COLORMAP ON OPENGL PROBLEMS  TO SEE WHAT THESE ARE USING MESA
#undef MESA
*/

#if ( HAVE_SYSLOG_H && defined( USE_SYSLOG ))
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#endif

#if ( __VMS_VER >= 70000000 )
#include <lib$routines.h>
#include <mail$routines.h>
#include <maildef.h>
struct itmlst_3 {
	unsigned short int buflen;
	unsigned short int itmcode;
	void       *bufadr;
	unsigned short int *retlen;
};

#endif

extern char *getenv(const char *);
extern void checkResources(void);
extern void initPasswd(void);
extern int  checkPasswd(char *);

#if defined( USE_GL ) && !defined( MESA )
extern XVisualInfo *getGLVisual(Display * display, int screen, XVisualInfo * wantVis, int mono);
extern void FreeAllGL(Display * display);

#endif

#ifdef USE_OLD_EVENT_LOOP
extern int  usleep(unsigned int);

#endif

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
extern void logoutUser(void);

#endif

#if 0
	/* #if !defined( AIXV3 ) && !defined( __hpux ) && !defined( __bsdi__ ) */
extern int  select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

#if !defined( __hpux ) && !defined( apollo )
extern int  select(size_t, int *, int *, int *, const struct timeval *);

#endif

#endif
extern int  nice(int);

char       *ProgramName;	/* argv[0] */
pid_t       ProgramPID;		/* for memcheck.c not supplied */
perscreen   Scr[MAXSCREENS];
Display    *dsp = NULL;		/* server display connection */

extern char user[PASSLENGTH];
extern float saturation;
extern float delta3d;
extern int  delay;
extern int  batchcount;
extern int  cycles;
extern int  size;
extern Bool nolock;
extern Bool inwindow;
extern Bool inroot;
extern Bool mono;
extern Bool allowaccess;
extern Bool allowroot;
extern Bool debug;
extern Bool echokeys;
extern Bool enablesaver;
extern Bool fullrandom;
extern Bool grabmouse;
extern Bool grabserver;
extern Bool install;
extern Bool timeelapsed;
extern Bool usefirst;
extern Bool verbose;
extern int  nicelevel;
extern int  lockdelay;
extern int  timeout;
extern Bool wireframe;
extern Bool use3d;

extern char *fontname;
extern char *messagefont;
extern char *background;
extern char *foreground;
extern char *text_name;
extern char *text_pass;
extern char *text_info;
extern char *text_valid;
extern char *text_invalid;
extern char *geometry;
extern char *icongeometry;
extern char *none3d;
extern char *right3d;
extern char *left3d;
extern char *both3d;

#ifdef USE_SOUND
extern char *locksound;
extern char *infosound;
extern char *validsound;
extern char *invalidsound;
extern void play_sound(char *string);
extern Bool sound;

#endif
#ifdef USE_AUTO_LOGOUT
extern int  logoutAuto;

#endif

#ifdef USE_BUTTON_LOGOUT
extern int  logoutButton;
extern int  enable_button;
extern char *logoutButtonLabel;
extern char *logoutButtonHelp;
extern char *logoutFailedString;

#endif

#ifdef USE_DTSAVER
extern Bool dtsaver;

#endif

static int  onepause = 0;
static int  screen = 0;		/* current screen */

static int         screens;		/* number of screens */
static Window win[MAXSCREENS];	/* window used to cover screen */
static Window icon[MAXSCREENS];	/* window used during password typein */

#ifdef USE_BUTTON_LOGOUT
static Window button[MAXSCREENS];

#endif
static Window root[MAXSCREENS];	/* convenience pointer to the root window */
static GC   textgc[MAXSCREENS];	/* graphics context used for text rendering */
static GC   msgtextgc[MAXSCREENS];	/* graphics context for message rendering */
static unsigned long fgcol[MAXSCREENS];		/* used for text rendering */
static unsigned long bgcol[MAXSCREENS];		/* background of text screen */
static unsigned long nonecol[MAXSCREENS];
static unsigned long rightcol[MAXSCREENS];
static unsigned long leftcol[MAXSCREENS];
static unsigned long bothcol[MAXSCREENS];	/* Can change with -install */
static int  iconx[MAXSCREENS];	/* location of left edge of icon */
static int  icony[MAXSCREENS];	/* location of top edge of icon */
static int  msgx[MAXSCREENS];	/* location of left edge of message */
static int  msgy[MAXSCREENS];	/* location of top edge of message */

static Cursor mycursor;		/* blank cursor */
static Pixmap lockc;
static Pixmap lockm;		/* pixmaps for cursor and mask */
static char no_bits[] =
{0};				/* dummy array for the blank cursor */
static int  passx, passy;	/* position of the ?'s */
static int  iconwidth, iconheight;
static int  timex, timey;	/* position for the times */
static XFontStruct *font;
static XFontStruct *msgfont;
static int  sstimeout;		/* screen saver parameters */
static int  ssinterval;
static int  ssblanking;
static int  ssexposures;
static unsigned long start_time;
static char *message[MESSAGELINES + 1];		/* Message is stored here */

/* GEOMETRY STUFF */
static int  sizeconfiguremask;
static XWindowChanges fullsizeconfigure, minisizeconfigure;
static int  fullscreen = False;

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
static int  tried_logout = 0;

#endif

#ifdef USE_SOUND
static int  got_invalid = 0;

#endif

#define AllPointerEventMask \
	(ButtonPressMask | ButtonReleaseMask | \
	EnterWindowMask | LeaveWindowMask | \
	PointerMotionMask | PointerMotionHintMask | \
	Button1MotionMask | Button2MotionMask | \
	Button3MotionMask | Button4MotionMask | \
	Button5MotionMask | ButtonMotionMask | \
	KeymapStateMask)

#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_AUTH
#endif

static void
syslogStart(void)
{
	struct passwd *pw;
	struct group *gr;

	pw = getpwuid(getuid());
	gr = getgrgid(getgid());

	(void) openlog(ProgramName, LOG_PID, SYSLOG_FACILITY);
	syslog(SYSLOG_INFO, "Start: %s, %s, %s",
	       pw->pw_name, gr->gr_name, XDisplayString(dsp));
}

void
syslogStop(void)
{
	struct passwd *pw;
	struct group *gr;
	int         secs, mins;

	secs = (int) (seconds() - start_time);
	mins = secs / 60;
	secs %= 60;

	pw = getpwuid(getuid());
	gr = getgrgid(getgid());

	syslog(SYSLOG_INFO, "Stop: %s, %s, %s, %dm %ds",
	       pw->pw_name, gr->gr_name, XDisplayString(dsp), mins, secs);
}

#endif

void
error(char *buf)
{
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	syslog(SYSLOG_WARNING, buf);
	if (!nolock) {
		syslogStop();
		closelog();
	}
#else
	(void) fprintf(stderr, buf);
#endif
	exit(1);
}

/* Server access control support. */

static XHostAddress *XHosts;	/* the list of "friendly" client machines */
static int  HostAccessCount;	/* the number of machines in XHosts */
static Bool HostAccessState;	/* whether or not we even look at the list */

static void
XGrabHosts(Display * display)
{
	XHosts = XListHosts(display, &HostAccessCount, &HostAccessState);
	if (XHosts)
		XRemoveHosts(display, XHosts, HostAccessCount);
	XEnableAccessControl(display);
}

static void
XUngrabHosts(Display * display)
{
	if (XHosts) {
		XAddHosts(display, XHosts, HostAccessCount);
		XFree((char *) XHosts);
	}
	if (HostAccessState == False)
		XDisableAccessControl(display);
}


/* Simple wrapper to get an asynchronous grab on the keyboard and mouse. If
   either grab fails, we sleep for one second and try again since some window
   manager might have had the mouse grabbed to drive the menu choice that
   picked "Lock Screen..".  If either one fails the second time we print an
   error message and exit. */
static void
GrabKeyboardAndMouse(void)
{
	Status      status;
	char       *buf = NULL;

	status = XGrabKeyboard(dsp, win[0], True,
			       GrabModeAsync, GrabModeAsync, CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabKeyboard(dsp, win[0], True,
				  GrabModeAsync, GrabModeAsync, CurrentTime);

		if (status != GrabSuccess) {
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf, "%s, could not grab keyboard! (%d)\n",
				       ProgramName, status);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	}
	status = XGrabPointer(dsp, win[0], True, AllPointerEventMask,
			      GrabModeAsync, GrabModeAsync, None, mycursor,
			      CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabPointer(dsp, win[0], True, AllPointerEventMask,
				GrabModeAsync, GrabModeAsync, None, mycursor,
				      CurrentTime);

		if (status != GrabSuccess) {
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf, "%s, could not grab pointer! (%d)\n",
				       ProgramName, status);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	}
}

/* Assuming that we already have an asynch grab on the pointer, just grab it
   again with a new cursor shape and ignore the return code. */
static void
XChangeGrabbedCursor(Cursor cursor)
{
	if (!debug && grabmouse && !inwindow && !inroot)
		(void) XGrabPointer(dsp, win[0], True, AllPointerEventMask,
		    GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
}

/* This is a private data structure, don't touch */
static ModeInfo modeinfo[MAXSCREENS];

/*-
 *    Return True of False indicating if the given window has changed
 *    size relative to the window geometry cached in the mode_info
 *    struct.  If the window handle given does not match the one in
 *    the cache, or if the width/height are not the same, then True
 *    is returned and the window handle in the cache is cleared.
 *    This causes mode_info() to reload the window info next time
 *    it is called.
 *    This function has non-obvious side-effects.  I feel so dirty.  Rh
 */

static int
window_size_changed(int scrn, Window window)
{
	XWindowAttributes xgwa;
	ModeInfo   *mi = &modeinfo[scrn];

	if (MI_WINDOW(mi) != window) {
		MI_WINDOW(mi) = None;	/* forces reload on next mode_info() */
		return (True);
	} else {
		(void) XGetWindowAttributes(dsp, window, &xgwa);
		if ((MI_WIN_WIDTH(mi) != xgwa.width) ||
		    (MI_WIN_HEIGHT(mi) != xgwa.height)) {
			MI_WINDOW(mi) = None;
			return (True);
		}
	}

	return (False);
}

/*-
 *    Return a pointer to an up-to-date ModeInfo struct for the given
 *      screen, window and iconic mode.  Because the number of screens
 *      is limited, and much of the information is screen-specific, this
 *      function keeps static copies of ModeInfo structs are kept for
 *      each screen.  This also eliminates redundant calls to the X server
 *      to acquire info that does change.
 */

static ModeInfo *
mode_info(int scrn, Window window, int iconic)
{
	XWindowAttributes xgwa;
	ModeInfo   *mi;

	mi = &modeinfo[scrn];

	if (MI_WIN_FLAG_NOT_SET(mi, WI_FLAG_INFO_INITTED)) {
		/* This stuff only needs to be set once per screen */

		(void) memset((char *) mi, 0, sizeof (ModeInfo));

		MI_DISPLAY(mi) = dsp;	/* global */
		MI_SCREEN(mi) = scrn;
		MI_REAL_SCREEN(mi) = scrn;	/* TODO, for multiscreen debugging */
		MI_SCREENPTR(mi) = ScreenOfDisplay(dsp, scrn);
		MI_NUM_SCREENS(mi) = screens;	/* TODO */
		MI_MAX_SCREENS(mi) = screens;

		MI_WIN_BLACK_PIXEL(mi) = BlackPixel(dsp, scrn);
		MI_WIN_WHITE_PIXEL(mi) = WhitePixel(dsp, scrn);

		MI_PERSCREEN(mi) = &Scr[scrn];

		/* accessing globals here */
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_MONO,
			     (mono || CellsOfScreen(MI_SCREENPTR(mi)) <= 2));
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_INWINDOW, inwindow);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_INROOT, inroot);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_NOLOCK, nolock);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_INSTALL, install);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_DEBUG, debug);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_USE3D, use3d &&
			    !(mono || CellsOfScreen(MI_SCREENPTR(mi)) <= 2));
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_VERBOSE, verbose);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_FULLRANDOM, fullrandom);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_WIREFRAME, wireframe);
		MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_INFO_INITTED, True);
	}
	if (MI_WINDOW(mi) != window) {
		MI_WINDOW(mi) = window;

		(void) XGetWindowAttributes(dsp, window, &xgwa);

		MI_WIN_WIDTH(mi) = xgwa.width;
		MI_WIN_HEIGHT(mi) = xgwa.height;
	}
	MI_WIN_SET_FLAG_STATE(mi, WI_FLAG_ICONIC, iconic);

	MI_WIN_DEPTH(mi) = DisplayPlanes(dsp, scrn);
	MI_VISUAL(mi) = DefaultVisual(dsp, scrn);
	MI_WIN_COLORMAP(mi) = DefaultColormap(dsp, scrn);

	MI_DELTA3D(mi) = delta3d;
	MI_PAUSE(mi) = 0;	/* use default if mode doesn't change this */

	MI_DELAY(mi) = delay;	/* globals */
	MI_BATCHCOUNT(mi) = batchcount;
	MI_CYCLES(mi) = cycles;
	MI_SIZE(mi) = size;
	MI_SATURATION(mi) = saturation;

	return (mi);
}


/* Restore all grabs, reset screensaver, restore colormap, close connection. */
void
finish(void)
{
	for (screen = 0; screen < screens; screen++) {
		if (win[screen] != None) {
			release_last_mode(mode_info(screen, win[screen], False));
		}
		if (icon[screen] != None) {
			release_last_mode(mode_info(screen, icon[screen], True));
		}
	}

	XSync(dsp, False);
#ifdef USE_VROOT
	if (inroot)
		XClearWindow(dsp, win[0]);
#endif
	if (!nolock && !allowaccess) {
		if (grabserver)
			XUngrabServer(dsp);
		XUngrabHosts(dsp);
	}
	XUngrabPointer(dsp, CurrentTime);
	XUngrabKeyboard(dsp, CurrentTime);
	if (!enablesaver && !nolock) {
		XSetScreenSaver(dsp, sstimeout, ssinterval,
				ssblanking, ssexposures);
	}
	XFlush(dsp);
#if defined( USE_GL ) && !defined( MESA )
	FreeAllGL(dsp);
#endif
#ifndef __sgi
  /*-
   * The following line seems to cause a core dump on the SGI.
   * More work is needed on the cleanup code.
   */
	XCloseDisplay(dsp);
#endif
	(void) nice(0);
}

static int
xio_error(Display * d)
{
	exit(1);
	return 1;
}

/* Convenience function for drawing text */
static void
putText(Window w, GC gc, char *string, int bold, int left, int *px, int *py)
				/* which window */
				/* gc */
				/* text to write */
				/* 1 = make it bold */
				/* left edge of text */
				/* current x and y, input & return */
{
#define PT_BUFSZ 2048
	char        buf[PT_BUFSZ], *p, *s;
	int         x = *px, y = *py, last, len;

	(void) strncpy(buf, string, PT_BUFSZ);
	buf[PT_BUFSZ - 1] = 0;

	p = buf;
	last = 0;
	for (;;) {
		s = p;
		for (; *p; ++p)
			if (*p == '\n')
				break;
		if (!*p)
			last = 1;
		*p = 0;

		if ((len = strlen(s))) {	/* yes, "=", not "==" */
			XDrawImageString(dsp, w, gc, x, y, s, len);
			if (bold)
				XDrawString(dsp, w, gc, x + 1, y, s, len);
		}
		if (!last) {
			y += font->ascent + font->descent + 2;
			x = left;
		} else {
			if (len)
				x += XTextWidth(font, s, len);
			break;
		}
		p++;
	}
	*px = x;
	*py = y;
}

static void
statusUpdate(int isnew, int scr)
{
	int         left, x, y, len;
	char        buf[1024];
	XWindowAttributes xgwa;
	static int  last_time;

#ifdef USE_BUTTON_LOGOUT
	int         ysave;
	static      made_button, last_tried_lo = 0;

#endif /* USE_BUTTON_LOGOUT */

	len = (int) (seconds() - start_time) / 60;

#ifdef USE_BUTTON_LOGOUT
	if (tried_logout && !last_tried_lo) {
		last_tried_lo = 1;
		isnew = 1;
#ifdef USE_AUTO_LOGOUT
		logoutAuto = 0;
#endif
	}
	if (isnew)
		made_button = 0;
#endif /* USE_BUTTON_LOGOUT */

	if (isnew || last_time != len)
		last_time = len;
	else
		return;

	(void) XGetWindowAttributes(dsp, win[scr], &xgwa);
	x = left = timex;
	y = timey;

	if (timeelapsed) {
		if (len < 60)
			(void) sprintf(buf,
#ifdef NL
				       "%d minute%s op slot.                                        \n",
				       len, len == 1 ? "" : "n");
#else
#ifdef DE
				       "Seit %d Minute%s gesperrt.                                  \n",
				       len, len == 1 ? "" : "n");
#else
				       "%d minute%s elapsed since locked.                           \n",
				       len, len == 1 ? "" : "s");
#endif
#endif
		else
			(void) sprintf(buf,
#ifdef NL
				       "%d:%02d uur op slot.                                           \n",
#else
#ifdef DE
				       "Seit %d:%02d Stunden gesperrt.                                 \n",
#else
				       "%d:%02d hours elapsed since locked.                            \n",
#endif
#endif
				       len / 60, len % 60);
		putText(win[scr], textgc[scr], buf, False, left, &x, &y);
	}
#ifdef USE_BUTTON_LOGOUT
	if (enable_button) {
		if (logoutButton > len) {
			int         tmp = logoutButton - len;

			if (tmp < 60)
				(void) sprintf(buf,
#ifdef NL
					       "Over %d minute%s verschijnt de " logout " knop.                 \n",
					       tmp, (tmp == 1) ? "" : "n");
#else
#ifdef DE
					       "In %d Minute%s erscheint der Auslogger.                       \n",
					       tmp, (tmp == 1) ? "" : "n");
#else
					       "%d minute%s until the public logout button appears.           \n",
					       tmp, (tmp == 1) ? "" : "s");
#endif
#endif
			else
				(void) sprintf(buf,
#ifdef NL
					       "Over %d:%02d uur verschijnt de " logout " knop.               \n",
#else
#ifdef DE
					       "In %d:%02d Stunden erscheint der Auslogger.                 \n",
#else
					       "%d:%02d hours until the public logout button appears.       \n",
#endif
#endif
					       tmp / 60, tmp % 60);
			putText(win[scr], textgc[scr], buf, False, left, &x, &y);
		} else {
			putText(win[scr], textgc[scr],
#ifdef NL
				"Schop me eruit                                               \n",
#else
#ifdef DE
				"Werft mich raus                                              \n",
#else
				"Kick me out                                                  \n",
#endif
#endif
				False, left, &x, &y);
			if (!made_button || (isnew && tried_logout)) {
				made_button = 1;
				ysave = y;

				y += font->ascent + font->descent + 8;	/* Leave a gap for validating */
				XUnmapWindow(dsp, button[scr]);
				XSetForeground(dsp, Scr[scr].gc, bgcol[scr]);
				XFillRectangle(dsp, win[scr], Scr[scr].gc, left, y,
					       xgwa.width - left,
				     5 + 2 * (font->ascent + font->descent));
				XSetForeground(dsp, Scr[scr].gc, fgcol[scr]);

				if (tried_logout) {
					putText(win[scr], textgc[scr], logoutFailedString,
						True, left, &x, &y);
				} else {
					XMoveWindow(dsp, button[scr], left, y);
					XMapWindow(dsp, button[scr]);
					XRaiseWindow(dsp, button[scr]);
					(void) sprintf(buf, " %s ", logoutButtonLabel);
					XSetForeground(dsp, Scr[scr].gc, WhitePixel(dsp, scr));
					XDrawString(dsp, button[scr], Scr[scr].gc, 0, font->ascent + 1,
						    buf, strlen(buf));
					XSetForeground(dsp, Scr[scr].gc, fgcol[scr]);
					y += 5 + 2 * font->ascent + font->descent;
					putText(win[scr], textgc[scr], logoutButtonHelp,
						False, left, &x, &y);
				}
				y = ysave;
				x = left;
			}
		}
	}
#endif /* USE_BUTTON_LOGOUT */
#ifdef USE_AUTO_LOGOUT
	if (logoutAuto) {
		int         tmp = logoutAuto - len;

		if (tmp < 60)
			(void) sprintf(buf, "%d minute%s until auto-logout.    \n",
				       tmp, (tmp == 1) ? "" : "s");
		else
			(void) sprintf(buf, "%d:%02d hours until auto-logout.  \n",
				       tmp / 60, tmp % 60);
		putText(win[scr], textgc[scr], buf, False, left, &x, &y);
	}
#endif /* USE_AUTO_LOGOUT */
}

#ifdef USE_AUTO_LOGOUT
static void
checkLogout(void)
{
	if (nolock || tried_logout || !logoutAuto)
		return;

	if (logoutAuto * 60 < (int) (seconds() - start_time)) {
		tried_logout = 1;
		logoutAuto = 0;
		logoutUser();
	}
}

#endif /* USE_AUTO_LOGOUT */

#ifndef USE_OLD_EVENT_LOOP

/* subtract timer t2 from t1, return result in t3.  Result is not allowed to
   go negative, set to zero if result underflows */

static
void
sub_timers(struct timeval *t1, struct timeval *t2, struct timeval *t3)
{
	struct timeval tmp;
	int         borrow = 0;

	if (t1->tv_usec < t2->tv_usec) {
		borrow++;
		tmp.tv_usec = (1000000 + t1->tv_usec) - t2->tv_usec;
	} else {
		tmp.tv_usec = t1->tv_usec - t2->tv_usec;
	}

	/* Be careful here, timeval fields may be unsigned.  To avoid
	   underflow in an unsigned int, add the borrow value to the
	   subtrahend for the relational test.  Same effect, but avoids the
	   possibility of a negative intermediate value. */
	if (t1->tv_sec < (t2->tv_sec + borrow)) {
		tmp.tv_usec = tmp.tv_sec = 0;
	} else {
		tmp.tv_sec = t1->tv_sec - t2->tv_sec - borrow;
	}

	*t3 = tmp;
}

/* return 0 on event recieved, -1 on timeout */
static int
runMainLoop(int maxtime, int iconscreen)
{
	static int  lastdelay = -1, lastmaxtime = -1;
	int         fd = ConnectionNumber(dsp), r;
	struct timeval sleep_time, first, repeat;
	struct timeval elapsed, tmp;
	fd_set      reads;
	unsigned long started;

	first.tv_sec = first.tv_usec = 0;
	elapsed.tv_sec = elapsed.tv_usec = 0;
	repeat.tv_sec = delay / 1000000;
	repeat.tv_usec = delay % 1000000;

	started = seconds();

	for (;;) {
		if (delay != lastdelay || maxtime != lastmaxtime || onepause) {
			if (!delay || (maxtime && delay / 1000000 > maxtime)) {
				repeat.tv_sec = maxtime;
				repeat.tv_usec = 0;
			} else {
				repeat.tv_sec = delay / 1000000;
				repeat.tv_usec = delay % 1000000;
			}
			if (onepause) {
				first.tv_sec = onepause / 1000000;
				first.tv_usec = onepause % 1000000;
			} else {
				first = repeat;
			}
			lastdelay = delay;
			lastmaxtime = maxtime;
			onepause = 0;
			sleep_time = first;
		} else {
			sleep_time = repeat;
		}

		/* subtract time spent doing last loop iteration */
		sub_timers(&sleep_time, &elapsed, &sleep_time);

		FD_ZERO(&reads);
		FD_SET(fd, &reads);

#if DCE_PASSWD
		r = _select_sys(fd + 1,
			(fd_set *) & reads, (fd_set *) NULL, (fd_set *) NULL,
				(struct timeval *) &sleep_time);
#else
#if defined(__cplusplus) || defined(c_plusplus)
		r = select(fd + 1,
			(fd_set *) & reads, (fd_set *) NULL, (fd_set *) NULL,
			   (struct timeval *) &sleep_time);
#else
		r = select(fd + 1,
			   (void *) &reads, (void *) NULL, (void *) NULL,
			   (struct timeval *) &sleep_time);
#endif
#endif

		if (r == 1)
			return 0;
		if (r > 0 || (r == -1 && errno != EINTR))
			(void) fprintf(stderr,
				       "Unexpected select() return value: %d (errno=%d)\n", r, errno);

		(void) gettimeofday(&tmp, NULL);	/* get time before
							   calling mode proc */

		for (screen = 0; screen < screens; screen++) {
			Window      cbwin;
			Bool        iconic;
			ModeInfo   *mi;

			if (screen == iconscreen) {
				cbwin = icon[screen];
				iconic = True;
			} else {
				cbwin = win[screen];
				iconic = False;
			}
			if (cbwin != None) {
				mi = mode_info(screen, cbwin, iconic);
				call_callback_hook((LockStruct *) NULL, mi);
				if (MI_PAUSE(mi) > onepause)	/* Better on multiscreens to pause */
					onepause = MI_PAUSE(mi);
			}
		}

		XSync(dsp, False);

		/* check for events received during the XSync() */
		if (QLength(dsp)) {
			return 0;
		}
		if (maxtime && ((int) (seconds() - started) > maxtime)) {
			return -1;
		}
		/* if (mindelay) usleep(mindelay); */

		/* get the time now, figure how long it took */
		(void) gettimeofday(&elapsed, NULL);
		sub_timers(&elapsed, &tmp, &elapsed);
	}
}

#endif /* !USE_OLD_EVENT_LOOP */

static int
ReadXString(char *s, int slen)
{
	XEvent      event;
	char        keystr[20];
	char        c;
	int         i;
	int         bp;
	int         len;
	int         thisscreen = screen;
	char        pwbuf[PASSLENGTH];
	int         first_key = 1;

	for (screen = 0; screen < screens; screen++)
		if (thisscreen == screen) {
			call_init_hook((LockStruct *) NULL,
				       mode_info(screen, icon[screen], True));
		} else {
			call_init_hook((LockStruct *) NULL,
				       mode_info(screen, win[screen], False));
		}
	statusUpdate(True, thisscreen);
	bp = 0;
	*s = 0;

	for (;;) {
		unsigned long lasteventtime = seconds();

		while (!XPending(dsp)) {
#ifdef USE_OLD_EVENT_LOOP
			for (screen = 0; screen < screens; screen++)
				if (thisscreen == screen)
					call_callback_hook(NULL, mode_info(screen,
							icon[screen], True));
				else
					call_callback_hook(NULL, mode_info(screen,
							win[screen], False));
			statusUpdate(False, thisscreen);
			XSync(dsp, False);
			(void) usleep(onepause ? onepause : delay);
			onepause = 0;
#else
			statusUpdate(False, thisscreen);
			if (runMainLoop(MIN(timeout, 5), thisscreen) == 0)
				break;
#endif

			if (timeout < (int) (seconds() - lasteventtime)) {
				screen = thisscreen;
				return 1;
			}
		}

		screen = thisscreen;
		XNextEvent(dsp, &event);

		/*
		 * This event handling code should be unified with the
		 * similar code in justDisplay().
		 */
		switch (event.type) {
			case KeyPress:
				len = XLookupString((XKeyEvent *) & event, keystr, 20, NULL, NULL);
				for (i = 0; i < len; i++) {
					c = keystr[i];
					switch (c) {
						case 8:	/* ^H */
						case 127:	/* DEL */
							if (bp > 0)
								bp--;
							break;
						case 10:	/* ^J */
						case 13:	/* ^M */
							if (first_key && usefirst)
								break;
							s[bp] = '\0';
							return 0;
						case 21:	/* ^U */
							bp = 0;
							break;
						default:
							s[bp] = c;
							if (bp < slen - 1)
								bp++;
							else
								XSync(dsp, True);	/* flush input buffer */
					}
				}
				XSetForeground(dsp, Scr[screen].gc, bgcol[screen]);
				if (echokeys) {
					(void) memset((char *) pwbuf, '?', slen);
					XFillRectangle(dsp, win[screen], Scr[screen].gc,
						 passx, passy - font->ascent,
					       XTextWidth(font, pwbuf, slen),
					       font->ascent + font->descent);
					XDrawString(dsp, win[screen], textgc[screen],
						    passx, passy, pwbuf, bp);
				}
				/* eat all events if there are more than
				   enough pending... this keeps the Xlib event
				   buffer from growing larger than all
				   available memory and crashing xlock. */
				if (XPending(dsp) > 100) {	/* 100 is arbitrarily
								   big enough */
					register Status status;

					do {
						status = XCheckMaskEvent(dsp,
									 KeyPressMask | KeyReleaseMask, &event);
					} while (status);
					XBell(dsp, 100);
				}
				break;

#ifdef USE_MOUSE_MOTION
			case MotionNotify:
#endif
			case ButtonPress:
				if (((XButtonEvent *) & event)->window == icon[screen]) {
					return 1;
				}
#ifdef USE_BUTTON_LOGOUT
				if (((XButtonEvent *) & event)->window == button[screen]) {
					XSetFunction(dsp, Scr[screen].gc, GXxor);
					XSetForeground(dsp, Scr[screen].gc, fgcol[screen]);
					XFillRectangle(dsp, button[screen], Scr[screen].gc,
						       0, 0, 500, 100);
					XSync(dsp, False);
					tried_logout = 1;
					logoutUser();
					XSetFunction(dsp, Scr[screen].gc, GXcopy);
				}
#endif
				break;

			case Expose:
				if (event.xexpose.count != 0)
					break;
				/* fall through on last expose event */
			case VisibilityNotify:
				/* next line for -geometry */
				if (event.xvisibility.state != VisibilityUnobscured) {
					/* window was restacked or exposed */
					if (!debug && !inwindow)
						XRaiseWindow(dsp, event.xvisibility.window);
					call_refresh_hook((LockStruct *) NULL,
							  mode_info(screen, win[screen], False));
					s[0] = '\0';
					return 1;
				}
				break;
			case ConfigureNotify:
				/* next line for -geometry */
				if (!fullscreen)
					break;
				/* window config changed */
				if (!debug && !inwindow)
					XRaiseWindow(dsp, event.xconfigure.window);
				if (window_size_changed(screen, win[screen])) {
					call_init_hook((LockStruct *) NULL,
						       mode_info(screen, win[screen], False));
				}
				s[0] = '\0';
				return 1;
			case KeymapNotify:
			case KeyRelease:
			case ButtonRelease:
#ifndef USE_MOUSE_MOTION
			case MotionNotify:
#endif
			case LeaveNotify:
			case EnterNotify:
			case NoExpose:
			case CirculateNotify:
			case DestroyNotify:
			case GravityNotify:
			case MapNotify:
			case ReparentNotify:
			case UnmapNotify:
				break;

			default:
				(void) fprintf(stderr, "%s: unexpected event: %d\n",
					       ProgramName, event.type);
				break;
		}
		first_key = 0;
	}
}

static int
getPassword(void)
{
	XWindowAttributes xgwa;
	int         x, y, left, done, remy;
	char        buffer[PASSLENGTH];
	char      **msgp;

	(void) nice(0);
	if (!fullscreen)
		XConfigureWindow(dsp, win[screen], sizeconfiguremask,
				 &fullsizeconfigure);


	(void) XGetWindowAttributes(dsp, win[screen], &xgwa);

	XChangeGrabbedCursor(XCreateFontCursor(dsp, XC_left_ptr));

	XSetForeground(dsp, Scr[screen].gc, bgcol[screen]);
	XFillRectangle(dsp, win[screen], Scr[screen].gc,
		       0, 0, xgwa.width, xgwa.height);

	XMapWindow(dsp, icon[screen]);
	XRaiseWindow(dsp, icon[screen]);

	x = left = iconx[screen] + iconwidth + font->max_bounds.width;
	y = icony[screen] + font->ascent;

	putText(win[screen], textgc[screen], text_name, True, left, &x, &y);
	putText(win[screen], textgc[screen], user, False, left, &x, &y);
	putText(win[screen], textgc[screen], "\n", False, left, &x, &y);

	putText(win[screen], textgc[screen], text_pass, True, left, &x, &y);
	putText(win[screen], textgc[screen], " ", False, left, &x, &y);

	passx = x;
	passy = y;

	y += font->ascent + font->descent + 2;
	if (y < icony[screen] + iconheight + font->ascent + 2)
		y = icony[screen] + iconheight + font->ascent + 2;
	x = left = iconx[screen];
	putText(win[screen], textgc[screen], text_info, False, left, &x, &y);
	putText(win[screen], textgc[screen], "\n", False, left, &x, &y);

	timex = x;
	timey = y;

#ifdef USE_AUTO_LOGOUT
	if (logoutAuto) {
		y += font->ascent + font->descent + 2;
	}
#endif
#ifdef USE_BUTTON_LOGOUT
	if (enable_button) {
		y += font->ascent + font->descent + 2;
	}
#endif
	if (timeelapsed) {
		y += font->ascent + font->descent + 2;
	}
	remy = y;

	putText(win[screen], textgc[screen], "\n", False, left, &x, &y);

	y = msgy[screen];
	if (*message) {
		for (msgp = message; *msgp; ++msgp) {
			y += msgfont->ascent + msgfont->descent + 2;
			XDrawString(dsp, win[screen], msgtextgc[screen],
				    msgx[screen], y, *msgp, strlen(*msgp));
		}
	}
	XFlush(dsp);

	y = remy;

	done = False;
	while (!done) {
#ifdef USE_SOUND
		if (sound && !got_invalid) {
			play_sound(infosound);
		}
		got_invalid = 0;
#endif
		if (ReadXString(buffer, PASSLENGTH))
			break;

		XSetForeground(dsp, Scr[screen].gc, bgcol[screen]);

		XFillRectangle(dsp, win[screen], Scr[screen].gc,
			       iconx[screen], y - font->ascent,
			XTextWidth(font, text_invalid, strlen(text_invalid)),
			       font->ascent + font->descent + 2);

		XDrawString(dsp, win[screen], textgc[screen],
			    iconx[screen], y, text_valid, strlen(text_valid));
		XFlush(dsp);

		done = checkPasswd(buffer);

		if (!done && !*buffer) {
			/* just hit return, and it was not his password */
			break;
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
		} else if (!done) {
			/* bad password... log it... */
			(void) printf("failed unlock attempt on user %s\n", user);
			syslog(SYSLOG_NOTICE, "%s: failed unlock attempt on user %s\n",
			       ProgramName, user);
#endif
		}
		/* clear plaintext password so you can not grunge around
		   /dev/kmem */
		(void) memset((char *) buffer, 0, sizeof (buffer));

		if (done) {
#ifdef USE_SOUND
			if (sound)
				play_sound(validsound);
#endif
			if (!fullscreen)
				XConfigureWindow(dsp, win[screen], sizeconfiguremask,
						 &minisizeconfigure);
			return 0;
		} else {
			XSync(dsp, True);	/* flush input buffer */
			(void) sleep(1);
			XFillRectangle(dsp, win[screen], Scr[screen].gc,
				       iconx[screen], y - font->ascent,
			    XTextWidth(font, text_valid, strlen(text_valid)),
				       font->ascent + font->descent + 2);
			XDrawString(dsp, win[screen], textgc[screen],
			iconx[screen], y, text_invalid, strlen(text_invalid));
			if (echokeys)	/* erase old echo */
				XFillRectangle(dsp, win[screen], Scr[screen].gc,
					       passx, passy - font->ascent,
					       xgwa.width - passx,
					       font->ascent + font->descent);
#ifdef USE_SOUND
			if (sound)
				play_sound(invalidsound);
			got_invalid = 1;
#endif
		}
	}
	XChangeGrabbedCursor(mycursor);
	XUnmapWindow(dsp, icon[screen]);
#ifdef USE_BUTTON_LOGOUT
	XUnmapWindow(dsp, button[screen]);
#endif
	if (!fullscreen)
		XConfigureWindow(dsp, win[screen], sizeconfiguremask,
				 &minisizeconfigure);
	(void) nice(nicelevel);
	return 1;
}

static int
event_screen(Display * display, Window event_win)
{
	int         i;

	for (i = 0; i < screens; i++) {
		if (event_win == RootWindow(display, i)) {
			return (i);
		}
	}
	return (0);
}

static int
justDisplay(void)
{
	int         timetodie = False;
	int         not_done = True;
	XEvent      event;

	for (screen = 0; screen < screens; screen++) {
		call_init_hook((LockStruct *) NULL, mode_info(screen, win[screen], False));
	}

	while (not_done) {
		while (!XPending(dsp)) {
#ifdef USE_OLD_EVENT_LOOP
			(void) usleep(onepause ? onepause : delay);
			onepause = 0;
			for (screen = 0; screen < screens; screen++)
				call_callback_hook((LockStruct *) NULL,
				      mode_info(screen, win[screen], False));
#ifdef USE_AUTO_LOGOUT
			checkLogout();
#endif
			XSync(dsp, False);
#else /* !USE_OLD_EVENT_LOOP */
#ifdef USE_AUTO_LOGOUT
			if (runMainLoop(30, -1) == 0)
				break;
			checkLogout();
#else
			(void) runMainLoop(0, -1);
#endif
#endif /* !USE_OLD_EVENT_LOOP */
		}
		XNextEvent(dsp, &event);

		/*
		 * This event handling code should be unified with the
		 * similar code in ReadXString().
		 */
		switch (event.type) {
			case Expose:
				if (event.xexpose.count != 0) {
					break;
				}
				/* fall through on last expose event of the series */

			case VisibilityNotify:
				if (!debug && !inwindow) {
					XRaiseWindow(dsp, event.xany.window);
				}
				for (screen = 0; screen < screens; screen++) {
					call_refresh_hook((LockStruct *) NULL,
							  mode_info(screen, win[screen], False));
				}
				break;

			case ConfigureNotify:
				if (!debug && !inwindow)
					XRaiseWindow(dsp, event.xconfigure.window);
				for (screen = 0; screen < screens; screen++) {
					if (window_size_changed(screen, win[screen])) {
						call_init_hook((LockStruct *) NULL,
							       mode_info(screen, win[screen], False));
					}
				}
				break;

			case ButtonPress:
				if (event.xbutton.button == Button2) {
					/* call change hook only for clicked window? */
					for (screen = 0; screen < screens; screen++) {
						call_change_hook((LockStruct *) NULL,
						mode_info(screen, win[screen],
							  False));
					}
				} else {
					screen = event_screen(dsp, event.xbutton.root);
					not_done = False;
				}
				break;

#ifdef USE_MOUSE_MOTION
			case MotionNotify:
				screen = event_screen(dsp, event.xmotion.root);
				not_done = False;
				break;
#endif

			case KeyPress:
				screen = event_screen(dsp, event.xkey.root);
				not_done = False;
				break;
		}

		if (!nolock && lockdelay &&
		    (lockdelay <= (int) (seconds() - start_time))) {
			timetodie = True;
			not_done = False;
		}
	}

	/* KLUDGE SO TVTWM AND VROOT WILL NOT MAKE XLOCK DIE */
	if (screen >= screens)
		screen = 0;

	lockdelay = False;
	if (usefirst)
		XPutBackEvent(dsp, &event);
	return timetodie;
}


static void
sigcatch(int signum)
{
	ModeInfo   *mi = mode_info(0, win[0], False);
	char       *name = (mi == NULL) ? "unknown" : MI_NAME(mi);
	char       *buf;


	finish();
	buf = (char *) malloc(strlen(ProgramName) + strlen(name) + 160);
	(void) sprintf(buf,
		       "Access control list restored.\n%s: caught signal %d while running %s mode (uid %ld).\n",
		       ProgramName, signum, name, (long) getuid());
	error(buf);
	(void) free((void *) buf);	/* Should never get here */
}

static void
lockDisplay(Bool do_display)
{
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	syslogStart();
#endif
#ifdef USE_SOUND
	if (sound && !inwindow && !inroot)
		play_sound(locksound);
#endif
	if (!allowaccess) {
#if defined( SYSV ) || defined( SVR4 )
		sigset_t    oldsigmask;
		sigset_t    newsigmask;

		(void) sigemptyset(&newsigmask);
		(void) sigaddset(&newsigmask, SIGHUP);
		(void) sigaddset(&newsigmask, SIGINT);
		(void) sigaddset(&newsigmask, SIGQUIT);
		(void) sigaddset(&newsigmask, SIGTERM);
		(void) sigprocmask(SIG_BLOCK, &newsigmask, &oldsigmask);
#else
		int         oldsigmask;

#ifndef VMS
		extern int  sigblock(int);
		extern int  sigsetmask(int);

		oldsigmask = sigblock(sigmask(SIGHUP) |
				      sigmask(SIGINT) |
				      sigmask(SIGQUIT) |
				      sigmask(SIGTERM));
#endif
#endif

		(void) signal(SIGHUP, (void (*)(int)) sigcatch);
		(void) signal(SIGINT, (void (*)(int)) sigcatch);
		(void) signal(SIGQUIT, (void (*)(int)) sigcatch);
		(void) signal(SIGTERM, (void (*)(int)) sigcatch);
		/* we should trap ALL signals, especially the deadly ones */
		(void) signal(SIGSEGV, (void (*)(int)) sigcatch);
		(void) signal(SIGBUS, (void (*)(int)) sigcatch);
		(void) signal(SIGFPE, (void (*)(int)) sigcatch);

		if (grabserver)
			XGrabServer(dsp);
		XGrabHosts(dsp);

#if defined( SYSV ) || defined( SVR4 )
		(void) sigprocmask(SIG_SETMASK, &oldsigmask, &oldsigmask);
#else
		(void) sigsetmask(oldsigmask);
#endif
	}
#if defined( __hpux ) || defined( __apollo )
	XHPDisableReset(dsp);
#endif
	do {
		if (do_display)
			(void) justDisplay();
		else
			do_display = True;
	} while (getPassword());
#if defined( __hpux ) || defined( __apollo )
	XHPEnableReset(dsp);
#endif
}

static void
read_message()
{
	FILE       *msgf = NULL;
	char        buf[121];
	char       *home = getenv("HOME");
	char       *buffer;
	int         i, j, cr;

	if (!home)
		home = "";

#if ( __VMS_VER >= 70000000 )
	buffer = (char *) malloc(255);

#else
	buffer = (char *) malloc(strlen(home) + 32);

#endif

#ifdef VMS
	(void) sprintf(buffer, "%s%s", home, ".xlockmessage");
#else
	(void) sprintf(buffer, "%s/%s", home, ".xlockmessage");
#endif
	msgf = my_fopen(buffer, "r");
	if (msgf == NULL) {
#ifdef VMS
		(void) sprintf(buffer, "%s%s", home, ".plan");
#else
		(void) sprintf(buffer, "%s/%s", home, ".plan");
#endif
		msgf = my_fopen(buffer, "r");
	}
	if (msgf == NULL) {
#ifndef VMS
		(void) sprintf(buffer, "%s/%s", home, ".signature");
#else
#if ( __VMS_VER >= 70000000 )
/* Get signature file for VMS 7.0 and higher */
		char       *buffer1 = (char *) malloc(256);
		unsigned int ival;
		unsigned long int mail_context = 0;
		unsigned short int buflen, buflen1;
		struct itmlst_3 item[2], itm_d[1];

		itm_d[0].buflen = 0;
		itm_d[0].itmcode = 0;
		item[0].buflen = 255;
		item[0].itmcode = MAIL$_USER_SIGFILE;
		item[0].bufadr = buffer1;
		item[0].retlen = &buflen1;
		item[1].buflen = 255;
		item[1].itmcode = MAIL$_USER_FULL_DIRECTORY;
		item[1].bufadr = buffer;
		item[1].retlen = &buflen;
		item[2].buflen = 0;
		item[2].itmcode = 0;
		ival = mail$user_begin(&mail_context, itm_d, item);
		(void) mail$user_end(&mail_context, itm_d, itm_d);
		(void) strncat(buffer, buffer1, buflen1);
		(void) free((void *) buffer1);
#else
		(void) sprintf(buffer, "%s%s", home, ".signature");
#endif
#endif
		msgf = my_fopen(buffer, "r");
	}
	if (msgf != NULL) {
		for (i = 0; i < MESSAGELINES; i++) {
			if (fgets(buf, 120, msgf)) {
				cr = strlen(buf) - 1;
				if (buf[cr] == '\n') {
					buf[cr] = '\0';
				}
				/* this expands tabs to 8 spaces */
				for (j = 0; j < cr; j++) {
					if (buf[j] == '\t') {
						int         k, tab = 8 - (j % 8);

						for (k = 120 - tab; k > j; k--) {
							buf[k + tab - 1] = buf[k];
						}
						for (k = j; k < j + tab; k++) {
							buf[k] = ' ';
						}
						cr += tab;
						if (cr > 120)
							cr = 120;
					}
				}
				buf[cr] = '\0';

				message[i] = (char *) malloc(strlen(buf) + 1);
				(void) strcpy(message[i], buf);
			}
		}
		message[i] = NULL;
		(void) fclose(msgf);
	} else {
		message[0] = NULL;
	}
	(void) free((void *) buffer);
	buffer = NULL;
}

int
main(int argc, char **argv)
{
	XSetWindowAttributes xswa;
	XGCValues   xgcv;
	XColor      nullcolor;
	char      **msgp;
	char       *buf;
	int         tmp;
	uid_t       ruid;

#if ultrix
	extern gid_t rgid;

#else
	gid_t       rgid;

#endif
#if defined( HAVE_SETEUID ) || defined( HAVE_SETREUID )
	uid_t       euid;

#if ultrix
	extern gid_t egid;

#else
	gid_t       egid;

#endif
#endif

#ifdef OSF1_ENH_SEC
	set_auth_parameters(argc, argv);
#endif

	ruid = getuid();
	rgid = getgid();
#ifdef HAVE_SETEUID
	/* save effective uid and gid for later */
	euid = geteuid();
	egid = getegid();

	/* revoke root privs temporarily, to get the correct .Xauthority */
	(void) setegid(rgid);
	(void) seteuid(ruid);
#else
#ifdef HAVE_SETREUID
	/* save effective uid and gid for later */
	euid = geteuid();
	egid = getegid();

	/* revoke root privs temporarily, to get the correct .Xauthority */
	(void) setregid(egid, rgid);
	(void) setreuid(euid, ruid);

#endif
#endif

	ProgramName = strrchr(argv[0], '/');
	if (ProgramName)
		ProgramName++;
	else
		ProgramName = argv[0];

	start_time = seconds();
	ProgramPID = getpid();	/* for memcheck.c not supplied */
#if 1
	SRAND((long) start_time);	/* random mode needs the seed set. */
#else
	SRAND((long) ProgramPID);
#endif

	getResources(argc, argv);

#ifdef HAVE_SETEUID
	/* become root to get the password */
	(void) setegid(egid);
	(void) seteuid(euid);
#else
#ifdef HAVE_SETREUID
	/* become root to get the password */
	(void) setregid(rgid, egid);
	(void) setreuid(ruid, euid);

#endif
#endif

	initPasswd();

/* revoke root privs, if there were any */
#ifdef ultrix
/*-
 * Potential security problem on ultrix
 * Here's the problem.  Later on you need setgid to a root group to
 * use the crypt program.  I do not want to keep the password in memory.
 * That means other parts will be running setgid as well.
 */

#if 1				/* Change 1 to 0 */
	UNTESTED    CODE, COMMENT OUT AND SEE IF IT WORKS, PLEASE GET BACK TO ME
#endif

#ifdef HAVE_SETEUID
	/* Lets try to dampen it a bit */
	            (void) setegid(rgid);

#else
#ifdef HAVE_SETREUID
	            (void) setregid(egid, rgid);

#else
	            (void) setgid(rgid);	/* Forget it */

#endif
#endif
#else
	(void) setgid(rgid);
#endif
	(void) setuid(ruid);

	checkResources();

	font = XLoadQueryFont(dsp, fontname);
	if (font == NULL) {
		(void) fprintf(stderr, "%s: can not find font: %s, using %s...\n",
			       ProgramName, fontname, FALLBACK_FONTNAME);
		font = XLoadQueryFont(dsp, FALLBACK_FONTNAME);
		if (font == NULL) {
			buf = (char *) malloc(strlen(ProgramName) +
					      strlen(FALLBACK_FONTNAME) + 80);
			(void) sprintf(buf,
				       "%s: can not even find %s!!!\n", ProgramName, FALLBACK_FONTNAME);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	} {
		int         flags, x, y;
		unsigned int w, h;

		if (*icongeometry == '\0') {
			iconwidth = DEF_ICONW;
			iconheight = DEF_ICONH;
		} else {
			flags = XParseGeometry(icongeometry, &x, &y, &w, &h);
			iconwidth = flags & WidthValue ? w : DEF_ICONW;
			iconheight = flags & HeightValue ? h : DEF_ICONH;
			if (iconwidth < MINICONW)
				iconwidth = MINICONW;
			else if (iconwidth > MAXICONW)
				iconwidth = MAXICONW;
			if (iconheight < MINICONH)
				iconheight = MINICONH;
			else if (iconheight > MAXICONH)
				iconheight = MAXICONH;
		}
		if (*geometry == '\0') {
			fullscreen = True;
		} else {
			flags = XParseGeometry(geometry, &x, &y, &w, &h);
			if (w < MINICONW)
				w = MINICONW;
			if (h < MINICONH)
				h = MINICONH;
			minisizeconfigure.x = flags & XValue ? x : 0;
			minisizeconfigure.y = flags & YValue ? y : 0;
			minisizeconfigure.width = flags & WidthValue ? w : iconwidth;
			minisizeconfigure.height = flags & HeightValue ? h : iconheight;
		}
	}

	msgfont = XLoadQueryFont(dsp, messagefont);
	if (msgfont == NULL) {
		(void) fprintf(stderr, "%s: can't find font: %s, using %s...\n",
			       ProgramName, messagefont, FALLBACK_FONTNAME);
		msgfont = XLoadQueryFont(dsp, FALLBACK_FONTNAME);
		if (msgfont == NULL) {
			buf = (char *) malloc(strlen(ProgramName) +
					      strlen(FALLBACK_FONTNAME) + 80);
			(void) sprintf(buf,
				       "%s: can not even find %s!!!\n", ProgramName, FALLBACK_FONTNAME);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	}
	read_message();

	screens = ScreenCount(dsp);

#ifdef FORCESINGLE
	screens = 1;
#endif
	if (screens > MAXSCREENS) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: can only support %d screens.\n", ProgramName, MAXSCREENS);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
#ifdef USE_DTSAVER
	/* The CDE Session Manager provides the windows for the screen saver
	   to draw into. */
	if (dtsaver) {
		Window     *saver_wins;
		int         num_wins;
		int         this_win;
		int         this_screen;
		XWindowAttributes xgwa;

		for (this_screen = 0; this_screen < screens; this_screen++)
			win[this_screen] = None;

		/* Get the list of requested windows */
		if (!DtSaverGetWindows(dsp, &saver_wins, &num_wins)) {
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf,
				       "%s: Unable to get screen saver info.\n", ProgramName);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
		for (this_win = 0; this_win < num_wins; this_win++) {
			(void) XGetWindowAttributes(dsp, saver_wins[this_win], &xgwa);
			this_screen = XScreenNumberOfScreen(xgwa.screen);
			if (win[this_screen] != None) {
				buf = (char *) malloc(strlen(ProgramName) + 80);
				(void) sprintf(buf,
					       "%s: Two windows on screen %d\n", ProgramName, this_screen);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			}
			win[this_screen] = saver_wins[this_win];
		}

		/* Reduce to the last screen requested */
		for (this_screen = screens - 1; this_screen >= 0; this_screen--) {
			if (win[this_screen] != None)
				break;
			screens--;
		}
	}
#endif

	for (screen = 0; screen < screens; screen++) {
		Screen     *scr = ScreenOfDisplay(dsp, screen);
		Colormap    cmap = DefaultColormapOfScreen(scr);

		root[screen] = RootWindowOfScreen(scr);
		fgcol[screen] = allocPixel(dsp, cmap, foreground, "Black");
		bgcol[screen] = allocPixel(dsp, cmap, background, "White");
		if (use3d) {
			XColor      C;

#ifdef CALCULATE_BOTH
			XColor      C2;

#endif
			unsigned long planemasks[10];
			unsigned long pixels[10];

			if (!install || !XAllocColorCells(dsp, cmap, False, planemasks, 2, pixels,
							  1)) {
				/* did not get the needed colours.  Use normal 3d view without */
				/* color overlapping */
				nonecol[screen] = allocPixel(dsp, cmap, none3d, DEF_NONE3D);
				rightcol[screen] = allocPixel(dsp, cmap, right3d, DEF_RIGHT3D);
				leftcol[screen] = allocPixel(dsp, cmap, left3d, DEF_LEFT3D);
				bothcol[screen] = allocPixel(dsp, cmap, both3d, DEF_BOTH3D);

			} else {
				/*
				   * attention: the mixture of colours will only be guaranteed, if
				   * the right black is used.  The problems with BlackPixel would
				   * be that BlackPixel | leftcol need not be equal to leftcol.  The
				   * same holds for rightcol (of course). That's why the right black
				   * (black3dcol) must be used when GXor is used as put function.
				   * I have allocated four colors above:
				   * pixels[0],                                - 3d black
				   * pixels[0] | planemasks[0],                - 3d right eye color
				   * pixels[0] | planemasks[1],                - 3d left eye color
				   * pixels[0] | planemasks[0] | planemasks[1] - 3d white
				 */

				if (!XParseColor(dsp, cmap, none3d, &C))
					XParseColor(dsp, cmap, DEF_NONE3D, &C);
				nonecol[screen] = C.pixel = pixels[0];
				XStoreColor(dsp, cmap, &C);

				if (!XParseColor(dsp, cmap, right3d, &C))
					XParseColor(dsp, cmap, DEF_RIGHT3D, &C);
				rightcol[screen] = C.pixel = pixels[0] | planemasks[0];
				XStoreColor(dsp, cmap, &C);

#ifdef CALCULATE_BOTH
				C2.red = C.red;
				C2.green = C.green;
				C2.blue = C.blue;
#else
				if (!XParseColor(dsp, cmap, left3d, &C))
					XParseColor(dsp, cmap, DEF_LEFT3D, &C);
#endif
				leftcol[screen] = C.pixel = pixels[0] | planemasks[1];
				XStoreColor(dsp, cmap, &C);

#ifdef CALCULATE_BOTH
				C.red |= C2.red;	/* or them together... */
				C.green |= C2.green;
				C.blue |= C2.blue;
#else
				if (!XParseColor(dsp, cmap, both3d, &C))
					XParseColor(dsp, cmap, DEF_BOTH3D, &C);
#endif
				bothcol[screen] = C.pixel = pixels[0] | planemasks[0] | planemasks[1];
				XStoreColor(dsp, cmap, &C);
			}

		}
		Scr[screen].bgcol = bgcol[screen];
		Scr[screen].fgcol = fgcol[screen];
		Scr[screen].nonecol = nonecol[screen];
		Scr[screen].rightcol = rightcol[screen];
		Scr[screen].leftcol = leftcol[screen];
		Scr[screen].bothcol = bothcol[screen];
		Scr[screen].cmap = None;

		xswa.override_redirect = True;
		xswa.background_pixel = BlackPixel(dsp, screen);
		xswa.border_pixel = WhitePixel(dsp, screen);
		xswa.event_mask = KeyPressMask | ButtonPressMask |
#ifdef USE_MOUSE_MOTION
			MotionNotify |
#endif
			VisibilityChangeMask | ExposureMask | StructureNotifyMask;
#define WIDTH (inwindow? WidthOfScreen(scr)/2 : (debug? WidthOfScreen(scr) - 100 : WidthOfScreen(scr)))
#define HEIGHT (inwindow? HeightOfScreen(scr)/2 : (debug? HeightOfScreen(scr) - 100 : HeightOfScreen(scr)))
#if defined( USE_GL ) && !defined( MESA )
#define CWMASK (((debug||inwindow||inroot)? 0 : CWOverrideRedirect) | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask)
#else
#define CWMASK (((debug||inwindow||inroot)? 0 : CWOverrideRedirect) | CWBackPixel | CWBorderPixel | CWEventMask)
#endif
#ifdef USE_VROOT
		if (inroot) {
			win[screen] = root[screen];
			XChangeWindowAttributes(dsp, win[screen], CWBackPixel, &xswa);
			/* this gives us these events from the root window */
			XSelectInput(dsp, win[screen], VisibilityChangeMask | ExposureMask);
		} else
#endif
#ifdef USE_DTSAVER
		if (!dtsaver)
#endif
		{
#if defined( USE_GL ) && !defined( MESA )
			XVisualInfo *gotVis;

			if (!(gotVis = getGLVisual(dsp, screen, NULL, 0))) {
				if (!(gotVis = getGLVisual(dsp, screen, NULL, 1))) {
					(void) fprintf(stderr, "GL can not render with root visual\n");
					return (1);
				}
			}
			xswa.colormap = XCreateColormap(dsp, root[screen], gotVis->visual, AllocNone);
			Scr[screen].cmap = xswa.colormap;

#define XLOCKWIN_DEPTH  (gotVis->depth)
#define XLOCKWIN_VISUAL (gotVis->visual)
#else
#define XLOCKWIN_DEPTH  CopyFromParent
#define XLOCKWIN_VISUAL CopyFromParent
#endif

			if (fullscreen) {
				win[screen] = XCreateWindow(dsp, root[screen], 0, 0,
							    (unsigned int) WIDTH, (unsigned int) HEIGHT, 0,
				XLOCKWIN_DEPTH, InputOutput, XLOCKWIN_VISUAL,
							    CWMASK, &xswa);
			} else {
				sizeconfiguremask = CWX | CWY | CWWidth | CWHeight;
				fullsizeconfigure.x = 0;
				fullsizeconfigure.y = 0;
				fullsizeconfigure.width = WIDTH;
				fullsizeconfigure.height = HEIGHT;
				win[screen] = XCreateWindow(dsp, root[screen],
						   (int) minisizeconfigure.x,
						   (int) minisizeconfigure.y,
				      (unsigned int) minisizeconfigure.width,
				     (unsigned int) minisizeconfigure.height,
							    0, XLOCKWIN_DEPTH, InputOutput, XLOCKWIN_VISUAL,
							    CWMASK, &xswa);
			}
		}
		if (debug || inwindow) {
			XWMHints    xwmh;

			xwmh.flags = InputHint;
			xwmh.input = True;
			XChangeProperty(dsp, win[screen],
			       XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
					(unsigned char *) &xwmh, sizeof (xwmh) / sizeof (int));
		}
		/*if (mono || CellsOfScreen(scr) <= 2) {
		   Scr[screen].pixels[0] = fgcol[screen];
		   Scr[screen].pixels[1] = bgcol[screen];
		   Scr[screen].npixels = 2;
		   } else */
		fixColormap(dsp, win[screen], screen, saturation,
			    mono, install, inroot, inwindow, verbose);

		if (debug) {
			iconx[screen] = (DisplayWidth(dsp, screen) - 100 -
					 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
			icony[screen] = (DisplayHeight(dsp, screen) - 100) / 6;
		} else {
			iconx[screen] = (DisplayWidth(dsp, screen) -
					 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
			icony[screen] = DisplayHeight(dsp, screen) / 6;
		}


		msgx[screen] = msgy[screen] = 0;
		for (msgp = message; *msgp; ++msgp) {
			tmp = XTextWidth(msgfont, *msgp, strlen(*msgp));
			if (tmp > msgx[screen])
				msgx[screen] = tmp;
			++msgy[screen];
		}
		if (debug) {
			msgx[screen] = (DisplayWidth(dsp, screen) - 100 - msgx[screen]) / 2;
			msgy[screen] = DisplayHeight(dsp, screen) - 100 -
				(msgy[screen] + 4) * (msgfont->ascent + msgfont->descent + 2);
		} else {
			msgx[screen] = (DisplayWidth(dsp, screen) - msgx[screen]) / 2;
			msgy[screen] = DisplayHeight(dsp, screen) -
				(msgy[screen] + 4) * (msgfont->ascent + msgfont->descent + 2);
		}

		xswa.border_pixel = WhitePixel(dsp, screen);
		xswa.background_pixel = BlackPixel(dsp, screen);
		xswa.event_mask = ButtonPressMask;
#if defined( USE_GL ) && !defined( MESA )
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask | CWColormap
#else
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask
#endif
		if (nolock)
			icon[screen] = None;
		else
			icon[screen] = XCreateWindow(dsp, win[screen],
						iconx[screen], icony[screen],
				    iconwidth, iconheight, 1, CopyFromParent,
						 InputOutput, CopyFromParent,
						     CIMASK, &xswa);

#ifdef USE_BUTTON_LOGOUT
		{
			char        buf[1024];
			int         w, h;

			(void) sprintf(buf, " %s ", logoutButtonLabel);
			w = XTextWidth(font, buf, strlen(buf));
			h = font->ascent + font->descent + 2;
			button[screen] = XCreateWindow(dsp, win[screen],
					       0, 0, w, h, 1, CopyFromParent,
						 InputOutput, CopyFromParent,
						       CIMASK, &xswa);
		}
#endif
		XMapWindow(dsp, win[screen]);
		XRaiseWindow(dsp, win[screen]);

#if 0
		if (install && cmap != DefaultColormapOfScreen(scr))
			setColormap(dsp, win[screen], cmap, inwindow);
#endif

		xgcv.font = font->fid;
		xgcv.foreground = WhitePixel(dsp, screen);
		xgcv.background = BlackPixel(dsp, screen);
		Scr[screen].gc = XCreateGC(dsp, win[screen],
				GCFont | GCForeground | GCBackground, &xgcv);

		xgcv.foreground = fgcol[screen];
		xgcv.background = bgcol[screen];
		textgc[screen] = XCreateGC(dsp, win[screen],
				GCFont | GCForeground | GCBackground, &xgcv);
		xgcv.font = msgfont->fid;
		msgtextgc[screen] = XCreateGC(dsp, win[screen],
				GCFont | GCForeground | GCBackground, &xgcv);
	}
	lockc = XCreateBitmapFromData(dsp, root[0], no_bits, 1, 1);
	lockm = XCreateBitmapFromData(dsp, root[0], no_bits, 1, 1);
	mycursor = XCreatePixmapCursor(dsp, lockc, lockm,
				       &nullcolor, &nullcolor, 0, 0);
	XFreePixmap(dsp, lockc);
	XFreePixmap(dsp, lockm);

	if (!grabmouse || inwindow || inroot)
		nolock = 1;
	else if (!debug)
		GrabKeyboardAndMouse();

	if (!enablesaver && !nolock) {
		XGetScreenSaver(dsp, &sstimeout, &ssinterval,
				&ssblanking, &ssexposures);
		XResetScreenSaver(dsp);		/* make sure not blank now */
		XSetScreenSaver(dsp, 0, 0, 0, 0);	/* disable screen saver */
	}
	(void) nice(nicelevel);

	(void) XSetIOErrorHandler(xio_error);

	if (nolock) {
		(void) justDisplay();
	} else if (lockdelay) {
		if (justDisplay()) {
			lockDisplay(False);
		} else
			nolock = 1;
	} else {
		lockDisplay(True);
	}
	finish();
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	if (!nolock) {
		syslogStop();
		closelog();
	}
#endif

#ifdef VMS
	return 1;
#else
	return 0;
#endif
}
