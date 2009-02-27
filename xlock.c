#ifndef lint
static char sccsid[] = "@(#)xlock.c	23.21 91/06/27 XLOCK";
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
 * Changes of David Bagley <bagleyd@source.asset.com>
 * 11-Jul-94: added -inwindow option from Greg Bowering
 *            <greg@cs.adelaide.edu.au>
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 10-Jun-94: patch for BSD from Victor Langeveld <vic@mbfys.kun.nl>
 * 02-May-94: patched to work on Linux from Darren Senn's
 *            (sinster@scintilla.capitola.ca.us) xlock for Linux.
 *            Took out "bounce" since it was too buggy (maybe I will put
 *            it back later).
 * 21-Mar-94: patch to to trap Shift-Ctrl-Reset courtesy of Jamie Zawinski
 *            <jwz@lucid.com>, patched the patch (my mistake) for AIXV3 and
 *            HP from <R.K.Lloyd@csc.liv.ac.uk>.
 * 01-Dec-93: added patch for AIXV3 from
 *            (Tom McConnell, tmcconne@sedona.intel.com) also added a patch
 *            for HP-UX 8.0.
 * 29-Jul-93: "hyper", "helix", "rock", and "blot" (also tips on "maze") I
 *            got courtesy of Jamie Zawinski <jwz@lucid.com>;
 *            at the time I couldn't get his stuff to work for the hpux 8.0,
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
 *	      added -screensaver (don't disable screen saver) for the paranoid.
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
 *	      Added -saver option. (just do display... don't lock.)
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

#include <stdio.h>
#include <signal.h>
#include <string.h>
#ifndef VMS
#include <pwd.h>
#endif
#ifdef __FreeBSD__
#include <sys/types.h>
#include <stdlib.h>
#endif

#include "xlock.h"
#include <X11/cursorfont.h>
#include <X11/Xatom.h>

extern char *crypt();
extern char *getenv();

char       *ProgramName;	/* argv[0] */
perscreen   Scr[MAXSCREENS];
Display    *dsp = NULL;		/* server display connection */
int         screen;		/* current screen */
void        (*callback) () = NULL;
void        (*init) () = NULL;

static int  screens;		/* number of screens */
static Window win[MAXSCREENS];	/* window used to cover screen */
static Window icon[MAXSCREENS];	/* window used during password typein */
static Window root[MAXSCREENS];	/* convenience pointer to the root window */
static GC   textgc[MAXSCREENS];	/* graphics context used for text rendering */
static long fgcol[MAXSCREENS];	/* used for text rendering */
static long bgcol[MAXSCREENS];	/* background of text screen */
static int  iconx[MAXSCREENS];	/* location of left edge of icon */
static int  icony[MAXSCREENS];	/* location of top edge of icon */
static Cursor mycursor;		/* blank cursor */
static Pixmap lockc;
static Pixmap lockm;		/* pixmaps for cursor and mask */
static char no_bits[] = {0};	/* dummy array for the blank cursor */
static int  passx;		/* position of the ?'s */
static int  passy;
static XFontStruct *font;
static int  sstimeout;		/* screen saver parameters */
static int  ssinterval;
static int  ssblanking;
static int  ssexposures;

#define PASSLENGTH 20
#define FALLBACK_FONTNAME	"fixed"
#define ICONW			64
#define ICONH			64

#define AllPointerEventMask \
	(ButtonPressMask | ButtonReleaseMask | \
	EnterWindowMask | LeaveWindowMask | \
	PointerMotionMask | PointerMotionHintMask | \
	Button1MotionMask | Button2MotionMask | \
	Button3MotionMask | Button4MotionMask | \
	Button5MotionMask | ButtonMotionMask | \
	KeymapStateMask)


/* VARARGS1 */
void
error(s1, s2)
    char       *s1, *s2;
{
    (void) fprintf(stderr, s1, ProgramName, s2);
    exit(1);
}

/*
 * Server access control support.
 */

static XHostAddress *XHosts;	/* the list of "friendly" client machines */
static int  HostAccessCount;	/* the number of machines in XHosts */
static Bool HostAccessState;	/* whether or not we even look at the list */

static void
XGrabHosts(display)
    Display    *display;
{
    XHosts = XListHosts(display, &HostAccessCount, &HostAccessState);
    if (XHosts)
	XRemoveHosts(display, XHosts, HostAccessCount);
    XEnableAccessControl(display);
}

static void
XUngrabHosts(display)
    Display    *display;
{
    if (XHosts) {
	XAddHosts(display, XHosts, HostAccessCount);
	XFree((char *) XHosts);
    }
    if (HostAccessState == False)
	XDisableAccessControl(display);
}


/*
 * Simple wrapper to get an asynchronous grab on the keyboard and mouse.
 * If either grab fails, we sleep for one second and try again since some
 * window manager might have had the mouse grabbed to drive the menu choice
 * that picked "Lock Screen..".  If either one fails the second time we print
 * an error message and exit.
 */
static void
GrabKeyboardAndMouse()
{
    Status      status;

    status = XGrabKeyboard(dsp, win[0], True,
			   GrabModeAsync, GrabModeAsync, CurrentTime);
    if (status != GrabSuccess) {
	(void) sleep(1);
	status = XGrabKeyboard(dsp, win[0], True,
			       GrabModeAsync, GrabModeAsync, CurrentTime);

	if (status != GrabSuccess)
	    error("%s: couldn't grab keyboard! (%d)\n", status);
    }
    status = XGrabPointer(dsp, win[0], True, AllPointerEventMask,
			  GrabModeAsync, GrabModeAsync, None, mycursor,
			  CurrentTime);
    if (status != GrabSuccess) {
	(void) sleep(1);
	status = XGrabPointer(dsp, win[0], True, AllPointerEventMask,
			      GrabModeAsync, GrabModeAsync, None, mycursor,
			      CurrentTime);

	if (status != GrabSuccess)
	    error("%s: couldn't grab pointer! (%d)\n", status);
    }
}


/*
 * Assuming that we already have an asynch grab on the pointer,
 * just grab it again with a new cursor shape and ignore the return code.
 */
static void
XChangeGrabbedCursor(cursor)
    Cursor      cursor;
{
#ifndef DEBUG
    if (!inwindow)
      (void) XGrabPointer(dsp, win[0], True, AllPointerEventMask,
		    GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
#endif
}


/*
 * Restore all grabs, reset screensaver, restore colormap, close connection.
 */
static void
finish()
{
    XSync(dsp, False);
    if (!nolock && !allowaccess)
	XUngrabHosts(dsp);
    XUngrabPointer(dsp, CurrentTime);
    XUngrabKeyboard(dsp, CurrentTime);
    if (!enablesaver)
	XSetScreenSaver(dsp, sstimeout, ssinterval, ssblanking, ssexposures);
    XFlush(dsp);
    XCloseDisplay(dsp);
}


static int
ReadXString(s, slen)
    char       *s;
    int         slen;
{
    XEvent      event;
    char        keystr[20];
    char        c;
    int         i;
    int         bp;
    int         len;
    int         thisscreen = screen;
    char        pwbuf[PASSLENGTH];

    for (screen = 0; screen < screens; screen++)
	if (thisscreen == screen)
	    init(icon[screen]);
	else
	    init(win[screen]);
    bp = 0;
    *s = 0;
    while (True) {
	unsigned long lasteventtime = seconds();
	while (!XPending(dsp)) {
	    for (screen = 0; screen < screens; screen++)
		if (thisscreen == screen)
		    callback(icon[screen]);
		else
		    callback(win[screen]);
	    XFlush(dsp);
	    (void) usleep(delay);
	    if (seconds() - lasteventtime > timeout) {
		screen = thisscreen;
		return 1;
	    }
	}
	screen = thisscreen;
	XNextEvent(dsp, &event);
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
		(void) memset(pwbuf, '?', slen);
		XFillRectangle(dsp, win[screen], Scr[screen].gc,
			       passx, passy - font->ascent,
			       XTextWidth(font, pwbuf, slen),
			       font->ascent + font->descent);
		XDrawString(dsp, win[screen], textgc[screen],
			    passx, passy, pwbuf, bp);
	    }
	    /*
	     * eat all events if there are more than enough pending... this
	     * keeps the Xlib event buffer from growing larger than all
	     * available memory and crashing xlock.
	     */
	    if (XPending(dsp) > 100) {	/* 100 is arbitrarily big enough */
		register Status status;
		do {
		    status = XCheckMaskEvent(dsp,
				      KeyPressMask | KeyReleaseMask, &event);
		} while (status);
		XBell(dsp, 100);
	    }
	    break;

	case ButtonPress:
	    if (((XButtonEvent *) & event)->window == icon[screen]) {
		return 1;
	    }
	    break;

	case VisibilityNotify:
	    if (event.xvisibility.state != VisibilityUnobscured) {
#ifndef DEBUG
                if (!inwindow)
		  XRaiseWindow(dsp, win[screen]);
#endif
		s[0] = '\0';
		return 1;
	    }
	    break;

	case KeymapNotify:
	case KeyRelease:
	case ButtonRelease:
	case MotionNotify:
	case LeaveNotify:
	case EnterNotify:
	case NoExpose:
	    break;

	default:
	    (void) fprintf(stderr, "%s: unexpected event: %d\n",
		     ProgramName, event.type);
	    break;
	}
    }
}



static int
getPassword()
{
#ifdef VMS
#include <uaidef.h>
#endif

    char        buffer[PASSLENGTH];
    char        userpass[PASSLENGTH];
    char        rootpass[PASSLENGTH];
    char       *user;
    XWindowAttributes xgwa;
    int         y, left, done;
#ifdef VMS
    char *root = "SYSTEM";
 
    struct itmlst {
      short buflen;
      short code;
      long  addr;
      long  retadr;
    };
    struct itmlst il[4];
 
    short uai_salt,root_salt;
    char hash_password[9], hash_system[9],root_password[9],root_system[9];
    char uai_encrypt,root_encrypt;
 
    struct ascid {
      short  len;
      char dtype;
      char class;
      char *addr;
    };
 
    struct ascid username, password, rootuser;

    user = cuserid(NULL);
    il[0].buflen = 2;
    il[0].code   = UAI$_SALT;
    il[0].addr   = &uai_salt;
    il[0].retadr = 0;
    il[1].buflen = 8;
    il[1].code   = UAI$_PWD;
    il[1].addr   = &hash_password;
    il[1].retadr = 0;
    il[2].buflen = 1;
    il[2].code   = UAI$_ENCRYPT;
    il[2].addr   = &uai_encrypt;
    il[2].retadr = 0;
    il[3].buflen = 0;
    il[3].code   = 0;
    il[3].addr   = 0;
    il[3].retadr = 0;
 
    username.len   = strlen(user);
    username.dtype = 0;
    username.class = 0;
    username.addr  = user;
 
    sys$getuai(0,0,&username,&il,0,0,0);
 
    il[0].buflen = 2;
    il[0].code   = UAI$_SALT;
    il[0].addr   = &root_salt;
    il[0].retadr = 0;
    il[1].buflen = 8;
    il[1].code   = UAI$_PWD;
    il[1].addr   = &root_password;
    il[1].retadr = 0;
    il[2].buflen = 1;
    il[2].code   = UAI$_ENCRYPT;
    il[2].addr   = &root_encrypt;
    il[2].retadr = 0;
    il[3].buflen = 0;
    il[3].code   = 0;
    il[3].addr   = 0;
    il[3].retadr = 0;
 
    rootuser.len   = strlen(root);
    rootuser.dtype = 0;
    rootuser.class = 0;
    rootuser.addr  = root;
 
    sys$getuai(0,0,&rootuser,&il,0,0,0);
#else
    struct passwd *pw;

    pw = (struct passwd *)getpwnam("root");
    (void) strcpy(rootpass, pw->pw_passwd);

#ifdef SOLARIS_SHADOW
    pw = (struct passwd *)getpwnam(cuserid(NULL));
#else
    pw = (struct passwd *)getpwuid(getuid());
#endif
#if defined(HAS_SHADOW) && defined(linux)
   {
     struct spwd *spw;

     if ((spw = getspnam(pw->pw_name)) == NULL)
       pw->pw_passwd = "";
     else
       pw->pw_passwd = spw->sp_pwdp;
    }
#endif
    (void) strcpy(userpass, pw->pw_passwd);

    user = pw->pw_name;
#endif

    XGetWindowAttributes(dsp, win[screen], &xgwa);

    XChangeGrabbedCursor(XCreateFontCursor(dsp, XC_left_ptr));

    XSetForeground(dsp, Scr[screen].gc, bgcol[screen]);
    XFillRectangle(dsp, win[screen], Scr[screen].gc,
		   0, 0, xgwa.width, xgwa.height);

    XMapWindow(dsp, icon[screen]);
    XRaiseWindow(dsp, icon[screen]);

    left = iconx[screen] + ICONW + font->max_bounds.width;
    y = icony[screen] + font->ascent;

    XDrawString(dsp, win[screen], textgc[screen],
		left, y, text_name, strlen(text_name));
    XDrawString(dsp, win[screen], textgc[screen],
		left + 1, y, text_name, strlen(text_name));
    XDrawString(dsp, win[screen], textgc[screen],
		left + XTextWidth(font, text_name, strlen(text_name)), y,
		user, strlen(user));

    y += font->ascent + font->descent + 2;
    XDrawString(dsp, win[screen], textgc[screen],
		left, y, text_pass, strlen(text_pass));
    XDrawString(dsp, win[screen], textgc[screen],
		left + 1, y, text_pass, strlen(text_pass));

    passx = left + 1 + XTextWidth(font, text_pass, strlen(text_pass))
	+ XTextWidth(font, " ", 1);
    passy = y;

    y = icony[screen] + ICONH + font->ascent + 2;
    XDrawString(dsp, win[screen], textgc[screen],
		iconx[screen], y, text_info, strlen(text_info));

    XFlush(dsp);

    y += font->ascent + font->descent + 2;

    done = False;
    while (!done) {
	if (ReadXString(buffer, PASSLENGTH))
	    break;

	/*
	 * we don't allow for root to have no password, but we handle the case
	 * where the user has no password correctly; they have to hit return
	 * only
	 */
#ifdef VMS
        password.len   = strlen(&buffer);
        password.dtype = 0;
        password.class = 0;
        password.addr  = &buffer;
 
        str$upcase(&password,&password);
 
        sys$hash_password(&password,uai_encrypt,uai_salt,
                          &username,&hash_system);
        sys$hash_password(&password,root_encrypt,root_salt,
                          &rootuser,&root_system);
        hash_password[8]=0;
        hash_system[8]=0;
        root_password[8]=0;
        root_system[8]=0;
        done = !((strcmp(hash_password,hash_system)) &&
                 (!allowroot || strcmp(root_password,root_system)));
#else
#if defined(HAS_SHADOW) && defined(linux)
        done = !((strcmp(pw_encrypt(buffer, userpass), userpass)) &&
          (!allowroot || strcmp(pw_encrypt(buffer, rootpass), rootpass)));
#else
	done = !((strcmp(crypt(buffer, userpass), userpass)) &&
          (!allowroot || strcmp(crypt(buffer, rootpass), rootpass)));
#endif
#endif
	if (!done && !*buffer) {
	    /* just hit return, and it wasn't his password */
	    break;
	}
	if (!*userpass && *buffer) {
	    /*
	     * the user has no password, but something was typed anyway.
	     * sounds fishy: don't let him in...
	     */
	    done = False;
	}
	/* clear plaintext password so you can't grunge around /dev/kmem */
	(void) memset(buffer, 0, sizeof(buffer));

	XSetForeground(dsp, Scr[screen].gc, bgcol[screen]);

	XFillRectangle(dsp, win[screen], Scr[screen].gc,
		       iconx[screen], y - font->ascent,
		       XTextWidth(font, text_invalid, strlen(text_invalid)),
		       font->ascent + font->descent + 2);

	XDrawString(dsp, win[screen], textgc[screen],
		    iconx[screen], y, text_valid, strlen(text_valid));

	if (done)
	    return 0;
	else {
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
	}
    }
    XChangeGrabbedCursor(mycursor);
    XUnmapWindow(dsp, icon[screen]);
    return 1;
}


static void
justDisplay()
{
    XEvent      event;

    for (screen = 0; screen < screens; screen++)
	init(win[screen]);
    do {
	while (!XPending(dsp)) {
	    for (screen = 0; screen < screens; screen++)
		callback(win[screen]);
	    XFlush(dsp);
	    (void) usleep(delay);
	}
	XNextEvent(dsp, &event);
#ifndef DEBUG
        if (!inwindow && event.type == VisibilityNotify)
	    XRaiseWindow(dsp, event.xany.window);
#endif
    } while (event.type != ButtonPress && event.type != KeyPress);
    for (screen = 0; screen < screens; screen++)
	if (event.xbutton.root == RootWindow(dsp, screen))
	    break;
    if (usefirst)
	XPutBackEvent(dsp, &event);
}


static void
sigcatch()
{
    finish();
    error("%s: caught terminate signal.\nAccess control list restored.\n");
}


static void
lockDisplay()
{
    if (!allowaccess) {
#if defined(SYSV) || defined(SVR4)
	sigset_t    oldsigmask;
	sigset_t    newsigmask;

	sigemptyset(&newsigmask);
	sigaddset(&newsigmask, SIGHUP);
	sigaddset(&newsigmask, SIGINT);
	sigaddset(&newsigmask, SIGQUIT);
	sigaddset(&newsigmask, SIGTERM);
	sigprocmask(SIG_BLOCK, &newsigmask, &oldsigmask);
#else
	int         oldsigmask;
#ifndef VMS
	oldsigmask = sigblock(sigmask(SIGHUP) |
			      sigmask(SIGINT) |
			      sigmask(SIGQUIT) |
			      sigmask(SIGTERM));
#endif
#endif

	(void) signal(SIGHUP, (void (*) ()) sigcatch);
	(void) signal(SIGINT, (void (*) ()) sigcatch);
	(void) signal(SIGQUIT, (void (*) ()) sigcatch);
	(void) signal(SIGTERM, (void (*) ()) sigcatch);

	XGrabHosts(dsp);

#if defined(SYSV) || defined(SVR4)
	(void) sigprocmask(SIG_SETMASK, &oldsigmask, &oldsigmask);
#else
	(void) sigsetmask(oldsigmask);
#endif
    }
#ifdef __hpux
	XHPDisableReset(dsp);
#endif
    do {
	justDisplay();
    } while (getPassword());
#ifdef __hpux
	XHPEnableReset(dsp);
#endif
}


long
allocpixel(cmap, name, def)
    Colormap    cmap;
    char       *name;
    char       *def;
{
    XColor      col;
    XColor      tmp;
    XParseColor(dsp, cmap, name, &col);
    if (!XAllocColor(dsp, cmap, &col)) {
	(void) fprintf(stderr, "couldn't allocate: %s, using %s instead\n",
		 name, def);
	XAllocNamedColor(dsp, cmap, def, &col, &tmp);
    }
    return col.pixel;
}


int
main(argc, argv)
    int         argc;
    char       *argv[];
{
    XSetWindowAttributes xswa;
    XGCValues   xgcv;
    XColor      nullcolor;

    ProgramName = strrchr(argv[0], '/');
    if (ProgramName)
	ProgramName++;
    else
	ProgramName = argv[0];

    srandom(time((long *) 0));	/* random mode needs the seed set. */

    GetResources(argc, argv);

    CheckResources();

    font = XLoadQueryFont(dsp, fontname);
    if (font == NULL) {
	(void) fprintf(stderr, "%s: can't find font: %s, using %s...\n",
		 ProgramName, fontname, FALLBACK_FONTNAME);
	font = XLoadQueryFont(dsp, FALLBACK_FONTNAME);
	if (font == NULL)
	    error("%s: can't even find %s!!!\n", FALLBACK_FONTNAME);
    }
    screens = ScreenCount(dsp);
    if (screens > MAXSCREENS)
	error("%s: can only support %d screens.\n", MAXSCREENS);
    for (screen = 0; screen < screens; screen++) {
	Screen     *scr = ScreenOfDisplay(dsp, screen);
	Colormap    cmap = DefaultColormapOfScreen(scr);

	root[screen] = RootWindowOfScreen(scr);
        bgcol[screen] = allocpixel(cmap, background, "White");
        fgcol[screen] = allocpixel(cmap, foreground, "Black");

	if (mono || CellsOfScreen(scr) == 2) {
	    Scr[screen].pixels[0] = fgcol[screen];
	    Scr[screen].pixels[1] = bgcol[screen];
	    Scr[screen].npixels = 2;
	} else {
	    int         colorcount = NUMCOLORS;
	    u_char      red[NUMCOLORS];
	    u_char      green[NUMCOLORS];
	    u_char      blue[NUMCOLORS];
	    int         i;

	    hsbramp(0.0, saturation, 1.0, 1.0, saturation, 1.0, colorcount,
		    red, green, blue);
	    Scr[screen].npixels = 0;
	    for (i = 0; i < colorcount; i++) {
		XColor      xcolor;

		xcolor.red = red[i] << 8;
		xcolor.green = green[i] << 8;
		xcolor.blue = blue[i] << 8;
		xcolor.flags = DoRed | DoGreen | DoBlue;

		if (!XAllocColor(dsp, cmap, &xcolor))
		    break;

		Scr[screen].pixels[i] = xcolor.pixel;
		Scr[screen].npixels++;
	    }
	    if (verbose)
		(void) fprintf(stderr,
                         "%d pixels allocated\n", Scr[screen].npixels);
	}

	xswa.override_redirect = True;
	xswa.background_pixel = BlackPixelOfScreen(scr);
	xswa.event_mask = KeyPressMask | ButtonPressMask | VisibilityChangeMask;

#ifdef DEBUG
#define WIDTH WidthOfScreen(scr) - 100
#define HEIGHT HeightOfScreen(scr) - 100
#define CWMASK CWBackPixel | CWEventMask
#else
#define WIDTH (inwindow? WidthOfScreen(scr)/2 : WidthOfScreen(scr))
#define HEIGHT (inwindow? HeightOfScreen(scr)/2 : HeightOfScreen(scr))
#define CWMASK ((inwindow? 0 : CWOverrideRedirect) | CWBackPixel | CWEventMask)
#endif

	win[screen] = XCreateWindow(dsp, root[screen], 0, 0, WIDTH, HEIGHT, 0,
				 CopyFromParent, InputOutput, CopyFromParent,
				    CWMASK, &xswa);

#ifdef DEBUG
	{
	    XWMHints    xwmh;

	    xwmh.flags = InputHint;
	    xwmh.input = True;
	    XChangeProperty(dsp, win[screen],
			    XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
			(unsigned char *) &xwmh, sizeof(xwmh) / sizeof(int));
	}
#else
        if (inwindow) {
            XWMHints    xwmh;
 
            xwmh.flags = InputHint;
            xwmh.input = True;
            XChangeProperty(dsp, win[screen],
                            XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
                        (unsigned char *) &xwmh, sizeof(xwmh) / sizeof(int));
        }
#endif

	iconx[screen] = (DisplayWidth(dsp, screen) -
			 XTextWidth(font, text_info, strlen(text_info))) / 2;

	icony[screen] = DisplayHeight(dsp, screen) / 6;

	xswa.border_pixel = fgcol[screen];
	xswa.background_pixel = bgcol[screen];
	xswa.event_mask = ButtonPressMask;
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask
	icon[screen] = XCreateWindow(dsp, win[screen],
				     iconx[screen], icony[screen],
				     ICONW, ICONH, 1, CopyFromParent,
				     InputOutput, CopyFromParent,
				     CIMASK, &xswa);

	XMapWindow(dsp, win[screen]);
	XRaiseWindow(dsp, win[screen]);

	xgcv.foreground = WhitePixelOfScreen(scr);
	xgcv.background = BlackPixelOfScreen(scr);
	Scr[screen].gc = XCreateGC(dsp, win[screen],
				   GCForeground | GCBackground, &xgcv);

	xgcv.foreground = fgcol[screen];
	xgcv.background = bgcol[screen];
	xgcv.font = font->fid;
	textgc[screen] = XCreateGC(dsp, win[screen],
				GCFont | GCForeground | GCBackground, &xgcv);
    }
    lockc = XCreateBitmapFromData(dsp, root[0], no_bits, 1, 1);
    lockm = XCreateBitmapFromData(dsp, root[0], no_bits, 1, 1);
    mycursor = XCreatePixmapCursor(dsp, lockc, lockm,
				   &nullcolor, &nullcolor, 0, 0);
    XFreePixmap(dsp, lockc);
    XFreePixmap(dsp, lockm);


    if (!enablesaver) {
	XGetScreenSaver(dsp, &sstimeout, &ssinterval,
			&ssblanking, &ssexposures);
	XSetScreenSaver(dsp, 0, 0, 0, 0);	/* disable screen saver */
    }
#ifndef DEBUG
    if (!inwindow)
      GrabKeyboardAndMouse();
#endif

    (void) nice(nicelevel);

    if (nolock)
	justDisplay();
    else
	lockDisplay();

    finish();

#ifdef VMS
    return 1;
#else
    return 0;
#endif
}
