#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xlock.c	4.07 97/11/24 xlockmore";

#endif

/*-
 * xlock.c - X11 client to lock a display and show a screen saver.
 *
 * Copyright (c) 1988-91 by Patrick J. Naughton.
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
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

#ifdef STANDALONE


/*-
 * xscreensaver compatibility layer for xlockmore modules.
 * xscreensaver, Copyright (c) 1997 Jamie Zawinski <jwz@netscape.com>
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
 * By Jamie Zawinski <jwz@netscape.com> on 10-May-97; based on the ideas
 * in the older xlock.h by Charles Hannum <mycroft@ai.mit.edu>.  (I had
 * to redo it, since xlockmore has diverged so far from xlock...)
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "screenhack.h"
#include "mode.h"

#define countof(x) (sizeof((x))/sizeof(*(x)))

extern ModeSpecOpt xlockmore_opts[];
extern const char *app_defaults;
char       *message, *messagefile, *messagesfile, *program;
char       *messagefontname;
int         neighbors;


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
			new->option = (char *) malloc(strlen(old->option) + 5);
			(void) strcpy(new->option, "-no-");
			(void) strcat(new->option, old->option + 1);
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
		 "-text", "-filename", "-program", "-neighbors",
		 "-wireframe", "-use3d", "-mouse", "-fullrandom", "-verbose"};

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
					new->specifier = options[i - 1].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-use3d")) {
					new->option = "-3d";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-3d";
					new->specifier = options[i - 1].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-mouse")) {
					new->option = "-mouse";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-mouse";
					new->specifier = options[i - 1].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-fullrandom")) {
					new->option = "-fullrandom";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-fullrandom";
					new->specifier = options[i - 1].specifier;
					new->argKind = XrmoptionNoArg;
					new->value = "False";
				} else if (!strcmp(new->option, "-verbose")) {
					new->option = "-verbose";
					new->argKind = XrmoptionNoArg;
					new->value = "True";
					new = &options[i++];
					new->option = "-no-verbose";
					new->specifier = options[i - 1].specifier;
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
	s = (char *) malloc(50);
	(void) strcpy(s, progclass);
	(void) strcat(s, ".background: black");
	defaults[i++] = s;

	s = (char *) malloc(50);
	(void) strcpy(s, progclass);
	(void) strcat(s, ".foreground: white");
	defaults[i++] = s;

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
		s = (char *) malloc(strlen(xlockmore_opts->vars[j].name) +
				    strlen(def) + 10);
		(void) strcpy(s, "*");
		(void) strcat(s, xlockmore_opts->vars[j].name);
		(void) strcat(s, ": ");
		(void) strcat(s, def);
		defaults[i++] = s;
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
	int         orig_pause;

	(void) memset((char *) &mi, 0, sizeof (mi));
	mi.dpy = dpy;
	mi.window = window;
	XGetWindowAttributes(dpy, window, &mi.xgwa);

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

		mi.colors = (XColor *) calloc(mi.npixels, sizeof (*mi.colors));

		mi.writable_p = want_writable_colors;

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

		if (mi.npixels <= 2)
			goto MONO;
		else {
			int         i;

			mi.pixels = (unsigned long *)
				calloc(mi.npixels, sizeof (*mi.pixels));
			for (i = 0; i < mi.npixels; i++)
				mi.pixels[i] = mi.colors[i].pixel;
		}
	}

	gcv.foreground = mi.white;
	gcv.background = mi.black;
	mi.gc = XCreateGC(dpy, window, GCForeground | GCBackground, &gcv);

	mi.pause = get_integer_resource("delay", "Usecs");

	mi.batchcount = get_integer_resource("count", "Int");
	mi.cycles = get_integer_resource("cycles", "Int");
	mi.size = get_integer_resource("size", "Int");
	mi.ncolors = get_integer_resource("ncolors", "Int");
	mi.bitmap = get_string_resource("bitmap", "String");
	messagefontname = get_string_resource("font", "String");
	message = get_string_resource("text", "String");
	messagefile = get_string_resource("filename", "String");
	messagesfile = get_string_resource("fortunefile", "String");
	program = get_string_resource("program", "String");
	neighbors = get_integer_resource("neighbors", "Int");


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

	mi.mouse = get_boolean_resource("mouse", "Boolean");
	mi.wireframe_p = get_boolean_resource("wireframe", "Boolean");
	mi.fullrandom = get_boolean_resource("fullrandom", "Boolean");
	mi.verbose = get_boolean_resource("verbose", "Boolean");

	mi.root_p = (window == RootWindowOfScreen(mi.xgwa.screen));


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
		hack_draw(&mi);
		XSync(dpy, False);
		if (mi.pause)
			usleep(mi.pause);
		mi.pause = orig_pause;

		if (hack_free) {
			if (i++ > (mi.batchcount / 4) &&
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
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H && defined(AIXV3)
#include <sys/select.h>
#endif
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
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
#include <X11/extensions/dpms.h>
extern unsigned char DPMSQueryExtension(Display *, int *, int *);
extern int  DPMSGetTimeouts(Display *, unsigned short *, unsigned short *, unsigned short *);
extern int  DPMSSetTimeouts(Display *, unsigned short, unsigned short, unsigned short);
extern int  dpmsstandby;
extern int  dpmssuspend;
extern int  dpmsoff;

#endif

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

extern void defaultVisualInfo(Display * display, int screen);

#ifdef USE_OLD_EVENT_LOOP
extern int  usleep(unsigned int);

#endif

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
extern void logoutUser(Display * display);

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

#ifdef DEBUG
pid_t       ProgramPID;		/* for memcheck.c */

#endif
ScreenInfo *Scr = NULL;

/* This is a private data structure, don't touch */
static ModeInfo *modeinfo = NULL;

Window      parent;
Bool        parentSet = False;
Display    *dsp = NULL;		/* server display connection */

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
extern Bool echokeys;
extern Bool enablesaver;
extern Bool fullrandom;
extern Bool grabmouse;
extern Bool grabserver;
extern Bool install;
extern Bool mousemotion;
extern Bool timeelapsed;
extern Bool usefirst;
extern Bool verbose;
extern Bool remote;
extern int  nicelevel;
extern int  lockdelay;
extern int  timeout;
extern Bool wireframe;
extern Bool use3d;
extern Bool mouse;

extern char *fontname;
extern char *planfontname;

#ifdef USE_MB
XFontSet    fontset;
extern char *fontsetname;
XRectangle  mbRect;

#endif
extern char *background;
extern char *foreground;
extern char *text_user;
extern char *text_pass;
extern char *text_info;
extern char *text_valid;
extern char *text_invalid;
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
extern void play_sound(char *string);
extern Bool sound;

#endif

extern char *startCmd;
extern char *endCmd;
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
static char *plantext[TEXTLINES + 1];	/* Message is stored here */

/* GEOMETRY STUFF */
static int  sizeconfiguremask;
static XWindowChanges minisizeconfigure;
static int  fullscreen = False;

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
static int  tried_logout = 0;

#endif

#ifdef USE_SOUND
static int  got_invalid = 0;

#endif

#if (defined( SYSV ) || defined( SVR4 )) && defined( SOLARIS2 ) && !defined( HAVE_STRUCT_SIGSET_T )
#if !defined( __cplusplus ) && !defined( c_plusplus ) && !defined ( _SIGSET_T )
#define _SIGSET_T
typedef struct {		/* signal set type */
	unsigned long __sigbits[4];
} sigset_t;

 /* This is good for gcc compiled on Solaris-2.5 but used on Solaris-2.6 */

#endif
#endif

#ifdef ORIGINAL_XPM_PATCH
#if defined( USE_XPM ) || defined( USE_XPMINC )
#if USE_XPMINC
#include <xpm.h>
#else
#include <X11/xpm.h>		/* Normal spot */
#endif
#endif

static XpmImage *mail_xpmimg = NULL;
static XpmImage *nomail_xpmimg = NULL;

#else
#include <bitmaps/mailempty.xbm>
#include <bitmaps/mailfull.xbm>

#define NOMAIL_WIDTH   mailempty_width
#define NOMAIL_HEIGHT  mailempty_height
#define NOMAIL_BITS    mailempty_bits
#define MAIL_WIDTH   mailfull_width
#define MAIL_HEIGHT  mailfull_height
#define MAIL_BITS    mailfull_bits
#endif

int         signalUSR1 = 0;
int         signalUSR2 = 0;
char        old_default_mode[20] = "";
void        SigUsr1(int sig);
void        SigUsr2(int sig);

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
syslogStop(char *displayName)
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
	       pw->pw_name, gr->gr_name, displayName, mins, secs);
}

#endif

void
error(char *buf)
{
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	extern Display *dsp;

	syslog(SYSLOG_WARNING, buf);
	if (!nolock) {
		if (strstr(buf, "unable to open display") == NULL)
			syslogStop(XDisplayString(dsp));
		else
			syslogStop("unknown display");
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
	char       *buf = NULL;

	status = XGrabKeyboard(display, window, True,
			       GrabModeAsync, GrabModeAsync, CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabKeyboard(display, window, True,
				  GrabModeAsync, GrabModeAsync, CurrentTime);

		if (status != GrabSuccess) {
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf, "%s, could not grab keyboard! (%d)\n",
				       ProgramName, status);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	}
	status = XGrabPointer(display, window, True, AllPointerEventMask,
			      GrabModeAsync, GrabModeAsync, None, mycursor,
			      CurrentTime);
	if (status != GrabSuccess) {
		(void) sleep(1);
		status = XGrabPointer(display, window, True, AllPointerEventMask,
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
ChangeGrabbedCursor(Display * display, Window window, Cursor cursor)
{
	if (!debug && grabmouse && !inwindow && !inroot)
		(void) XGrabPointer(display, window, True, AllPointerEventMask,
		    GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);
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
		MI_SET_FLAG_STATE(mi, WI_FLAG_INSTALL, install);
		MI_SET_FLAG_STATE(mi, WI_FLAG_DEBUG, debug);
		MI_SET_FLAG_STATE(mi, WI_FLAG_USE3D, use3d &&
			    !(mono || CellsOfScreen(MI_SCREENPTR(mi)) <= 2));
		MI_SET_FLAG_STATE(mi, WI_FLAG_VERBOSE, verbose);
		MI_SET_FLAG_STATE(mi, WI_FLAG_FULLRANDOM, fullrandom);
		MI_SET_FLAG_STATE(mi, WI_FLAG_WIREFRAME, wireframe);
		MI_SET_FLAG_STATE(mi, WI_FLAG_MOUSE, mouse);
		MI_SET_FLAG_STATE(mi, WI_FLAG_INFO_INITTED, True);
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


/* Restore all grabs, reset screensaver, restore colormap, close connection. */
void
finish(Display * display, Bool closeDisplay)
{
	int         screen;

	for (screen = startscreen; screen < screens; screen++) {
		if (Scr[screen].window != None) {
			release_last_mode(mode_info(display, screen, Scr[screen].window, False));
		}
		if (Scr[screen].icon != None) {
			release_last_mode(mode_info(display, screen, Scr[screen].icon, True));
		}
	}
#ifdef ORIGINAL_XPM_PATCH
	if (mail_xpmimg)
		(void) free((void *) mail_xpmimg);
	if (nomail_xpmimg)
		(void) free((void *) nomail_xpmimg);
#endif

	XSync(display, False);
#ifdef USE_VROOT
	if (inroot)
		XClearWindow(display, Scr[startscreen].window);
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
#ifndef __sgi
  /*-
   * The following line seems to cause a core dump on the SGI.
   * More work is needed on the cleanup code.
   */
	if (closeDisplay)
		(void) XCloseDisplay(display);
#endif
	(void) nice(0);
}

static int
xio_error(Display * d)
{
#ifdef USE_DTSAVER
	if (dtsaver) {		/* this is normal when run as -dtsaver */
		/* X connection to :0.0 broken (explicit kill or server shutdown). */
		exit(0);
	}
#endif
	error("xio_error\n");
	return ((debug) ? 0 : 1);	/* suppress message unless debugging */
}

/* Convenience function for drawing text */
static void
putText(Display * display, Window window, GC gc,
	char *string, int bold, int left, int *px, int *py)
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
		putText(dsp, Scr[scr].window, Scr[scr].textgc, buf, False, left, &x, &y);
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
			putText(dsp, Scr[scr].window, Scr[scr].textgc, buf, False, left, &x, &y);
		} else {
			putText(dsp, Scr[scr].window, Scr[scr].textgc,
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
				XUnmapWindow(dsp, Scr[scr].button);
				XSetForeground(dsp, Scr[scr].gc, Scr[scr].bg_pixel);
				XFillRectangle(dsp, Scr[scr].window, Scr[scr].gc, left, y,
					       xgwa.width - left,
				     5 + 2 * (font->ascent + font->descent));
				XSetForeground(dsp, Scr[scr].gc, Scr[scr].fg_pixel);

				if (tried_logout) {
					putText(dsp, Scr[scr].window, Scr[scr].textgc, logoutFailedString,
						True, left, &x, &y);
				} else {
					XMoveWindow(dsp, Scr[scr].button, left, y);
					XMapWindow(dsp, Scr[scr].button);
					XRaiseWindow(dsp, Scr[scr].button);
					(void) sprintf(buf, " %s ", logoutButtonLabel);
					XSetForeground(dsp, Scr[scr].gc, Scr[screen].white_pixel);
					(void) XDrawString(dsp, Scr[scr].button, Scr[scr].gc,
							   0, font->ascent + 1, buf, strlen(buf));
					XSetForeground(dsp, Scr[scr].gc, Scr[scr].fg_pixel);
					y += 5 + 2 * font->ascent + font->descent;
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
			(void) sprintf(buf, "%d minute%s until auto-logout.    \n",
				       tmp, (tmp == 1) ? "" : "s");
		else
			(void) sprintf(buf, "%d:%02d hours until auto-logout.  \n",
				       tmp / 60, tmp % 60);
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
		logoutUser(display);
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

#ifdef USE_NEW_EVENT_LOOP
	struct timeval loopStart, timeSinceLoop;
	unsigned long lastIter = 0, desiredIter = 0;

	GETTIMEOFDAY(&loopStart);
#endif

	first.tv_sec = first.tv_usec = 0;
	elapsed.tv_sec = elapsed.tv_usec = 0;
	repeat.tv_sec = delay / 1000000;
	repeat.tv_usec = delay % 1000000;

	started = seconds();

	for (;;) {
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

		FD_ZERO(&reads);
		FD_SET(fd, &reads);

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
				if (cbwin != None) {
					mi = mode_info(dsp, screen, cbwin, iconic);
					call_callback_hook((LockStruct *) NULL, mi);
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
#ifdef USE_NEW_EVENT_LOOP
		}
#endif
		/* if (mindelay) usleep(mindelay); */

		/* get the time now, figure how long it took */
		GETTIMEOFDAY(&elapsed);
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

			if (timeout < (int) (seconds() - lasteventtime)) {
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
				XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel);
				if (echokeys) {
					(void) memset((char *) pwbuf, '?', slen);
					XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
						 passx, passy - font->ascent,
					       XTextWidth(font, pwbuf, slen),
					       font->ascent + font->descent);
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
					logoutUser(dsp);
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
				if (!debug && !inwindow)
					XRaiseWindow(dsp, event.xconfigure.window);
				if (window_size_changed(screen, Scr[screen].window)) {
					call_init_hook((LockStruct *) NULL,
						       mode_info(dsp, screen, Scr[screen].window, False));
				}
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
	int         screen = MI_SCREEN(mi);
	XWindowAttributes xgwa;

	(void) XGetWindowAttributes(dsp, Scr[screen].window, &xgwa);

	x = left = Scr[screen].iconpos.x;
	y = Scr[screen].iconpos.y - font->ascent + font->descent + 2;

	/*XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel); */
	XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
		       x, y - font->ascent,
		       xgwa.width - x, font->ascent + font->descent + 2);

	putText(dsp, Scr[screen].window, Scr[screen].textgc, MI_NAME(mi),
		True, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, ": ", True, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, MI_DESC(mi),
		False, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);
}

static int
getPassword(void)
{
	XWindowAttributes xgwa;
	int         x, y, left, done, remy;
	char        buffer[PASSLENGTH];
	char      **planp;
	char       *hostbuf = NULL;
	ModeInfo   *mi = &modeinfo[screen];

#ifdef SAFEWORD
	char        chall[PASSLENGTH];

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
	(void) nice(0);
	if (!fullscreen)
		XConfigureWindow(dsp, Scr[screen].window, sizeconfiguremask,
				 &(Scr[screen].fullsizeconfigure));


	(void) XGetWindowAttributes(dsp, Scr[screen].window, &xgwa);

	ChangeGrabbedCursor(dsp, Scr[screen].window,
			    XCreateFontCursor(dsp, XC_left_ptr));

	XSetForeground(dsp, Scr[screen].gc, Scr[screen].bg_pixel);
	XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
		       0, 0, xgwa.width, xgwa.height);

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
		x = ((xgwa.width / 2) - (mbxpm->width / 2));
		y = ((Scr[screen].planpos.y - 5) - mbxpm->height);

		if (mbxpm) {
			XCopyArea(dsp, mbi->pixmap,
				  Scr[screen].window, Scr[screen].mb.mbgc,
				  0, 0,
				  mbxpm->width, mbxpm->height, x, y);
		}
#else
		mailboxInfo *mbi;
		int         x, y;

		if (system(mailCmd)) {
			mbi = &Scr[screen].mb.nomail;
		} else {
			mbi = &Scr[screen].mb.mail;
		}
		x = ((xgwa.width / 2) - (mbi->width / 2));
		y = ((Scr[screen].planpos.y - 5) - mbi->height);
		XSetTSOrigin(dsp, Scr[screen].mb.mbgc, x, y);
		XSetStipple(dsp, Scr[screen].mb.mbgc, mbi->pixmap);
		XSetFillStyle(dsp, Scr[screen].mb.mbgc, FillOpaqueStippled);
		XFillRectangle(dsp, Scr[screen].window, Scr[screen].mb.mbgc,
			       x, y, mbi->width, mbi->height);
#endif
	}
	XMapWindow(dsp, Scr[screen].icon);
	XRaiseWindow(dsp, Scr[screen].icon);

	modeDescription(mi);

	x = left = Scr[screen].iconpos.x + iconwidth + font->max_bounds.width;
	y = Scr[screen].iconpos.y + font->ascent;

	if ((hostbuf = (char *) malloc(strlen(user) + strlen(hostname) + 2)) != NULL) {
		(void) sprintf(hostbuf, "%s@%s", user, hostname);

		putText(dsp, Scr[screen].window, Scr[screen].textgc, text_user, True, left, &x, &y);
		putText(dsp, Scr[screen].window, Scr[screen].textgc, hostbuf, False, left, &x, &y);
		(void) free((void *) hostbuf);
	}
	if (displayname && displayname[0] != ':') {
		char       *displaybuf = NULL;
		char       *colon = (char *) strchr(displayname, ':');
		int         n = colon - displayname;

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
			(void) free((void *) displaybuf);
		}
	}
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);

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
	y += font->ascent + font->descent + 6;
	if (y < Scr[screen].iconpos.y + iconheight + font->ascent + 12)
		y = Scr[screen].iconpos.y + iconheight + font->ascent + 12;
	x = left = Scr[screen].iconpos.x;
	putText(dsp, Scr[screen].window, Scr[screen].textgc, text_info, False, left, &x, &y);
	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);

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

	putText(dsp, Scr[screen].window, Scr[screen].textgc, "\n", False, left, &x, &y);

	y = Scr[screen].planpos.y;
	if (*plantext) {
		for (planp = plantext; *planp; ++planp) {
#ifdef USE_MB
			y += mbRect.height + 2;
#else
			y += planfont->ascent + planfont->descent + 2;
#endif
			(void) XDrawString(dsp, Scr[screen].window, Scr[screen].plantextgc,
			   Scr[screen].planpos.x, y, *planp, strlen(*planp));
		}
	}
	XFlush(dsp);

	done = False;
	while (!done) {
#ifdef USE_SOUND
		if (sound && !got_invalid) {
			play_sound(infosound);
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
				       font->ascent + font->descent + 2);

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
		if (ReadXString(buffer, PASSLENGTH))
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
		XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
			       Scr[screen].iconpos.x, y - font->ascent,
			XTextWidth(font, text_invalid, strlen(text_invalid)),
			       font->ascent + font->descent + 2);

		(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
		   Scr[screen].iconpos.x, y, text_valid, strlen(text_valid));
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
#endif
		/* clear plaintext password so you can not grunge around
		   /dev/kmem */
		(void) memset((char *) buffer, 0, sizeof (buffer));

		if (done) {
#ifdef USE_SOUND
			if (sound)
				play_sound(validsound);
#endif
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				     Scr[screen].iconpos.x, y - font->ascent,
			XTextWidth(font, text_invalid, strlen(text_invalid)),
				       font->ascent + font->descent + 2);

			(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
					   Scr[screen].iconpos.x, y, text_valid, strlen(text_valid));
			XFlush(dsp);

			if (!fullscreen)
				XConfigureWindow(dsp, Scr[screen].window, sizeconfiguremask,
						 &minisizeconfigure);
			return 0;
		} else {
			XSync(dsp, True);	/* flush input buffer */
			(void) sleep(1);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				     Scr[screen].iconpos.x, y - font->ascent,
			    XTextWidth(font, text_valid, strlen(text_valid)),
				       font->ascent + font->descent + 2);
			(void) XDrawString(dsp, Scr[screen].window, Scr[screen].textgc,
					   Scr[screen].iconpos.x, y, text_invalid, strlen(text_invalid));
			if (echokeys)	/* erase old echo */
				XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
					       passx, passy - font->ascent,
					       xgwa.width - passx,
					       font->ascent + font->descent);
			XSync(dsp, True);	/* flush input buffer */
			(void) sleep(1);
			XFillRectangle(dsp, Scr[screen].window, Scr[screen].gc,
				     Scr[screen].iconpos.x, y - font->ascent,
			XTextWidth(font, text_invalid, strlen(text_invalid)),
				       font->ascent + font->descent + 2);
#ifdef USE_SOUND
			if (sound)
				play_sound(invalidsound);
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
	(void) nice(nicelevel);
#ifdef FX
	if (mesa_3Dfx_fullscreen)
		setenv("MESA_GLX_FX", "fullscreen", 0);
#endif
	return 1;
}

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
				if (!debug && !inwindow)
					XRaiseWindow(display, event.xconfigure.window);
				for (screen = startscreen; screen < screens; screen++) {
					if (window_size_changed(screen, Scr[screen].window)) {
						call_init_hook((LockStruct *) NULL,
							       mode_info(display, screen, Scr[screen].window, False));
					}
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

		if (!nolock && lock_delay &&
		    (lock_delay <= (int) (seconds() - start_time))) {
			timetodie = True;
			not_done = False;
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


static void
sigcatch(int signum)
{
	ModeInfo   *mi = mode_info(dsp, startscreen, Scr[startscreen].window, False);
	char       *name = (mi == NULL) ? "unknown" : MI_NAME(mi);
	char       *buf;


	finish(dsp, True);
	buf = (char *) malloc(strlen(ProgramName) + strlen(name) + 160);
	(void) sprintf(buf,
		       "Access control list restored.\n%s: caught signal %d while running %s mode (uid %ld).\n",
		       ProgramName, signum, name, (long) getuid());
	error(buf);
	(void) free((void *) buf);	/* Should never get here */
}

static void
lockDisplay(Display * display, Bool do_display)
{
#if defined( HAVE_SYSLOG_H ) && defined( USE_SYSLOG )
	syslogStart();
#endif
#ifdef USE_SOUND
	if (sound && !inwindow && !inroot && !lockdelay)
		play_sound(locksound);
#endif
	if (!allowaccess) {
#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 )
		sigset_t    oldsigmask;
		sigset_t    newsigmask;

		(void) sigemptyset(&newsigmask);
#ifndef DEBUG
		(void) sigaddset(&newsigmask, SIGHUP);
#endif
		(void) sigaddset(&newsigmask, SIGINT);
		(void) sigaddset(&newsigmask, SIGQUIT);
		(void) sigaddset(&newsigmask, SIGTERM);
		(void) sigprocmask(SIG_BLOCK, &newsigmask, &oldsigmask);
#else
		int         oldsigmask;

#ifndef VMS
		extern int  sigblock(int);
		extern int  sigsetmask(int);

		oldsigmask = sigblock(
#ifndef DEBUG
					     sigmask(SIGHUP) |
#endif
					     sigmask(SIGINT) |
					     sigmask(SIGQUIT) |
					     sigmask(SIGTERM));
#endif
#endif

#ifndef DEBUG
		(void) signal(SIGHUP, (void (*)(int)) sigcatch);
#endif
		(void) signal(SIGINT, (void (*)(int)) sigcatch);
		(void) signal(SIGQUIT, (void (*)(int)) sigcatch);
		(void) signal(SIGTERM, (void (*)(int)) sigcatch);
		/* we should trap ALL signals, especially the deadly ones */
		(void) signal(SIGSEGV, (void (*)(int)) sigcatch);
		(void) signal(SIGBUS, (void (*)(int)) sigcatch);
		(void) signal(SIGFPE, (void (*)(int)) sigcatch);

		if (grabserver)
			XGrabServer(display);
		XGrabHosts(display);

#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 )
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

static void
read_plan()
{
	FILE       *planf = NULL;
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
		planf = my_fopen(buffer, "r");
	}
	if (planf != NULL) {
		for (i = 0; i < TEXTLINES; i++) {
			if (fgets(buf, 120, planf)) {
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

				plantext[i] = (char *) malloc(strlen(buf) + 1);
				(void) strcpy(plantext[i], buf);
			}
		}
		plantext[i] = NULL;
		(void) fclose(planf);
	} else {
		plantext[0] = NULL;
	}
	(void) free((void *) buffer);
	buffer = NULL;
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

int
main(int argc, char **argv)
{
	XSetWindowAttributes xswa;
	XGCValues   xgcv;
	XColor      nullcolor;
	char      **planp;
	char       *buf;
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

#if 0
	/* synchronize -- so I'm aware of errors immediately */
	/* Too slow only for debugging */
	XSynchronize(dsp, True);
#endif

#ifdef USE_MB
	fontset = createFontSet(dsp, fontsetname);
	XmbTextExtents(fontset, "A", 1, NULL, &mbRect);
#endif
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
			buf = (char *) malloc(strlen(ProgramName) +
					      strlen(FALLBACK_FONTNAME) + 80);
			(void) sprintf(buf,
				       "%s: can not even find %s!!!\n", ProgramName, FALLBACK_FONTNAME);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
	}
	read_plan();

	screens = ScreenCount(dsp);
	modeinfo = (ModeInfo *) calloc(screens, sizeof (ModeInfo));
	Scr = (ScreenInfo *) calloc(screens, sizeof (ScreenInfo));

#ifdef FORCESINGLE
	/* Safer to keep this after the calloc in case ScreenCount is used */
	screens = 1;
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
			buf = (char *) malloc(strlen(ProgramName) + 80);
			(void) sprintf(buf,
				       "%s: Unable to get screen saver info.\n", ProgramName);
			error(buf);
			(void) free((void *) buf);	/* Should never get here */
		}
		for (this_win = 0; this_win < num_wins; this_win++) {
			(void) XGetWindowAttributes(dsp, saver_wins[this_win], &xgwa);
			this_screen = XScreenNumberOfScreen(xgwa.screen);
			if (Scr[this_screen].window != None) {
				buf = (char *) malloc(strlen(ProgramName) + 80);
				(void) sprintf(buf,
					       "%s: Two windows on screen %d\n", ProgramName, this_screen);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			}
			Scr[this_screen].window = saver_wins[this_win];
		}
#if 0
		for (this_screen = screens - 1; this_screen >= 0; this_screen--) {
			if (Scr[this_screen].window != None)
				break;
			screens--;
		}
#endif
	}
#endif

	if (
#ifdef USE_DTSAVER
		   dtsaver ||
#endif
		   inwindow) {
		/* Reduce to the last screen requested */
		int         this_screen = DefaultScreen(dsp);

		screens = this_screen + 1;
		startscreen = this_screen;
	}
	for (screen = startscreen; screen < screens; screen++) {
		Screen     *scr = ScreenOfDisplay(dsp, screen);
		Colormap    cmap = (Colormap) NULL;

#ifdef ORIGINAL_XPM_PATCH
		if (mailIcon && mailIcon[0]) {
			if ((mail_xpmimg = (XpmImage *) malloc(sizeof (XpmImage))))
				if (XpmReadFileToXpmImage(mailIcon, mail_xpmimg,
					   (XpmInfo *) NULL) != XpmSuccess) {
					(void) free((void *) mail_xpmimg);
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
		Scr[screen].colormap = cmap = xswa.colormap =
			XCreateColormap(dsp, Scr[screen].root, Scr[screen].visual, AllocNone);

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
#define CWMASK (((debug||inwindow||inroot)? 0 : CWOverrideRedirect) | CWBackPixel | CWBorderPixel | CWEventMask | CWColormap)
#define XLOCKWIN_DEPTH (Scr[screen].depth)
#define XLOCKWIN_VISUAL (Scr[screen].visual)

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
		}
		if (debug || inwindow) {
			XWMHints    xwmh;

			xwmh.flags = InputHint;
			xwmh.input = True;
			XChangeProperty(dsp, Scr[screen].window,
			       XA_WM_HINTS, XA_WM_HINTS, 32, PropModeReplace,
					(unsigned char *) &xwmh, sizeof (xwmh) / sizeof (int));
		}
		fixColormap(dsp, Scr[screen].window, screen, ncolors, saturation,
			    mono, install, inroot, inwindow, verbose);

		if (debug) {
			Scr[screen].iconpos.x = (DisplayWidth(dsp, screen) - 100 -
						 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
			Scr[screen].iconpos.y = (DisplayHeight(dsp, screen) - 100) / 6;
		} else {
			Scr[screen].iconpos.x = (DisplayWidth(dsp, screen) -
						 MAX(512, XTextWidth(font, text_info, strlen(text_info)))) / 2;
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
#ifdef USE_MB
				(Scr[screen].planpos.y + 4) * (mbRect.height + 2);
#else
				(Scr[screen].planpos.y + 4) * (planfont->ascent + planfont->descent + 2);
#endif
		} else {
			Scr[screen].planpos.x = (DisplayWidth(dsp, screen) -
						 Scr[screen].planpos.x) / 2;
			Scr[screen].planpos.y = DisplayHeight(dsp, screen) -
#ifdef USE_MB
				(Scr[screen].planpos.y + 4) * (mbRect.height + 2);
#else
				(Scr[screen].planpos.y + 4) * (planfont->ascent + planfont->descent + 2);
#endif
		}

		xswa.border_pixel = Scr[screen].white_pixel;
		xswa.background_pixel = Scr[screen].black_pixel;
		xswa.event_mask = ButtonPressMask;
#define CIMASK CWBorderPixel | CWBackPixel | CWEventMask | CWColormap
		if (nolock)
			Scr[screen].icon = None;
		else {
			Scr[screen].icon = XCreateWindow(dsp, Scr[screen].window,
				Scr[screen].iconpos.x, Scr[screen].iconpos.y,
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
				Scr[screen].button = XCreateWindow(dsp, Scr[screen].window,
					       0, 0, w, h, 1, CopyFromParent,
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
			XpmCreatePixmapFromXpmImage(dsp, Scr[screen].window, mail_xpmimg,
						 &Scr[screen].mb.mail.pixmap,
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
			extern void pickPixmap(Display * display, Drawable drawable, char *name,
					       int default_width, int default_height, unsigned char *default_bits,
				    int *width, int *height, Pixmap * pixmap,
					       int *graphics_format);

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
	(void) nice(nicelevel);

	(void) XSetIOErrorHandler(xio_error);

	/* start the command startcmd */
	if (startCmd && *startCmd) {

		if ((cmd_pid = FORK()) == -1) {
			(void) fprintf(stderr, "Failed to launch \"%s\"\n", startCmd);
			perror(ProgramName);
			cmd_pid = 0;
		} else if (!cmd_pid) {
#ifndef VMS
			setpgid(0, 0);
#endif
#if 0
#if (defined(sun) && defined(__svr4__)) || defined(sgi) || defined(__hpux) || defined(linux)
			setsid();
#else
			setpgrp(getpid(), getpid());
#endif
#endif
#if defined( SYSV ) || defined( SVR4 ) || ( __VMS_VER >= 70000000 )
			(void) sigprocmask(SIG_SETMASK, &old_sigmask, &old_sigmask);
#else
			(void) sigsetmask(old_sigmask);
#endif
			system(startCmd);
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
			system(endCmd);
			exit(0);
		}
	}
#ifdef VMS
	return 1;
#else
	return 0;
#endif
}

void
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

void
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

#endif /* STANDALONE */
