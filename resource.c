#ifndef lint
static char sccsid[] = "@(#)resource.c	2.7 95/02/21 xlockmore";
#endif
/*-
 * resource.c - resource management for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *
 * Changes of David Bagley (bagleyd@source.asset.com)
 * 21-Dec-94: patch for -delay, -batchcount and -saturation for X11R5+
 *            from Patrick D Sullivan <pds@bss.com>. 
 * 18-Dec-94: -inroot option added from Bill Woodward <wpwood@pencom.com>.
 * 31-Oct-94: bomb mode was added from <D.T.Shield@compsci.liverpool.ac.uk>.
 * 20-Sep-94: added bat mode from Lorenzo Patocchi <patol@info.isbiel.ch>.
 * 11-Jul-94: added grav mode, and inwindow option from Greg Bowering
 *            <greg@cs.adelaide.edu.au>
 * 22-Jun-94: Modified for VMS
 *            <Anthony.D.Clarke@Support.Hatfield.Raytheon.bae.eurokom.ie>
 * 17-Jun-94: default changed from life to blank
 * 21-Mar-94: patch fix for AIXV3 from <R.K.Lloyd@csc.liv.ac.uk>
 * 01-Dec-93: added patch for AIXV3 from Tom McConnell
 *            <tmcconne@sedona.intel.com>
 * 29-Jun-93: added spline, maze, sphere, hyper, helix, rock, and blot mode.
 *
 * Changes of Patrick J. Naughton
 * 25-Sep-91: added worm mode.
 * 24-Jun-91: changed name to username.
 * 06-Jun-91: Added flame mode.
 * 24-May-91: Added -name and -usefirst and -resources.
 * 16-May-91: Added random mode and pyro mode.
 * 26-Mar-91: CheckResources: delay must be >= 0.
 * 29-Oct-90: Added #include <ctype.h> for missing isupper() on some OS revs.
 *	      moved -mode option, reordered Xrm database evaluation.
 * 28-Oct-90: Added text strings.
 * 26-Oct-90: Fix bug in mode specific options.
 * 31-Jul-90: Fix ':' handling in parsefilepath
 * 07-Jul-90: Created from resource work in xlock.c
 *
 */

#include "xlock.h"
#include <stdio.h>
#include <netdb.h>
#include <math.h>
#include <ctype.h>
#include <X11/Xresource.h>

/*
 * Declare external interface routines for supported screen savers.
 */

extern void inithop();
extern void drawhop();

extern void initlife();
extern void drawlife();

extern void initqix();
extern void drawqix();

extern void initswarm();
extern void drawswarm();

extern void initrotor();
extern void drawrotor();

extern void initpyro();
extern void drawpyro();

extern void initflame();
extern void drawflame();

extern void initworm();
extern void drawworm();

extern void initspline();
extern void drawspline();

extern void initmaze();
extern void drawmaze();

extern void initsphere();
extern void drawsphere();

extern void inithyper();
extern void drawhyper();

extern void inithelix();
extern void drawhelix();

extern void initrock();
extern void drawrock();

extern void initblot();
extern void drawblot();

extern void initgrav();
extern void drawgrav();

extern void initbounce();
extern void drawbounce();

extern void initworld();
extern void drawworld();

extern void initrect();
extern void drawrect();
 
extern void initbat();
extern void drawbat();

extern void initgalaxy();
extern void drawgalaxy();

extern void initkaleid();
extern void drawkaleid();

extern void initwator();
extern void drawwator();

extern void initlife3d();
extern void drawlife3d();

extern void initswirl();
extern void drawswirl();

extern void initimage();
extern void drawimage();

extern void initbomb();
extern void drawbomb();

extern void initblank();
extern void drawblank();

extern int full_lock();

typedef struct {
    char       *cmdline_arg;
    void        (*lp_init) ();
    void        (*lp_callback) ();
    int         def_delay;
    int         def_batchcount;
    float       def_saturation;
    char       *desc;
}           LockStruct;

#define NUMSPECIAL 4 /* # of Special Screens */
static char randomstring[] = "random";
static char bombstring[] = "bomb";

static LockStruct LockProcs[] = {
    {"hop", inithop, drawhop, 1000, 1000, 1.0, "Hopalong iterated fractals"},
    {"qix", initqix, drawqix, 30000, 64, 1.0, "Spinning lines a la Qix(tm)"},
    {"life", initlife, drawlife, 750000, 125, 1.0, "Conway's game of Life"},
    {"swarm", initswarm, drawswarm, 10000, 100, 1.0, "Swarm of bees"},
    {"rotor", initrotor, drawrotor, 10000, 4, 0.4, "Tom's Roto-Rooter"},
    {"pyro", initpyro, drawpyro, 15000, 40, 1.0, "Fireworks"},
    {"flame", initflame, drawflame, 10000, 20, 1.0, "Cosmic Flame Fractals"},
    {"worm", initworm, drawworm, 10000, 20, 1.0, "Wiggly Worms"},
    {"spline", initspline, drawspline, 30000, 6, 0.4, "Moving Splines"},
    {"maze", initmaze, drawmaze, 10000, 40, 1.0, "aMAZEing"},
    {"sphere", initsphere, drawsphere, 10000, 1, 1.0, "Shaded spheres"},
    {"hyper", inithyper, drawhyper, 10000, 1, 1.0, "Spinning Tesseract"},
    {"helix", inithelix, drawhelix, 10000, 1, 1.0, "Helix"},
    {"rock", initrock, drawrock, 20000, 100, 1.0, "Asteroid field"},
    {"blot", initblot, drawblot, 10000, 6, 0.4, "Rorschach's ink blot test"},
    {"grav", initgrav, drawgrav, 10000, 10, 1.0, "Orbiting planets"},
    {"bounce", initbounce, drawbounce, 10000, 10, 1.0, "Bouncing ball"},
    {"world", initworld, drawworld, 100000, 8, 0.3, "Random Spinning Earths"},
    {"rect", initrect, drawrect, 10000, 100, 1.0, "Greynetic rectangles"},
    {"bat", initbat, drawbat, 100000, 6, 1.0, "Flying Bats"},
    {"galaxy", initgalaxy, drawgalaxy, 100, 3, 1.0, "Spinning galaxies"},
    {"kaleid", initkaleid, drawkaleid, 2000, 4, 1.0, "Kaleidoscope"},
    {"wator", initwator, drawwator, 750000, 4, 1.0, "Dewdney's Planet Wa-Tor"},
    {"life3d", initlife3d, drawlife3d, 1000000, 80, 1.0, "Bays' 3D Life"},
    {"swirl", initswirl, drawswirl, 5000, 5, 1.0, "Animated swirling patterns"},
    {"image", initimage, drawimage, 2000000, 1, 1.0, "Random Bouncing Image"},
    {bombstring, initbomb, drawbomb, 1000000, 1, 1.0, "Temporary screen lock"},
    {"blank", initblank, drawblank, 4000000, 1, 1.0, "Blank screen"},
    {randomstring, NULL, NULL, 0, 0, 0.0, "Random mode"},
};

#define NUMPROCS (sizeof LockProcs / sizeof LockProcs[0])

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64	/* SunOS 3.5 does not define this */
#endif

extern char *getenv();

#ifndef DEF_FILESEARCHPATH
#ifdef VMS
#define DEF_FILESEARCHPATH "DECW$SYSTEM_DEFAULTS:DECW$%N.DAT%S"
#else
#define DEF_FILESEARCHPATH "/usr/lib/X11/%T/%N%S"
#endif
#endif
#ifdef VMS
#define DEF_DISPLAY     "DECW$DISPLAY:"
#else
#define DEF_DISPLAY	""
#endif
#define DEF_MODE	"blank"
#ifndef AIXV3
#define DEF_FONT	"-b&h-lucida-medium-r-normal-sans-24-*-*-*-*-*-iso8859-1"
#else /* AIXV3 */
#define DEF_FONT	"fixed"
#endif /* AIXV3 */
#define DEF_BG		"White"
#define DEF_FG		"Black"
#define DEF_NAME	"Name: "
#define DEF_PASS	"Password: "
#define DEF_INFO	"Enter password to unlock; select icon to lock."
#define DEF_VALID	"Validating login..."
#define DEF_INVALID	"Invalid login."
#define DEF_TIMEOUT	"30"	/* secs till password entry times out */
#define DEF_BC		"100"	/* vectors (or whatever) per batch */
#define DEF_DELAY	"200000"/* microseconds between batches */
#define DEF_NICE	"10"	/* xlock process nicelevel */
#define DEF_SAT		"1.0"	/* color ramp saturation 0->1 */
#define DEF_CLASSNAME	"XLock"
#define DEF_GEOMETRY	""

#ifdef AUTO_LOGOUT
#define DEF_LOGOUT	"30"    /* minutes until auto-logout */
#endif

#ifdef LOGOUT_BUTTON

#define DEF_BTN_LABEL	"Logout" /* string that appears in logout button */

/* this string appears immediately below logout button */
#define DEF_BTN_HELP	"Click here to logout"

/* this string appears in place of the logout button if user could not
   be logged out */
#define DEF_FAIL	"Auto-logout failed"

#endif

static char *classname;
static char modename[1024];
static char modeclass[1024];

static XrmOptionDescRec genTable[] = {
    {"-mode", ".mode", XrmoptionSepArg, (caddr_t) NULL},
    {"-nolock", ".nolock", XrmoptionNoArg, (caddr_t) "on"},
    {"+nolock", ".nolock", XrmoptionNoArg, (caddr_t) "off"},
    {"-remote", ".remote", XrmoptionNoArg, (caddr_t) "on"},
    {"+remote", ".remote", XrmoptionNoArg, (caddr_t) "off"},
    {"-mono", ".mono", XrmoptionNoArg, (caddr_t) "on"},
    {"+mono", ".mono", XrmoptionNoArg, (caddr_t) "off"},
#ifndef ALWAYS_ALLOW_ROOT
    {"-allowroot", ".allowroot", XrmoptionNoArg, (caddr_t) "on"},
    {"+allowroot", ".allowroot", XrmoptionNoArg, (caddr_t) "off"},
#endif
    {"-enablesaver", ".enablesaver", XrmoptionNoArg, (caddr_t) "on"},
    {"+enablesaver", ".enablesaver", XrmoptionNoArg, (caddr_t) "off"},
    {"-allowaccess", ".allowaccess", XrmoptionNoArg, (caddr_t) "on"},
    {"+allowaccess", ".allowaccess", XrmoptionNoArg, (caddr_t) "off"},
    {"-grabmouse", ".grabmouse", XrmoptionNoArg, (caddr_t) "on"},
    {"+grabmouse", ".grabmouse", XrmoptionNoArg, (caddr_t) "off"},
    {"-echokeys", ".echokeys", XrmoptionNoArg, (caddr_t) "on"},
    {"+echokeys", ".echokeys", XrmoptionNoArg, (caddr_t) "off"},
    {"-usefirst", ".usefirst", XrmoptionNoArg, (caddr_t) "on"},
    {"+usefirst", ".usefirst", XrmoptionNoArg, (caddr_t) "off"},
    {"-v", ".verbose", XrmoptionNoArg, (caddr_t) "on"},
    {"+v", ".verbose", XrmoptionNoArg, (caddr_t) "off"},
    {"-inwindow",".inwindow", XrmoptionNoArg, (caddr_t) "on"},
    {"+inwindow",".inwindow", XrmoptionNoArg, (caddr_t) "off"},
    {"-inroot",".inroot", XrmoptionNoArg, (caddr_t) "on"},
    {"+inroot",".inroot", XrmoptionNoArg, (caddr_t) "off"},
    {"-timeelapsed", ".timeelapsed", XrmoptionNoArg, (caddr_t) "on"},
    {"+timeelapsed", ".timeelapsed", XrmoptionNoArg, (caddr_t) "off"},
    {"-install", ".install", XrmoptionNoArg, (caddr_t) "on"},
    {"+install", ".install", XrmoptionNoArg, (caddr_t) "off"},
    {"-nice", ".nice", XrmoptionSepArg, (caddr_t) NULL},
    {"-timeout", ".timeout", XrmoptionSepArg, (caddr_t) NULL},
    {"-font", ".font", XrmoptionSepArg, (caddr_t) NULL},
    {"-bg", ".background", XrmoptionSepArg, (caddr_t) NULL},
    {"-fg", ".foreground", XrmoptionSepArg, (caddr_t) NULL},
    {"-background", ".background", XrmoptionSepArg, (caddr_t) NULL},
    {"-foreground", ".foreground", XrmoptionSepArg, (caddr_t) NULL},
    {"-username", ".username", XrmoptionSepArg, (caddr_t) NULL},
    {"-password", ".password", XrmoptionSepArg, (caddr_t) NULL},
    {"-info", ".info", XrmoptionSepArg, (caddr_t) NULL},
    {"-validate", ".validate", XrmoptionSepArg, (caddr_t) NULL},
    {"-invalid", ".invalid", XrmoptionSepArg, (caddr_t) NULL},
    {"-geometry", ".geometry", XrmoptionSepArg, (caddr_t) NULL},
#ifdef AUTO_LOGOUT
    {"-forceLogout", ".forceLogout", XrmoptionSepArg, (caddr_t) NULL},
#endif
#ifdef LOGOUT_BUTTON
    {"-logoutButtonLabel", ".logoutButtonLabel", XrmoptionSepArg, (caddr_t) NULL},
    {"-logoutButtonHelp", ".logoutButtonHelp", XrmoptionSepArg, (caddr_t) NULL},
    {"-logoutFailedString", ".logoutFailedString", XrmoptionSepArg, (caddr_t) NULL},
#endif

};
#define genEntries (sizeof genTable / sizeof genTable[0])

char Delay[64], BatchCount[64], Saturation[64];
static XrmOptionDescRec modeTable[] = {
    {"-delay", Delay, XrmoptionSepArg, (caddr_t) NULL},
    {"-batchcount", BatchCount, XrmoptionSepArg, (caddr_t) NULL},
    {"-saturation", Saturation, XrmoptionSepArg, (caddr_t) NULL},
};
#define modeEntries (sizeof modeTable / sizeof modeTable[0])

static XrmOptionDescRec cmdlineTable[] = {
    {"-display", ".display", XrmoptionSepArg, (caddr_t) NULL},
    {"-nolock", ".nolock", XrmoptionNoArg, (caddr_t) "on"},
    {"+nolock", ".nolock", XrmoptionNoArg, (caddr_t) "off"},
    {"-remote", ".remote", XrmoptionNoArg, (caddr_t) "on"},
    {"+remote", ".remote", XrmoptionNoArg, (caddr_t) "off"},
    {"-inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "on"},
    {"+inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "off"},
    {"-inroot", ".inroot", XrmoptionNoArg, (caddr_t) "on"},
    {"+inroot", ".inroot", XrmoptionNoArg, (caddr_t) "off"},
};
#define cmdlineEntries (sizeof cmdlineTable / sizeof cmdlineTable[0])

static XrmOptionDescRec nameTable[] = {
    {"-name", ".name", XrmoptionSepArg, (caddr_t) NULL},
};


typedef struct {
    char       *opt;
    char       *desc;
}           OptionStruct;

static OptionStruct opDesc[] = {
    {"-help", "print out this message"},
    {"-resources", "print default resource file to standard output"},
    {"-display displayname", "X server to contact"},
    {"-name resourcename", "class name to use for resources (default is XLock)"},
    {"-/+mono", "turn on/off monochrome override"},
    {"-/+nolock", "turn on/off no password required mode"},
    {"-/+remote", "turn on/off remote host access"},
#ifndef ALWAYS_ALLOW_ROOT
    {"-/+allowroot", "turn on/off allow root password mode"},
#endif
    {"-/+enablesaver", "turn on/off enable X server screen saver"},
    {"-/+allowaccess", "turn on/off allow new clients to connect"},
    {"-/+grabmouse", "turn on/off grabbing of mouse and keyboard"},
    {"-/+echokeys", "turn on/off echo '?' for each password key"},
    {"-/+usefirst", "turn on/off using the first char typed in password"},
    {"-/+v", "turn on/off verbose mode"},
    {"-/+inwindow", "turn on/off making xlock run in a window"},
    {"-/+inroot", "turn on/off making xlock run in the root window"},
    {"-/+timeelapsed", "turn on/off clock"},
    {"-/+install", "whether to use private colormap if needed (yes/no)"},
    {"-delay usecs", "microsecond delay between screen updates"},
    {"-batchcount num", "number of things per batch"},
    {"-nice level", "nice level for xlock process"},
    {"-timeout seconds", "number of seconds before password times out"},
    {"-saturation value", "saturation of color ramp"},
    {"-font fontname", "font to use for password prompt"},
    {"-bg color", "background color to use for password prompt"},
    {"-fg color", "foreground color to use for password prompt"},
    {"-username string", "text string to use for Name prompt"},
    {"-password string", "text string to use for Password prompt"},
    {"-info string", "text string to use for instructions"},
    {"-validate string", "text string to use for validating password message"},
    {"-invalid string", "text string to use for invalid password message"},
    {"-geometry geom", "geometry for non-full screen lock"},
};
#define opDescEntries (sizeof opDesc / sizeof opDesc[0])

char       *displayname;
char       *mode;
char       *fontname;
char       *background;
char       *foreground;
char       *text_name;
char       *text_pass;
char       *text_info;
char       *text_valid;
char       *text_invalid;
char       *geometry;
float       saturation;
int         nicelevel;
int         delay;
int         batchcount;
int         timeout;
#ifdef AUTO_LOGOUT
int         forceLogout;
#endif
#ifdef LOGOUT_BUTTON
int         enable_button = 1;
char       *logoutButtonLabel;
char       *logoutButtonHelp;
char       *logoutFailedString;
#endif
Bool        mono;
Bool        nolock;
Bool        remote;
#ifdef ALWAYS_ALLOW_ROOT
Bool        allowroot = 1;
#else
Bool        allowroot;
#endif
Bool        enablesaver;
Bool        allowaccess;
Bool        grabmouse;
Bool        echokeys;
Bool        usefirst;
Bool        verbose;
Bool        inwindow;
Bool        inroot;
Bool        timeelapsed;
Bool        install;

#define t_String	0
#define t_Float		1
#define t_Int		2
#define t_Bool		3

typedef struct {
    caddr_t    *var;
    char       *name;
    char       *class;
    char       *def;
    int         type;
}           argtype;

static argtype genvars[] = {
    {(caddr_t *) &fontname, "font", "Font", DEF_FONT, t_String},
    {(caddr_t *) &background, "background", "Background", DEF_BG, t_String},
    {(caddr_t *) &foreground, "foreground", "Foreground", DEF_FG, t_String},
    {(caddr_t *) &text_name, "username", "Username", DEF_NAME, t_String},
    {(caddr_t *) &text_pass, "password", "Password", DEF_PASS, t_String},
    {(caddr_t *) &text_info, "info", "Info", DEF_INFO, t_String},
    {(caddr_t *) &text_valid, "validate", "Validate", DEF_VALID, t_String},
    {(caddr_t *) &text_invalid, "invalid", "Invalid", DEF_INVALID, t_String},
    {(caddr_t *) &geometry, "geometry", "Geometry", DEF_GEOMETRY, t_String},
    {(caddr_t *) &nicelevel, "nice", "Nice", DEF_NICE, t_Int},
    {(caddr_t *) &timeout, "timeout", "Timeout", DEF_TIMEOUT, t_Int},
    {(caddr_t *) &mono, "mono", "Mono", "off", t_Bool},
    {(caddr_t *) &nolock, "nolock", "NoLock", "off", t_Bool},
    {(caddr_t *) &remote, "remote", "Remote", "off", t_Bool},
#ifndef ALWAYS_ALLOW_ROOT
    {(caddr_t *) &allowroot, "allowroot", "AllowRoot", "off", t_Bool},
#endif
    {(caddr_t *) &enablesaver, "enablesaver", "EnableSaver", "off", t_Bool},
    {(caddr_t *) &allowaccess, "allowaccess", "AllowAccess", "off", t_Bool},
    {(caddr_t *) &grabmouse, "grabmouse", "GrabMouse", "on", t_Bool},
    {(caddr_t *) &echokeys, "echokeys", "EchoKeys", "off", t_Bool},
    {(caddr_t *) &usefirst, "usefirst", "Usefirst", "off", t_Bool},
    {(caddr_t *) &verbose, "verbose", "Verbose", "off", t_Bool},
    {(caddr_t *) &inwindow, "inwindow", "InWindow", "off", t_Bool},
    {(caddr_t *) &inroot, "inroot", "InRoot", "off", t_Bool},
    {(caddr_t *) &timeelapsed, "timeelapsed", "TimeElapsed", "off", t_Bool},
    {(caddr_t *) &install, "install", "Install", "off", t_Bool},
#ifdef AUTO_LOGOUT
    {(caddr_t *) &forceLogout, "forceLogout", "ForceLogout", DEF_LOGOUT, t_Int
},
#endif
#ifdef LOGOUT_BUTTON
    {(caddr_t *) &logoutButtonLabel, "logoutButtonLabel",
               "LogoutButtonLabel", DEF_BTN_LABEL, t_String},
    {(caddr_t *) &logoutButtonHelp, "logoutButtonHelp",
               "LogoutButtonHelp", DEF_BTN_HELP, t_String},
    {(caddr_t *) &logoutFailedString, "logoutFailedString",
               "LogoutFailedString", DEF_FAIL, t_String},
#endif
};
#define NGENARGS (sizeof genvars / sizeof genvars[0])

static argtype modevars[] = {
    {(caddr_t *) &delay, "delay", "Delay", DEF_DELAY, t_Int},
    {(caddr_t *) &batchcount, "batchcount", "BatchCount", DEF_BC, t_Int},
    {(caddr_t *) &saturation, "saturation", "Saturation", DEF_SAT, t_Float},
};
#define NMODEARGS (sizeof modevars / sizeof modevars[0])

#ifdef VMS
static char *stripname(string)  
char *string;    
{
  char *characters;
 
  while (string && *string++ != ']'); 
  characters = string; 
  while (characters)
    if (*characters == '.') {  
      *characters = '\0';   
      return string; 
    } else 
      characters++; 
  return string; 
}
#endif

static void
Syntax(badOption)
    char       *badOption;
{
    int         col, len, i;

    (void) fprintf(stderr, "%s:  bad command line option \"%s\"\n\n",
	     ProgramName, badOption);

    (void) fprintf(stderr, "usage:  %s", ProgramName);
    col = 8 + strlen(ProgramName);
    for (i = 0; i < opDescEntries; i++) {
	len = 3 + strlen(opDesc[i].opt);	/* space [ string ] */
	if (col + len > 79) {
	    (void) fprintf(stderr, "\n   ");	/* 3 spaces */
	    col = 3;
	}
	(void) fprintf(stderr, " [%s]", opDesc[i].opt);
	col += len;
    }

    len = 8 + strlen(LockProcs[0].cmdline_arg);
    if (col + len > 79) {
	(void) fprintf(stderr, "\n   ");	/* 3 spaces */
	col = 3;
    }
    (void) fprintf(stderr, " [-mode %s", LockProcs[0].cmdline_arg);
    col += len;
    for (i = 1; i < NUMPROCS; i++) {
	len = 3 + strlen(LockProcs[i].cmdline_arg);
	if (col + len > 79) {
	    (void) fprintf(stderr, "\n   ");	/* 3 spaces */
	    col = 3;
	}
	(void) fprintf(stderr, " | %s", LockProcs[i].cmdline_arg);
	col += len;
    }
    (void) fprintf(stderr, "]\n");

    (void) fprintf(stderr, "\nType %s -help for a full description.\n\n",
	     ProgramName);
    exit(1);
}

static void
Help()
{
    int         i;

    (void) fprintf(stderr, "usage:\n        %s [-options ...]\n\n",
             ProgramName);
    (void) fprintf(stderr, "where options include:\n");
    for (i = 0; i < opDescEntries; i++) {
	(void) fprintf(stderr, "    %-28s %s\n",
                 opDesc[i].opt, opDesc[i].desc);
    }

    (void) fprintf(stderr, "    %-28s %s\n", "-mode mode", "animation mode");
    (void) fprintf(stderr, "    where mode is one of:\n");
    for (i = 0; i < NUMPROCS; i++) {
	(void) fprintf(stderr, "          %-23s %s\n",
		 LockProcs[i].cmdline_arg, LockProcs[i].desc);
    }
    (void) putc('\n', stderr);

    exit(0);
}

static void
DumpResources()
{
    int         i;

    (void) printf("%s.mode: %s\n", classname, DEF_MODE);

    for (i = 0; i < NGENARGS; i++)
	(void) printf("%s.%s: %s\n",
	       classname, genvars[i].name, genvars[i].def);

    for (i = 0; i < NUMPROCS - 1; i++) {
	(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
	       "delay", LockProcs[i].def_delay);
	(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
	       "batchcount", LockProcs[i].def_batchcount);
	(void) printf("%s.%s.%s: %g\n", classname, LockProcs[i].cmdline_arg,
	       "saturation", LockProcs[i].def_saturation);
    }
    exit(0);
}


static void
LowerString(s)
    char       *s;
{

    while (*s) {
	if (isupper(*s))
	    *s += ('a' - 'A');
	s++;
    }
}

static void
GetResource(database, parentname, parentclass,
	    name, class, valueType, def, valuep)
    XrmDatabase database;
    char       *parentname;
    char       *parentclass;
    char       *name;
    char       *class;
    int         valueType;
    char       *def;
    caddr_t    *valuep;		/* RETURN */
{
    char       *type;
    XrmValue    value;
    char       *string;
    char        buffer[1024];
    char        fullname[1024];
    char        fullclass[1024];
    int         len;

    (void) sprintf(fullname, "%s.%s", parentname, name);
    (void) sprintf(fullclass, "%s.%s", parentclass, class);
    if (XrmGetResource(database, fullname, fullclass, &type, &value)) {
	string = value.addr;
	len = value.size;
    } else {
	string = def;
	len = strlen(string);
    }
    (void) strncpy(buffer, string, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    switch (valueType) {
    case t_String:
	{
	    char       *s = (char *) malloc(len + 1);
	    if (s == (char *) NULL)
		error("%s: GetResource - couldn't allocate memory");
	    (void) strncpy(s, string, len);
	    s[len] = '\0';
	    *((char **) valuep) = s;
	}
	break;
    case t_Bool:
	LowerString(buffer);
	*((int *) valuep) = (!strcmp(buffer, "true") ||
			     !strcmp(buffer, "on") ||
			     !strcmp(buffer, "enabled") ||
			     !strcmp(buffer, "yes")) ? True : False;
	break;
    case t_Int:
	*((int *) valuep) = atoi(buffer);
	break;
    case t_Float:
	*((float *) valuep) = (float) atof(buffer);
	break;
    }
}


static      XrmDatabase
parsefilepath(xfilesearchpath, TypeName, ClassName)
    char       *xfilesearchpath;
    char       *TypeName;
    char       *ClassName;
{
    XrmDatabase database = NULL;
    char        appdefaults[1024];
    char       *src;
    char       *dst;

    src = xfilesearchpath;
    appdefaults[0] = '\0';
    dst = appdefaults;
    while (1) {
	if (*src == '%') {
	    src++;
	    switch (*src) {
	    case '%':
	    case ':':
		*dst++ = *src++;
		*dst = '\0';
		break;
	    case 'T':
		(void) strcat(dst, TypeName);
		src++;
		dst += strlen(TypeName);
		break;
	    case 'N':
		(void) strcat(dst, ClassName);
		src++;
		dst += strlen(ClassName);
		break;
	    case 'S':
		src++;
		break;
	    default:
		src++;
		break;
	    }
#ifdef VMS
        } else if (*src == '#') {/* Colons required in VMS use #*/
#else
	} else if (*src == ':') {
#endif
	    database = XrmGetFileDatabase(appdefaults);
	    if (database == NULL) {
		dst = appdefaults;
		src++;
	    } else
		break;
	} else if (*src == '\0') {
	    database = XrmGetFileDatabase(appdefaults);
	    break;
	} else {
	    *dst++ = *src++;
	    *dst = '\0';
	}
    }
    return database;
}


static void
open_display()
{
    if (displayname != NULL) {
	char       *colon = (char *) strchr(displayname, ':');
	int         n = colon - displayname;

	if (colon == NULL)
	    error("%s: Malformed -display argument, \"%s\"\n", displayname);

	/*
	 * only restrict access to other displays if we are locking and if the
	 * Remote resource is not set.
	 */
	if (nolock)
	    remote = True;
	if (!remote && n
		&& strncmp(displayname, "unix", n)
		&& strncmp(displayname, "localhost", n)) {
	    char        hostname[MAXHOSTNAMELEN];
	    struct hostent *host;
	    char      **hp;
	    int         badhost = 1;

	    if (gethostname(hostname, MAXHOSTNAMELEN))
		error("%s: Can't get local hostname.\n");

	    if (!(host = gethostbyname(hostname)))
		error("%s: Can't get hostbyname.\n");

	    if (strncmp(displayname, host->h_name, n)) {
		for (hp = host->h_aliases; *hp; hp++) {
		    if (!strncmp(displayname, *hp, n)) {
			badhost = 0;
			break;
		    }
		}
		if (badhost) {
		    *colon = (char) 0;
		    error("%s: can't lock %s's display\n", displayname);
		}
	    }
	}
    } else
	displayname = ":0.0";
    if (!(dsp = XOpenDisplay(displayname)))
	error("%s: unable to open display %s.\n", displayname);
}


void
printvar(class, var)
    char       *class;
    argtype     var;
{
    switch (var.type) {
    case t_String:
	(void) fprintf(stderr, "%s.%s: %s\n",
		class, var.name, *((char **) var.var));
	break;
    case t_Bool:
	(void) fprintf(stderr, "%s.%s: %s\n",
		class, var.name, *((int *) var.var)
		? "True" : "False");
	break;
    case t_Int:
	(void) fprintf(stderr, "%s.%s: %d\n",
		class, var.name, *((int *) var.var));
	break;
    case t_Float:
	(void) fprintf(stderr, "%s.%s: %g\n",
		class, var.name, *((float *) var.var));
	break;
    }
}


void
GetResources(argc, argv)
    int         argc;
    char       *argv[];
{
    XrmDatabase RDB = NULL;
    XrmDatabase nameDB = NULL;
    XrmDatabase modeDB = NULL;
    XrmDatabase cmdlineDB = NULL;
    XrmDatabase generalDB = NULL;
    XrmDatabase homeDB = NULL;
    XrmDatabase applicationDB = NULL;
    XrmDatabase serverDB = NULL;
    XrmDatabase userDB = NULL;
    char        userfile[1024];
    char       *homeenv;
    char       *userpath;
    char       *env;
    char       *serverString;
    int         i;

    XrmInitialize();

    for (i = 0; i < argc; i++) {
	if (!strncmp(argv[i], "-help", strlen(argv[i])))
	    Help();
	/* NOTREACHED */
    }

    /*
     * get -name arg from command line so you can have different resource
     * files for different configurations/machines etc...
     */
#ifdef VMS
    /*Strip off directory and .exe; parts*/
    ProgramName = stripname(ProgramName);
#endif
    XrmParseCommand(&nameDB, nameTable, 1, ProgramName,
		    &argc, argv);
    GetResource(nameDB, ProgramName, "*", "name", "Name", t_String,
		DEF_CLASSNAME, &classname);


    homeenv = getenv("HOME");
    if (!homeenv)
	homeenv = "";

    env = getenv("XFILESEARCHPATH");
    applicationDB = parsefilepath(env ? env : DEF_FILESEARCHPATH,
				  "app-defaults", classname);

    XrmParseCommand(&cmdlineDB, cmdlineTable, cmdlineEntries, ProgramName,
		    &argc, argv);

    userpath = getenv("XUSERFILESEARCHPATH");
    if (!userpath) {
	env = getenv("XAPPLRESDIR");
	if (env)
	    (void) sprintf(userfile, "%s/%%N:%s/%%N", env, homeenv);
	else
#ifdef VMS
            (void) sprintf(userfile, "%sDECW$%%N.DAT#%sDECW$XDEFAULTS.DAT",
                    homeenv, homeenv);
#else
	    (void) sprintf(userfile, "%s/%%N", homeenv);
#endif
	userpath = userfile;
    }
    userDB = parsefilepath(userpath, "app-defaults", classname);

    (void) XrmMergeDatabases(applicationDB, &RDB);
    (void) XrmMergeDatabases(userDB, &RDB);
    (void) XrmMergeDatabases(cmdlineDB, &RDB);

    env = getenv("DISPLAY");
    GetResource(RDB, ProgramName, classname, "display", "Display", t_String,
		env ? env : DEF_DISPLAY, &displayname);
    GetResource(RDB, ProgramName, classname, "nolock", "NoLock", t_Bool,
		"off", (caddr_t *) &nolock);
    GetResource(RDB, ProgramName, classname, "remote", "Remote", t_Bool,
		"off", (caddr_t *) &remote);
    GetResource(RDB, ProgramName, classname, "inwindow", "InWindow", t_Bool,
		"off", (caddr_t *) &inwindow);
    GetResource(RDB, ProgramName, classname, "inroot", "InRoot", t_Bool,
		"off", (caddr_t *) &inroot);
    GetResource(RDB, ProgramName, classname, "mono", "Mono", t_Bool,
		"off", (caddr_t *) &mono);

    open_display();
    serverString = XResourceManagerString(dsp);
    if (serverString) {
	serverDB = XrmGetStringDatabase(serverString);
	(void) XrmMergeDatabases(serverDB, &RDB);
    } else {
	char        buf[1024];
	(void) sprintf(buf, "%s/.Xdefaults", homeenv);
	homeDB = XrmGetFileDatabase(buf);
	(void) XrmMergeDatabases(homeDB, &RDB);
    }

    XrmParseCommand(&generalDB, genTable, genEntries, ProgramName, &argc, argv);
    (void) XrmMergeDatabases(generalDB, &RDB);

    GetResource(RDB, ProgramName, classname, "mode", "Mode", t_String,
		DEF_MODE, (caddr_t *) &mode);

    /*
     * if RAND < mode, then just grab a random entry from the table
     * or root is trying to run bomb ... sorry not a good idea
     */
   
    if (!strcmp(mode, randomstring))
      mode = LockProcs[RAND() % (NUMPROCS - NUMSPECIAL)].cmdline_arg;
    if (!getuid() && !strcmp(mode, bombstring))
       error("%s: mode %s can not run as root.\n", bombstring);
#ifdef FORCE_BOMB
    /*
     *  Force temporary locking mode
     */
    if ( !full_lock() )
      mode = LockProcs[(NUMPROCS - NUMSPECIAL + 1)].cmdline_arg;
#endif

    (void) sprintf(modename, "%s.%s", ProgramName, mode);
    (void) sprintf(modeclass, "%s.%s", classname, mode);

    /* X11R5 may need these changes to make the modeTable stuff work */
    for (i = 0; i < NMODEARGS; i++)
      (void) sprintf(modeTable[i].specifier, ".%s.%s", mode, modevars[i].name);

    XrmParseCommand(&modeDB, modeTable, modeEntries, ProgramName, &argc, argv);
    /*XrmParseCommand(&modeDB, modeTable, modeEntries, modeclass, &argc, argv);*/
    (void) XrmMergeDatabases(modeDB, &RDB);

    /* Parse the rest of the command line */
    for (argc--, argv++; argc > 0; argc--, argv++) {
	if (**argv != '-')
	    Syntax(*argv);
	switch (argv[0][1]) {
	case 'r':
	    DumpResources();
	    /* NOTREACHED */
	default:
	    Syntax(*argv);
	    /* NOTREACHED */
	}
    }

    /* the RDB is set, now query load the variables from the database */

    for (i = 0; i < NGENARGS; i++)
	GetResource(RDB, ProgramName, classname,
		    genvars[i].name, genvars[i].class,
		    genvars[i].type, genvars[i].def, genvars[i].var);

    for (i = 0; i < NMODEARGS; i++)
	GetResource(RDB, modename, modeclass,
		    modevars[i].name, modevars[i].class,
		    modevars[i].type, modevars[i].def, modevars[i].var);

    (void) XrmDestroyDatabase(RDB);

#ifndef FORCE_BOMB
     if ( full_lock() ) {
#ifdef AUTO_LOGOUT
       forceLogout = 0;
#endif
#ifdef LOGOUT_BUTTON
       enable_button = 0;
#endif
     }
#endif

    if (verbose) {
	for (i = 0; i < NGENARGS; i++)
	    printvar(classname, genvars[i]);
	for (i = 0; i < NMODEARGS; i++)
	    printvar(modename, modevars[i]);
    }
}


void CheckResources()
{
    int         i;

    if (batchcount < 1)
	Syntax("-batchcount argument must be positive.");
    if (saturation < 0.0 || saturation > 1.0)
	Syntax("-saturation argument must be between 0.0 and 1.0.");
    if (delay < 0)
	Syntax("-delay argument must be positive.");

    /* in case they have a 'xlock*mode: ' empty resource */
    if (!mode || *mode == '\0')
      mode = DEF_MODE;

    for (i = 0; i < NUMPROCS; i++) {
	if (!strncmp(LockProcs[i].cmdline_arg, mode, strlen(mode))) {
	    init = LockProcs[i].lp_init;
	    callback = LockProcs[i].lp_callback;
	    break;
	}
    }
    if (i == NUMPROCS) {
	(void) fprintf(stderr, "Unknown mode: ");
	Syntax(mode);
    }
}
