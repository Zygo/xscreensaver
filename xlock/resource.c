#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)resource.c	4.04 97/07/28 xlockmore";

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
 * Changes maintained by David Bagley <bagleyd@bigfoot.com>
 * 01-Apr-97: Tom Schmidt <tschmidt@micron.com>
 *            Fixed memory leak.  Made -visual option a hacker mode for now.
 * 20-Mar-97: Tom Schmidt <tschmidt@micron.com>
 *            Added -visual option.
 *  3-Apr-96: Jouk Jansen <joukj@crys.chem.uva.nl>
 *            Supply for wildcards for filenames for random selection
 * 18-Mar-96: Ron Hitchens <ron@idiom.com>
 *            Setup chosen mode with set_default_mode() for new hook scheme.
 *  6-Mar-96: Jouk Jansen <joukj@crys.chem.uva.nl>
 *            Remote node checking for VMS fixed
 * 20-Dec-95: Ron Hitchens <ron@idiom.com>
 *            Resource parsing fixed for "nolock".
 * 02-Aug-95: Patch to use default delay, etc., from mode.h thanks to
 *            Roland Bock <exp120@physik.uni-kiel.d400.de>
 * 17-Jun-95: Split out mode.h from resource.c .
 * 29-Mar-95: Added -cycles for more control over a lockscreen similar to
 *            -delay, -batchcount, and -saturation.
 * 21-Feb-95: MANY patches from Heath A. Kehoe <hakehoe@icaen.uiowa.edu>.
 * 21-Dec-94: patch for -delay, -batchcount and -saturation for X11R5+
 *            from Patrick D Sullivan <pds@bss.com>. 
 * 18-Dec-94: -inroot option added from Bill Woodward <wpwood@pencom.com>.
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
 * 26-Mar-91: checkResources: delay must be >= 0.
 * 29-Oct-90: Added #include <ctype.h> for missing isupper() on some OS revs.
 *	      moved -mode option, reordered Xrm database evaluation.
 * 28-Oct-90: Added text strings.
 * 26-Oct-90: Fix bug in mode specific options.
 * 31-Jul-90: Fix ':' handling in parsefilepath
 * 07-Jul-90: Created from resource work in xlock.c
 *
 */

#include "xlock.h"
#include "mode.h"
#include "version.h"
#if VMS
#if ( __VMS_VER < 70000000 )
#ifdef __DECC
#define gethostname decc$gethostname
#define gethostbyname decc$gethostbyname
#endif
#else
#include <socket.h>
#endif
#endif

#ifndef offsetof
#define offsetof(s,m) ((char*)(&((s *)0)->m)-(char*)0)
#endif

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
extern int  fullLock();

#endif

extern char *getenv(const char *);

#ifndef DEF_FILESEARCHPATH
#ifdef VMS
#include <descrip>
#include <iodef>
#include <ssdef>
#include <stsdef>
#include <types.h>
#include <starlet.h>

#define DEF_FILESEARCHPATH "DECW$SYSTEM_DEFAULTS:DECW$%N.DAT%S"
#define BUFSIZE 132
#define DECW$C_WS_DSP_TRANSPORT 2	/*  taken from wsdriver.lis */
#define IO$M_WS_DISPLAY 0x00000040	/* taken from wsdriver */

struct descriptor_t {		/* descriptor structure         */
	unsigned short len;
	unsigned char type;
	unsigned char c_class;
	char       *ptr;
};
typedef struct descriptor_t dsc;

/* $dsc creates a descriptor for a predefined string */

#define $dsc(name,string) dsc name = { sizeof(string)-1,14,1,string}

/* $dscp creates a descriptor pointing to a buffer allocated elsewhere */

#define $dscp(name,size,addr) dsc name = { size,14,1,addr }

static int  descr();

#else
#define DEF_FILESEARCHPATH "/usr/lib/X11/%T/%N%S"
#endif
#endif
#ifndef DEF_MODE
#if 0
#define DEF_MODE	"blank"	/* May be safer */
#else
#define DEF_MODE	"random"	/* May be more interesting */
#endif
#endif
#define DEF_DELAY	"200000"	/* microseconds between batches */
#define DEF_BATCHCOUNT	"100"	/* vectors (or whatever) per batch */
#define DEF_CYCLES	"1000"	/* timeout in cycles for a batch */
#define DEF_SIZE	"0"	/* size, default if 0 */
#define DEF_SATURATION	"1.0"	/* color ramp saturation 0->1 */
#define DEF_NICE	"10"	/* xlock process nicelevel */
#define DEF_LOCKDELAY	"0"	/* secs until lock */
#define DEF_TIMEOUT	"30"	/* secs until password entry times out */
#ifndef DEF_FONT
#ifdef AIXV3
#define DEF_FONT	"fixed"
#else /* !AIXV3 */
#define DEF_FONT	"-b&h-lucida-medium-r-normal-sans-24-*-*-*-*-*-iso8859-1"
#endif /* !AIXV3 */
#endif
#define DEF_MSGFONT     "-adobe-courier-medium-r-*-*-14-*-*-*-m-*-iso8859-1"
#define DEF_BG		"White"
#define DEF_FG		"Black"
#define DEF_NAME	"Name: "
#define DEF_PASS	"Password: "
#define DEF_INFO	"Enter password to unlock; select icon to lock."
#define DEF_VALID	"Validating login..."
#define DEF_INVALID	"Invalid login."
#define DEF_GEOMETRY	""
#define DEF_ICONGEOMETRY	""
#define DEF_DELTA3D	"1.5"	/* space between things in 3d mode relative to their size */
#ifndef DEF_MESSAGESFILE
#define DEF_MESSAGESFILE	""
#endif
#ifndef DEF_MESSAGEFILE
#define DEF_MESSAGEFILE ""
#endif
#ifndef DEF_MESSAGE
/* #define DEF_MESSAGE "I am out running around." */
#define DEF_MESSAGE ""
#endif
#ifndef DEF_IMAGEFILE
#define DEF_IMAGEFILE ""
#endif
#define DEF_CLASSNAME	"XLock"
/*-
  Grid     Number of Neigbors
  ----     ------------------
  Square   4 or 8
  Hexagon  6
  Triangle 3, 9 or 12     <- 9 is not too mathematically sound...
*/
#define DEF_NEIGHBORS  "0"	/* automata mode will choose best or random value */
#define DEF_MOUSE   "False"

#ifdef USE_RPLAY
#define DEF_LOCKSOUND	"thank-you"
#define DEF_INFOSOUND	"identify-please"
#define DEF_VALIDSOUND	"complete"
#define DEF_INVALIDSOUND	"not-programmed"
#else /* !USE_RPLAY */
#if defined ( DEF_PLAY ) || defined ( USE_NAS )
#define DEF_LOCKSOUND	"thank-you.au"
#define DEF_INFOSOUND	"identify-please.au"
#define DEF_VALIDSOUND	"complete.au"
#define DEF_INVALIDSOUND	"not-programmed.au"
#else /* !DEF_PLAY && !USE_NAS */
#ifdef USE_VMSPLAY
#define DEF_LOCKSOUND	"[]thank-you.au"
#define DEF_INFOSOUND	"[]identify-please.au"
#define DEF_VALIDSOUND	"[]complete.au"
#define DEF_INVALIDSOUND	"[]not-programmed.au"
#endif /* !USE_VMSPLAY */
#endif /* !DEF_PLAY && !USE_NAS */
#endif /* !USE_RPLAY */

#if defined( USE_AUTO_LOGOUT ) && !defined( DEF_AUTO_LOGOUT )
#if ( USE_AUTO_LOGOUT <= 0 )
#define DEF_AUTO_LOGOUT "120"	/* User Default, can be overridden */
#else
#define DEF_AUTO_LOGOUT "0"	/* User Default, can be overridden */
#endif
#endif

#if defined( USE_BUTTON_LOGOUT )
#if !defined( DEF_BUTTON_LOGOUT )
#if ( USE_BUTTON_LOGOUT <= 0 )
#define DEF_BUTTON_LOGOUT "5"	/* User Default, can be overridden */
#else
#define DEF_BUTTON_LOGOUT "0"	/* User Default, can be overridden */
#endif
#endif

#define DEF_BTN_LABEL	"Logout"	/* string that appears in logout button */

/* this string appears immediately below logout button */
#define DEF_BTN_HELP	"Click here to logout"

/* this string appears in place of the logout button if user could not be
   logged out */
#define DEF_FAIL	"Auto-logout failed"

#endif

extern char *ProgramName;
extern Display *dsp;

/* For modes with text, marquee & nose */
extern char *program;
extern char *messagesfile;
extern char *messagefile;
extern char *message;
extern char *mfont;

/* For modes with images, images and puzzle support ras and xpm */
extern char *imagefile;

/* For automata modes */
int         neighbors;

/* For eyes, julia, & swarm modes */
Bool        mouse;

char        hostname[MAXHOSTNAMELEN];
static char *mode;
char       *displayname = NULL;
static char *classname;
static char *modename;
static char *modeclassname;
extern Window parent;
static char *parentname;
extern Bool parentSet;

#if HAVE_DIRENT_H
static struct dirent ***images_list;
int         num_list;
struct dirent **image_list;
char        filename_r[MAXNAMLEN];
char        directory_r[DIRBUF];

#endif

static XrmOptionDescRec genTable[] =
{
	{"-mode", ".mode", XrmoptionSepArg, (caddr_t) NULL},
	{"-nolock", ".nolock", XrmoptionNoArg, (caddr_t) "on"},
	{"+nolock", ".nolock", XrmoptionNoArg, (caddr_t) "off"},
	{"-inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "on"},
	{"+inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "off"},
	{"-inroot", ".inroot", XrmoptionNoArg, (caddr_t) "on"},
	{"+inroot", ".inroot", XrmoptionNoArg, (caddr_t) "off"},
	{"-remote", ".remote", XrmoptionNoArg, (caddr_t) "on"},
	{"+remote", ".remote", XrmoptionNoArg, (caddr_t) "off"},
	{"-mono", ".mono", XrmoptionNoArg, (caddr_t) "on"},
	{"+mono", ".mono", XrmoptionNoArg, (caddr_t) "off"},
	{"-allowaccess", ".allowaccess", XrmoptionNoArg, (caddr_t) "on"},
	{"+allowaccess", ".allowaccess", XrmoptionNoArg, (caddr_t) "off"},
	{"-allowroot", ".allowroot", XrmoptionNoArg, (caddr_t) "on"},
	{"+allowroot", ".allowroot", XrmoptionNoArg, (caddr_t) "off"},
	{"-debug", ".debug", XrmoptionNoArg, (caddr_t) "on"},
	{"+debug", ".debug", XrmoptionNoArg, (caddr_t) "off"},
	{"-echokeys", ".echokeys", XrmoptionNoArg, (caddr_t) "on"},
	{"+echokeys", ".echokeys", XrmoptionNoArg, (caddr_t) "off"},
	{"-enablesaver", ".enablesaver", XrmoptionNoArg, (caddr_t) "on"},
	{"+enablesaver", ".enablesaver", XrmoptionNoArg, (caddr_t) "off"},
	{"-fullrandom", ".fullrandom", XrmoptionNoArg, (caddr_t) "on"},
	{"+fullrandom", ".fullrandom", XrmoptionNoArg, (caddr_t) "off"},
	{"-grabmouse", ".grabmouse", XrmoptionNoArg, (caddr_t) "on"},
	{"+grabmouse", ".grabmouse", XrmoptionNoArg, (caddr_t) "off"},
	{"-grabserver", ".grabserver", XrmoptionNoArg, (caddr_t) "on"},
	{"+grabserver", ".grabserver", XrmoptionNoArg, (caddr_t) "off"},
	{"-install", ".install", XrmoptionNoArg, (caddr_t) "on"},
	{"+install", ".install", XrmoptionNoArg, (caddr_t) "off"},
	{"-sound", ".sound", XrmoptionNoArg, (caddr_t) "on"},
	{"+sound", ".sound", XrmoptionNoArg, (caddr_t) "off"},
	{"-timeelapsed", ".timeelapsed", XrmoptionNoArg, (caddr_t) "on"},
	{"+timeelapsed", ".timeelapsed", XrmoptionNoArg, (caddr_t) "off"},
	{"-usefirst", ".usefirst", XrmoptionNoArg, (caddr_t) "on"},
	{"+usefirst", ".usefirst", XrmoptionNoArg, (caddr_t) "off"},
	{"-verbose", ".verbose", XrmoptionNoArg, (caddr_t) "on"},
	{"+verbose", ".verbose", XrmoptionNoArg, (caddr_t) "off"},
	{"-nice", ".nice", XrmoptionSepArg, (caddr_t) NULL},
	{"-lockdelay", ".lockdelay", XrmoptionSepArg, (caddr_t) NULL},
	{"-timeout", ".timeout", XrmoptionSepArg, (caddr_t) NULL},
	{"-font", ".font", XrmoptionSepArg, (caddr_t) NULL},
	{"-msgfont", ".msgfont", XrmoptionSepArg, (caddr_t) NULL},
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
	{"-icongeometry", ".icongeometry", XrmoptionSepArg, (caddr_t) NULL},

	{"-wireframe", ".wireframe", XrmoptionNoArg, (caddr_t) "on"},
	{"+wireframe", ".wireframe", XrmoptionNoArg, (caddr_t) "off"},

	{"-use3d", ".use3d", XrmoptionNoArg, (caddr_t) "on"},
	{"+use3d", ".use3d", XrmoptionNoArg, (caddr_t) "off"},
	{"-delta3d", ".delta3d", XrmoptionSepArg, (caddr_t) NULL},
	{"-none3d", ".none3d", XrmoptionSepArg, (caddr_t) NULL},
	{"-right3d", ".right3d", XrmoptionSepArg, (caddr_t) NULL},
	{"-left3d", ".left3d", XrmoptionSepArg, (caddr_t) NULL},
	{"-both3d", ".both3d", XrmoptionSepArg, (caddr_t) NULL},

    /* For modes with text, marquee & nose */
	{"-program", ".program", XrmoptionSepArg, (caddr_t) NULL},
	{"-messagesfile", ".messagesfile", XrmoptionSepArg, (caddr_t) NULL},
	{"-messagefile", ".messagefile", XrmoptionSepArg, (caddr_t) NULL},
	{"-message", ".message", XrmoptionSepArg, (caddr_t) NULL},
	{"-mfont", ".mfont", XrmoptionSepArg, (caddr_t) NULL},
    /* For modes with images, images and puzzle support ras and xpm */
	{"-imagefile", ".imagefile", XrmoptionSepArg, (caddr_t) NULL},
    /* For automata modes */
	{"-neighbors", ".neighbors", XrmoptionSepArg, (caddr_t) NULL},
    /* For eyes and julia modes */
	{"-mouse", ".mouse", XrmoptionNoArg, (caddr_t) "on"},
	{"+mouse", ".mouse", XrmoptionNoArg, (caddr_t) "off"},

#ifdef USE_XLOCKRC
	{"-cpasswd", ".cpasswd", XrmoptionSepArg, (caddr_t) NULL},
#endif
#ifdef USE_AUTO_LOGOUT
	{"-logoutAuto", ".logoutAuto", XrmoptionSepArg, (caddr_t) NULL},
#endif
#ifdef USE_BUTTON_LOGOUT
	{"-logoutButton", ".logoutButton", XrmoptionSepArg, (caddr_t) NULL},
{"-logoutButtonLabel", ".logoutButtonLabel", XrmoptionSepArg, (caddr_t) NULL},
 {"-logoutButtonHelp", ".logoutButtonHelp", XrmoptionSepArg, (caddr_t) NULL},
	{"-logoutFailedString", ".logoutFailedString", XrmoptionSepArg, (caddr_t) NULL},
#endif
#ifdef USE_DTSAVER
	{"-dtsaver", ".dtsaver", XrmoptionNoArg, (caddr_t) "on"},
	{"+dtsaver", ".dtsaver", XrmoptionNoArg, (caddr_t) "off"},
#endif
#ifdef USE_SOUND
	{"-locksound", ".locksound", XrmoptionSepArg, (caddr_t) NULL},
	{"-infosound", ".infosound", XrmoptionSepArg, (caddr_t) NULL},
	{"-validsound", ".validsound", XrmoptionSepArg, (caddr_t) NULL},
	{"-invalidsound", ".invalidsound", XrmoptionSepArg, (caddr_t) NULL},
#endif

};

#define genEntries (sizeof genTable / sizeof genTable[0])

static XrmOptionDescRec modeTable[] =
{
	{"-delay", "*delay", XrmoptionSepArg, (caddr_t) NULL},
	{"-batchcount", "*batchcount", XrmoptionSepArg, (caddr_t) NULL},
	{"-cycles", "*cycles", XrmoptionSepArg, (caddr_t) NULL},
	{"-size", "*size", XrmoptionSepArg, (caddr_t) NULL},
	{"-saturation", "*saturation", XrmoptionSepArg, (caddr_t) NULL},
};

#define modeEntries (sizeof modeTable / sizeof modeTable[0])

static XrmOptionDescRec cmdlineTable[] =
{
	{"-display", ".display", XrmoptionSepArg, (caddr_t) NULL},
	{"-visual", ".visual", XrmoptionSepArg, (caddr_t) NULL},
	{"-parent", ".parent", XrmoptionSepArg, (caddr_t) NULL},
	{"-nolock", ".nolock", XrmoptionNoArg, (caddr_t) "on"},
	{"+nolock", ".nolock", XrmoptionNoArg, (caddr_t) "off"},
	{"-remote", ".remote", XrmoptionNoArg, (caddr_t) "on"},
	{"+remote", ".remote", XrmoptionNoArg, (caddr_t) "off"},
	{"-inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "on"},
	{"+inwindow", ".inwindow", XrmoptionNoArg, (caddr_t) "off"},
	{"-inroot", ".inroot", XrmoptionNoArg, (caddr_t) "on"},
	{"+inroot", ".inroot", XrmoptionNoArg, (caddr_t) "off"},
#ifdef USE_DTSAVER
	{"-dtsaver", ".dtsaver", XrmoptionNoArg, (caddr_t) "on"},
	{"+dtsaver", ".dtsaver", XrmoptionNoArg, (caddr_t) "off"},
#endif
	{"-xrm", NULL, XrmoptionResArg, (caddr_t) NULL}
};

#define cmdlineEntries (sizeof cmdlineTable / sizeof cmdlineTable[0])

static XrmOptionDescRec nameTable[] =
{
	{"-name", ".name", XrmoptionSepArg, (caddr_t) NULL},
};

static OptionStruct opDesc[] =
{
	{"-help", "print out this message to standard output"},
	{"-version", "print version number (if >= 4.00) to standard output"},
	{"-resources", "print default resource file to standard output"},
	{"-display displayname", "X server to contact"},
	{"-visual visualname", "X visual to use"},
	{"-parent", "parent window id (for inwindow)"},
{"-name resourcename", "class name to use for resources (default is XLock)"},
	{"-delay usecs", "microsecond delay between screen updates"},
	{"-batchcount num", "number of things per batch"},
	{"-cycles num", "number of cycles per batch"},
	{"-size num", "size of a unit in a mode, default is 0"},
	{"-saturation value", "saturation of color ramp"},
	{"-/+nolock", "turn on/off no password required"},
	{"-/+inwindow", "turn on/off making xlock run in a window"},
	{"-/+inroot", "turn on/off making xlock run in the root window"},
	{"-/+remote", "turn on/off remote host access"},
	{"-/+mono", "turn on/off monochrome override"},
	{"-/+allowaccess", "turn on/off allow new clients to connect"},
#ifndef ALWAYS_ALLOW_ROOT
	{"-/+allowroot", "turn on/off allow root password to unlock"},
#else
 {"-/+allowroot", "turn on/off allow root password to unlock (off ignored)"},
#endif
	{"-/+debug", "whether to use debug xlock (yes/no)"},
	{"-/+echokeys", "turn on/off echo '?' for each password key"},
	{"-/+enablesaver", "turn on/off enable X server screen saver"},
	{"-/+grabmouse", "turn on/off grabbing of mouse and keyboard"},
	{"-/+grabserver", "turn on/off grabbing of server"},
	{"-/+install", "whether to use private colormap if needed (yes/no)"},
	{"-/+fullrandom", "turn on/off full random choice of mode-options"},
	{"-/+sound", "whether to use sound if configured for it (yes/no)"},
	{"-/+timeelapsed", "turn on/off clock"},
	{"-/+usefirst", "turn on/off using the first char typed in password"},
	{"-/+verbose", "turn on/off verbosity"},
	{"-nice level", "nice level for xlock process"},
	{"-lockdelay seconds", "number of seconds until lock"},
	{"-timeout seconds", "number of seconds before password times out"},
	{"-font fontname", "font to use for password prompt"},
	{"-msgfont fontname", "font to use for message"},
	{"-bg color", "background color to use for password prompt"},
	{"-fg color", "foreground color to use for password prompt"},
	{"-background color", "background color to use for password prompt"},
	{"-foreground color", "foreground color to use for password prompt"},
	{"-username string", "text string to use for Name prompt"},
	{"-password string", "text string to use for Password prompt"},
	{"-info string", "text string to use for instructions"},
  {"-validate string", "text string to use for validating password message"},
      {"-invalid string", "text string to use for invalid password message"},
	{"-geometry geom", "geometry for non-full screen lock"},
   {"-icongeometry geom", "geometry for password window (location ignored)"},

	{"-/+wireframe", "turn on/off wireframe"},

	{"-/+use3d", "turn on/off 3d view"},
   {"-delta3d value", "space between the center of your 2 eyes for 3d mode"},
	{"-none3d color", "color to be used for null in 3d mode"},
	{"-right3d color", "color to be used for the right eye in 3d mode"},
	{"-left3d color", "color to be used for the left eye in 3d mode"},
	{"-both3d color", "color to be used overlap in 3d mode"},

    /* For modes with text, marquee & nose */
   {"-program programname", "program to get messages from, usually fortune"},
	{"-messagesfile formatted-filename", "formatted file message to say"},
	{"-messagefile filename", "file message to say"},
	{"-message string", "message to say"},
	{"-mfont mode-fontname", "font for a specific mode"},
    /* For modes with images, images and puzzle support ras and xpm */
	{"-imagefile filename", "image file"},
    /* For automata modes */
      {"-neighbors num", "squares 4 or 8, hexagons 6, triangles 3, 9 or 12"},
    /* For eyes and julia modes */
	{"-/+mouse", "turn on/off the grabbing the mouse"},

#ifdef USE_XLOCKRC
	{"-cpasswd crypted-password", "text string of encrypted password"},
#endif
#ifdef USE_AUTO_LOGOUT
	{"-logoutAuto minutes", "number of minutes until auto logout (not more than forced auto logout time)"},
#endif
#ifdef USE_BUTTON_LOGOUT
	{"-logoutButton minutes", "number of minutes until logout button appears (not more than forced button time)"},
    {"-logoutButtonLabel string", "text string to use inside logout button"},
   {"-logoutButtonHelp string", "text string to use for logout button help"},
	{"-logoutFailedString string", "text string to use for failed logout attempts"},
#endif
#ifdef USE_DTSAVER
	{"-/+dtsaver", "turn on/off CDE Saver Mode"},
#endif
#ifdef USE_SOUND
	{"-locksound string", "sound to use at locktime"},
	{"-infosound string", "sound to use for information"},
	{"-validsound string", "sound to use when password is valid"},
	{"-invalidsound string", "sound to use when password is invalid"},
#endif
};

#define opDescEntries (sizeof opDesc / sizeof opDesc[0])

int         delay;
int         batchcount;
int         cycles;
int         size;
float       saturation;

Bool        nolock;
Bool        inwindow;
Bool        inroot;
Bool        mono;

Bool        allowaccess;

#ifdef ALWAYS_ALLOW_ROOT
Bool        allowroot = 1;

#else
Bool        allowroot;

#endif
Bool        debug;
Bool        echokeys;
Bool        enablesaver;
Bool        fullrandom;
Bool        grabmouse;
Bool        grabserver;
Bool        install;
Bool        sound;
Bool        timeelapsed;
Bool        usefirst;
Bool        verbose;
Bool        remote;

static char *visualname;
int         VisualClassWanted;

int         nicelevel;
int         lockdelay;
int         timeout;

char       *fontname;
char       *messagefont;
char       *background;
char       *foreground;
char       *text_user;
char       *text_pass;
char       *text_info;
char       *text_valid;
char       *text_invalid;
char       *geometry;
char       *icongeometry;

Bool        wireframe;

Bool        use3d;
float       delta3d;
char       *none3d;
char       *right3d;
char       *left3d;
char       *both3d;

#ifdef USE_XLOCKRC
char       *cpasswd;

#endif
#ifdef USE_AUTO_LOGOUT
int         logoutAuto;

#endif
#ifdef USE_BUTTON_LOGOUT
int         enable_button = 1;

int         logoutButton;
char       *logoutButtonLabel;
char       *logoutButtonHelp;
char       *logoutFailedString;

#endif
#ifdef USE_DTSAVER
Bool        dtsaver;

#endif
#ifdef USE_SOUND
char       *locksound;
char       *infosound;
char       *validsound;
char       *invalidsound;

#endif

static argtype genvars[] =
{
    {(caddr_t *) & allowaccess, "allowaccess", "AllowAccess", "off", t_Bool},
#ifndef ALWAYS_ALLOW_ROOT
	{(caddr_t *) & allowroot, "allowroot", "AllowRoot", "off", t_Bool},
#endif
	{(caddr_t *) & debug, "debug", "Debug", "off", t_Bool},
	{(caddr_t *) & echokeys, "echokeys", "EchoKeys", "off", t_Bool},
    {(caddr_t *) & enablesaver, "enablesaver", "EnableSaver", "off", t_Bool},
	{(caddr_t *) & fullrandom, "fullrandom", "FullRandom", "off", t_Bool},
	{(caddr_t *) & grabmouse, "grabmouse", "GrabMouse", "on", t_Bool},
	{(caddr_t *) & grabserver, "grabserver", "GrabServer", "off", t_Bool},
	{(caddr_t *) & install, "install", "Install", "on", t_Bool},
	{(caddr_t *) & mono, "mono", "Mono", "off", t_Bool},
	{(caddr_t *) & sound, "sound", "Sound", "off", t_Bool},
    {(caddr_t *) & timeelapsed, "timeelapsed", "TimeElapsed", "off", t_Bool},
	{(caddr_t *) & usefirst, "usefirst", "Usefirst", "off", t_Bool},
	{(caddr_t *) & verbose, "verbose", "Verbose", "off", t_Bool},
	{(caddr_t *) & visualname, "visual", "Visual", "", t_String},

	{(caddr_t *) & nicelevel, "nice", "Nice", DEF_NICE, t_Int},
   {(caddr_t *) & lockdelay, "lockdelay", "LockDelay", DEF_LOCKDELAY, t_Int},
	{(caddr_t *) & timeout, "timeout", "Timeout", DEF_TIMEOUT, t_Int},
	{(caddr_t *) & fontname, "font", "Font", DEF_FONT, t_String},
    {(caddr_t *) & messagefont, "msgfont", "MsgFont", DEF_MSGFONT, t_String},
    {(caddr_t *) & background, "background", "Background", DEF_BG, t_String},
    {(caddr_t *) & foreground, "foreground", "Foreground", DEF_FG, t_String},
	{(caddr_t *) & text_user, "username", "Username", DEF_NAME, t_String},
	{(caddr_t *) & text_pass, "password", "Password", DEF_PASS, t_String},
	{(caddr_t *) & text_info, "info", "Info", DEF_INFO, t_String},
     {(caddr_t *) & text_valid, "validate", "Validate", DEF_VALID, t_String},
   {(caddr_t *) & text_invalid, "invalid", "Invalid", DEF_INVALID, t_String},
    {(caddr_t *) & geometry, "geometry", "Geometry", DEF_GEOMETRY, t_String},
	{(caddr_t *) & icongeometry, "icongeometry", "IconGeometry", DEF_ICONGEOMETRY, t_String},

	{(caddr_t *) & wireframe, "wireframe", "WireFrame", "off", t_Bool},

	{(caddr_t *) & use3d, "use3d", "Use3D", "off", t_Bool},
	{(caddr_t *) & delta3d, "delta3d", "Delta3D", DEF_DELTA3D, t_Float},
	{(caddr_t *) & none3d, "none3d", "None3D", DEF_NONE3D, t_String},
	{(caddr_t *) & right3d, "right3d", "Right3D", DEF_RIGHT3D, t_String},
	{(caddr_t *) & left3d, "left3d", "Left3D", DEF_LEFT3D, t_String},
	{(caddr_t *) & both3d, "both3d", "Both3D", DEF_BOTH3D, t_String},

	{(caddr_t *) & program, "program", "Program", DEF_PROGRAM, t_String},
	{(caddr_t *) & messagesfile, "messagesfile", "Messagesfile", DEF_MESSAGESFILE, t_String},
	{(caddr_t *) & messagefile, "messagefile", "Messagefile", DEF_MESSAGEFILE, t_String},
	{(caddr_t *) & message, "message", "Message", DEF_MESSAGE, t_String},
	{(caddr_t *) & mfont, "mfont", "MFont", DEF_MFONT, t_String},
{(caddr_t *) & imagefile, "imagefile", "Imagefile", DEF_IMAGEFILE, t_String},
   {(caddr_t *) & neighbors, "neighbors", "Neighbors", DEF_NEIGHBORS, t_Int},
	{(caddr_t *) & mouse, "mouse", "Mouse", DEF_MOUSE, t_Bool},

#ifdef USE_XLOCKRC
	{(caddr_t *) & cpasswd, "cpasswd", "cpasswd", "", t_String},
#endif
#ifdef USE_AUTO_LOGOUT
	{(caddr_t *) & logoutAuto, "logoutAuto", "logoutAuto", DEF_AUTO_LOGOUT, t_Int},
#endif
#ifdef USE_BUTTON_LOGOUT
	{(caddr_t *) & logoutButton, "logoutButton", "LogoutButton", DEF_BUTTON_LOGOUT, t_Int},
	{(caddr_t *) & logoutButtonLabel, "logoutButtonLabel",
	 "LogoutButtonLabel", DEF_BTN_LABEL, t_String},
	{(caddr_t *) & logoutButtonHelp, "logoutButtonHelp",
	 "LogoutButtonHelp", DEF_BTN_HELP, t_String},
	{(caddr_t *) & logoutFailedString, "logoutFailedString",
	 "LogoutFailedString", DEF_FAIL, t_String},
#endif
#ifdef USE_SOUND
{(caddr_t *) & locksound, "locksound", "LockSound", DEF_LOCKSOUND, t_String},
{(caddr_t *) & infosound, "infosound", "InfoSound", DEF_INFOSOUND, t_String},
	{(caddr_t *) & validsound, "validsound", "ValidSound", DEF_VALIDSOUND, t_String},
	{(caddr_t *) & invalidsound, "invalidsound", "InvalidSound", DEF_INVALIDSOUND, t_String},
#endif
#if 0
    /* These resources require special handling.  They must be examined
     * before the display is opened.  They are evaluated by individual
     * calls to GetResource(), so they should not be evaluated again here.
     * For example, X-terminals need this special treatment.
     */
	{(caddr_t *) & nolock, "nolock", "NoLock", "off", t_Bool},
	{(caddr_t *) & inwindow, "inwindow", "InWindow", "off", t_Bool},
	{(caddr_t *) & inroot, "inroot", "InRoot", "off", t_Bool},
	{(caddr_t *) & remote, "remote", "Remote", "off", t_Bool},
#endif
};

#define NGENARGS (sizeof genvars / sizeof genvars[0])

static argtype modevars[] =
{
	{(caddr_t *) & delay, "delay", "Delay", DEF_DELAY, t_Int},
{(caddr_t *) & batchcount, "batchcount", "BatchCount", DEF_BATCHCOUNT, t_Int},
	{(caddr_t *) & cycles, "cycles", "Cycles", DEF_CYCLES, t_Int},
	{(caddr_t *) & size, "size", "Size", DEF_SIZE, t_Int},
	{(caddr_t *) & saturation, "saturation", "Saturation", DEF_SATURATION, t_Float},
};

#define NMODEARGS (sizeof modevars / sizeof modevars[0])

static int  modevaroffs[NMODEARGS] =
{
	offsetof(LockStruct, def_delay),
	offsetof(LockStruct, def_batchcount),
	offsetof(LockStruct, def_cycles),
	offsetof(LockStruct, def_size),
	offsetof(LockStruct, def_saturation)
};

#ifdef VMS
static char *
stripname(char *string)
{
	char       *characters;

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
Syntax(char *badOption)
{
	int         col, len, i;

	(void) fprintf(stderr, "%s:  bad command line option \"%s\"\n\n",
		       ProgramName, badOption);

	(void) fprintf(stderr, "usage:  %s", ProgramName);
	col = 8 + strlen(ProgramName);
	for (i = 0; i < (int) opDescEntries; i++) {
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
	for (i = 1; i < numprocs; i++) {
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
Help(void)
{
	int         i;

	(void) printf("usage:\n        %s [-options ...]\n\n", ProgramName);
	(void) printf("where options include:\n");
	for (i = 0; i < (int) opDescEntries; i++) {
		(void) printf("    %-28s %s\n", opDesc[i].opt, opDesc[i].desc);
	}

	(void) printf("    %-28s %s\n", "-mode mode", "animation mode");
	(void) printf("    where mode is one of:\n");
	for (i = 0; i < numprocs; i++) {
		int         j;

		(void) printf("          %-23s %s\n",
			      LockProcs[i].cmdline_arg, LockProcs[i].desc);
		for (j = 0; j < LockProcs[i].msopt->numvarsdesc; j++)
			(void) printf("              %-23s %s\n",
				      LockProcs[i].msopt->desc[j].opt, LockProcs[i].msopt->desc[j].desc);
	}
	(void) printf("\n");

	exit(0);
}

static void
Version(void)
{
	(void) printf("XLock version %s\n", VERSION);
	exit(0);
}

static void
DumpResources(void)
{
	int         i, j;

	(void) printf("%s.mode: %s\n", classname, DEF_MODE);

	for (i = 0; i < (int) NGENARGS; i++)
		(void) printf("%s.%s: %s\n",
			      classname, genvars[i].name, genvars[i].def);

	for (i = 0; i < numprocs; i++) {
		(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
			      "delay", LockProcs[i].def_delay);
		(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
			      "batchcount", LockProcs[i].def_batchcount);
		(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
			      "cycles", LockProcs[i].def_cycles);
		(void) printf("%s.%s.%s: %d\n", classname, LockProcs[i].cmdline_arg,
			      "size", LockProcs[i].def_size);
		(void) printf("%s.%s.%s: %g\n", classname, LockProcs[i].cmdline_arg,
			      "saturation", LockProcs[i].def_saturation);
		for (j = 0; j < LockProcs[i].msopt->numvarsdesc; j++)
			(void) printf("%s.%s.%s: %s\n", classname, LockProcs[i].cmdline_arg,
				      LockProcs[i].msopt->vars[j].name,
				      LockProcs[i].msopt->vars[j].def);
	}
	exit(0);
}


static void
LowerString(char *s)
{

	while (*s) {
		if (isupper(*s))
			*s += ('a' - 'A');
		s++;
	}
}

static void
GetResource(XrmDatabase database, char *parentname, char *parentclassname,
     char *name, char *classname, int valueType, char *def, caddr_t * valuep)
{
	char       *type;
	XrmValue    value;
	char       *string;
	char        buffer[1024];
	char       *fullname;
	char       *fullclassname;
	int         len;

	fullname = (char *) malloc(strlen(parentname) + strlen(name) + 2);
	fullclassname = (char *) malloc(strlen(parentclassname) +
					strlen(classname) + 2);

	(void) sprintf(fullname, "%s.%s", parentname, name);
	(void) sprintf(fullclassname, "%s.%s", parentclassname, classname);
	if (XrmGetResource(database, fullname, fullclassname, &type, &value)) {
		string = value.addr;
		len = value.size - 1;
	} else {
		string = def;
		if (string == NULL) {
			*valuep = NULL;
			return;
		}
		len = strlen(string);
	}
	(void) free((void *) fullname);
	(void) free((void *) fullclassname);

	(void) strncpy(buffer, string, sizeof (buffer));
	buffer[sizeof (buffer) - 1] = '\0';

	switch (valueType) {
		case t_String:
			{
/*-
 * PURIFY 4.0.1 on Solaris 2 and on SunOS4 reports a 1 byte memory leak on
 * the following line. */
				char       *s = (char *) malloc(len + 1);

				if (s == (char *) NULL) {
					char       *buf = (char *) malloc(strlen(ProgramName) + 80);

					(void) sprintf(buf,
						       "%s: GetResource - could not allocate memory", ProgramName);
					error(buf);
					(void) free((void *) buf);	/* Should never get here */
				}
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
parsefilepath(char *xfilesearchpath, char *TypeName, char *ClassName)
{
	XrmDatabase database = NULL;
	char       *appdefaults;
	char       *src;
	char       *dst;
	int         i, maxlen;

	i = maxlen = 0;
	for (src = xfilesearchpath; *src != '\0'; src++) {
		if (*src == '%') {
			src++;
			switch (*src) {
				case '%':
				case ':':
					i++;
					break;
				case 'T':
					i += strlen(TypeName);
					break;
				case 'N':
					i += strlen(ClassName);
					break;
				default:
					break;
			}
#ifdef VMS
		} else if (*src == '#') {	/* Colons required in VMS use # */
#else
		} else if (*src == ':') {
#endif
			if (i > maxlen)
				maxlen = i;
			i = 0;
		} else
			i++;
	}
	if (i > maxlen)
		maxlen = i;

	/* appdefaults will be at most this long */
	appdefaults = (char *) malloc(maxlen + 1);

	src = xfilesearchpath;
	dst = appdefaults;
	*dst = '\0';
	for (;;) {
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
		} else if (*src == '#') {	/* Colons required in VMS use # */
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
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 reports this also when using X11R5, but not
   * with OpenWindow 3.0 (X11R4 based). */
			database = XrmGetFileDatabase(appdefaults);
			break;
		} else {
			*dst++ = *src++;
			*dst = '\0';
		}
	}
	(void) free((void *) appdefaults);
	return database;
}

#ifdef VMS
/*-
 * FUNCTIONAL DESCRIPTION:
 * int get_info (chan, item, ret_str, ret_len)
 * Fetch a single characteristics from the pseudo-workstation
 * device and return the information.
 * (Taken and modified from the v5.4 fiche. PPL/SG 2/10/91
 * FORMAL PARAMETERS:
 *      chan:   the device channel
 *      item:   the characteristic to show
 *      ret_str: str pointer to information
 * ret_len: length of above string
 *  IMPLICIT INPUTS:
 *      none
 *  IMPLICIT OUTPUTS:
 *      none
 *  COMPLETION CODES:
 * errors returned by SYS$QIO
 *  SIDE EFFECTS:
 *      none
 * Hacked from Steve Garrett's xservername (as posted to INFO-VAX)
 */

int
get_info(unsigned long chan, unsigned long item,
	 char *ret_str, unsigned long *ret_len)
{
	unsigned long iosb[2];
	int         status;
	char        itembuf[BUFSIZE];
	struct dsc$descriptor itemval;

	itemval.dsc$w_length = BUFSIZE;
	itemval.dsc$b_dtype = 0;
	itemval.dsc$b_class = 0;
	itemval.dsc$a_pointer = &itembuf[0];

	status = sys$qiow(0, chan, IO$_SENSEMODE | IO$M_WS_DISPLAY, &iosb, 0, 0,
			  itemval.dsc$a_pointer,
			  itemval.dsc$w_length,
			  item, 0, 0, 0);
	if (status != SS$_NORMAL)
		return (status);
	if (iosb[0] != SS$_NORMAL)
		return (iosb[0]);

	itemval.dsc$w_length = iosb[1];
	*ret_len = iosb[1];
	itembuf[*ret_len] = 0;
	(void) strcpy(ret_str, &itembuf[0]);

	return (status);
}


/* routine that will return descripter of asciz string */

static int
descr(char *name)
{
	static      $dscp(d1, 0, 0);
	static      $dscp(d2, 0, 0);
	static      $dscp(d3, 0, 0);
	static      $dscp(d4, 0, 0);
	static      $dscp(d5, 0, 0);
	static      $dscp(d6, 0, 0);
	static dsc *tbl[] =
	{&d1, &d2, &d3, &d4, &d5, &d6};
	static int  didx = 0;

	if (didx == 6)
		didx = 0;

	tbl[didx]->len = strlen(name);
	tbl[didx]->ptr = name;

	return (int) tbl[didx++];
}

#endif

static void
open_display(void)
{
	char       *buf;
	struct hostent *host;

	if (!(dsp = XOpenDisplay(displayname))) {
		buf = (char *) malloc(strlen(ProgramName) + strlen(displayname) + 80);
		(void) sprintf(buf,
		"%s: unable to open display %s.\n", ProgramName, displayname);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	displayname = DisplayString(dsp);


#if defined(__cplusplus) || defined(c_plusplus)		/* !__bsdi__ */
#if HAVE_GETHOSTNAME
	extern int  gethostname(char *, size_t);

#else
#define gethostname(name,namelen) sysinfo(SI_HOSTNAME,name,namelen)
#endif
#endif

	if (gethostname(hostname, MAXHOSTNAMELEN)) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			   "%s: Can not get local hostname.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	if (!(host = (struct hostent *) gethostbyname(hostname))) {
		buf = (char *) malloc(strlen(ProgramName) + 80);
		(void) sprintf(buf,
			       "%s: Can not get hostbyname.\n", ProgramName);
		error(buf);
		(void) free((void *) buf);	/* Should never get here */
	}
	/*
	 * only restrict access to other displays if we are locking and if the
	 * Remote resource is not set.
	 */
	if (nolock || inwindow || inroot)
		remote = True;

#ifdef VMS
	if (!remote && ((displayname[0] == '_') |
			((displayname[0] == 'W') &
			 (displayname[1] == 'S') &
			 (displayname[2] == 'A')))) {	/* this implies a v5.4 system. The return
							   value is a device name, which must be
							   interrogated to find the real information */
		unsigned long chan;
		unsigned int status;
		unsigned long len;
		char        server_transport[100];

		status = sys$assign(descr(displayname), &chan, 0, 0);
		if (!(status & 1))
			displayname = " ";
		else {
			status = get_info(chan, DECW$C_WS_DSP_TRANSPORT,
					  server_transport, &len);
			if (!(status & 1))
				exit(status);

			if (strcmp(server_transport, "LOCAL")) {
				strcat(displayname, "'s display via ");
				strncat(displayname, server_transport, len);
				buf = (char *) malloc(strlen(ProgramName) + strlen(displayname) + 80);
				(void) sprintf(buf,
					       "%s: can not lock %s\n", ProgramName, displayname);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			}
		}
	} else {
#endif /* VMS */

		if (displayname != NULL) {
			char       *colon = (char *) strchr(displayname, ':');
			int         n = colon - displayname;

			if (colon == NULL) {
				buf = (char *) malloc(strlen(ProgramName) +
						   strlen(displayname) + 80);
				(void) sprintf(buf,
				 "%s: Malformed -display argument, \"%s\"\n",
					       ProgramName, displayname);
				error(buf);
				(void) free((void *) buf);	/* Should never get here */
			}
			if (!remote && n
			    && strncmp(displayname, "unix", n)
			    && strncmp(displayname, "localhost", n)) {
				int         badhost = 1;
				char      **hp;

				if (strncmp(displayname, host->h_name, n)) {
					for (hp = host->h_aliases; *hp; hp++) {
						if (!strncmp(displayname, *hp, n)) {
							badhost = 0;
							break;
						}
					}
					if (badhost) {
						*colon = (char) 0;
						buf = (char *) malloc(strlen(ProgramName) +
						   strlen(displayname) + 80);
						(void) sprintf(buf,
							       "%s: can not lock %s's display\n", ProgramName, displayname);
						error(buf);
						(void) free((void *) buf);	/* Should never get here */
					}
				}
			}
		}
#ifdef VMS
	}
#endif
}

static void
printvar(char *classname, argtype var)
{
	switch (var.type) {
		case t_String:
			(void) fprintf(stderr, "%s.%s: %s\n",
				  classname, var.name, *((char **) var.var));
			break;
		case t_Bool:
			(void) fprintf(stderr, "%s.%s: %s\n",
				       classname, var.name, *((int *) var.var)
				       ? "True" : "False");
			break;
		case t_Int:
			(void) fprintf(stderr, "%s.%s: %d\n",
				    classname, var.name, *((int *) var.var));
			break;
		case t_Float:
			(void) fprintf(stderr, "%s.%s: %g\n",
				  classname, var.name, *((float *) var.var));
			break;
	}
}


void
getResources(int argc, char **argv)
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
	char       *userfile = NULL;
	char       *homeenv;
	char       *userpath;
	char       *env;
	char       *serverString;
	int         i, j;
	int         max_length;
	extern Display *dsp;

	XrmInitialize();

	for (i = 0; i < argc; i++) {
		if (!strncmp(argv[i], "-help", strlen(argv[i])))
			Help();
		if (!strncmp(argv[i], "-version", strlen(argv[i])))
			Version();
	}

	/*
	 * get -name arg from command line so you can have different resource
	 * files for different configurations/machines etc...
	 */
#ifdef VMS
	/*Strip off directory and .exe; parts */
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
		if (env) {
			userfile = (char *) malloc(strlen(env) + strlen(homeenv) + 8);
			(void) strcpy(userfile, env);
			(void) strcat(userfile, "/%N:");
			(void) strcat(userfile, homeenv);
			(void) strcat(userfile, "/%N");
		} else {
#ifdef VMS
			userfile = (char *) malloc(2 * strlen(homeenv) + 31);
			(void) strcpy(userfile, homeenv);
			(void) strcat(userfile, "DECW$%N.DAT#");
			(void) strcat(userfile, homeenv);
			(void) strcat(userfile, "DECW$XDEFAULTS.DAT");
#else
			userfile = (char *) malloc(strlen(homeenv) + 4);
			(void) strcpy(userfile, homeenv);
			(void) strcat(userfile, "/%N");
#endif
		}
		userpath = userfile;
	}
	userDB = parsefilepath(userpath, "app-defaults", classname);

	if (userfile)
		(void) free((void *) userfile);

	(void) XrmMergeDatabases(applicationDB, &RDB);
	(void) XrmMergeDatabases(userDB, &RDB);
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 does not report this error. */
	(void) XrmMergeDatabases(cmdlineDB, &RDB);

	GetResource(RDB, ProgramName, classname, "display", "Display", t_String,
		    "", &displayname);
	GetResource(RDB, ProgramName, classname, "parent", "Parent", t_String,
		    "", &parentname);
	if (*parentname) {
		if (sscanf(parentname, "%ld", &parent))
			parentSet = True;
	}
	GetResource(RDB, ProgramName, classname, "nolock", "NoLock", t_Bool,
		    "off", (caddr_t *) & nolock);
	GetResource(RDB, ProgramName, classname, "inwindow", "InWindow", t_Bool,
		    "off", (caddr_t *) & inwindow);
	GetResource(RDB, ProgramName, classname, "inroot", "InRoot", t_Bool,
		    "off", (caddr_t *) & inroot);
	GetResource(RDB, ProgramName, classname, "remote", "Remote", t_Bool,
		    "off", (caddr_t *) & remote);
#ifdef USE_DTSAVER
	GetResource(RDB, ProgramName, classname, "dtsaver", "DtSaver", t_Bool,
		    "off", (caddr_t *) & dtsaver);
	if (dtsaver) {
		inroot = False;
		inwindow = True;
		nolock = True;
	}
#endif

	open_display();
	serverString = XResourceManagerString(dsp);
	if (serverString) {
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 reports this also when using X11R5, but not
   * with OpenWindow 3.0 (X11R4 based). */
		serverDB = XrmGetStringDatabase(serverString);
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 does not report this error. */
		(void) XrmMergeDatabases(serverDB, &RDB);
	} else {
		char       *buf = (char *) malloc(strlen(homeenv) + 12);

		(void) sprintf(buf, "%s/.Xdefaults", homeenv);
		homeDB = XrmGetFileDatabase(buf);
		(void) XrmMergeDatabases(homeDB, &RDB);
		(void) free((void *) buf);
	}

	XrmParseCommand(&generalDB, genTable, genEntries, ProgramName, &argc, argv);
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 does not report this error. */
	(void) XrmMergeDatabases(generalDB, &RDB);

	GetResource(RDB, ProgramName, classname, "mode", "Mode", t_String,
		    DEF_MODE, (caddr_t *) & mode);

	XrmParseCommand(&modeDB, modeTable, modeEntries, ProgramName, &argc, argv);
	(void) XrmMergeDatabases(modeDB, &RDB);

	for (i = 0; i < numprocs; i++) {
		XrmDatabase optDB = NULL;
		ModeSpecOpt *ms = LockProcs[i].msopt;

		if (!ms->numopts)
			continue;
		XrmParseCommand(&optDB, ms->opts, ms->numopts,
				ProgramName, &argc, argv);
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 does not report this error. */
		(void) XrmMergeDatabases(optDB, &RDB);
	}

	/* the RDB is set, now query load the variables from the database */

	for (i = 0; i < (int) NGENARGS; i++)
		GetResource(RDB, ProgramName, classname,
			    genvars[i].name, genvars[i].classname,
			    genvars[i].type, genvars[i].def, genvars[i].var);

	max_length = 0;
	for (i = 0; i < numprocs; i++) {
		j = strlen(LockProcs[i].cmdline_arg);
		if (j > max_length)
			max_length = j;
	}

	modename = (char *) malloc(strlen(ProgramName) + max_length + 2);
	modeclassname = (char *) malloc(strlen(classname) + max_length + 2);

	for (i = 0; i < numprocs; i++) {
		argtype    *v;
		ModeSpecOpt *ms = LockProcs[i].msopt;

		(void) sprintf(modename, "%s.%s", ProgramName, LockProcs[i].cmdline_arg);
		(void) sprintf(modeclassname, "%s.%s", classname, LockProcs[i].cmdline_arg);

		for (j = 0; j < (int) NMODEARGS; j++) {
			char        buf[16];
			void       *p = (void *) ((char *) (&LockProcs[i]) + modevaroffs[j]);

			if (modevars[j].type == t_Int)
				(void) sprintf(buf, "%d", *(int *) p);
			else
				(void) sprintf(buf, "%g", *(float *) p);
			GetResource(RDB, modename, modeclassname,
				    modevars[j].name, modevars[j].classname,
				    modevars[j].type, buf, (caddr_t *) p);
			if (!strcmp(mode, LockProcs[i].cmdline_arg)) {
				GetResource(RDB, modename, modeclassname,
				     modevars[j].name, modevars[j].classname,
				     modevars[j].type, buf, modevars[j].var);
			}
		}
		if (!ms->numvarsdesc)
			continue;
		v = ms->vars;
		for (j = 0; j < ms->numvarsdesc; j++)
			GetResource(RDB, modename, modeclassname, v[j].name, v[j].classname,
				    v[j].type, v[j].def, v[j].var);
	}

	/*XrmPutFileDatabase(RDB, "/tmp/xlock.rsrc.out"); */
/* PURIFY 4.0.1 on Solaris 2 reports an uninitialized memory read on the next
   * line.  PURIFY 4.0.1 on SunOS4 reports this also when using X11R5, but not
   * with OpenWindow 3.0 (X11R4 based). */
	(void) XrmDestroyDatabase(RDB);

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

#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
	if (fullLock()) {
#ifdef USE_AUTO_LOGOUT
		logoutAuto = 0;
#endif
#ifdef USE_BUTTON_LOGOUT
		enable_button = 0;
#endif
	} else {
#ifdef USE_AUTO_LOGOUT
#if ( USE_AUTO_LOGOUT > 0 )	/* Could be USER defined if 0 */
		if (logoutAuto > USE_AUTO_LOGOUT)
			logoutAuto = USE_AUTO_LOGOUT;
		else if (logoutAuto <= 0)	/* Handle 0 as a special case */
			logoutAuto = USE_AUTO_LOGOUT;
#else
		if (logoutAuto <= 0)	/* Handle 0 as a special case */
			(void) sscanf(DEF_AUTO_LOGOUT, "%d", &logoutAuto);
#endif
#endif
#ifdef USE_BUTTON_LOGOUT
#if ( USE_BUTTON_LOGOUT > 0 )	/* Could be USER defined if 0 */
		if (logoutButton > USE_BUTTON_LOGOUT)
			logoutButton = USE_BUTTON_LOGOUT;
		else if (logoutButton <= 0)	/* Handle 0 as a special case */
			logoutButton = USE_BUTTON_LOGOUT;
#else
		if (logoutButton <= 0)	/* Handle 0 as a special case */
			(void) sscanf(DEF_BUTTON_LOGOUT, "%d", &logoutButton);
#endif
#endif
	}
#endif
#ifdef USE_DTSAVER
	if (dtsaver) {
		inroot = False;
		inwindow = True;
		nolock = True;
		enablesaver = True;
		grabmouse = False;
		grabserver = False;
		install = False;
		lockdelay = 0;
		geometry = DEF_GEOMETRY;
	}
#endif

	if (verbose) {
		for (i = 0; i < (int) NGENARGS; i++)
			printvar(classname, genvars[i]);
		for (i = 0; i < (int) NMODEARGS; i++)
			printvar(modename, modevars[i]);
	}
	if (*visualname == '\0') {
		VisualClassWanted = -1;
		if (verbose)
			(void) fprintf(stderr, "Using default visual class\n");
	} else {
		VisualClassWanted = visualClassFromName(visualname);
		if (verbose)
			(void) fprintf(stderr, "Using visual class %s\n",
				       nameOfVisualClass(VisualClassWanted));
	}
#if HAVE_DIRENT_H
	/* Evaluate imagefile */
	if (strcmp(imagefile, DEF_IMAGEFILE)) {
		extern void get_dir(char *fullpath, char *dir, char *filename);
		extern int  sel_image(struct dirent *name);
		extern int  scan_dir(const char *directoryname, struct dirent ***namelist,
				     int         (*select) (struct dirent *),
			int         (*compare) (const void *, const void *));

		get_dir(imagefile, directory_r, filename_r);
		images_list = (struct dirent ***) malloc(sizeof (struct dirent **));

		num_list = scan_dir(directory_r, images_list, sel_image, NULL);
		image_list = *images_list;
		if (debug)
			for (i = 0; i < num_list; i++)
				(void) printf("File number %d: %s\n", i, image_list[i]->d_name);

	} else
		num_list = 0;
#endif
	(void) free((void *) modename);
	(void) free((void *) modeclassname);
	(void) free((void *) classname);
}


void
checkResources(void)
{
	int         i;

	if (delay < 0)
		Syntax("-delay argument must not be negative.");
	if (cycles < 0)
		Syntax("-cycles argument must not be negative.");
#if 0
	if (batchcount < 0)
		Syntax("-batchcount argument must not be negative.");
	if (size < 0)
		Syntax("-size argument must not be negative.");
#endif
	if (saturation < 0.0 || saturation > 1.0)
		Syntax("-saturation argument must be between 0.0 and 1.0.");
	if (delta3d < 0.0 || delta3d > 20.0)
		Syntax("-delta3d argument must be between 0.0 and 20.0.");

	/* in case they have a 'xlock*mode: ' empty resource */
	if (!mode || *mode == '\0')
		mode = DEF_MODE;

	for (i = 0; i < numprocs; i++) {
		if (!strcmp(LockProcs[i].cmdline_arg, mode)) {
			set_default_mode(&LockProcs[i]);
			break;
		}
	}
	if (i == numprocs) {
		(void) fprintf(stderr, "Unknown mode: ");
		Syntax(mode);
	}
}
