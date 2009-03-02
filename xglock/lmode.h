/*-
 * @(#)lmode.h	4.00 97/01/01 xlockmore
 *
 * lmode.h - mode management for xlock, the X Window System lockscreen.
 *
 * Copyright (c) 1991 by Patrick J. Naughton.
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 *
 * 08-Jul-98: Adapted to xglock by Remi Cohen-Scali <remi.cohenscali@pobox.com> 
 * 18-Mar-96: Ron Hitchens <ron@idiom.com>
 *		Extensive revision to define new data types for
 *		the new mode calling scheme.
 * 02-Jun-95: Extracted out of resource.c.
 *
 */

/*-
 * Declare external interface routines for supported screen savers.
 */

#ifndef _LMODE_H_
#define _LMODE_H_

/* -------------------------------------------------------------------- */

typedef struct LockStruct_s {
	gchar       *cmdline_arg;	/* mode name */
	gint         def_delay;		/* default delay for mode */
	gint         def_batchcount;
	gint         def_cycles;
	gint         def_size;
	gfloat       def_saturation;
	gchar       *desc;		/* text description of mode */
	void        *userdata;		/* for use by the mode */
} LockStruct;

LockStruct  LockProcs[] =
{
	{"blot",
	 200000, 6, 30, 1, 0.4,
	 "Shows Rorschach's ink blot test", (void *) NULL},
	{"bouboule",
	 5000, 100, 1, 15, 1.0,
	 "Shows Mimi's bouboule of moving stars", (void *) NULL},
	{"bug",
	 75000, 10, 32767, -4, 1.0,
	 "Shows Palmiter's bug evolution and garden of Eden", (void *) NULL},
	{"clock",
	 100000, -16, 200, -200, 1.0,
	 "Shows Packard's clock", (void *) NULL},
	{"crystal",
	 60000, -40, 200, -15, 1.0,
	 "Shows polygons in 2D plane groups", (void *) NULL},
	{"daisy",
	 100000, 300, 350, 1, 1.0,
	 "Shows a meadow of daisies", (void *) NULL},
	{"dclock",
	 10000, 1, 10000, 1, 0.2,
	 "Shows a floating digital clock", (void *) NULL},
	{"demon",
	 50000, 0, 1000, -7, 1.0,
	 "Shows Griffeath's cellular automata", (void *) NULL},
	{"eyes",
	 20000, -8, 5, 1, 1.0,
	 "Shows eyes following a bouncing grelb", (void *) NULL},
	{"flag",
	 50000, 1, 1000, -7, 1.0,
	 "Shows a flying flag of your operating system", (void *) NULL},
	{"flame",
	 750000, 20, 10000, 1, 1.0,
	 "Shows cosmic flame fractals", (void *) NULL},
	{"grav",
	 10000, -12, 1, 1, 1.0,
	 "Shows orbiting planets", (void *) NULL},
	{"helix",
	 25000, 1, 100, 1, 1.0,
	 "Shows string art", (void *) NULL},
	{"hyper",
	 10000, 1, 300, 1, 1.0,
	 "Shows a spinning tesseract in 4D space", (void *) NULL},
	{"ico",
	 100000, 0, 400, 0, 1.0,
	 "Shows a bouncing polyhedra", (void *) NULL},
	{"image",
	 2000000, -10, 1, 1, 1.0,
	 "Shows randomly appearing logos", (void *) NULL},
	{"kaleid",
	 20000, 1, 700, 1, 1.0,
	 "Shows a kaleidoscope", (void *) NULL},
	{"laser",
	 20000, -10, 200, 1, 1.0,
	 "Shows spinning lasers", (void *) NULL},
	{"life",
	 750000, 40, 140, 0, 1.0,
	 "Shows Conway's game of Life", (void *) NULL},
	{"life1d",
	 10000, 1, 10, 0, 1.0,
	 "Shows Wolfram's game of 1D Life", (void *) NULL},
	{"life3d",
	 1000000, 35, 85, 1, 1.0,
	 "Shows Bays' game of 3D Life", (void *) NULL},
	{"lightning",
	 10000, 1, 1, 1, 0.6,
	 "Shows Keith's fractal lightning bolts", (void *) NULL},
	{"lisa",
	 25000, 1, 256, -1, 1.0,
	 "Shows animated lisajous loops", (void *) NULL},
	{"lissie",
	 10000, -2, 2000, -200, 0.6,
	 "Shows lissajous worms", (void *) NULL},
	{"loop",
	 100000, 1, 1600, -12, 1.0,
	 "Shows Langton's self-producing loops", (void *) NULL},
	{"marquee",
	 100000, 1, 1, 1, 1.0,
	 "Shows messages", (void *) NULL},
	{"nose",
	 100000, 1, 1, 1, 1.0,
    "Shows a man with a big nose runs around spewing out messages", (void *) NULL},
	{"penrose",
	 10000, 1, 1, -40, 1.0,
	 "Shows Penrose's quasiperiodic tilings", (void *) NULL},
	{"petal",
	 10000, -500, 400, 1, 1.0,
	 "Shows various GCD Flowers", (void *) NULL},
	{"puzzle",
	 10000, 250, 1, 1, 1.0,
	 "Shows a puzzle being scrambled and then solved", (void *) NULL},
	{"pyro",
	 15000, 100, 1, -3, 1.0,
	 "Shows fireworks", (void *) NULL},
	{"qix",
	 30000, 1, 64, 1, 1.0,
	 "Shows spinning lines a la Qix(tm)", (void *) NULL},
	{"roll",
	 100000, 25, 1, -64, 1.0,
	 "Shows a rolling ball", (void *) NULL},
	{"rotor",
	 10000, 4, 20, 1, 0.4,
	 "Shows Tom's Roto-Rooter", (void *) NULL},
	{"shape",
	 10000, 100, 256, 1, 1.0,
	 "Shows stippled rectangles, ellipses, and triangles", (void *) NULL},
	{"sierpinski",
	 400000, 2000, 100, 1, 1.0,
	 "Shows Sierpinski's triangle", (void *) NULL},
	{"spline",
	 30000, -6, 2048, 1, 0.4,
	 "Shows colorful moving splines", (void *) NULL},
	{"star",
	 40000, 100, 1, 100, 0.2,
	 "Shows a star field with a twist", (void *) NULL},
	{"swarm",
	 10000, 100, 1, 1, 1.0,
	 "Shows a swarm of bees following a wasp", (void *) NULL},
	{"triangle",
	 10000, 1, 1, 1, 1.0,
	 "Shows a triangle mountain range", (void *) NULL},
	{"tube",
	 25000, 1, 20000, -200, 1.0,
	 "Shows an animated tube", (void *) NULL},
	{"turtle",
	 1000000, 1, 20, 1, 1.0,
	 "Shows turtle fractals", (void *) NULL},
	{"wire",
	 500000, 1000, 150, -8, 1.0,
	 "Shows a random circuit with 2 electrons", (void *) NULL},
	{"world",
	 100000, -16, 1, 1, 0.3,
	 "Shows spinning Earths", (void *) NULL},
	{"worm",
	 17000, -20, 10, -3, 1.0,
	 "Shows wiggly worms", (void *) NULL},
/* NORMALLY HIGH CPU USAGE MODES (kind of subjective) */
	{"ant",
	 1000, -3, 40000, -7, 1.0,
	 "Shows Langton's and Turk's generalized ants", (void *) NULL},
	{"ball",
	 10000, 10, 20, -100, 1.0,
	 "Shows bouncing balls", (void *) NULL},
	{"bat",
	 100000, -8, 1, 0, 1.0,
	 "Shows bouncing flying bats", (void *) NULL},
	{"bounce",
	 10000, -10, 1, 0, 1.0,
	 "Shows bouncing footballs", (void *) NULL},
	{"braid",
	 1000, 15, 100, 1, 1.0,
	 "Shows random braids and knots", (void *) NULL},
	{"drift",
	 10000, 30, 1, 1, 1.0,
	 "Shows cosmic drifting flame fractals", (void *) NULL},
	{"forest",
	 400000, 100, 200, 1, 1.0,
	 "Shows binary trees of a fractal forest", (void *) NULL},
	{"galaxy",
	 100, -5, 250, -3, 1.0,
	 "Shows crashing spiral galaxies", (void *) NULL},
	{"hop",
	 10000, 1000, 2500, 1, 1.0,
	 "Shows real plane iterated fractals", (void *) NULL},
	{"ifs",
	 1000, 1, 1, 1, 1.0,
	 "Shows a modified iterated function system", (void *) NULL},
	{"julia",
	 10000, 1000, 20, 1, 1.0,
	 "Shows the Julia set", (void *) NULL},
	{"maze",
	 1000, 1, 300, -40, 1.0,
	 "Shows a random maze and a depth first search solution", (void *) NULL},
	{"mountain",
	 1000, 30, 100, 1, 1.0,
	 "Shows Papo's mountain range", (void *) NULL},
	{"pacman",
	 100000, 10, 1, 0, 1.0,
	 "Shows Pacman(tm)", (void *) NULL},
	{"slip",
	 50000, 35, 50, 1, 1.0,
	 "Shows slipping blits", (void *) NULL},
	{"sphere",
	 10000, 1, 20, 0, 1.0,
	 "Shows a bunch of shaded spheres", (void *) NULL},
	{"spiral",
	 5000, -40, 350, 1, 1.0,
	 "Shows helixes of dots", (void *) NULL},
	{"strange",
	 1000, 1, 1, 1, 1.0,
	 "Shows strange attractors", (void *) NULL},
	{"swirl",
	 5000, 5, 1, 1, 1.0,
	 "Shows animated swirling patterns", (void *) NULL},
	{"voters",
	 1000, 0, 327670, 0, 1.0,
	 "Shows Dewdney's Voters", (void *) NULL},
	{"wator",
	 750000, 1, 32767, 0, 1.0,
	 "Shows Dewdney's Water-Torus planet of fish and sharks", (void *) NULL},
#if defined( USE_XPM ) || defined( USE_XPMINC )
	{"cartoon",
	 10000, 1, 1, 1, 1.0,
	 "Shows bouncing cartoons", (void *) NULL},
#endif
#ifdef USE_GL
	{"atlantis",
	 100, 4, 250, 6000, 1.0,
	 "Shows moving sharks/whales/dolphin", (void *) NULL},
	{"cage",
	 1000, 1, 1, 1, 1.0,
	 "Shows the Impossible Cage, an Escher-like GL scene", (void *) NULL},
	{"gears",
	 1000, 1, 2, 1, 1.0,
	 "Shows GL's gears", (void *) NULL},
	{"moebius",
	 1000, 1, 1, 1, 1.0,
       "Shows Moebius Strip II, an Escher-like GL scene with ants", (void *) NULL},
	{"morph3d",
	 1000, 0, 1, 1, 1.0,
	 "Shows GL morphing polyhedra", (void *) NULL},
	{"pipes",
	 1000, 2, 5, 500, 1.0,
	 "Shows a selfbuilding pipe system", (void *) NULL},
	{"sproingies",
	 1000, 5, 0, 400, 1.0,
  "Shows Sproingies!  Nontoxic.  Safe for pets and small children", (void *) NULL},
	{"stairs",
	 1000, 1, 1, 1, 1.0,
	 "Shows Infinite Stairs, an Escher-like GL scene", (void *) NULL},
	{"superquadrics",
	 1000, 25, 40, 1, 1.0,
	 "Shows 3D mathematical shapes", (void *) NULL},
#endif
/* NEW POSSIBLY, BUGGY MODES */
#ifdef USE_HACKERS
	{"fadeplot",
	 30000, 1500, 1, 1, 1.0,
	 "Shows fadeplot", (void *) NULL},
#endif
/* SPECIAL MODES */
	{"blank",
	 3000000, 1, 1, 1, 1.0,
	 "Shows nothing but a black screen", (void *) NULL},
#ifdef USE_BOMB
	{"bomb",
	 100000, 10, 20, 1, 1.0,
	 "Shows a bomb and will autologout after a time", (void *) NULL},
	{"random",
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank and bomb", (void *) NULL},
#else
	{"random",
	 1, 1, 1, 1, 1.0,
	 "Shows a random mode from above except blank", (void *) NULL},
#endif
};

/* Number of modes (set in main) */
guint nb_mode = 0;

/* Defaults values available */
#define DEF_DELAY		0
#define DEF_BATCHCOUNT		1
#define DEF_CYCLES		2
#define DEF_SIZE		3
#define DEF_SATURATION		4
#define NB_DEFAULTED_OPTIONS 	5

/* Default values options names */
static gchar *defaulted_options[NB_DEFAULTED_OPTIONS] = {
        "delay",
        "batchcount",
        "cycles",
        "size",
        "saturation",
    };

/* Number of default values options */
static guint nb_defaultedOptions = NB_DEFAULTED_OPTIONS;

/*------------------------------------*/
/*       Boolean xlock options        */
/*------------------------------------*/

/* boolean option entry */
typedef struct struct_option_bool_s {
	gchar       *cmdarg;
	gchar       *label;
	gchar       *desc;
        gchar	    defval;
        gchar 	    value; 
} struct_option_bool;

/* Description of the boolean options */
struct_option_bool BoolOpt[] =
{
  {"mono", "mono", "The mono option causes xlock to display monochrome", '\000', '\000'},
  {"nolock", "nolock", "The nolock option causes xlock to only draw the patterns and not lock the display", '\000', '\000'},
  {"remote", "remote", "The remote option causes xlock to not stop you from locking remote X11 server", '\000', '\000'},
  {"allowroot", "allowroot", "The allowroot option allow the root password to unlock the server", '\000', '\000'},
  {"enablesaver", "enablesaver", "This option enables the use of the normal Xserver screen saver", '\000', '\000'},
  {"resetsaver", "resetsaver", "This option enables the call of XResetScreenSaver", '\000', '\000'},
  {"allowaccess", "allowaccess", "For servers not allowing clients to modify host access, left the X11 server open", '\000', '\000'},
#ifdef USE_VTLOCK
  {"lockvt", "lockvt", "This option control the VT switch locking", '\000', '\000'},
#endif
  {"mousemotion", "mousemotion", "Allows to turn on/off the mouse sensitivity to bring up pass window", '\001', '\000'},
  {"grabmouse", "grabmouse", "This option causes xlock to grab mouse and keyboard", '\001', '\000'},
  {"grabserver", "grabserver", "The grabserver option causes xlock to grab the server", '\001', '\000'},
  {"echokeys", "echokeys", "This option causes xlock to echo a question mark for each typed character", '\000', '\000'},
  {"usefirst", "usefirst", "This option enables xlock to use the first keystroke in the password", '\000', '\000'},
  {"verbose", "verbose", "verbose launch", '\000', '\000'}, 
  {"debug", "debug", "This option allows xlock to be locked in a window", '\000', '\000'}, 
  {"wireframe", "wireframe", "This option turns on wireframe rendering mode for GL", '\000', '\000'}, 
  {"install", "install", "Allows xlock to install its own colormap if xlock runs out of colors", '\000', '\000'}, 
  {"sound", "sound", "Allows you to turn on and off sound if installed with the capability", '\000', '\000'}, 
  {"timeelapsed", "timeelapsed", "Allows you to find out how long a machine is locked", '\000', '\000'}, 
  {"fullrandom", "fullrandom", "Turn on/off randomness options within modes", '\000', '\000'}, 
  {"use3d", "use3d", "Turn on/off 3d view, available on bouboule, pyro, star, and worm", '\000', '\000'}, 
  {"mouse", "mouse", "Turn on and off mouse interaction in eyes, julia, and swarm", '\000', '\000'},
#if 0
  {"dtsaver", "dtsaver", "Turn on/off CDE Saver Mode. Only available if CDE support was compiled in", '\000', '\000'},
#endif
};

/* Number of boolean options (set in main) */
guint nb_boolOpt = 0; 

/* Boolean option dialog callback struct */
typedef struct struct_option_bool_callback_s {
    GtkWidget		*boolopt_dialog;
} struct_option_bool_callback;

/*------------------------------------*/
/*          General options           */
/*------------------------------------*/

/* Gen option entry struct */
typedef struct struct_option_gen_s {
    gchar      *cmdarg;
    gchar      *label;
    gchar      *desc;
    gchar      *help_anchor;
    GtkWidget  *text_widget; 
} struct_option_gen;

/* Description of the general option */
struct_option_gen generalOpt[] =
{
  {"username", "username", "text string to use for Name prompt", "opt_gen_", (GtkWidget *)NULL},
  {"password", "password", "text string to use for Password prompt", "gen_opt_", (GtkWidget *)NULL},
  {"info", "info", "text string to use for instruction", "gen_opt_", (GtkWidget *)NULL},
  {"validate", "validate", "text string to use for validating password message", "gen_opt_", (GtkWidget *)NULL},
  {"invalid", "invalidate", "text string to use for invalid password message", "gen_opt_", (GtkWidget *)NULL},
  {"message", "message", "message to say", "gen_opt_", (GtkWidget *)NULL}, 
  {"delay", "delay", "The delay option sets the speed at which a mode will operate", "gen_opt_", (GtkWidget *)NULL}, 
  {"batchcount", "batchcount", "This option sets number of things to do per batch to num", "gen_opt_", (GtkWidget *)NULL}, 
  {"cycles", "cycles", "This option delay is used for some mode as parameter", "gen_opt_", (GtkWidget *)NULL}, 
  {"ncolors", "ncolors", "This option delay is used for some mode as parameter", "gen_opt_", (GtkWidget *)NULL}, 
  {"size", "size", "This option delay is used for some mode as parameter", "gen_opt_", (GtkWidget *)NULL}, 
  {"saturation", "saturation", "This option delay is used for some GL mode as parameter", "gen_opt_", (GtkWidget *)NULL},
  {"nice", "nice", "This option sets system nicelevel of the xlock process", "gen_opt_", (GtkWidget *)NULL},
  {"lockdelay", "lockdelay", "This option set the delay between launch and lock", "gen_opt_", (GtkWidget *)NULL}, 
  {"timeout", "timeout", "The timeout option sets the password screen timeout", "gen_opt_", (GtkWidget *)NULL}, 
  {"geometry", "geometry", "geometry of mode window", "gen_opt_", (GtkWidget *)NULL}, 
  {"icongeometry", "icongeometry", "geometry of mode icon window", "gen_opt_", (GtkWidget *)NULL}, 
  {"glgeometry", "geometry", "geometry of GL mode window", "gen_opt_", (GtkWidget *)NULL}, 
  {"delta3d", "delta3d", "Turn on/off 3d view, available on bouboule, pyro, star, and worm", "gen_opt_", (GtkWidget *)NULL},
  {"neighbors", "neighbors", "Sets number of neighbors of cell (3, 4, 6, 9, 12) for automata modes", "gen_opt_", (GtkWidget *)NULL},
  {"cpasswd", "cpasswd", "Sets the key to be this text string to unlock", "gen_opt_", (GtkWidget *)NULL}, 
  {"program", "program", "program used as a fortune generator", "gen_opt_", (GtkWidget *)NULL},
#ifdef USE_AUTO_LOGOUT
  {"forceLogout", "forceLogout", "This option sets minutes to auto-logout", "gen_opt_", (GtkWidget *)NULL},
#endif
#ifdef USE_BUTTON_LOGOUT
  {"logoutButtonHelp", "logoutButtonHelp", "Text string is a message shown outside logout", "gen_opt_", (GtkWidget *)NULL}, 
  {"logoutButtonLabel", "logoutButtonLabel", "Text string is a message shown inside logout button", "gen_opt_", (GtkWidget *)NULL}, 
#endif
#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
  {"logoutFailedString", "logoutFailedString", "Text string is shown when a logout is attempted and fails", "gen_opt_", (GtkWidget *)NULL},
#endif
  {"startCmd", "startCmd", "Command to execute when the screen is locked", "gen_opt_", (GtkWidget *)NULL}, 
  {"endCmd", "endCmd", "Command to execute when the screen is unlocked", "gen_opt_", (GtkWidget *)NULL}, 
#if defined( USE_AUTO_LOGOUT ) || defined( USE_BUTTON_LOGOUT )
  {"logoutCmd", "logoutCmd", "Command to execute when the user is logged out", "gen_opt_", (GtkWidget *)NULL},
#endif
  {"mailCmd", "mailCmd", "Command to execute when mails are checked", "gen_opt_", (GtkWidget *)NULL}, 
#ifdef USE_DPMS
  {"dpmsstandby", "dpmsstandby", "Allows one to set DPMS Standby for monitor (0 is infinite)", "gen_opt_", (GtkWidget *)NULL}, 
  {"dpmssuspend", "dpmssuspend", "Allows one to set DPMS Suspend for monitor (0 is infinite)", "gen_opt_", (GtkWidget *)NULL}, 
  {"dpmsoff", "dpmsoff", "Allows one to set DPMS Power Off for monitor (0 is infinite)", "gen_opt_", (GtkWidget *)NULL},
#endif
};

/* Number of general options (set in main) */
guint nb_genOpt = 0; 

/* General option dialog callback struct */
typedef struct struct_option_gen_callback_s {
    GtkWidget  *gen_dialog; 
    GtkWidget  *text_widget; 
} struct_option_gen_callback;

/*------------------------------------*/
/*      Font/File/Color options       */
/*------------------------------------*/

/* Option type (font/color/file) */
typedef enum enum_type_option_fntcol_e {
    TFNTCOL_FONT = 0,
    TFNTCOL_COLOR,
    TFNTCOL_FILE
} enum_type_option_fntcol; 

/* Font/Color/File option entry struct */
typedef struct struct_option_fntcol_s {
    enum_type_option_fntcol 	type; 
    gchar       	       *cmdarg;
    gchar       	       *label;
    gchar       	       *desc;
    gchar		       *help_anchor;
    GtkWidget		       *entry; 
    GtkWidget		       *drawing_area; 
} struct_option_fntcol;

/* Description of the font and color option */
struct_option_fntcol fntcolorOpt[] =
{
  {TFNTCOL_COLOR, "bg", "background", "background color to use for password prompt", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_COLOR, "fg", "foreground", "foreground color to use for password prompt", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_COLOR, "none3d", "none3d", "color used for empty size in 3d mode", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_COLOR, "right3d", "right3d", "color used for right eye in 3d mode", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_COLOR, "left3d", "left3d", "color used for left eye in 3d mode", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_COLOR, "both3d", "both3d", "color used for overlapping images for left and right eye in 3d mode", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_FONT, "font", "font", "font to use for password prompt", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_FONT, "messagefont", "msgfont", "font to use for message", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL},
  {TFNTCOL_FONT, "planfont", "planfont", "font to use for lower part of password screen", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
  {TFNTCOL_FILE, "messagesfile", "messagesfile", "file to be used as the fortune generator", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
  {TFNTCOL_FILE, "messagefile", "messagefile", "file whose contents are displayed", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
  {TFNTCOL_FILE, "bitmap", "bitmap", "sets the xbm, xpm, or ras file to be displayed with some modes", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
  {TFNTCOL_FILE, "lifefile", "lifefile", "sets the lifeform (only one format: #P xlife format)", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
  {TFNTCOL_FILE, "life3dfile", "life3dfile", "sets the lifeform (only one format similar to #P xlife format)", "opt_fntcol_", 
     (GtkWidget *)NULL, (GtkWidget *)NULL}, 
};

/* Number of font/color/file options (set in main) */
guint nb_fntColorOpt = 0; 

/* Font/Color/File option dialog callback struct */
typedef struct struct_option_fntcol_callback_s {
    GtkWidget  		       *fntcol_dialog; 
    GtkWidget		       *entry; 
    GtkWidget		       *drawing_area;
} struct_option_fntcol_callback;

/* Colors handling */
GdkVisual *gdkvisual; 
GdkColormap *gdkcolormap; 

#endif /* !_LMODE_H_ */
