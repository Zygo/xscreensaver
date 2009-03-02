#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xlock.c	5.15 2005/01/22 xlockmore";

#endif

/*-
 * xlock.c - X11 client to lock a display and show a screen saver.
 *
 * Copyright (c) 1988-91 by Patrick J. Naughton.
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@jwz.org>
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
 * 23-Feb-03: Timothy Reed <treed1@twcny.rr.com>
 *            Added PAM_putText to converse with passwd.c PAM_conv function
 *            to converse with any PAM.
 * 09-Mar-01: showfps stuff corrected
 * 01-Nov-00: Allocation checks
 * 08-Apr-98: A way for users to unlock each others display.  Kind of defeats
 *            the lock but the unlocked user is mailed and and entry is
 *            written to syslogd.  Thanks to Mark Kirk <mkirk@pdi.com> for
 *            his vizlock-1.0 patch.  Compile-time switch for this is
 *            GLOBAL_UNLOCK.  This is probably full of security holes when
 *            enabled... :)
 * 01-Jul-98: Eric Lassauge <lassauge AT users.sourceforge.net> &
 *              Remi Cohen-Scali <remi.cohenscali@pobox.com>
 *            Added support for locking VT switching (-/+vtlock)
 *            Added code is enclosed in USE_VTLOCK
 * 01-May-97: Matthew Rench <mdrench@mtu.edu>
 *            Added DPMS options.
 * 02-Dec-97: Strong user authentication, utilizing the SafeWord API.
 *            Author unknown.
 * 01-May-97: Scott Carter <scarter@sdsc.edu>
 *            Added code to stat .xlocktext, .plan, and .signature files
 *            before reading the message;  only regular files are read (no
 *            pipes or special files).
 *            Added code to replace tabs with 8 spaces in the plantext buffer.
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
 *            <jwz@jwz.org>, patched the patch (my mistake) for AIXV3
 *            and HP from <R.K.Lloyd@csc.liv.ac.uk>.
 * 01-Dec-93: added patch for AIXV3 from Tom McConnell
 *            <tmcconne@sedona.intel.com> also added a patch for HP-UX 8.0.
 * 29-Jul-93: "hyper", "helix", "rock", and "blot" (also tips on "maze") I
 *            got courtesy of Jamie Zawinski <jwz@jwz.org>;
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

#ifdef HAVE_SETPRIORITY
/* Problems using nice with setuid */
#include <sys/time.h>
#include <sys/resource.h>
#else /* !HAVE_SETPRIORITY */
/* extern int nice(int); */
#endif /* HAVE_SETPRIORITY */

#ifdef STANDALONE

/*-
 * xscreensaver compatibility layer for xlockmore modules.
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
 * This file, along with xlockmore.h, make it possible to compile an xlockmore
 * module into a standalone program, and thus use it with xscreensaver.
 * By Jamie Zawinski <jwz@jwz.org> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "screenhack.h"
#include "mode.h"
#include "vis.h"

#define countof(x) (sizeof((x))/sizeof(*(x)))

extern ModeSpecOpt xlockmore_opts[];
extern const char *app_defaults;
char       *message, *messagefile, *messagesfile, *program;
char       *messagefontname;

void
pre_merge_options(void)
{
	int         i, j;
	char       *s;

	/* Translate the xlockmore `opts[]' argument to a form that
	   screenhack.c expects.
	 */
	for (i = 0; i < xlockmore_opts->numopts; i++) {
		XrmOptionDescRec *old = &xlockmore_opts->opts[i];
		XrmOptionDescRec *new = &options[i];

		if (old->option[0] == '-')
			new->option = old->option;
		else {
			/* Convert "+foo" to "-no-foo". */
			if ((new->option = (char *) malloc(strlen(old->option) +
					5)) != NULL) {
				(void) strcpy(new->option, "-no-");
				(void) strcat(new->option, old->option + 1);
			}
		}

		new->specifier = strrchr(old->specifier, '.');
		if (!new->specifier)
			abort();

		new->argKind = old->argKind;
		new->value = old->value;
	}

	/* Add extra args, if they're mentioned in the defaults... */
	{
		char       *args[] =
		{"-delay", "-count", "-cycles", "-size", "-ncolors", "-bitmap",
		 "-text", "-filename", "-program",
		 "-wireframe", "-use3d", "-verbose"};

		for (j = 0; j < countof(args); j++)
			if (strstr(app_defaults, args[j] + 1)) {
				XrmOptionDescRec *new = &options[i++];

				new->option = args[j];
				new->specifier = strdup(args[j]);
				new->specifier[0] = '.';
				if (!strcmp(new->option, "-wireframe")) {
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-wireframe";
					new->specifier = options[i - 2].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-use3d")) {
					new->option = "-3d";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-3d";
					new->specifier = options[i - 2].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-verbose")) {
					new->option = "-verbose";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-verbose";
					new->specifier = options[i - 2].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else {
					new->argKind = XrmoptionSepArg;
					new->value = 0;
				}
			}
	}


	/* Construct the kind of `defaults' that screenhack.c expects from
	   the xlockmore `vars[]' argument.
	 */
	i = 0;

	/* Put on the PROGCLASS.background/foreground resources. */
	if ((s = (char *) malloc(50)) != NULL) {
		(void) strcpy(s, progclass);
		(void) strcat(s, ".background: black");
		defaults[i++] = s;
	}

	if ((s = (char *) malloc(50)) != NULL) {
		(void) strcpy(s, progclass);
		(void) strcat(s, ".foreground: white");
		defaults[i++] = s;
	}

	/* Copy the lines out of the `app_defaults' var and into this array. */
	s = strdup(app_defaults);
	while (s && *s) {
		defaults[i++] = s;
		s = strchr(s, '\n');
		if (s)
			*s++ = 0;
	}

	/* Copy the defaults out of the `xlockmore_opts->' variable. */
	for (j = 0; j < xlockmore_opts->numvarsdesc; j++) {
		const char *def = xlockmore_opts->vars[j].def;

		if (!def)
			def = "False";
		if (def == ((char *) 1))
			def = "True";
		if ((s = (char *) malloc(strlen(xlockmore_opts->vars[j].name) +
				    strlen(def) + 10)) != NULL) {
			(void) strcpy(s, "*");
			(void) strcat(s, xlockmore_opts->vars[j].name);
			(void) strcat(s, ": ");
			(void) strcat(s, def);
			defaults[i++] = s;
		}
	}

	defaults[i] = 0;
}


static void
xlockmore_read_resources(void)
{
	int         i;

	for (i = 0; i < xlockmore_opts->numvarsdesc; i++) {
		void       *var = xlockmore_opts->vars[i].var;
		Bool       *var_b = (Bool *) var;
		char      **var_c = (char **) var;
		int        *var_i = (int *) var;
		float      *var_f = (float *) var;

		switch (xlockmore_opts->vars[i].type) {
			case t_String:
				*var_c = get_string_resource(xlockmore_opts->vars[i].name,
					  xlockmore_opts->vars[i].classname);
				break;
			case t_Float:
				*var_f = get_float_resource(xlockmore_opts->vars[i].name,
					  xlockmore_opts->vars[i].classname);
				break;
			case t_Int:
				*var_i = get_integer_resource(xlockmore_opts->vars[i].name,
					  xlockmore_opts->vars[i].classname);
				break;
			case t_Bool:
				*var_b = get_boolean_resource(xlockmore_opts->vars[i].name,
					  xlockmore_opts->vars[i].classname);
				break;
			default:
				abort();
		}
	}
}

void
xlockmore_screenhack(Display * dpy, Window window,
		     Bool want_writable_colors,
		     Bool want_uniform_colors,
		     Bool want_smooth_colors,
		     Bool want_bright_colors,
		     void        (*hack_init) (ModeInfo *),
		     void        (*hack_draw) (ModeInfo *),
		     void        (*hack_free) (ModeInfo *))
{
	ModeInfo    mi;
	XGCValues   gcv;
	XColor      color;
	int         i;
	time_t      start, now;
	struct timeval drawstart, drawstop;
	unsigned long drawelapsed;
	int         orig_pause;

	(void) memset((char *) &mi, 0, sizeof (mi));
	mi.dpy = dpy;
	mi.window = window;
	(void) XGetWindowAttributes(dpy, window, &mi.xgwa);

	color.flags = DoRed | DoGreen | DoBlue;
	color.red = color.green = color.blue = 0;
	if (!XAllocColor(dpy, mi.xgwa.colormap, &color))
		abort();
	mi.black = color.pixel;
	color.red = color.green = color.blue = 0xFFFF;
	if (!XAllocColor(dpy, mi.xgwa.colormap, &color))
		abort();
	mi.white = color.pixel;

	if (mono_p) {
		static unsigned long pixels[2];
		static XColor colors[2];

	      MONO:
		mi.npixels = 2;
		mi.pixels = pixels;
		mi.colors = colors;
		pixels[0] = mi.black;
		pixels[1] = mi.white;
		colors[0].flags = DoRed | DoGreen | DoBlue;
		colors[1].flags = DoRed | DoGreen | DoBlue;
		colors[0].red = colors[0].green = colors[0].blue = 0;
		colors[1].red = colors[1].green = colors[1].blue = 0xFFFF;
		mi.writable_p = False;
	} else {
		mi.npixels = get_integer_resource("ncolors", "Integer");
		if (mi.npixels <= 0)
			mi.npixels = 64;
		else if (mi.npixels > 256)
			mi.npixels = 256;

		mi.writable_p = want_writable_colors;
		if ((mi.colors = (XColor *) calloc(mi.npixels,
			sizeof (*mi.colors))) != NULL) {

			if (want_uniform_colors)
				make_uniform_colormap(dpy, mi.xgwa.visual, mi.xgwa.colormap,
					      mi.colors, &mi.npixels,
					      True, &mi.writable_p, True);
			else if (want_smooth_colors)
				make_smooth_colormap(dpy, mi.xgwa.visual, mi.xgwa.colormap,
					     mi.colors, &mi.npixels,
					     True, &mi.writable_p, True);
			else
				make_random_colormap(dpy, mi.xgwa.visual, mi.xgwa.colormap,
					     mi.colors, &mi.npixels,
					     want_bright_colors,
					     True, &mi.writable_p, True);
		}

		if (mi.npixels <= 2)
			goto MONO;
		else {
			if ((mi.pixels = (unsigned long *)
				calloc(mi.npixels, sizeof (*mi.pixels))) != NULL) {
				int         i;

				for (i = 0; i < mi.npixels; i++)
					mi.pixels[i] = mi.colors[i].pixel;
			}
		}
	}

	gcv.foreground = mi.white;
	gcv.background = mi.black;
	mi.gc = XCreateGC(dpy, window, GCForeground | GCBackground, &gcv);

	mi.pause = get_integer_resource("delay", "Usecs");

	mi.count = get_integer_resource("count", "Int");
	mi.cycles = get_integer_resource("cycles", "Int");
	mi.size = get_integer_resource("size", "Int");
	mi.ncolors = get_integer_resource("ncolors", "Int");
	mi.bitmap = get_string_resource("bitmap", "String");
	messagefontname = get_string_resource("font", "String");
	message = get_string_resource("text", "String");
	messagefile = get_string_resource("filename", "String");
	messagesfile = get_string_resource("fortunefile", "String");
	program = get_string_resource("program", "String");


#if 0
	decay = get_boolean_resource("decay", "Boolean");
	if (decay)
		mi.fullrandom = False;

	trail = get_boolean_resource("trail", "Boolean");
	if (trail)
		mi.fullrandom = False;

	grow = get_boolean_resource("grow", "Boolean");
	if (grow)
		mi.fullrandom = False;

	liss = get_boolean_resource("liss", "Boolean");
	if (liss)
		mi.fullrandom = False;

	ammann = get_boolean_resource("ammann", "Boolean");
	if (ammann)
		mi.fullrandom = False;

	jong = get_boolean_resource("jong", "Boolean");
	if (jong)
		mi.fullrandom = False;

	sine = get_boolean_resource("sine", "Boolean");
	if (sine)
		mi.fullrandom = False;
#endif

	mi.threed = get_boolean_resource("use3d", "Boolean");
	mi.threed_delta = get_float_resource("delta3d", "Boolean");
	mi.threed_right_color = get_pixel_resource("right3d", "Color", dpy,
						   mi.xgwa.colormap);
	mi.threed_left_color = get_pixel_resource("left3d", "Color", dpy,
						  mi.xgwa.colormap);
	mi.threed_both_color = get_pixel_resource("both3d", "Color", dpy,
						  mi.xgwa.colormap);
	mi.threed_none_color = get_pixel_resource("none3d", "Color", dpy,
						  mi.xgwa.colormap);

	mi.wireframe_p = get_boolean_resource("wireframe", "Boolean");
        mi.fps_p = get_boolean_resource("showfps", "Boolean");
	mi.verbose = get_boolean_resource("verbose", "Boolean");

	mi.root_p = (window == RootWindowOfScreen(mi.xgwa.screen));
	mi.is_drawn = False;


	if (mi.pause < 0)
		mi.pause = 0;
	else if (mi.pause > 100000000)
		mi.pause = 100000000;
	orig_pause = mi.pause;

	xlockmore_read_resources();

	XClearWindow(dpy, window);

	i = 0;
	start = time((time_t) 0);

	hack_init(&mi);
	do {
		(void) gettimeofday(&drawstart, NULL);
		hack_draw(&mi);
		XSync(dpy, False);
		(void) gettimeofday(&drawstop, NULL);
		drawelapsed = (drawstop.tv_sec - drawstart.tv_sec)*1000000 +
			(drawstop.tv_usec - drawstart.tv_usec);
		if (mi.pause > drawelapsed)
			(void) usleep(mi.pause - drawelapsed);
		mi.pause = orig_pause;

		if (hack_free) {
			if (i++ > (mi.count / 4) &&
			    (start + 5) < (now = time((time_t) 0))) {
				i = 0;
				start = now;
				hack_free(&mi);
				hack_init(&mi);
				XSync(dpy, False);
			}
		}
	} while (1);
}

#else /* STANDALONE */
#include "xlock.h"
#include "color.h"
#include "util.h"
#include "iostuff.h"
#include "passwd.h"
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H && defined(AIXV3)
#include <sys/select.h>
#endif

#ifndef WIN32
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#else
#include "Xapi.h"
#endif /* WIN32 */

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
#ifdef USE_DPMS
#define MIN_DPMS 30		/* 30 second minimum */
#if 1
#include <X11/Xmd.h>
#include <X11/Xdmcp.h>
#include <X11/extensions/dpms.h>
#ifdef SunCplusplus
extern Bool DPMSQueryExtension(Display *, int *, int *);
extern Bool  DPMSGetTimeouts(Display *, unsigned short *, unsigned short *, unsigned short *);
extern Status  DPMSSetTimeouts(Display *, unsigned short, unsigned short, unsigned short);
extern int  DPMSCapable(Display *);
extern int  DPMSInfo(Display *, CARD16 *, BOOL *);
#endif
#else /* XFree86 < 4.x */
#include <X11/extensions/dpms.h>
#ifndef __NetBSD
extern unsigned char DPMSQueryExtension(Display *, int *, int *);
#endif
extern int  DPMSGetTimeouts(Display *, unsigned short *, unsigned short *, unsigned short *);
extern int  DPMSSetTimeouts(Display *, unsigned short, unsigned short, unsigned short);
#endif
extern int  dpmsstandby;
extern int  dpmssuspend;
extern int  dpmsoff;

#endif

#ifdef HAVE_KRB5
#include <krb5.h>
#endif /* HAVE_KRB5 */

#if ( HAVE_SYSLOG_H && (defined( USE_SYSLOG ) || defined( GLOBAL_UNLOCK )))
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#ifndef SYSLOG_FACILITY
#define SYSLOG_FACILITY LOG_AUTH
#endif
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

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

extern void checkResources(void);
extern void defaultVisualInfo(Display * display, int screen);

#ifdef USE_OLD_EVENT_LOOP
extern int  usleep(unsigned int);

#endif

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
extern void logoutUser(Display * display
#ifdef CLOSEDOWN_LOGOUT
 , int screens
#endif
);

#endif

#if 0
	/* #if !defined( AIXV3 ) && !defined( __hpux ) && !defined( __bsdi__ ) */
extern int  select(int, fd_set *, fd_set *, fd_set *, struct timeval *);

#if !defined( __hpux ) && !defined( apollo )
extern int  select(size_t, int *, int *, int *, const struct timeval *);

#endif

#endif

char       *ProgramName;	/* argv[0] */

#ifdef DEBUG
pid_t       ProgramPID;		/* for memcheck.c */

#endif
ScreenInfo *Scr = (ScreenInfo *) NULL;

/* This is a private data structure, don't touch */
static ModeInfo *modeinfo = (ModeInfo *) NULL;

Window      parent;
Bool        parentSet = False;
Display    *dsp = (Display *) NULL;	/* server display connection */

extern char user[PASSLENGTH];
extern char hostname[MAXHOSTNAMELEN];
extern char *displayname;
extern char *bitmap;
extern int  delay;
extern int  count;
extern int  cycles;
extern int  size;
extern int  ncolors;
extern float saturation;
extern float delta3d;
extern Bool nolock;
extern Bool resetsaver;
extern Bool inwindow;
extern Bool inroot;
extern Bool mono;
extern Bool allowaccess;
extern Bool allowroot;
extern Bool debug;
extern Bool description;
extern Bool echokeys;
extern char* echokey;
extern Bool enablesaver;
extern Bool fullrandom;
extern Bool grabmouse;
extern Bool grabserver;
extern Bool install;
extern Bool mousemotion;
extern int  unlockdelay;
extern Bool timeelapsed;
extern Bool usefirst;
extern Bool verbose;
extern Bool remote;
extern int  nicelevel;
extern int  lockdelay;
extern int  timeout;
extern Bool wireframe;
#ifdef USE_GL
extern Bool showfps;
extern Bool fpsTop;
#endif
extern Bool use3d;
#ifdef HAVE_KRB5
extern int krb5_valid;
#endif

extern char *fontname;
extern char *planfontname;

#ifdef USE_MB
XFontSet    fontset;
extern char *fontsetname;
XFontSet    planfontset;
extern char *planfontsetname;
XRectangle  mbRect;
XRectangle  planmbRect;

#define fontheight (mbRect.height)
#define planfontheight (planmbRect.height)

#else

#define fontheight (font->ascent + font->descent)
#define planfontheight (planfont->ascent + planfont->descent)

#endif
extern char *background;
extern char *foreground;
extern char *text_user;
extern char *text_pass;
extern char *text_info;
#ifdef HAVE_KRB5
extern char *text_krbinfo;
#endif
extern char *text_valid;
extern char *text_invalid;
extern char *text_invalidCapsLock;
extern char *failed_attempt;
extern char *failed_attempts;

extern char *geometry;
extern char *icongeometry;

#ifdef FX
extern char *glgeometry;

#endif
extern char *none3d;
extern char *right3d;
extern char *left3d;
extern char *both3d;

#ifdef USE_SOUND
extern char *locksound;
extern char *infosound;
extern char *validsound;
extern char *invalidsound;
#ifdef USE_ESOUND
extern char *welcomesound;
extern char *shutdownsound;
#endif
extern void playSound(char *string, Bool verbose);
#ifdef USE_ESOUND
extern int init_sound(void);
extern void shutdown_sound(void);
#endif
extern Bool sound;

#endif

extern char *startCmd;
extern char *endCmd;
#ifndef VMS
extern char *pipepassCmd;
#endif
extern char *logoutCmd;

extern char *mailCmd;
extern char *mailIcon;
extern char *nomailIcon;

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

static int  screen = 0;		/* current screen */
int         startscreen = 0;
static int  screens;		/* number of screens */

static Cursor mycursor;		/* blank cursor */
static Pixmap lockc;
static Pixmap lockm;		/* pixmaps for cursor and mask */
static char no_bits[] =
{0};				/* dummy array for the blank cursor */
static int  passx, passy;	/* position of the ?'s */
static int  iconwidth, iconheight;
static int  lock_delay;
static int  count_failed;

#ifdef FX
static int  glwidth, glheight;

#endif
static int  timex, timey;	/* position for the times */
static XFontStruct *font;
static XFontStruct *planfont;
static int  sstimeout;		/* screen saver parameters */
static int  ssinterval;
static int  ssblanking;
static int  ssexposures;
static unsigned long start_time;
static char *plantext[TEXTLINES + 2];	/* Message is stored here */

/* GEOMETRY STUFF */
static int  sizeconfiguremask;
static XWindowChanges minisizeconfigure;
static Bool fullscreen = False;

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
static int  tried_logout = 0;

#endif

#ifdef USE_SOUND
static int  got_invalid = 0;

#endif

/* This still might not be right for Solaris, change the "1"->"0" if error */
#if 1
#if (defined( SYSV ) || defined( SVR4 )) && defined( SOLARIS2 ) && !defined( HAVE_STRUCT_SIGSET_T )
#if !defined( __cplusplus ) && !defined( c_plusplus ) && !defined ( _SIGSET_T )

#define _SIGSET_T
typedef struct {		/* signal set type */
	unsigned long __sigbits[4];
} sigset_t;

 /* This is good for gcc compiled on Solaris-2.5 but used on Solaris-2.6 */

#endif
#endif
#endif
#if !defined( SYSV ) && !defined( SVR4 ) && !defined( VMS ) && !defined(__cplusplus) && !defined(c_plusplus)
extern int  sigblock(int);
extern int  sigsetmask(int);
#endif

#ifdef ORIGINAL_XPM_PATCH
#ifdef HAVE_XPM
#include <X11/xpm.h>
#endif

static XpmImage *mail_xpmimg = NULL;
static XpmImage *nomail_xpmimg = NULL;

#else
#ifndef WIN32
#include <bitmaps/mailempty.xbm>
#include <bitmaps/mailfull.xbm>

#define NOMAIL_WIDTH   mailempty_width
#define NOMAIL_HEIGHT  mailempty_height
#define NOMAIL_BITS    mailempty_bits
#define MAIL_WIDTH   mailfull_width
#define MAIL_HEIGHT  mailfull_height
#define MAIL_BITS    mailfull_bits
#endif /* !WIN32 */
#endif

#ifdef USE_VTLOCK
	/* EL - RCS : for VT switching locking */
extern int  vtlock;		/* Have to lock VT switching */
extern int  vtlocked;		/* VT switching is locked */

extern void dovtlock(void);	/* Actual do it */
extern void dovtunlock(void);	/* & undo it functions */

#endif

static int  signalUSR1 = 0;
static int  signalUSR2 = 0;
char * old_default_mode = (char *) NULL;

#define AllPointerEventMask \
	(ButtonPressMask | ButtonReleaseMask | \
	EnterWindowMask | LeaveWindowMask | \
	PointerMotionMask | PointerMotionHintMask | \
	Button1MotionMask | Button2MotionMask | \
	Button3MotionMask | Button4MotionMask | \
	Button5MotionMask | ButtonMotionMask | \
	KeymapStateMask)

#ifdef USE_VROOT
#include "vroot.h"
#endif

#ifdef SAFEWORD
#include "custpb.h"
#include "custf.h"

extern char *text_dpass;
extern char *text_fpass;
extern char *text_chall;
static int  challx, chally;

extern      pbmain();

struct pblk pblock;		/* our instance of the pblk */
struct pblk *pb;		/* global pointer to the pblk */

extern int  checkDynamic();

#endif

#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
static void
syslogStart(void)
{
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	char uid[16];
	char gid[16];

	pw = getpwuid(getuid());
	gr = getgrgid(getgid());
	if (pw == NULL)
		sprintf(uid, "%ld", (long) getuid());
	if (gr == NULL)
		sprintf(gid, "%ld", (long) getgid());
	(void) openlog(ProgramName, LOG_PID, SYSLOG_FACILITY);
	syslog(SYSLOG_INFO, "Start: %s, %s, %s",
		(pw == NULL) ? uid : pw->pw_name,
		(gr == NULL) ? gid : gr->gr_name,
		XDisplayString(dsp));
}

void
syslogStop(char *displayName)
{
	int         secs, mins;
	struct passwd *pw = NULL;
	struct group *gr = NULL;
	char uid[16];
	char gid[16];

	pw = getpwuid(getuid());
	gr = getgrgid(getgid());
	if (pw == NULL)
		sprintf(uid, "%ld", (long) getuid());
	if (gr == NULL)
		sprintf(gid, "%ld", (long) getgid());
	secs = (int) (seconds() - start_time);
	mins = secs / 60;
	secs %= 60;
	syslog(SYSLOG_INFO, "Stop: %s, %s, %s, %dm %ds",
		(pw == NULL) ? uid : pw->pw_name,
		(gr == NULL) ? gid : gr->gr_name,
		displayName, mins, secs);
}

#endif

#define ERROR_BUF 2048
char error_buf[ERROR_BUF];

void
error(const char *buf)
{
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	syslog(SYSLOG_WARNING, "%s", buf);
	if (!nolock) {
		if (strstr(buf, "unable to open display") == NULL)
			syslogStop(XDisplayString(dsp));
		else
			syslogStop("unknown display");
		closelog();
	}
#else
	(void) fprintf(stderr, "%s", buf);
#endif
	exit(1);
}

/* Server access control support. */

static XHostAddress *XHosts;	/* the list of "friendly" client machines */
static int  HostAccessCount;	/* the number of machines in XHosts */
static Bool HostAccessState;	/* whether or not we even look at the list */

/* define window class (used by WindowMaker attributes configuration) */
XClassHint xclasshint={(char *) "xlock", (char *) "xlock"};

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
		XFree((caddr_t) XHosts);
	}
	if (HostAccessState == False)
		XDisableAccessControl(display);
}

/* Provides support for the DPMS options. */
#ifdef USE_DPMS
static void
SetDPMS(Display * display, int nstandby, int nsuspend, int noff)
{
	static unsigned short standby = 0, suspend = 0, off = 0, flag = 0;
	int         dummy;

	if (DPMSQueryExtension(display, &dummy, &dummy)) {
		if (!flag) {
			DPMSGetTimeouts(display, &standby, &suspend, &off);
			flag++;
		}
		if ((nstandby < 0) && (nsuspend < 0) && (noff < 0))
			DPMSSetTimeouts(display, standby, suspend, off);
		else
			DPMSSetTimeouts(display,
					(nstandby <= 0 ? 0 : (nstandby > MIN_DPMS ? nstandby : MIN_DPMS)),
					(nsuspend <= 0 ? 0 : (nsuspend > MIN_DPMS ? nsuspend : MIN_DPMS)),
					(noff <= 0 ? 0 : (noff > MIN_DPMS ? noff : MIN_DPMS)));
	}
}

static int
monitor_powered_on_p(Display *dpy)
{
  int result;
  int event_number, error_number;
  BOOL onoff = False;
  CARD16 state;

  if (!DPMSQueryExtension(dpy, &event_number, &error_number))
    /* DPMS extention is not supported */
    result = True;

  else if (!DPMSCapable(dpy))
    /* Monitor is incapable of DPMS */
    result = True;

  else
    {
      DPMSInfo(dpy, &state, &onoff);
      if (!onoff)
	/*  DPMS is disabled */
	result = True;
      else {
	switch (state) {
	case DPMSModeOn:      result = True;  break;
	case DPMSModeStandby: result = False; break;
	case DPMSModeSuspend: result = False; break;
	case DPMSModeOff:     result = False; break;
	default:	      result = True;  break;
	}
      }
    }
  return result;
}
#else
static int
monitor_powered_on_p(Display *dpy)
{
  return 1;
}
#endif


/*-
 * Simple wrapper to get an asynchronous grab on the keyboard and mouse. If
 * either grab fails, we sleep for one second and try again since some window
 * manager might have had the mouse grabbed to drive the menu choice that
 * picked "Lock Screen..".  If either one fails the second time we print an
 * error message and exit.
 */
static void
GrabKeyboardAndMouse(Display * display, Window window)
{
	Status      status;

	status = XGrabKeyboard(display, window, True,
			       GrabModeAsync, GrabModeAsync, CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabKeyboard(display, window, True,
				  GrabModeAsync, GrabModeAsync, CurrentTime);

		if (status != GrabSuccess) {
			(void) sprintf(error_buf,
				"%s, could not grab keyboard! (%d)\n",
				(strlen(ProgramName) < ERROR_BUF - 80) ?
				ProgramName : "xlock", status);
			error(error_buf);
		}
	}
	status = XGrabPointer(display, window, True,
		(unsigned int) AllPointerEventMask, GrabModeAsync,
		GrabModeAsync, None, mycursor, CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabPointer(display, window, True,
			(unsigned int) AllPointerEventMask, GrabModeAsync,
			GrabModeAsync, None, mycursor, CurrentTime);

		if (status != GrabSuccess) {
			(void) sprintf(error_buf,
				"%s, could not grab pointer! (%d)\n",
				(strlen(ProgramName) < ERROR_BUF - 80) ?
				ProgramName : "xlock", status);
			error(error_buf);
		}
	}
}

/* Assuming that we already have an asynch grab on the pointer, just grab it
   again with a new cursor shape and ignore the return code. */
static void
ChangeGrabbedCursor(Display * display, Window window, Cursor cursor)
{
	if (!debug && grabmouse && !inwindow && !inroot)
		(void) XGrabPointer(display, window, True,
			(unsigned int) AllPointerEventMask, GrabModeAsync,
			GrabModeAsync, None, cursor, CurrentTime);
}

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
		if ((MI_WIDTH(mi) != xgwa.width) ||
		    (MI_HEIGHT(mi) != xgwa.height)) {
			MI_WINDOW(mi) = None;
			return (True);
		}
	}

	return (False);
}

#ifdef FX
void
resetSize(ModeInfo * mi, Bool setGL)
{
	char       *mesa_3Dfx_env;
	static int  width = 0, height = 0;

	mesa_3Dfx_env = getenv("MESA_GLX_FX");
	if (!mesa_3Dfx_env)
		return;
	if (tolower(mesa_3Dfx_env[0]) != 'f')
		return;

	/*
	 * This is not correct for multiscreens, but there are other problems
	 * here for multiscreens as well so I am not going bother
	 */
	if (!width)
		width = MI_WIDTH(mi);
	if (!height)
		height = MI_HEIGHT(mi);

	if (setGL) {
		MI_WIDTH(mi) = glwidth;
		MI_HEIGHT(mi) = glheight;
	} else {
		MI_WIDTH(mi) = width;
		MI_HEIGHT(mi) = height;
	}
	XResizeWindow(MI_DISPLAY(mi), MI_WINDOW(mi),
		      MI_WIDTH(mi), MI_HEIGHT(mi));
}
#endif

/*-
 *    Return a pointer to an up-to-date ModeInfo struct for the given
 *      screen, window and iconic mode.  Because the number of screens
 *      is limited, and much of the information is screen-specific, this
 *      function keeps static copies of ModeInfo structs are kept for
 *      each screen.  This also eliminates redundant calls to the X server
 *      to acquire info that does change.
 */

static ModeInfo *
mode_info(Display * display, int scrn, Window window, int iconic)
{
	XWindowAttributes xgwa;
	ModeInfo   *mi;

	mi = &modeinfo[scrn];

	if (MI_FLAG_NOT_SET(mi, WI_FLAG_INFO_INITTED)) {
		/* This stuff only needs to be set once per screen */

		(void) memset((char *) mi, 0, sizeof (ModeInfo));

		MI_DISPLAY(mi) = display;
		MI_SCREEN(mi) = scrn;
		MI_REAL_SCREEN(mi) = scrn;	/* TODO, for multiscreen debugging */
		MI_SCREENPTR(mi) = ScreenOfDisplay(display, scrn);
		MI_NUM_SCREENS(mi) = screens;
		MI_MAX_SCREENS(mi) = screens;	/* TODO, for multiscreen debugging */

		MI_SCREENINFO(mi) = &Scr[scrn];

		/* accessing globals here */
		MI_SET_FLAG_STATE(mi, WI_FLAG_MONO,
			     (mono || CellsOfScreen(MI_SCREENPTR(mi)) <= 2));
		MI_SET_FLAG_STATE(mi, WI_FLAG_INWINDOW, inwindow);
		MI_SET_FLAG_STATE(mi, WI_FLAG_INROOT, inroot);
		MI_SET_FLAG_STATE(mi, WI_FLAG_NOLOCK, nolock);
#ifdef USE_DTSAVER
		MI_SET_FLAG_STATE(mi, WI_FLAG_INSTALL, install && !inroot && !dtsaver);
#else
		MI_SET_FLAG_STATE(mi, WI_FLAG_INSTALL, install && !inroot);
#endif
		MI_SET_FLAG_STATE(mi, WI_FLAG_DEBUG, debug);
		MI_SET_FLAG_STATE(mi, WI_FLAG_USE3D, use3d &&
			    !(mono || CellsOfScreen(MI_SCREENPTR(mi)) <= 2));
		MI_SET_FLAG_STATE(mi, WI_FLAG_VERBOSE, verbose);
#ifdef WIN32
		MI_SET_FLAG_STATE(mi, WI_FLAG_FULLRANDOM, True);
#else
		MI_SET_FLAG_STATE(mi, WI_FLAG_FULLRANDOM, False);
#endif
		MI_SET_FLAG_STATE(mi, WI_FLAG_WIREFRAME, wireframe);
#ifdef USE_GL
		MI_SET_FLAG_STATE(mi, WI_FLAG_FPS, showfps);
#endif
		MI_SET_FLAG_STATE(mi, WI_FLAG_INFO_INITTED, True);
		MI_IS_DRAWN(mi) = False;
	}
	if (MI_WINDOW(mi) != window) {
		MI_WINDOW(mi) = window;

		(void) XGetWindowAttributes(display, window, &xgwa);

		MI_WIDTH(mi) = xgwa.width;
		MI_HEIGHT(mi) = xgwa.height;
	}
	MI_SET_FLAG_STATE(mi, WI_FLAG_ICONIC, iconic);

	MI_DELTA3D(mi) = delta3d;

	MI_DELAY(mi) = delay;	/* globals */
	MI_COUNT(mi) = count;
	MI_CYCLES(mi) = cycles;
	MI_SIZE(mi) = size;
	MI_NCOLORS(mi) = ncolors;
	MI_SATURATION(mi) = saturation;
	MI_BITMAP(mi) = bitmap;
	return (mi);
}


#if defined( GLOBAL_UNLOCK ) || defined( USE_PAM )
static int unamex = 0, unamey = 0;
#endif
#ifdef GLOBAL_UNLOCK

extern char *text_guser;
extern char global_user[PASSLENGTH];
extern void checkUser(char *buffer);
#define LOG_FACILITY    LOG_LOCAL2
#define LOG_LEVEL       LOG_INFO

static int
inform()
{
	/*
	- the time, in date(1) style, is generated with a combination of the
	  time(2) and ctime(3C) system calls.
	- the owner of the xlock process is learned with getlogin(3C).
	- global_user is a global character array  that holds the username
	  of the person wishing to unlock the display.
	- the hostname is learned with gethostname(2).
	- the syslog(3B) call uses the syslog facility to log
	  happenings at LOG_LEVEL to LOG_FACILITY
	- yes, a system(3S) call is used...
	- I never said it was pretty.

	*/

	char Mail[256];
	time_t unlock_time;

	/* learn what time it is and what host we are on */
	time(&unlock_time);

#if defined( HAVE_SYSLOG_H ) /* && defined( USE_SYSLOG ) */
	/* log this event to syslogd as defined */
	syslog(LOG_FACILITY | LOG_LEVEL, "%s: %s unlocked %s's display",
	  ProgramName, global_user, getlogin());
#endif
	/* if (mailCmd && mailCmd[0]) { */
		/* build a string suitable for use in system(3S) */
		(void) sprintf(Mail, "%s -s \"%s:\" %s << EOF\n %s unlocked "
			"%s's display on %s\n EOF",
			/* Want the mail with the subject line control */
#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 )
			"/usr/ucb/mail" ,
#else
			"/usr/bin/mail" ,
#endif
			ProgramName, getlogin(),
			global_user, hostname, ctime(&unlock_time));

		if (debug)
			(void) printf("%s\n", Mail);
		(void) system(Mail);
	/* } */
}
#endif

/* Restore all grabs, reset screensaver, restore colormap, close connection. */
void
finish(Display * display, Bool closeDisplay)
{
	int         scrn;

	for (scrn = startscreen; scrn < screens; scrn++) {
		if (Scr[scrn].window != None) {
			release_last_mode(mode_info(display, scrn, Scr[scrn].window, False));
		}
		if (Scr[scrn].icon != None) {
			release_last_mode(mode_info(display, scrn, Scr[scrn].icon, True));
		}
	}
#ifdef ORIGINAL_XPM_PATCH
	if (mail_xpmimg)
		free(mail_xpmimg);
	if (nomail_xpmimg)
		free(nomail_xpmimg);
#endif

	XSync(display, False);
#ifdef USE_VROOT
	if (inroot)
		XClearWindow(display, Scr[startscreen].window);
#endif

#ifdef GLOBAL_UNLOCK
	/* Check to see if the owner is doing the unlocking themselves,
	and if not then inform the display owner of who is unlocking */

	if (strcmp(global_user, getlogin()) != 0)
		inform();
#endif

	if (!nolock && !allowaccess) {
		if (grabserver)
			XUngrabServer(display);
		XUngrabHosts(display);
	}
	XUngrabPointer(display, CurrentTime);
	XUngrabKeyboard(display, CurrentTime);
	if (!enablesaver && !nolock) {
		XSetScreenSaver(display, sstimeout, ssinterval, ssblanking, ssexposures);
	}
	XFlush(display);
#if 0
/* Report that this gives "bad file descriptor" on XFree86 4.x */
#ifndef __sgi
  /*-
   * The following line seems to cause a core dump on the SGI.
   * More work is needed on the cleanup code.
   */
	if (closeDisplay)
		(void) XCloseDisplay(display);
#endif
#endif
#ifdef HAVE_SETPRIORITY
	(void) setpriority(0, 0, 0);
#else /* !HAVE_SETPRIORITY */
	(void) nice(0);
#endif /* HAVE_SETPRIORITY */
#ifdef USE_VTLOCK
	/* EL - RCS : Unlock VT switching */
	if (vtlock && vtlocked)
		dovtunlock();
#endif
}

#ifdef __cplusplus
extern "C" {
#endif

static int
xio_error(Display * d)
{
#ifdef USE_DTSAVER
	if (dtsaver) {		/* this is normal when run as -dtsaver */
		/* X connection to :0.0 broken (explicit kill or server shutdown). */
		exit(0);
	}
#endif
	(void) sprintf(error_buf,
		"%s: xio_error\n",
		(strlen(ProgramName) < ERROR_BUF - 80) ?
		ProgramName : "xlock");
	error(error_buf);
	return ((debug) ? 0 : 1);	/* suppress message unless debugging */
}

#ifdef __cplusplus
}
#endif

/* Convenience function for drawing text */
static void
putText(Display * display, Window window, GC gc,
	const char *string, int bold, int left, int *px, int *py)
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
			(void) XDrawImageString(display, window, gc, x, y, s, len);
			if (bold)
				(void) XDrawString(display, window, gc, x + 1, y, s, len);
		}
		if (!last) {
			y += fontheight + 2;
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

#ifdef JA
#include "xlock-msg-ja.h"
#endif

static void
statusUpdate(int isnew, int scr)
{
	int         left, x, y, len;
	char        buf[1024];
	XWindowAttributes xgwa;
	static int  last_time;

#ifdef USE_BUTTON_LOGOUT
	int         ysave;
	static int  made_button, last_tried_lo = 0;

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

	(void) XGetWindowAttributes(dsp, Scr[scr].window, &xgwa);
	x = left = timex;
	y = timey;

	if (timeelapsed) {
		if (len < 60)
			(void) sprintf(buf,
#ifdef DE
				       "Seit %d Minute%s gesperrt.                                  \n",
				       len, len <= 1 ? "" : "n"
#elif defined FR
				       "%d minute%s écoulée%s depuis verrouillage.                      \n",
				       len, len <= 1 ? "" : "s", len <= 1 ? "" : "s"
#elif defined NL
				       "%d minu%s op slot.                                        \n",
				       len, len <= 1 ? "ut" : "ten"
#elif defined JA
				       JA_TIME_ELAPSED_MINUTES,
				       len
#else
				       "%d minute%s elapsed since locked.                           \n",
				       len, len <= 1 ? "" : "s"
#endif
				);
		else
			(void) sprintf(buf,
#ifdef DE
				       "Seit %d:%02d Stunden gesperrt.                                 \n",
				       len / 60, len % 60
#elif defined FR
				       "%d:%02d heures écoulée%s depuis verouillage.                        \n",
			               len / 60, len % 60, (len / 60) <= 1 ? "" : "s"
#elif defined NL
				       "%d:%02d uur op slot.                                           \n",
				       len / 60, len % 60
#elif defined JA
				       JA_TIME_ELAPSED_HOURS,
				       len / 60, len % 60
#else
				       "%d:%02d hour%s elapsed since locked.                            \n",
				       len / 60, len % 60, (len / 60) <= 1 ? "" : "s"
#endif
				);
		putText(dsp, Scr[scr].window, Scr[scr].textgc, buf, False, left, &x, &y);
	}
#ifdef USE_BUTTON_LOGOUT
	if (enable_button) {
		if (logoutButton > len) {
			int         tmp = logoutButton - len;

			if (tmp < 60)
				(void) sprintf(buf,
#ifdef DE
					       "In %d Minute%s erscheint der Auslogger.                       \n",
					       tmp, (tmp <= 1) ? "" : "n"
#elif defined FR
					       "%d minute%s jusqu'à apparition du bouton lougout.             \n",
					       tmp, (tmp <= 1) ? "" : "s"
#elif defined NL
					       "Over %d minu%s verschijnt de logout knop.                   \n",
					       tmp, (tmp <= 1) ? "ut" : "ten"
#elif defined JA
					       JA_BUTTON_MINUTES,
					       tmp
#else
					       "%d minute%s until the public logout button appears.           \n",
					       tmp, (tmp <= 1) ? "" : "s"
#endif
					);
			else
				(void) sprintf(buf,
#ifdef DE
					       "In %d:%02d Stunden erscheint der Auslogger.                 \n",
#elif defined FR
					       "%d:%02d heures jusqu'à apparition du bouton lougout         \n",
#elif defined NL
					       "Over %d:%02d uur verschijnt de logout knop.                 \n",
#elif defined JA
					       JA_BUTTON_HOURS,
#else
					       "%d:%02d hours until the public logout button appears.       \n",
#endif
					       tmp / 60, tmp % 60);
			putText(dsp, Scr[scr].window, Scr[scr].textgc, buf, False, left, &x, &y);
		} else {
			/* Erase previous logout button message */
			putText(dsp, Scr[scr].window, Scr[scr].textgc,
				"                                                                           \n",
				False, left, &x, &y);
			if (!made_button || (isnew && tried_logout)) {
				made_button = 1;
				ysave = y;

				y += fontheight + 8;	/* Leave a gap for validating */
				XUnmapWindow(dsp, Scr[scr].button);
				XSetForeground(dsp, Scr[scr].gc, Scr[scr].bg_pixel);
				XFillRectangle(dsp, Scr[scr].window, Scr[scr].gc, left, y,
					       xgwa.width - left,
				     5 + 2 * (fontheight));
				XSetForeground(dsp, Scr[scr].gc, Scr[scr].fg_pixel);

				if (tried_logout) {
					putText(dsp, Scr[scr].window, Scr[scr].textgc, logoutFailedString,
						True, left, &x, &y);
				} else {
					XMoveWindow(dsp, Scr[scr].button, left, y);
					XMapWindow(dsp, Scr[scr].button);
					XRaiseWindow(dsp, Scr[scr].button);
					(void) sprintf(buf, " %s ", logoutButtonLabel);
					XSetForeground(dsp, Scr[scr].gc, Scr[scr].white_pixel);
					(void) XDrawString(dsp, Scr[scr].button, Scr[scr].gc,
							   0, font->ascent + 1, buf, strlen(buf));
					XSetForeground(dsp, Scr[scr].gc, Scr[scr].fg_pixel);
					y += 5 + 2 * fontheight;
					putText(dsp, Scr[scr].window, Scr[scr].textgc, logoutButtonHelp,
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
			(void) sprintf(buf,
#ifdef FR
				       "%d minute%s jusqu'à l'auto-logout.    \n",
				       tmp, (tmp <= 1) ? "" : "s"
#elif defined NL
                                       "%d minu%s tot auto-logout.            \n",
                                       tmp, (tmp <= 1) ? "ut" : "ten"
#elif defined JA
				       JA_AUTOLOGOUT_MINUTES,
				       tmp
#else
				       "%d minute%s until auto-logout.    \n",
				       tmp, (tmp <= 1) ? "" : "s"
#endif
				);
		else
			(void) sprintf(buf,
#ifdef FR
				       "%d:%02d heure%s jusqu'à auto-logout.  \n",
				       tmp / 60, tmp % 60, (tmp / 60) <= 1 ? "" : "s"
#elif defined NL
			               "%d:%02d uur tot auto-logout.          \n",
			               tmp / 60, tmp % 60
#elif defined JA
				       JA_AUTOLOGOUT_HOURS,
				       tmp / 60, tmp % 60
#else
				       "%d:%02d hours until auto-logout.  \n",
				       tmp / 60, tmp % 60
#endif
				);
		putText(dsp, Scr[scr].window, Scr[scr].textgc, buf, False, left, &x, &y);
	}
#endif /* USE_AUTO_LOGOUT */
}

#ifdef USE_AUTO_LOGOUT
static void
checkLogout(Display * display)
{
	if (nolock || tried_logout || !logoutAuto)
		return;
	if (logoutAuto * 60 < (int) (seconds() - start_time)) {
		tried_logout = 1;
		logoutAuto = 0;
		logoutUser(display
#ifdef CLOSEDOWN_LOGOUT
 , screens
#endif
		);
	}
}

#endif /* USE_AUTO_LOGOUT */

#ifndef USE_OLD_EVENT_LOOP
#ifndef WIN32

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
#endif /* !WIN32 */

/* return 0 on event received, -1 on timeout */
static int
runMainLoop(int maxtime, int iconscreen)
{
#ifndef WIN32
	static int  lastdelay = -1, lastmaxtime = -1;
	int         fd = ConnectionNumber(dsp), r;
        int         poweron=True;
	struct timeval sleep_time, first, repeat, elapsed, tmp;
	fd_set      reads;
	unsigned long started;

#ifdef USE_NEW_EVENT_LOOP
	struct timeval loopStart, timeSinceLoop;
	unsigned long lastIter = 0, desiredIter = 0;

	GETTIMEOFDAY(&loopStart);
#endif

	first.tv_sec = 0;
	first.tv_usec = 0;
	elapsed.tv_sec = 0;
	elapsed.tv_usec = 0;
	repeat.tv_sec = delay / 1000000;
	repeat.tv_usec = delay % 1000000;

	started = seconds();

	for (;;) {
	        poweron= monitor_powered_on_p(dsp);
                if(!poweron) (void) usleep(100000);

		if (signalUSR1 || signalUSR2) {
			int         i;

			if (signalUSR1) {
#ifdef DEBUG
				(void) printf("switch to mode %s\n", "blank");
#endif
				for (i = 0; i < numprocs; i++) {
					if (!strcmp(LockProcs[i].cmdline_arg, "blank")) {
						set_default_mode(&LockProcs[i]);
						break;
					}
				}
			} else {
#ifdef DEBUG
				(void) printf("switch to mode %s\n", old_default_mode);
#endif
				for (i = 0; i < numprocs; i++) {
					if (!strcmp(LockProcs[i].cmdline_arg, old_default_mode)) {
						set_default_mode(&LockProcs[i]);
						break;
					}
				}
			}

			for (screen = startscreen; screen < screens; screen++) {
				call_change_hook((LockStruct *) NULL,
						 mode_info(dsp, screen, Scr[screen].window, False));
			}

			signalUSR1 = signalUSR2 = 0;
		}
		if (delay != lastdelay || maxtime != lastmaxtime) {
			if (!delay || (maxtime && delay / 1000000 > maxtime)) {
				repeat.tv_sec = maxtime;
				repeat.tv_usec = 0;
			} else {
				repeat.tv_sec = delay / 1000000;
				repeat.tv_usec = delay % 1000000;
			}
			first = repeat;
			lastdelay = delay;
			lastmaxtime = maxtime;
			sleep_time = first;
		} else {
			sleep_time = repeat;
		}

		/* subtract time spent doing last loop iteration */
		sub_timers(&sleep_time, &elapsed, &sleep_time);

		/* (void) */ FD_ZERO(&reads);
		FD_SET(fd, &reads);
#ifdef VMS
		FD_SET(fd + 1, &reads);
#endif
#if DCE_PASSWD
		r = _select_sys(fd + 1,
			(fd_set *) & reads, (fd_set *) NULL, (fd_set *) NULL,
				(struct timeval *) &sleep_time);
#else
#if defined( __cplusplus ) || defined( c_plusplus )
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

		GETTIMEOFDAY(&tmp);	/* get time before calling mode proc */

#ifdef USE_NEW_EVENT_LOOP
		if (delay == 0) {
			desiredIter = lastIter + 1;
		} else {
			sub_timers(&tmp, &loopStart, &timeSinceLoop);
			desiredIter =
				(timeSinceLoop.tv_usec / delay) +
				(((timeSinceLoop.tv_sec % delay) * 1000000) / delay) +
				(timeSinceLoop.tv_sec / delay) * 1000000;
		}
		while (lastIter != desiredIter) {
			++lastIter;
#endif

			for (screen = startscreen; screen < screens; screen++) {
				Window      cbwin;
				Bool        iconic;
				ModeInfo   *mi;

				if (screen == iconscreen) {
					cbwin = Scr[screen].icon;
					iconic = True;
				} else {
					cbwin = Scr[screen].window;
					iconic = False;
				}
				if (cbwin != None && poweron) {
					mi = mode_info(dsp, screen, cbwin, iconic);
					call_callback_hook((LockStruct *) NULL, mi);
				}
			}

			if (poweron)
				XSync(dsp, False);
			else
				(void) usleep(100000);

			/* check for events received during the XSync() */
			if (QLength(dsp)) {
				return 0;
			}
			if (maxtime && ((int) (seconds() - started) > maxtime)) {
				return -1;
			}
#ifdef USE_NEW_EVENT_LOOP
		}
#endif
		/* if (mindelay) (void) usleep(mindelay); */

		/* get the time now, figure how long it took */
		GETTIMEOFDAY(&elapsed);
		sub_timers(&elapsed, &tmp, &elapsed);
	}
#else /* WIN32 */
	return 0;
#endif /* WIN32 */
}

#endif /* !USE_OLD_EVENT_LOOP */

static int
ReadXString(char *s, int slen, Bool *capsLock
#ifdef GLOBAL_UNLOCK
  , Bool pass
#elif defined( USE_PAM )
  , Bool PAM_echokeys
#endif

)
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

	*capsLock = False;
	for (screen = startscreen; screen < screens; screen++)
		if (thisscreen == screen) {
			call_init_hook((LockStruct *) NULL,
			     mode_info(dsp, screen, Scr[screen].icon, True));
		} else {
			call_init_hook((LockStruct *) NULL,
			  mode_info(dsp, screen, Scr[screen].window, False));
		}
	statusUpdate(True, thisscreen);
	bp = 0;
	*s = 0;

	for (;;) {
		unsigned long lasteventtime = seconds();
		KeySym keysym;

		while (!XPending(dsp)) {
#ifdef USE_OLD_EVENT_LOOP
			for (screen = startscreen; screen < screens; screen++)
				if (thisscreen == screen)
					call_callback_hook(NULL, mode_info(dsp, screen,
						    Scr[screen].icon, True));
				else
					call_callback_hook(NULL, mode_info(dsp, screen,
						 Scr[screen].window, False));
			statusUpdate(False, thisscreen);
			XSync(dsp, False);
			(void) usleep(delay);
#else
			statusUpdate(False, thisscreen);
			if (runMainLoop(MIN(timeout, 5), thisscreen) == 0)
				break;
#endif

			if ((timeout < (int) (seconds() - lasteventtime))) {
				screen = thisscreen;
				return 1;
			}
		}

		screen = thisscreen;
		(void) XNextEvent(dsp, &event);

		/*
		 * This event handling code should be unified with the
		 * similar code in justDisplay().
		 */
		switch (event.type) {
			case KeyPress:
				len = XLookupString((XKeyEvent *) & event,
					 keystr, 20, /*(KeySym *) NULL,*/
					 &keysym,
					 (XComposeStatus *) NULL);
				*capsLock = ((event.xkey.state & LockMask) != 0);

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
				XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel);
#ifdef GLOBAL_UNLOCK
				if (!pass) {
					XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
						 unamex, unamey - font->ascent,
					       XTextWidth(font, s, slen),
					       fontheight);
					(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
						    unamex, unamey, s, bp);
				} else
#elif defined( USE_PAM )
				if (PAM_echokeys) {

					XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
						 unamex, unamey - font->ascent,
					       XTextWidth(font, s, slen),
					       fontheight);

					(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
						    unamex, unamey, s, bp);
				} else
#endif
				if (echokeys) {
					if (bp > 0 && strcmp("swear", echokey) == 0) {
					  char swearkeys[] = "@^!$%?#*";
					  pwbuf[bp - 1] =
					    swearkeys[NRAND(strlen(swearkeys))];
					  pwbuf[bp] = '\0';
					} else
					  (void) memset((char *) pwbuf,
					    echokey[0], slen);
					XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
						 passx, passy - font->ascent,
					       XTextWidth(font, pwbuf, slen),
					       fontheight);
					(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
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

			case MotionNotify:
				if (!mousemotion)
					break;
				/* fall through on last mouse event */
			case ButtonPress:
				if (((XButtonEvent *) & event)->window == Scr[screen].icon) {
					return 1;
				}
#ifdef USE_BUTTON_LOGOUT
				if (((XButtonEvent *) & event)->window == Scr[screen].button) {
					XSetFunction(dsp, Scr[screen].gc, GXxor);
					XSetForeground(dsp, Scr[screen].gc, Scr[screen].fg_pixel);
					XFillRectangle(dsp, Scr[screen].button, Scr[screen].gc,
						       0, 0, 500, 100);
					XSync(dsp, False);
					tried_logout = 1;
					logoutUser(dsp
#ifdef CLOSEDOWN_LOGOUT
 , screens
#endif
					);
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
							  mode_info(dsp, screen, Scr[screen].window, False));
#ifndef USE_WINDOW_VISIBILITY
					s[0] = '\0';
					return 1;
#endif
				}
				break;
			case ConfigureNotify:
				/* next line for -geometry */
				if (!fullscreen)
					break;
				/* window config changed */
				if (!debug && !inwindow) {
					XRaiseWindow(dsp, event.xconfigure.window);
					fixColormap(mode_info(dsp, screen, Scr[screen].window, False), ncolors,
						saturation, mono, install, inroot, inwindow, verbose);
				}
				if (window_size_changed(screen, Scr[screen].window)) {
					call_init_hook((LockStruct *) NULL,
						       mode_info(dsp, screen, Scr[screen].window, False));
				} else if (install)
					/* next line : refresh would be logical. But some modes
 					 * look weird when continuing from an erased screen */
					call_refresh_hook((LockStruct *) NULL,
						mode_info(dsp, screen, Scr[screen].window, False));

				s[0] = '\0';
				return 1;
			case KeymapNotify:
			case KeyRelease:
			case ButtonRelease:
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

void
modeDescription(ModeInfo * mi)
{
	int         left, x, y;
	int         scrn = MI_SCREEN(mi);
	XWindowAttributes xgwa;

	(void) XGetWindowAttributes(dsp, Scr[scrn].window, &xgwa);

	x = left = Scr[scrn].iconpos.x;
	y = Scr[scrn].iconpos.y - fontheight + 2;

	XSetForeground(dsp, Scr[scrn].gc, Scr[scrn].bg_pixel);
	XFillRectangle(dsp, Scr[scrn].window, Scr[scrn].gc,
		       x, y - font->ascent,
		       xgwa.width - x, fontheight + 2);

	putText(dsp, Scr[scrn].window, Scr[scrn].textgc, MI_NAME(mi),
		True, left, &x, &y);
	putText(dsp, Scr[scrn].window, Scr[scrn].textgc, ": ", True, left, &x, &y);
	putText(dsp, Scr[scrn].window, Scr[scrn].textgc, MI_DESC(mi),
		False, left, &x, &y);
	putText(dsp, Scr[scrn].window, Scr[scrn].textgc, "\n", False, left, &x, &y);
}

void
update_plan() /* updates current time in plantext */
{
	time_t t;
	int i = 0;

	t = time(NULL);
	(void) strcpy(plantext[0], ctime(&t));
	for (i = 0; i < 29; i++)
		if (plantext[0][i] < '\040') { /* Cygwin weirdness */
			plantext[0][i] = '\0';
			break;
		}
	plantext[0][29] = '\0'; /* just in case. */
}

/* WIN32 machines handle reading passwords automatically */
#ifndef WIN32

static int
getPassword(void)
{
	XWindowAttributes xgwa;
	int         x, y, left, done, remy;
	char        buffer[PASSLENGTH];
	char      **planp;
	char       *hostbuf = (char *) NULL;
	ModeInfo   *mi = &modeinfo[screen];
	Bool        capsLock = False;
#ifdef USE_MB
	XFontSet    backfontset;
#endif

#ifdef FX
	Bool        mesa_3Dfx_fullscreen;
	char       *mesa_3Dfx_env;

	mesa_3Dfx_env = getenv("MESA_GLX_FX");
	if (mesa_3Dfx_env)
		if (tolower(mesa_3Dfx_env[0] == 'f')) {
			/* disabling fullscreen HW 3dfx rendering to
			   allow iconic mode window. */
			mesa_3Dfx_fullscreen = True;
			unsetenv("MESA_GLX_FX");
		}
#endif
#ifdef HAVE_SETPRIORITY
	(void) setpriority(0, 0, 0);
#else /* !HAVE_SETPRIORITY */
	(void) nice(0);
#endif /* HAVE_SETPRIORITY */
	if (!fullscreen)
		XConfigureWindow(dsp, Scr[screen].window, sizeconfiguremask,
				 &(Scr[screen].fullsizeconfigure));


	(void) XGetWindowAttributes(dsp, Scr[screen].window, &xgwa);

	ChangeGrabbedCursor(dsp, Scr[screen].window,
			    XCreateFontCursor(dsp, XC_left_ptr));

	MI_CLEARWINDOWCOLOR(mi, Scr[screen].bg_pixel);
	XMapWindow(dsp, Scr[screen].icon);
	XRaiseWindow(dsp, Scr[screen].icon);

	if (description)
		modeDescription(mi);

	x = left = Scr[screen].iconpos.x + iconwidth + font->max_bounds.width;
	y = Scr[screen].iconpos.y + font->ascent;

	if ((hostbuf = (char *) malloc(strlen(user) + strlen(hostname) +
			2)) != NULL) {
#ifdef HAVE_KRB5
		if (krb5_valid) {
			strcpy(hostbuf, user);
		} else
#endif /* HAVE_KRB5 */
		(void) sprintf(hostbuf, "%s@%s", user, hostname);

		putText(dsp, Scr[screen].window, Scr[screen].textgc, text_user, True, left, &x, &y);
		putText(dsp, Scr[screen].window, Scr[screen].textgc, hostbuf, False, left, &x, &y);
		free(hostbuf);
	}
	if (displayname && displayname[0] != ':') {
		char  *displaybuf = (char *) NULL;
		char  *colon = (char *) strchr(displayname, ':');
		int   n = colon - displayname;

		if ((displaybuf = (char *) malloc(n + 1)) != NULL) {
			(void) strncpy(displaybuf, displayname, n);
			displaybuf[n] = '\0';
			if (remote && n
			    && strcmp(displaybuf, "unix") != 0
			    && strcmp(displaybuf, "localhost") != 0
			    && strcmp(displaybuf, hostname) != 0) {
				putText(dsp, Scr[screen].window, Scr[screen].textgc, "  Display: ", True, left, &x, &y);
				putText(dsp, Scr[screen].window, Scr[screen].textgc, displaybuf, False, left, &x, &y);
			}
			free(displaybuf);
		}
	}
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);

#ifdef GLOBAL_UNLOCK
	putText(dsp, Scr[screen].window, Scr[screen].textgc, text_guser,
		True, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, " ",
		False, left, &x, &y);
		unamex = x;
                unamey = y;
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);
#endif

#ifdef SAFEWORD
	if (checkDynamic()) {
		pb = &pblock;
		(void) memset((char *) &pblock, 0, sizeof (pblock));
#if 0
		(void) strcpy(pblock.uport, "custpb");
		pblock.status = NO_STATUS;
#endif
		challx = x;
		chally = y;
		putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n\n",
			False, left, &x, &y);
		pb->mode = CHALLENGE;
		(void) strcpy(pb->id, user);
		pbmain(pb);
		if (pb->status != GOOD_USER) {
			chall[0] = 0;
			return 1;
		}
		if (pb->dynpwdf) {
			/* We do not really care if they also got a fixed password... */
			putText(dsp, Scr[screen].window, Scr[screen].textgc, text_dpass,
				True, left, &x, &y);
			putText(dsp, Scr[screen].window, Scr[screen].textgc, " ",
				False, left, &x, &y);
			passx = x;
			passy = y;
		} else if (pb->fixpwdf) {
			/* In the unlikely event they only got a fixed password... */
			putText(dsp, Scr[screen].window, Scr[screen].textgc, text_fpass,
				True, left, &x, &y);
			putText(dsp, Scr[screen].window, Scr[screen].textgc, " ",
				False, left, &x, &y);
			passx = x;
			passy = y;
		}
	} else
#endif
	{
		putText(dsp, Scr[screen].window, Scr[screen].textgc, text_pass, True, left, &x, &y);
		putText(dsp, Scr[screen].window, Scr[screen].textgc, " ", False, left, &x, &y);

		passx = x;
		passy = y;
	}

	y += fontheight + 6;
	if (y < Scr[screen].iconpos.y + iconheight + font->ascent + 12)
		y = Scr[screen].iconpos.y + iconheight + font->ascent + 12;
	x = left = Scr[screen].iconpos.x;
#ifdef HAVE_KRB5
	if (krb5_valid)
		putText(dsp, Scr[screen].window, Scr[screen].textgc, text_krbinfo, False, left, &x, &y);
	else
#endif /* HAVE_KRB5 */
	putText(dsp, Scr[screen].window, Scr[screen].textgc, text_info, False, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);
	if (count_failed > 0) {
		char * cnt = NULL;
		y += fontheight + 6;
		if (y < Scr[screen].iconpos.y + iconheight + font->ascent + 12)
			y = Scr[screen].iconpos.y + iconheight + font->ascent + 12;
		x = left = Scr[screen].iconpos.x;
		if ((cnt = (char *) malloc(strlen((count_failed == 1) ? failed_attempt : failed_attempts) + 16)) != NULL) {
		  (void) sprintf(cnt, "%d%s", count_failed, (count_failed == 1) ? failed_attempt : failed_attempts);
		  putText(dsp, Scr[screen].window, Scr[screen].textgc, cnt, False, left, &x, &y);
		  putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);
		  free(cnt);
		  y -= 2 * (fontheight + 2); /* go up */
		}
	}
	timex = x;
	timey = y;

#ifdef USE_AUTO_LOGOUT
	if (logoutAuto) {
		y += fontheight + 2;
	}
#endif
#ifdef USE_BUTTON_LOGOUT
	if (enable_button) {
		y += fontheight + 2;
	}
#endif
	if (timeelapsed) {
		y += fontheight + 2;
	}
	remy = y;

	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);

	y = Scr[screen].planpos.y;
	update_plan();
#ifdef USE_MB
    backfontset = fontset;
    fontset = planfontset;
#endif
	if (*plantext) {
		for (planp = plantext; *planp; ++planp) {
			y += planfontheight + 2;
			(void) XDrawString(dsp, Scr[screen].window,
				Scr[screen].plantextgc,
				Scr[screen].planpos.x, y,
				*planp, strlen(*planp));
		}
	}
#ifdef USE_MB
    fontset = backfontset;
#endif
	XFlush(dsp);

	if (mailCmd && mailCmd[0]) {
#ifdef ORIGINAL_XPM_PATCH
		XpmImage   *mbxpm;
		mailboxInfo *mbi;
		int         x, y;

		if (system(mailCmd)) {
			mbxpm = nomail_xpmimg;
			mbi = &Scr[screen].mb.nomail;
		} else {
			mbxpm = mail_xpmimg;
			mbi = &Scr[screen].mb.mail;
		}

		if (mbxpm) {
			x = (DisplayWidth(dsp, screen) - mbxpm->width) / 2;
			y = (Scr[screen].planpos.y - 5) - mbxpm->height;
			XCopyArea(dsp, mbi->pixmap,
				  Scr[screen].window, Scr[screen].mb.mbgc,
				  0, 0,
				  mbxpm->width, mbxpm->height, x, y);
		}
#else
		mailboxInfo *mbi;
		int         mbx, mby;

		if (system(mailCmd)) {
			mbi = &Scr[screen].mb.nomail;
		} else {
			mbi = &Scr[screen].mb.mail;
		}
		if (mbi) {
			mbx = (DisplayWidth(dsp, screen) - mbi->width) / 2;
			mby = (Scr[screen].planpos.y - 5) - mbi->height;
			XSetTSOrigin(dsp, Scr[screen].mb.mbgc, mbx, mby);
			XSetStipple(dsp, Scr[screen].mb.mbgc, mbi->pixmap);
			XSetFillStyle(dsp, Scr[screen].mb.mbgc, FillOpaqueStippled);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].mb.mbgc,
			       mbx, mby, mbi->width, mbi->height);
		}
#endif
	}
	done = False;
	while (!done) {
#ifdef USE_SOUND
		if (sound && !got_invalid) {
			playSound(infosound, verbose);
		}
		got_invalid = 0;
#endif

#ifdef SAFEWORD
		pb->mode = CHALLENGE;
		(void) strcpy(pb->id, user);
		pbmain(pb);
		if (pb->status != GOOD_USER) {
			chall[0] = 0;
			return 1;
		}
		left = x = challx;
		y = chally;
		if (strlen(pb->chal) > 0) {
			(void) strcpy(chall, pb->chal);
			putText(dsp, Scr[screen].window, Scr[screen].textgc, text_chall,
				True, left, &x, &y);
			putText(dsp, Scr[screen].window, Scr[screen].textgc, " ",
				False, left, &x, &y);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				       left + XTextWidth(font, text_chall, strlen(text_chall)),
				       y - font->ascent, XTextWidth(font, chall, strlen(chall)),
				       fontheight + 2);

			putText(dsp, Scr[screen].window, Scr[screen].textgc, chall,
				False, left, &x, &y);
		}
		putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n",
			False, left, &x, &y);
		if (pb->dynpwdf || pb->fixpwdf) {
			/* If they have a dynamic passwd we do not really care about
			   the fixed password... */
			putText(dsp, Scr[screen].window, Scr[screen].textgc,
				(pb->dynpwdf) ? text_dpass : text_fpass,
				True, left, &x, &y);
			putText(dsp, Scr[screen].window, Scr[screen].textgc, " ",
				False, left, &x, &y);
			passx = x;
			passy = y;
		}
#endif

#ifdef GLOBAL_UNLOCK
		if (ReadXString(global_user, PASSLENGTH, (Bool *) NULL, False))
			break;
		(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
			unamex, unamey,
			global_user, strlen(global_user));
		checkUser(global_user);
#endif
		if (ReadXString(buffer, PASSLENGTH, &capsLock
#ifdef GLOBAL_UNLOCK
		    , True
#elif defined( USE_PAM )
		    , False
#endif
		    ))
			break;
		y = remy;

		XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel);
#ifdef SAFEWORD
		(void) strcpy(pb->dynpwd, buffer);
		pb->mode = EVALUATE_ALL;
		pbmain(pb);

		done = (pb->status == PASS || pb->status == PASS_MASTER);
		pb->mode = UPDATE_LOGS;
		pbmain(pb);
#else
		{
		    XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
			       Scr[screen].iconpos.x, y - font->ascent,
			2 * XTextWidth(font, text_invalidCapsLock, strlen(text_invalidCapsLock)),
			       fontheight + 2);
		}

		(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
		   Scr[screen].iconpos.x, y, text_valid, strlen(text_valid));
		XFlush(dsp);

		done = checkPasswd(buffer);

		if (!done && !*buffer) {
			/* just hit return, and it was not his password */
			/* count_failed++; */  /* not really an attempt, is it? */
			break;
		} else if (!done) {
			/* bad password... log it... */
			count_failed++;
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
			(void) printf("failed unlock attempt on user %s\n", user);
			syslog(SYSLOG_NOTICE, "%s: failed unlock attempt on user %s\n",
			       ProgramName, user);
#endif
		}
#endif

#ifndef VMS
                if (done && pipepassCmd && pipepassCmd[0]) {
                        FILE *f;
                        if ((f = (FILE *) popen(pipepassCmd, "w"))) {
                                /* avoid stdio for this since we don't want to leave
                                   the plaintext password in memory */
                                char *p = buffer;
                                int len = strlen(buffer);

                                while (len > 0) {
                                        int wrote = write(fileno(f), p, len);
                                        if (wrote <= 0) break;
                                        len -= wrote;
                                        p   += wrote;
                                }
                                (void) pclose(f);
                        }
                }
#endif
		/* clear plaintext password so you can not grunge around
		   /dev/kmem */
		(void) memset((char *) buffer, 0, sizeof (buffer));

		if (done) {
#ifdef USE_SOUND
			if (sound)
				playSound(validsound, verbose);
#endif
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				Scr[screen].iconpos.x, y - font->ascent,
				2 * XTextWidth(font, text_invalidCapsLock, strlen(text_invalidCapsLock)),
				fontheight + 2);

			(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
					   Scr[screen].iconpos.x, y, text_valid, strlen(text_valid));
			XFlush(dsp);

			if (!fullscreen)
				XConfigureWindow(dsp, Scr[screen].window, sizeconfiguremask,
						 &minisizeconfigure);
			return 0;
		} else {
			char *textInvalid = (capsLock) ? text_invalidCapsLock :
				text_invalid;

			XSync(dsp, True);	/* flush input buffer */
			(void) sleep(1);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				     Scr[screen].iconpos.x, y - font->ascent,
			    XTextWidth(font, text_valid, strlen(text_valid)),
				       fontheight + 2);
			(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
					   Scr[screen].iconpos.x, y, textInvalid, strlen(textInvalid));
			if (echokeys)	/* erase old echo */
				XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
					       passx, passy - font->ascent,
					       xgwa.width - passx,
					       fontheight);
			XSync(dsp, True);	/* flush input buffer */
			(void) sleep(1);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				     Scr[screen].iconpos.x, y - font->ascent,
			XTextWidth(font, textInvalid, strlen(textInvalid)),
				       fontheight + 2);
	if (count_failed > 0) {
		char * cnt = NULL;
		y += fontheight + 2;
		if (y < Scr[screen].iconpos.y + iconheight + font->ascent + 12)
			y = Scr[screen].iconpos.y + iconheight + font->ascent + 12;
		x = left = Scr[screen].iconpos.x;
		if ((cnt = (char *) malloc(strlen((count_failed == 1) ? failed_attempt : failed_attempts) + 16)) != NULL) {
		  (void) sprintf(cnt, "%d%s", count_failed, (count_failed == 1) ? failed_attempt : failed_attempts);
		  putText(dsp, Scr[screen].window, Scr[screen].textgc, cnt, False, left, &x, &y);
		  putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);
		  free(cnt);
		}
	}

#ifdef USE_SOUND
			if (sound)
				playSound(invalidsound, verbose);
			got_invalid = 1;
#endif
		}
	}
	ChangeGrabbedCursor(dsp, Scr[screen].window, mycursor);
	XUnmapWindow(dsp, Scr[screen].icon);
#ifdef USE_BUTTON_LOGOUT
	XUnmapWindow(dsp, Scr[screen].button);
#endif
	if (!fullscreen)
		XConfigureWindow(dsp, Scr[screen].window, sizeconfiguremask,
				 &minisizeconfigure);
#ifdef HAVE_SETPRIORITY
	(void) setpriority(0, 0, nicelevel);
#else /* !HAVE_SETPRIORITY */
	(void) nice(nicelevel);
#endif /* HAVE_SETPRIORITY */
#ifdef FX
	if (mesa_3Dfx_fullscreen)
		setenv("MESA_GLX_FX", "fullscreen", 0);
#endif
	return 1;
}

#endif /* WIN32 */

static int
event_screen(Display * display, Window event_win)
{
	int         i;

	for (i = startscreen; i < screens; i++) {
		if (event_win == RootWindow(display, i)) {
			return (i);
		}
	}
	return (0);
}

static int
justDisplay(Display * display)
{
	int         timetodie = False;
	int         not_done = True;
	XEvent      event;

#ifdef USE_VTLOCK
        /* EL - RCS : lock VT switching */
	/*
	 * I think it has to be done here or 'switch/restore' modes won't
         * ever be useful!
	 */
        if (!nolock && vtlock && !vtlocked)
                dovtlock();
#endif

	for (screen = startscreen; screen < screens; screen++) {
		call_init_hook((LockStruct *) NULL,
		      mode_info(display, screen, Scr[screen].window, False));
	}

	while (not_done) {
		while (!XPending(display)) {
#ifdef USE_OLD_EVENT_LOOP
			(void) usleep(delay);
			for (screen = startscreen; screen < screens; screen++)
				call_callback_hook((LockStruct *) NULL,
						   mode_info(display, screen, Scr[screen].window, False));
#ifdef USE_AUTO_LOGOUT
			checkLogout(display);
#endif
			XSync(display, False);
#else /* !USE_OLD_EVENT_LOOP */
#ifdef USE_AUTO_LOGOUT
			if (runMainLoop(30, -1) == 0)
				break;
			checkLogout(display);
#else
			(void) runMainLoop(0, -1);
#endif
#endif /* !USE_OLD_EVENT_LOOP */
		}
		(void) XNextEvent(display, &event);
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
					XRaiseWindow(display, event.xany.window);
				}
				for (screen = startscreen; screen < screens; screen++) {
					call_refresh_hook((LockStruct *) NULL,
							  mode_info(display, screen, Scr[screen].window, False));
				}
				break;

			case ConfigureNotify:
				if (!debug && !inwindow) {
					XRaiseWindow(display, event.xconfigure.window);
				}

				for (screen = startscreen; screen < screens; screen++) {
					if (install)
						fixColormap(mode_info(dsp, screen, Scr[screen].window, False), ncolors, saturation,
							mono, install, inroot, inwindow, verbose);
					if (window_size_changed(screen, Scr[screen].window)) {
						call_init_hook((LockStruct *) NULL,
							       mode_info(display, screen, Scr[screen].window, False));
					} else if (install)
						/* next line : refresh would be logical. But some modes
						 * look weird when continuing from an erased screen */
						call_refresh_hook((LockStruct *) NULL,
							mode_info(display, screen, Scr[screen].window, False));
				}
				break;

			case ButtonPress:
				if (event.xbutton.button == Button2) {
					/* call change hook only for clicked window? */
					for (screen = startscreen; screen < screens; screen++) {
						call_change_hook((LockStruct *) NULL,
								 mode_info(display, screen, Scr[screen].window,
								     False));
					}
				} else {
					screen = event_screen(display, event.xbutton.root);
					not_done = False;
				}
				break;

			case MotionNotify:
				if (mousemotion) {
					screen = event_screen(display, event.xmotion.root);
					not_done = False;
				}
				break;

			case KeyPress:
				screen = event_screen(display, event.xkey.root);
				not_done = False;
				break;
		}

		if (!nolock)
		{
		    if (lock_delay && (lock_delay <= (int) (seconds() - start_time))) {
				timetodie = True;
				/* not_done = False; */
		    }
		    else if (unlockdelay && (unlockdelay > (int) (seconds() - start_time))) 
		    {
				not_done = True;
		    }
		}
	}

	/* KLUDGE SO TVTWM AND VROOT WILL NOT MAKE XLOCK DIE */
	if (screen >= screens)
		screen = startscreen;

	lock_delay = False;
	if (usefirst)
		(void) XPutBackEvent(display, &event);
	return timetodie;
}

#ifdef __cplusplus
extern "C" {
#endif

static void
sigcatch(int signum)
{
#ifndef WIN32
	ModeInfo   *mi = mode_info(dsp, startscreen, Scr[startscreen].window, False);
	const char       *name = (mi == NULL) ? "unknown" : MI_NAME(mi);

	finish(dsp, True);
	(void) sprintf(error_buf,
		"Access control list restored.\n%s: caught signal %d while running %s mode (uid %ld).\n",
		(strlen(ProgramName) < ERROR_BUF - 120) ?
		ProgramName: "xlock", signum,
		(strlen(ProgramName) + strlen(name) < ERROR_BUF - 120) ?
		name: "?", (long) getuid());
	error(error_buf);
#endif /* !WIN32 */
}

#ifdef __cplusplus
}
#endif

#ifndef WIN32
static void
lockDisplay(Display * display, Bool do_display)
{
#ifdef USE_VTLOCK
	/* EL - RCS : lock VT switching */
	if (!nolock && vtlock && !vtlocked)
		dovtlock();
#endif
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	syslogStart();
#endif
#ifdef USE_SOUND
	if (sound && !inwindow && !inroot && !lockdelay)
		playSound(locksound, verbose);
#endif
	if (!allowaccess) {
#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 ) || defined( __CYGWIN__ )
		sigset_t    oldsigmask;
		sigset_t    newsigmask;

		(void) sigemptyset(&newsigmask);
#ifndef DEBUG
		(void) sigaddset(&newsigmask, SIGHUP);
#endif
		(void) sigaddset(&newsigmask, SIGINT);
		(void) sigaddset(&newsigmask, SIGQUIT);
		(void) sigaddset(&newsigmask, SIGTERM);
		(void) sigprocmask(SIG_BLOCK, (sigset_t *)&newsigmask, (sigset_t *)&oldsigmask);
#else
		int         oldsigmask;

#ifndef VMS
		oldsigmask = sigblock(
#ifndef DEBUG
					     sigmask(SIGHUP) |
#endif
					     sigmask(SIGINT) |
					     sigmask(SIGQUIT) |
					     sigmask(SIGTERM));
#endif
#endif

		/* (void (*)(int)) sigcatch */
#ifndef DEBUG
		(void) signal(SIGHUP, sigcatch);
#endif
		(void) signal(SIGINT, sigcatch);
		(void) signal(SIGQUIT, sigcatch);
		(void) signal(SIGTERM, sigcatch);
		/* we should trap ALL signals, especially the deadly ones */
		(void) signal(SIGSEGV, sigcatch);
		(void) signal(SIGBUS, sigcatch);
		(void) signal(SIGFPE, sigcatch);

		if (grabserver)
			XGrabServer(display);
		XGrabHosts(display);

#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 ) || defined( __CYGWIN__ )
		(void) sigprocmask(SIG_SETMASK, &oldsigmask, &oldsigmask);
#else
		(void) sigsetmask(oldsigmask);
#endif
	}
#if defined( __hpux ) || defined( __apollo )
	XHPDisableReset(display);
#endif
	do {
		if (do_display)
			(void) justDisplay(display);
		else
			do_display = True;
	} while (getPassword());
#if defined( __hpux ) || defined( __apollo )
	XHPEnableReset(display);
#endif
}
#endif /* !WIN32 */

static void
read_plan(void)
{
	FILE       *planf = (FILE *) NULL;
	char        buf[121];
	const char *home = getenv("HOME");
	char       *buffer;
	int         i, j, len = 0;

	if (!home)
		home = "";

	if ((buffer = (char *) malloc(
#if ( __VMS_VER >= 70000000 )
	255

#else
	strlen(home) + 32

#endif
	)) == NULL) {
		error("low memory for plan");
	}

#ifdef VMS
	(void) sprintf(buffer, "%s%s", home, ".xlocktext");
#else
	(void) sprintf(buffer, "%s/%s", home, ".xlocktext");
#endif
	planf = my_fopen(buffer, "r");
	if (planf == NULL) {
#ifdef VMS
		(void) sprintf(buffer, "%s%s", home, ".plan");
#else
		(void) sprintf(buffer, "%s/%s", home, ".plan");
#endif
		planf = my_fopen(buffer, "r");
	}
	if (planf == NULL) {
#ifndef VMS
		(void) sprintf(buffer, "%s/%s", home, ".signature");
#else
#if ( __VMS_VER >= 70000000 )
/* Get signature file for VMS 7.0 and higher */
		char       *buffer1;
		unsigned int ival;
		unsigned long int mail_context = 0;
		unsigned short int buflen, buflen1;
		struct itmlst_3 item[2], itm_d[1];

		if ((buffer1 = (char *) malloc(256)) == NULL) {
			error("low memory for signature");
		}
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
		free(buffer1);
#else
		(void) sprintf(buffer, "%s%s", home, ".signature");
#endif
#endif
		planf = my_fopen(buffer, "r");
	}
	if ((plantext[0] = (char *) malloc(30)) == NULL) {
		/* this is for the current time */
		error("low memory for plan");
	} else {
		plantext[0][0] = '\0';
	}
	if (planf != NULL) {
		for (i = 0; i < TEXTLINES; i++) {
			if (fgets(buf, 120, planf) && (len = strlen(buf)) > 0) {
				if (buf[len - 1] == '\n') {
					buf[--len] = '\0';
				}
				/* this expands tabs to 8 spaces */
				for (j = 0; j < len; j++) {
					if (buf[j] == '\t') {
						int         k, tab = 8 - (j % 8);

						for (k = 120 - tab; k > j; k--) {
							buf[k + tab - 1] = buf[k];
						}
						for (k = j; k < j + tab; k++) {
							buf[k] = ' ';
						}
						len += tab;
						if (len > 120)
							len = 120;
					}
				}

				if ((plantext[i + 1] = (char *) malloc(strlen(buf) + 1)) == NULL) {
					error("low memory for plan");
				}
				(void) strcpy(plantext[i + 1], buf);
			}
		}
		plantext[i + 1] = (char *) NULL;
		(void) fclose(planf);
	} else {
		plantext[1] = (char *) NULL;
	}
	free(buffer);
	buffer = (char *) NULL;
}

#ifdef USE_MB
static      XFontSet
createFontSet(Display * display, char *name)
{
	XFontSet    xfs;
	char       *def, **miss;
	int         miss_count;

#define DEF_FONTSET2 "fixed,-*-14-*"

	if ((xfs = XCreateFontSet(display, name, &miss, &miss_count, &def)) == NULL) {
		(void) fprintf(stderr, "Could not create FontSet %s\n", name);
		if ((xfs = XCreateFontSet(display, DEF_FONTSET2, &miss, &miss_count, &def)) == NULL)
			(void) fprintf(stderr, "Could not create FontSet %s\n", DEF_FONTSET2);
	}
	return xfs;
}
#endif

#ifndef WIN32

#ifdef __cplusplus
extern "C" {
#endif

static void SigUsr2(int sig);

static void
SigUsr1(int sig)
{
#ifdef DEBUG
	(void) printf("Signal %d received\n", sig);
#endif
	signalUSR1 = 1;
	signalUSR2 = 0;

	(void) signal(SIGUSR1, SigUsr1);
	(void) signal(SIGUSR2, SigUsr2);
}

static void
SigUsr2(int sig)
{
#ifdef DEBUG
	(void) printf("Signal %d received\n", sig);
#endif
	signalUSR1 = 0;
	signalUSR2 = 1;

	(void) signal(SIGUSR1, SigUsr1);
	(void) signal(SIGUSR2, SigUsr2);
}

#ifdef __cplusplus
}
#endif

int
main(int argc, char **argv)
{
	XSetWindowAttributes xswa;
	XGCValues   xgcv;
	XColor      nullcolor;
	char      **planp;
	int         tmp;
	uid_t       ruid;
	pid_t       cmd_pid = 0;

#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 )
	static sigset_t old_sigmask;

#else
	static int  old_sigmask;

#endif

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

#ifdef USE_MB
	setlocale(LC_ALL, "");
#endif

	(void) signal(SIGUSR1, SigUsr1);
	(void) signal(SIGUSR2, SigUsr2);

#if defined( __FreeBSD__ ) && !defined( DEBUG )
	/* do not exit on FPE */
	fpsetmask(0);
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
#ifdef DEBUG
	ProgramPID = getpid();	/* for memcheck.c */
#endif
#if 1
	SRAND((long) start_time);	/* random mode needs the seed set. */
#else
	SRAND((long) ProgramPID);
#endif

	getResources(&dsp, argc, argv);

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

#if 1
	/* Change 1 to 0 */
#error	UNTESTED CODE, COMMENT OUT AND SEE IF IT WORKS, PLEASE GET BACK TO ME
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
#ifdef BAD_PAM
/* BAD_PAM must have root access to authenticate against shadow passwords */
      (void) seteuid(ruid);
/* for BAD_PAM to use shadow passwords, must call seteuid() later */
/* this prevent xlock from dropping privileges when using PAM modules, */
/* that needs root rights (pam_unix e.g.) */
#else

#ifdef USE_VTLOCK
	/* In order to lock VT switch we must issue an ioctl on console */
	/* (VT_LOCKSWITCH). This ioctl MUST be issued by root. */
	/* We need later to be able to do another seteuid(0), so let's */
	/* disable the overwrite of saved uid/gid */

	if (!vtlock)
#endif
	{
		(void) setgid(rgid);
		(void) setuid(ruid);
	}
#endif
#endif

#if 0
	/* synchronize -- so I am aware of errors immediately */
	/* Too slow only for debugging */
	(void) printf("DEBUGGING: XSynchronize version\n");
	XSynchronize(dsp, True);
#endif

#ifdef USE_MB
	fontset = createFontSet(dsp, fontsetname);
	XmbTextExtents(fontset, "Aj", 1, NULL, &mbRect);
	planfontset = createFontSet(dsp, planfontsetname);
	XmbTextExtents(planfontset, "Aj", 1, NULL, &planmbRect);
#endif
	checkResources();

	font = XLoadQueryFont(dsp, fontname);
	if (font == NULL) {
		(void) fprintf(stderr, "%s: can not find font: %s, using %s...\n",
			       ProgramName, fontname, FALLBACK_FONTNAME);
		font = XLoadQueryFont(dsp, FALLBACK_FONTNAME);
		if (font == NULL) {
			(void) sprintf(error_buf,
				"%s: can not even find %s!!!\n",
				(strlen(ProgramName) < ERROR_BUF - 80) ?
				ProgramName : "xlock",
				(strlen(ProgramName) + strlen(FALLBACK_FONTNAME) < ERROR_BUF - 80) ?
				FALLBACK_FONTNAME: "a font");
			error(error_buf);
		}
	} {
		int         flags, x, y;
		unsigned int w, h;

#ifdef FX
		if (!glgeometry || !*glgeometry) {
			glwidth = DEF_GLW;
			glheight = DEF_GLH;
		} else {
			flags = XParseGeometry(glgeometry, &x, &y, &w, &h);
			glwidth = flags & WidthValue ? w : DEF_GLW;
			glheight = flags & HeightValue ? h : DEF_GLH;
			if (glwidth < MINICONW)
				glwidth = MINICONW;
			if (glheight < MINICONH)
				glheight = MINICONH;
		}
#endif
		if (!icongeometry || !*icongeometry) {
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
		if (!geometry || !*geometry) {
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

	planfont = XLoadQueryFont(dsp, planfontname);
	if (planfont == NULL) {
		(void) fprintf(stderr, "%s: can't find font: %s, using %s...\n",
			       ProgramName, planfontname, FALLBACK_FONTNAME);
		planfont = XLoadQueryFont(dsp, FALLBACK_FONTNAME);
		if (planfont == NULL) {
			(void) sprintf(error_buf,
				"%s: can not even find %s!!!\n",
				(strlen(ProgramName) < ERROR_BUF - 80) ?
				ProgramName : "xlock",
				(strlen(ProgramName) + strlen(FALLBACK_FONTNAME) < ERROR_BUF - 80) ?
				FALLBACK_FONTNAME: "a font");
			error(error_buf);
		}
	}
	read_plan();

	screens = ScreenCount(dsp);
	if (((modeinfo = (ModeInfo *) calloc(screens,
			sizeof (ModeInfo))) == NULL) ||
	    ((Scr = (ScreenInfo *) calloc(screens,
			sizeof (ScreenInfo))) == NULL)) {
		error("low memory for info");
	}

#ifdef FORCESINGLE
	/* Safer to keep this after the calloc in case ScreenCount is used */
	startscreen = DefaultScreen(dsp);
	screens = startscreen + 1;
#endif

#if defined( USE_SOUND ) && defined( USE_ESOUND )
        sound = (sound && (init_sound() != -1));
#endif

#ifdef USE_DTSAVER
	/* The CDE Session Manager provides the windows for the screen saver
	   to draw into. */
	if (dtsaver) {
		Window     *saver_wins;
		int         num_wins;
		int         this_win;
		int         this_screen;
		XWindowAttributes xgwa;


		/* Get the list of requested windows */
		if (!DtSaverGetWindows(dsp, &saver_wins, &num_wins)) {
			(void) sprintf(error_buf,
				"%s: Unable to get screen saver info.\n",
				(strlen(ProgramName) < ERROR_BUF - 80) ?
				ProgramName : "xlock");
			error(error_buf);
		}
		for (this_win = 0; this_win < num_wins; this_win++) {
			(void) XGetWindowAttributes(dsp, saver_wins[this_win], &xgwa);
			this_screen = XScreenNumberOfScreen(xgwa.screen);
			if (Scr[this_screen].window != None) {
				(void) sprintf(error_buf,
					"%s: Two windows on screen %d\n",
					(strlen(ProgramName) < ERROR_BUF - 80) ?
					ProgramName : "xlock", this_screen);
				error(error_buf);
			}
			Scr[this_screen].window = saver_wins[this_win];
		}
		/* Reduce to the screens that have windows.  Avoid problems and */
		/* assume that if one fails there is only one good window. */
		for (this_screen = startscreen; this_screen < screens; this_screen++) {
			if (Scr[this_screen].window == None) {
				startscreen = DefaultScreen(dsp);
				screens = startscreen + 1;
				break;
			}
		}
	} else
#endif

	if (inwindow) {
		/* Reduce to the last screen requested */
		startscreen = DefaultScreen(dsp);
		screens = startscreen + 1;
	}
	for (screen = startscreen; screen < screens; screen++) {
		Screen     *scr = ScreenOfDisplay(dsp, screen);
		Colormap    cmap = (Colormap) NULL;

/* Start of MI_ROOT_PIXMAP hack */
		Window 			temp_rw;
		XGCValues		temp_gcv;
		GC			temp_gc;
		XWindowAttributes	temp_xgwa;

		temp_rw = (parentSet) ? parent : RootWindowOfScreen(scr);
		(void) XGetWindowAttributes(dsp, temp_rw, &temp_xgwa);
		temp_gcv.function = GXcopy;
		temp_gcv.subwindow_mode = IncludeInferiors;
		temp_gc = XCreateGC(dsp, temp_rw, GCFunction|GCSubwindowMode,
				   &temp_gcv);
		Scr[screen].root_pixmap = XCreatePixmap(dsp, temp_rw,
						   temp_xgwa.width,
						   temp_xgwa.height,
						   temp_xgwa.depth);
		XCopyArea(dsp, temp_rw, Scr[screen].root_pixmap,
			  temp_gc, 0, 0, temp_xgwa.width, temp_xgwa.height,
			  0, 0);
		XFreeGC(dsp, temp_gc);

/* End of MI_ROOT_PIXMAP hack */

#ifdef ORIGINAL_XPM_PATCH
		if (mailIcon && mailIcon[0]) {
			if ((mail_xpmimg = (XpmImage *) malloc(sizeof (XpmImage))))
				if (XpmReadFileToXpmImage(mailIcon, mail_xpmimg,
					   (XpmInfo *) NULL) != XpmSuccess) {
					free(mail_xpmimg);
					mail_xpmimg = NULL;
				}
		}
		if (nomailIcon && nomailIcon[0]) {
			if ((nomail_xpmimg = (XpmImage *) malloc(sizeof (XpmImage))))
				if (XpmReadFileToXpmImage(nomailIcon, nomail_xpmimg,
					   (XpmInfo *) NULL) != XpmSuccess) {
					free(nomail_xpmimg);
					nomail_xpmimg = NULL;
				}
		}
#endif

		Scr[screen].root = (parentSet) ? parent : RootWindowOfScreen(scr);
		defaultVisualInfo(dsp, screen);

/*-
 * Some window managers like fvwm and tvtwm do not like it when an application
 * thinks it can install a full screen colormap (i.e xlock).  It promptly
 * deinstalls the colormap and this might lead to white backgrounds if
 * CopyFromParent is not used.
 * But if CopyFromParent is used one can not change the visual
 * and one may get White = Black on PseudoColor (see bug mode).
 * There is another spot in xlock.c like this one...
 * As far as I can tell, this problem exists only if your using MesaGL.
 */
#if (defined( USE_GL ) && (!defined( MESA ) || defined( REALGLX ))) || !defined( COMPLIANT_COLORMAP )
		Scr[screen].colormap = cmap = xswa.colormap =
			XCreateColormap(dsp, Scr[screen].root, Scr[screen].visual, AllocNone);
#else
		cmap = DefaultColormapOfScreen(scr);
		Scr[screen].colormap = None;
#endif

		xswa.override_redirect = True;
		xswa.background_pixel = Scr[screen].black_pixel;
		xswa.border_pixel = Scr[screen].white_pixel;
		xswa.event_mask = KeyPressMask | ButtonPressMask |
			VisibilityChangeMask | ExposureMask | StructureNotifyMask;
		if (mousemotion)
			xswa.event_mask |= MotionNotify;

#ifdef USE_VROOT
		if (inroot) {
			Scr[screen].window = Scr[screen].root;
			XChangeWindowAttributes(dsp, Scr[screen].window, CWBackPixel, &xswa);
			/* this gives us these events from the root window */
			XSelectInput(dsp, Scr[screen].window,
				     VisibilityChangeMask | ExposureMask);
		} else
#endif
#ifdef USE_DTSAVER
		if (!dtsaver)
#endif
		{
#define WIDTH (inwindow? WidthOfScreen(scr)/2 : (debug? WidthOfScreen(scr) - 100 : WidthOfScreen(scr)))
#define HEIGHT (inwindow? HeightOfScreen(scr)/2 : (debug? HeightOfScreen(scr) - 100 : HeightOfScreen(scr)))
#if (defined( USE_GL ) && (!defined( MESA ) || defined( REALGLX ))) || !defined( COMPLIANT_COLORMAP )
#define CWMASK (((debug||inwindow||inroot)? 0 : CWOverrideRedirect) | CWBackPixel | CWBorderPixel | CWEventMask | CWColormap)
#else
#define CWMASK (((debug||inwindow||inroot)? 0 : CWOverrideRedirect) | CWBackPixel | CWBorderPixel | CWEventMask)
#endif

#if (defined( USE_GL ) && (!defined( MESA ) || defined( REALGLX ))) || !defined( COMPLIANT_COLORMAP )
#define XLOCKWIN_DEPTH (Scr[screen].depth)
#define XLOCKWIN_VISUAL (Scr[screen].visual)
#else
#define XLOCKWIN_DEPTH CopyFromParent
#define XLOCKWIN_VISUAL CopyFromParent
#endif

			if (fullscreen) {
				Scr[screen].window = XCreateWindow(dsp, Scr[screen].root, 0, 0,
								   (unsigned int) WIDTH, (unsigned int) HEIGHT, 0,
				XLOCKWIN_DEPTH, InputOutput, XLOCKWIN_VISUAL,
							      CWMASK, &xswa);
			} else {
				sizeconfiguremask = CWX | CWY | CWWidth | CWHeight;
				Scr[screen].fullsizeconfigure.x = 0;
				Scr[screen].fullsizeconfigure.y = 0;
				Scr[screen].fullsizeconfigure.width = WIDTH;
				Scr[screen].fullsizeconfigure.height = HEIGHT;
				Scr[screen].window = XCreateWindow(dsp, Scr[screen].root,
						   (int) minisizeconfigure.x,
						   (int) minisizeconfigure.y,
				      (unsigned int) minisizeconfigure.width,
				     (unsigned int) minisizeconfigure.height,
								   0, XLOCKWIN_DEPTH, InputOutput, XLOCKWIN_VISUAL,
							      CWMASK, &xswa);
			}
			XmbSetWMProperties(dsp, Scr[screen].window,
				"xlock","xlock", NULL, 0, NULL, NULL,
				&xclasshint);
		}
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
				Scr[screen].none_pixel = allocPixel(dsp, cmap, none3d, DEF_NONE3D);
				Scr[screen].right_pixel = allocPixel(dsp, cmap, right3d, DEF_RIGHT3D);
				Scr[screen].left_pixel = allocPixel(dsp, cmap, left3d, DEF_LEFT3D);
				Scr[screen].both_pixel = allocPixel(dsp, cmap, both3d, DEF_BOTH3D);

			} else {
				/*
				   * attention: the mixture of colours will only be guaranteed, if
				   * the right black is used.  The problems with BlackPixel would
				   * be that BlackPixel | left_pixel need not be equal to left_pixel.
				   * The same holds for rightcol (of course). That is why the right
				   * black (black3dcol) must be used when GXor is used as put
				   * function.  I have allocated four colors above:
				   * pixels[0],                                - 3d black
				   * pixels[0] | planemasks[0],                - 3d right eye color
				   * pixels[0] | planemasks[1],                - 3d left eye color
				   * pixels[0] | planemasks[0] | planemasks[1] - 3d white
				 */

				if (!XParseColor(dsp, cmap, none3d, &C))
					(void) XParseColor(dsp, cmap, DEF_NONE3D, &C);
				Scr[screen].none_pixel = C.pixel = pixels[0];
				XStoreColor(dsp, cmap, &C);

				if (!XParseColor(dsp, cmap, right3d, &C))
					(void) XParseColor(dsp, cmap, DEF_RIGHT3D, &C);
				Scr[screen].right_pixel = C.pixel = pixels[0] | planemasks[0];
				XStoreColor(dsp, cmap, &C);

#ifdef CALCULATE_BOTH
				C2.red = C.red;
				C2.green = C.green;
				C2.blue = C.blue;
#else
				if (!XParseColor(dsp, cmap, left3d, &C))
					(void) XParseColor(dsp, cmap, DEF_LEFT3D, &C);
#endif
				Scr[screen].left_pixel = C.pixel = pixels[0] | planemasks[1];
				XStoreColor(dsp, cmap, &C);

#ifdef CALCULATE_BOTH
				C.red |= C2.red;	/* or them together... */
				C.green |= C2.green;
				C.blue |= C2.blue;
#else
				if (!XParseColor(dsp, cmap, both3d, &C))
					(void) XParseColor(dsp, cmap, DEF_BOTH3D, &C);
#endif
				Scr[screen].both_pixel = C.pixel =
					pixels[0] | planemasks[0] | planemasks[1];
				XStoreColor(dsp, cmap, &C);
			}

		}
		fixColormap(mode_info(dsp, screen, Scr[screen].window, False), ncolors, saturation,
			    mono, install, inroot, inwindow, verbose);
		if (debug || inwindow) {
			XWMHints    xwmh;

			xwmh.flags = InputHint;
			xwmh.input = True;
			XChangeProperty(dsp, Scr[screen].window,
			       XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
					(unsigned char *) &xwmh, sizeof (xwmh) / sizeof (int));
		}
		if (debug) {
			Scr[screen].iconpos.x = (DisplayWidth(dsp, screen) - 100 -
#ifdef HAVE_KRB5
						 MAX(512, MAX(XTextWidth(font, text_info, strlen(text_info)), XTextWidth(font, text_krbinfo, strlen(text_krbinfo))))) / 2;
#else /* HAVE_KRB5 */
						 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
#endif /* HAVE_KRB5 */
			Scr[screen].iconpos.y = (DisplayHeight(dsp, screen) - 100) / 6;
		} else {
			Scr[screen].iconpos.x = (DisplayWidth(dsp, screen) -
#ifdef HAVE_KRB5
						 MAX(512, MAX(XTextWidth(font, text_info, strlen(text_info)), XTextWidth(font, text_krbinfo, strlen(text_krbinfo))))) / 2;
#else /* HAVE_KRB5 */
						 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
#endif /* HAVE_KRB5 */
			Scr[screen].iconpos.y = DisplayHeight(dsp, screen) / 6;
		}

		Scr[screen].planpos.x = Scr[screen].planpos.y = 0;

		for (planp = plantext; *planp; ++planp) {
			tmp = XTextWidth(planfont, *planp, strlen(*planp));
			if (tmp > Scr[screen].planpos.x)
				Scr[screen].planpos.x = tmp;
			++Scr[screen].planpos.y;
		}

		if (debug) {
			Scr[screen].planpos.x = (DisplayWidth(dsp, screen) - 100 -
						 Scr[screen].planpos.x) / 2;
			Scr[screen].planpos.y = DisplayHeight(dsp, screen) - 100 -
				(Scr[screen].planpos.y + 4) * (planfontheight + 2);
		} else {
			Scr[screen].planpos.x = (DisplayWidth(dsp, screen) -
						 Scr[screen].planpos.x) / 2;
			Scr[screen].planpos.y = DisplayHeight(dsp, screen) -
				(Scr[screen].planpos.y + 4) * (planfontheight + 2);
		}

		xswa.border_pixel = Scr[screen].white_pixel;
		xswa.background_pixel = Scr[screen].black_pixel;
		xswa.event_mask = ButtonPressMask;
#if (defined( USE_GL ) && (!defined( MESA ) || defined( REALGLX ))) || !defined( COMPLIANT_COLORMAP )
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask | CWColormap
#else
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask
#endif
		if (nolock)
			Scr[screen].icon = None;
		else {
			Scr[screen].icon = XCreateWindow(dsp, Scr[screen].window,
				Scr[screen].iconpos.x, Scr[screen].iconpos.y,
			      iconwidth, iconheight, 1, (int) CopyFromParent,
						 InputOutput, CopyFromParent,
							 CIMASK, &xswa);
#ifdef USE_BUTTON_LOGOUT
			{
				char       *buf;
				int         w, h;

				if ((buf = (char *) malloc(strlen(logoutButtonLabel) +
						 3)) == NULL) {
					w = (strlen(logoutButtonLabel) + 5) * 8;
				} else {
					(void) sprintf(buf, " %s ", logoutButtonLabel);
					w = XTextWidth(font, buf, strlen(buf));
					free(buf);
				}
				h = fontheight + 2;
				Scr[screen].button = XCreateWindow(dsp, Scr[screen].window,
					 0, 0, w, h, 1, (int) CopyFromParent,
						 InputOutput, CopyFromParent,
							      CIMASK, &xswa);
			}
#endif
		}
		XMapWindow(dsp, Scr[screen].window);
		XRaiseWindow(dsp, Scr[screen].window);

#if 0
		if (install && cmap != DefaultColormapOfScreen(scr))
			setColormap(dsp, Scr[screen].window, cmap, inwindow);
#endif

		xgcv.font = font->fid;
		xgcv.foreground = Scr[screen].white_pixel;
		xgcv.background = Scr[screen].black_pixel;
		Scr[screen].gc = XCreateGC(dsp, Scr[screen].window,
				GCFont | GCForeground | GCBackground, &xgcv);

		xgcv.foreground = Scr[screen].fg_pixel;
		xgcv.background = Scr[screen].bg_pixel;
		Scr[screen].textgc = XCreateGC(dsp, Scr[screen].window,
				GCFont | GCForeground | GCBackground, &xgcv);
		xgcv.font = planfont->fid;
		Scr[screen].plantextgc = XCreateGC(dsp, Scr[screen].window,
				GCFont | GCForeground | GCBackground, &xgcv);

#ifdef ORIGINAL_XPM_PATCH
		if (mail_xpmimg) {
			XpmAttributes xpm_attr;

			xpm_attr.valuemask = 0;
			XpmCreatePixmapFromXpmImage(dsp, Scr[screen].window,
				mail_xpmimg, &Scr[screen].mb.mail.pixmap,
				&Scr[screen].mb.mail.bitmap, &xpm_attr);
		}
		if (nomail_xpmimg) {
			XpmAttributes xpm_attr;

			xpm_attr.valuemask = 0;
			XpmCreatePixmapFromXpmImage(dsp, Scr[screen].window, nomail_xpmimg,
					       &Scr[screen].mb.nomail.pixmap,
				   &Scr[screen].mb.nomail.bitmap, &xpm_attr);
		}
		if (mail_xpmimg || nomail_xpmimg) {
			Scr[screen].mb.mbgc = XCreateGC(dsp, Scr[screen].window,
					GCFont | GCForeground | GCBackground,
							&xgcv);
		}
#else
		if (mailCmd && mailCmd[0]) {
			pickPixmap(dsp, Scr[screen].window, nomailIcon,
				   NOMAIL_WIDTH, NOMAIL_HEIGHT, NOMAIL_BITS,
				   &(Scr[screen].mb.nomail.width),
				   &(Scr[screen].mb.nomail.height),
				   &(Scr[screen].mb.nomail.pixmap),
				   &(Scr[screen].mb.nomail.graphics_format));
			pickPixmap(dsp, Scr[screen].window, mailIcon,
				   MAIL_WIDTH, MAIL_HEIGHT, MAIL_BITS,
				   &(Scr[screen].mb.mail.width),
				   &(Scr[screen].mb.mail.height),
				   &(Scr[screen].mb.mail.pixmap),
				   &(Scr[screen].mb.mail.graphics_format));
			Scr[screen].mb.mbgc = XCreateGC(dsp, Scr[screen].window,
					GCFont | GCForeground | GCBackground,
							&xgcv);
		}
#endif
	}
	lockc = XCreateBitmapFromData(dsp, Scr[startscreen].root, no_bits, 1, 1);
	lockm = XCreateBitmapFromData(dsp, Scr[startscreen].root, no_bits, 1, 1);
	mycursor = XCreatePixmapCursor(dsp, lockc, lockm,
				       &nullcolor, &nullcolor, 0, 0);
	XFreePixmap(dsp, lockc);
	XFreePixmap(dsp, lockm);

	if (!grabmouse || inwindow || inroot) {
		nolock = 1;
		enablesaver = 1;
	} else if (!debug) {
		GrabKeyboardAndMouse(dsp, Scr[startscreen].window);
	}
	if (!nolock) {
		XGetScreenSaver(dsp, &sstimeout, &ssinterval,
				&ssblanking, &ssexposures);
		if (resetsaver)
			XResetScreenSaver(dsp);		/* make sure not blank now */
		if (!enablesaver)
			XSetScreenSaver(dsp, 0, 0, 0, 0);	/* disable screen saver */
	}
#ifdef USE_DPMS
	if ((dpmsstandby >= 0) || (dpmssuspend >= 0) || (dpmsoff >= 0))
		SetDPMS(dsp, dpmsstandby, dpmssuspend, dpmsoff);
#endif
#ifdef HAVE_SETPRIORITY
	(void) setpriority(0, 0, nicelevel);
#else /* !HAVE_SETPRIORITY */
	(void) nice(nicelevel);
#endif /* HAVE_SETPRIORITY */

	(void) XSetIOErrorHandler(xio_error);

	/* start the command startcmd */
	if (startCmd && *startCmd) {

		if ((cmd_pid = FORK()) == -1) {
			(void) fprintf(stderr, "Failed to launch \"%s\"\n", startCmd);
			perror(ProgramName);
			cmd_pid = 0;
		} else if (!cmd_pid) {
#ifndef VMS
			(void) setpgid(0, 0);
#endif
#if 0
#if (defined(sun) && defined(__svr4__)) || defined(sgi) || defined(__hpux) || defined(linux)
			setsid();
#else
			setpgrp(getpid(), getpid());
#endif
#endif
#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 ) || defined( __CYGWIN__ )
			(void) sigprocmask(SIG_SETMASK, (sigset_t *)&old_sigmask, (sigset_t *)&old_sigmask);
#else
			(void) sigsetmask(old_sigmask);
#endif
			(void) system(startCmd);
			exit(0);
		}
	}
	lock_delay = lockdelay;
	if (nolock) {
		(void) justDisplay(dsp);
	} else if (lock_delay) {
		if (justDisplay(dsp)) {
			lockDisplay(dsp, False);
		} else
			nolock = 1;
	} else {
		lockDisplay(dsp, True);
	}
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	if (!nolock) {
		syslogStop(XDisplayString(dsp));
		closelog();
	}
#endif
#ifdef USE_DPMS
	SetDPMS(dsp, -1, -1, -1);
#endif
	finish(dsp, True);

	if (cmd_pid)
#if defined(NO_KILLPG) || defined(__svr4__) || defined(sgi) || defined(__hpux) || defined(VMS)
		kill(-cmd_pid, SIGTERM);
#else
		(void) killpg(cmd_pid, SIGTERM);
#endif
	if (endCmd && *endCmd) {
		if ((cmd_pid = FORK()) == -1) {
			(void) fprintf(stderr, "Failed to launch \"%s\"\n", endCmd);
			perror(ProgramName);
			cmd_pid = 0;
		} else if (!cmd_pid) {
			(void) system(endCmd);
			exit(0);
		}
	}

#if defined( USE_SOUND ) && defined( USE_ESOUND )
        shutdown_sound();
        sound = 0;
#endif

#ifdef VMS
	return 1;
#else
	return 0;
#endif
}
#endif


#ifdef USE_PAM
/* PAM_putText - method to have pam_converse functionality with in XLOCK */
void PAM_putText( const struct pam_message *msg, struct pam_response *resp, Bool PAM_echokeys )
{
  int x = 50, y = 50;
  int oldX, oldY;
  int left;
  char buffer[PASSLENGTH];

  x = left = Scr[screen].iconpos.x;
  y = Scr[screen].iconpos.y + font->ascent + 200;

#ifdef DEBUG
    (void) printf( "PAM_putText: message of style %d received: (%s)\n", msg->msg_style, msg->msg );
#endif
  if( ( msg->msg_style == PAM_PROMPT_ECHO_ON ) ||
      ( msg->msg_style == PAM_PROMPT_ECHO_OFF ) )
  {
    XFlush(dsp);
    XSync( dsp, True );  /* Flush Input Buffer */

    putText(dsp, Scr[startscreen].window, Scr[startscreen].textgc, msg->msg, True, left, &x, &y);
    putText(dsp, Scr[startscreen].window, Scr[startscreen].textgc, " ", True, left, &x, &y);

    unamex = x;
    unamey = y;

    if( ReadXString(buffer, PASSLENGTH, (Bool *) NULL
#ifdef GLOBAL_UNLOCK
		    , True
#elif defined( USE_PAM )
		    , PAM_echokeys
#endif
		    ));

    XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel);

    resp->resp = buffer;
    resp->resp_retcode = PAM_SUCCESS;

#ifdef DEBUG
    (void) printf( "Received Username: (%s)\n", resp->resp );
#endif
  }
  else
  {
    XFlush( dsp );
    /* Clears three lines of text before displaying the next message */
    oldX = x;
    oldY = y;
    XFillRectangle(dsp, Scr[startscreen].window, Scr[startscreen].gc,
		   Scr[screen].iconpos.x, y - font->ascent,
		   XTextWidth(font, msg->msg, strlen(msg->msg)),
		   (fontheight + 2) * 3);
    XSync( dsp, True );
    (void) sleep(1);
    /* display the message */
    putText(dsp, Scr[startscreen].window, Scr[startscreen].textgc, msg->msg, True, left, &x, &y);
    putText(dsp, Scr[startscreen].window, Scr[startscreen].textgc, " ", True, left, &x, &y);

    x = oldX;
    y = oldY;

    XSync( dsp, True );
    (void) sleep(3);

    XFlush( dsp );
    /* Clears three lines of text before displaying the next message */
    XFillRectangle(dsp, Scr[startscreen].window, Scr[startscreen].gc,
		   Scr[screen].iconpos.x, y - font->ascent,
		   XTextWidth(font, msg->msg, strlen(msg->msg)),
		   (fontheight + 2) * 3);
    XSync( dsp, True );
  }
}
#endif

#if defined( __hpux ) || defined( __apollo )
int
_main(int argc, char **argv)
{
  main(argc, argv);
}
#endif

#ifdef HAVE_KRB5
/* like putText, but takes a font argument */
static void
putTextFont(Display * display, Window window, XFontStruct *pfont, GC gc,
	    const char *string, int bold, int left, int *px, int *py)
				/* which window */
				/* font */
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
			(void) XDrawImageString(display, window, gc, x, y, s, len);
			if (bold)
				(void) XDrawString(display, window, gc, x + 1, y, s, len);
		}
		if (!last) {
			y += pfont->ascent + pfont->descent + 2;
			x = left;
		} else {
			if (len)
				x += XTextWidth(pfont, s, len);
			break;
		}
		p++;
	}
	*px = x;
	*py = y;
}

/*
 * Our Kerberos callback prompter.  Note that the widgets aren't exactly
 * pretty ... but see if I care.  We're assuming there won't be any embedded
 * newlines in the prompt strings (but we handle embedded newlines in
 * the banner string).
 */

krb5_error_code
xlock_prompter(krb5_context context, void *data, const char *name,
	       const char *banner, int numprompts, krb5_prompt prompts[])
{
	Window parent = Scr[screen].window;
	Window win, button;
	GC gc;
	XGCValues xgcv;
	int lines = 0, max = 0, width, i, height, bwidth, bheight, x, y,
	    leave = 0, cp = 0, count;
	XWindowAttributes xgwa;
	XSetWindowAttributes xswa;
	XEvent event;
	KeySym keysym;
	char buffer[20];
	char *ok = "OK", *c;
	const char *b;

	struct _promptinfo {
		int x;
		int y;
		int curx;
		int rp;
	} *pi = NULL;

	if (numprompts > 0)
		if (!(pi = malloc(sizeof(*pi) * numprompts)))
			return ENOMEM;

	/*
	 * Let's see what our longest line is (handle embedded newlines,
	 * but only in the banner)
	 */

	if (banner && banner[0] != NULL) {
		for (b = banner, c = index(banner, '\n'); c != NULL;
		     b = c, c = index(c + 1, '\n')) {
			width = XTextWidth(planfont, b, c - b);
			if (width > max)
				max = width;
			lines++;
		}
		width = XTextWidth(planfont, b, strlen(b));
		if (width > max)
			max = width;
		lines++;
	}

	for (i = 0; i < numprompts; i++) {
		/* 32 asterisks for entry should be enough */
		width = XTextWidth(planfont, prompts[i].prompt,
				   strlen(prompts[i].prompt)) +
			XTextWidth(planfont,
				   ": ********************************", 34);
		if (width > max)
			max = width;
		lines++;
	}

	/*
	 * we're saying a 10 pixel border around the text.  We add in
	 * 40 to the height because of the OK button
	 */

	height = (lines + 1) * (planfontheight + 2) + 40;
	width = max + 20;

	(void) XGetWindowAttributes(dsp, Scr[screen].window, &xgwa);

	if (width > xgwa.width) {
		x = 0;
		width = xgwa.width;
	} else
		x = (xgwa.width - width) / 2;

	if (height > xgwa.height) {
		y = 0;
		height = xgwa.height;
	} else
		y = (xgwa.height - height) / 2;

	xswa.override_redirect = True;
	xswa.border_pixel = Scr[screen].white_pixel;
	xswa.background_pixel = Scr[screen].white_pixel;
	xswa.event_mask = KeyPressMask;

	/*
	 * Create the main dialog window
	 */

	win = XCreateWindow(dsp, parent, x, y, width, height, 2,
			    CopyFromParent, CopyFromParent, Scr[screen].visual,
			    CWOverrideRedirect | CWBackPixel | CWBorderPixel |
			    CWEventMask, &xswa);

	XMapWindow(dsp, win);
	XRaiseWindow(dsp, win);

	/*
	 * Create the OK button window
	 */

	bheight = planfontheight + 10;
	bwidth = XTextWidth(planfont, ok, strlen(ok)) + 10;

	xswa.border_pixel = Scr[screen].black_pixel;
	xswa.event_mask = ButtonPressMask;

	button = XCreateWindow(dsp, win, (width - bwidth) / 2,
		 	       height - bheight - 5, bwidth, bheight, 2,
			       CopyFromParent, CopyFromParent,
			       Scr[screen].visual, CWOverrideRedirect |
			       CWBackPixel | CWBorderPixel | CWEventMask,
			       &xswa);

	XMapWindow(dsp, button);
	XRaiseWindow(dsp, button);

	x = 10;
	y = 10 + planfont->ascent;

	xgcv.font = planfont->fid;
	xgcv.foreground = Scr[screen].black_pixel;
	xgcv.background = Scr[screen].white_pixel;
	gc = XCreateGC(dsp, Scr[screen].window,
		       GCFont | GCForeground | GCBackground, &xgcv);

	if (banner && banner[0] != NULL)
		putTextFont(dsp, win, planfont, gc, banner, 1, x, &x, &y);

	for (i = 0; i < numprompts; i++) {
		x = 10;
		y += planfontheight + 2;
		putTextFont(dsp, win, planfont, gc, prompts[i].prompt, 1,
			    x, &x, &y);
		putTextFont(dsp, win, planfont, gc, ": ", 1, x, &x, &y);
		pi[i].x = pi[i].curx = x;
		pi[i].y = y;
		pi[i].rp = 0;
		memset(prompts[i].reply->data, 0, prompts[i].reply->length);
	}

	x = 5;
	y = 5 + planfont->ascent;

	putTextFont(dsp, button, planfont, gc, ok, 1, x, &x, &y);

	while (leave == 0) {
		XNextEvent(dsp, &event);

		switch (event.type) {

		case ButtonPress:
			if (event.xbutton.window == button)
				leave = 1;
			break;

		case KeyPress:
			count = XLookupString((XKeyEvent *) &event, buffer,
					      sizeof(buffer), &keysym, NULL);

			/*
			 * Try to handle specific keysyms
			 */

			if (keysym == XK_Return || keysym == XK_KP_Enter ||
			    keysym == XK_Linefeed) {
				if (++cp >= numprompts)
					leave = 1;
				break;
			}

			/*
			 * Ignore modifier keys
			 */

			if (keysym >= XK_Shift_L && keysym <= XK_Hyper_R)
				break;

			/*
			 * If we don't do any prompts, then we only accept
			 * Return/Enter/Linefeed
			 */

			if (numprompts == 0) {
				XBell(dsp, 100);
				break;
			}

			/*
			 * Handle Tab/Shift-Tab
			 */

			if (keysym == XK_Tab) {
				if (!(event.xkey.state & ShiftMask)) {
					if (++cp >= numprompts)
						cp = 0;
				} else {
					if (--cp < 0)
						cp = numprompts - 1;
				}
				break;
			}

			/*
			 * Handle BS/Delete
			 */

			if (keysym == XK_BackSpace || keysym == XK_Delete) {
				if (pi[cp].rp == 0) {
					XBell(dsp, 100);
					break;
				}
				prompts[cp].reply->data[--pi[cp].rp] = '\0';
				if (pi[cp].rp < 32) {
					pi[cp].curx -= XTextWidth(planfont,
								  "*", 1);
					XClearArea(dsp, win, pi[cp].curx,
						   pi[cp].y - planfont->ascent,
						   0, planfont->ascent +
						   planfont->descent, False);
				}
				break;
			}

			/*
			 * Handle keypresses
			 */

			for (i = 0; i < count; i++) {
				switch (buffer[i]) {
				case '\003':
				case '\025':
					XClearArea(dsp, win, pi[cp].x,
						   pi[cp].y - planfont->ascent,
						   0, planfont->ascent +
						   planfont->descent, False);
					memset(prompts[cp].reply->data, 0,
					       prompts[cp].reply->length);
					pi[cp].curx = pi[cp].x;
					pi[cp].rp = 0;
					break;
				default:
					if (pi[cp].rp + 1 >=
					    prompts[cp].reply->length) {
						XBell(dsp, 100);
						break;
					}
					prompts[cp].reply->data[pi[cp].rp++] =
								buffer[i];
					if (pi[cp].rp <= 32) {
						putTextFont(dsp, win, planfont,
							    gc, "*", 1,
							    pi[cp].curx,
							    &pi[cp].curx,
							    &pi[cp].y);
					}
					break;
				}
			}

			break;

		default:
			/* Nothing */
			break;

		}

	}

	if (pi)
		free(pi);
	XDestroyWindow(dsp, win);
	XFreeGC(dsp, gc);

	for (i = 0; i < numprompts; i++)
		prompts[i].reply->length = strlen(prompts[i].reply->data);

	return 0;
}
#endif /* HAVE_KRB5 */

#ifdef WIN32
#define MAX_TIMES 100
int showtext = 0;

void
hsbramp(double h1, double s1, double b1, double h2, double s2, double b2,
    int count, unsigned char *red, unsigned char *green, unsigned char *blue);

static void CalculateColors(int noofcolors, double saturation)
{

	/* free any previously allocated colormap */
	if (red != NULL)
		free(red);
	if (green != NULL)
		free(green);
	if (blue != NULL)
		free(blue);

	/* create a new colormap */
	if ((red = calloc(noofcolors+2, sizeof(unsigned char))) == NULL)
		return;
	if ((green = calloc(noofcolors+2, sizeof(unsigned char))) == NULL)
		return;
	if ((blue = calloc(noofcolors+2, sizeof(unsigned char))) == NULL)
		return;

	/* set the number of colors in the colormap */
	colorcount = noofcolors;

	/* populate the colormap */
	hsbramp(0.0, saturation, 1.0, 1.0, saturation, 1.0, noofcolors,
		    red, green, blue);

	/* store white and black colormap entries */
	red[noofcolors] = green[noofcolors] = blue[noofcolors] = 255;
	red[noofcolors+1] = green[noofcolors+1] = blue[noofcolors+1] = 0;
}

void xlockmore_set_mode_options(ModeSpecOpt *ms);

/*-
 *  xlockmore_create
 *    called when WIN32 event handling issues a create window
 */
unsigned int xlockmore_create(void)
{
	int i;
	ModeInfo *mi; /* get mode info */
	ModeSpecOpt *ms;

    /* seeds random number generation */
	SRAND((unsigned)time(NULL));
/*#define _DEBUG*/
#ifdef RANDOMMODE
	randommode = numprocs-1; /* random */
#else
#ifdef _DEBUG
	/* these numbers may not be reliable if any modes were added */
	randommode = 49; /* maze */
	randommode = 56; /* poly */
	randommode = 40; /* life1d */
	randommode = 80; /* voters */
	randommode = 85; /* xjack */
	randommode = 2; /* apollonian */
#else
	randommode = NRAND(numprocs);
#endif
#endif

	/* set variables */
	delay = LockProcs[randommode].def_delay;
	count = LockProcs[randommode].def_count;
	cycles = LockProcs[randommode].def_cycles;
	size = LockProcs[randommode].def_size;
	saturation = LockProcs[randommode].def_saturation;
	bitmap = LockProcs[randommode].def_bitmap;

	/* calculate new color map */
	CalculateColors(NUMCOLORS, saturation);

    /* allocate mem to the ModeInfo & ScreenInfo structures */
	screens = ScreenCount(dsp);
	if (((modeinfo = (ModeInfo *) calloc(screens,
			sizeof (ModeInfo))) == NULL) ||
	    ((Scr = (ScreenInfo *) calloc(screens,
			sizeof (ScreenInfo))) == NULL)) {
		error("low memory for info");
	}

	/* set some WIN32 specific stuff */
	mi = mode_info(dsp, 0, (int)hwnd, 0);
	MI_COLORMAP_SIZE(mi) = ncolors = mi->screeninfo->npixels = colorcount;
	MI_GC(mi) = GCCreate();
	MI_NUM_SCREENS(mi) = screens;

	for (screen = startscreen; screen < screens; screen++) {
		fixColormap(mode_info(dsp, screen, Scr[screen].window, False), ncolors, saturation,
				    mono, install, inroot, inwindow, verbose);
	}

	for (i=0; i<NUMCOLORS; i++)
		mi->screeninfo->pixels[i] = i;

	/* set default options for mode */
	ms = LockProcs[randommode].msopt;
	xlockmore_set_mode_options(ms);

	/* initialise mode */
	call_init_hook(&LockProcs[randommode], mi);

	showtext = 0;
	return delay;
}

/*-
 *  xlockmore_destroy
 *    called when WIN32 event handling issues a destroy window
 */
void xlockmore_destroy(void)
{
	call_release_hook(&LockProcs[randommode], mode_info(dsp, 0, (int)hwnd, 0));
}

#ifdef _DEBUG
static char debug_text[255] = { 0 };
#endif

/*-
 *  xlockmore_timer
 *    called when WIN32 event handling issues a timer request
 */
unsigned int xlockmore_timer(void)
{
	call_callback_hook(&LockProcs[randommode], mode_info(dsp, 0, (int)hwnd, 0));

#ifndef RANDOMMODE
  if (showtext < MAX_TIMES)
  {
  	xlockmore_win32_text(10, 10, LockProcs[randommode].cmdline_arg);
    xlockmore_win32_text(10, 30, LockProcs[randommode].desc);
    showtext++;
  }
#endif
#ifdef _DEBUG
  xlockmore_win32_text(10, 60, debug_text);
#endif

	return (LockProcs[randommode].def_delay / 1000);
}

/*-
 *  xlockmore_set_mode_options
 *    called from create. Sets any default options
 */
void xlockmore_set_mode_options(ModeSpecOpt *ms)
{
	int i;
	char **varChar;
	float *varFloat;
	int *varInt;

	for (i=0; i < ms->numvarsdesc; i++)
	{
		switch (ms->vars[i].type)
		{
		case t_Bool:
			varInt = (int *)ms->vars[i].var;
			if (strcmp(ms->vars[i].def, "True") == 0)
				*varInt = TRUE;
			else
				*varInt = FALSE;
			break;
		case t_Float:
			varFloat = (float *)ms->vars[i].var;
			*varFloat = atof(ms->vars[i].def);
			break;
		case t_Int:
			varInt = (int *)ms->vars[i].var;
			*varInt = atoi(ms->vars[i].def);
			break;
		case t_String:
			varChar = (char *)ms->vars[i].var;
			*varChar = ms->vars[i].def;
			break;
		}
	}
}

void xlockmore_win32_text(int xloc, int yloc, char *text)
{
	RECT rect;
	UINT nFlags = SetTextAlign(hdc, TA_RIGHT);

	(void) GetClientRect(hwnd, &rect);
	TextOut(hdc, rect.right - xloc, yloc, text, strlen(text));
	SetTextAlign(hdc, nFlags);
}

//	FILE *fp = NULL;
void xlockmore_set_debug(char *text)
{
#ifdef _DEBUG
	static FILE *fp = NULL;

	strcpy(debug_text, text);

#if 1
	if (fp == NULL)
	{
		fp = fopen("xlock_debug.log", "a");
		fprintf(fp, "\n");
	}

	fprintf(fp, "%s\n", text);
#endif
#endif
}

#endif /* WIN32 */

#endif /* STANDALONE */
