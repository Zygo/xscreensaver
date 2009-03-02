#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)xmlock.c      4.08 98/02/18 xlockmore";

#endif

/*-
 * xalock.c - main file for xalock, the gui interface to xlock.
 *
 * Copyright (c) 1997 by Charles Vidal
 *
 * See xlock.c for copying information.
 *
 * Requires -lxs -lXmu -lXaw
 * libsx.h is assumed to be installed in <X11/libsx.h>
 * for the ressource file , launcher editres.
 * thank's to Dominic Giamapolo <dbg@sgi.com> for his library libsx.
 *
 * Revision History:
 * Jun-25-97: written.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define HAVE_UNISTD_H 1
#endif /* HAVE_CONFIG_H */
#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/libsx.h>		/* gets us in the door with libsx */
#include "multireq.h"
#include "xalock.h"

#define NBTOGGLE 15

#define XLOCK	"xlock"
#define WINDOW_WIDTH 160
#define WINDOW_HEIGHT 100
#define WINDOW_GEOMETRY "160x100"

static int  delay;
static int  things2b;
static int  cycle2b;
static int  nice2b;
static int  timeout;
static int  untillock;
static int  saturation;
static int  delta3d;
static int  right3d;
static int  left3d;

/* variable for popup */

static char nameprog[1024];
static char namefile[1024];
static char namemesg[1024];
static char namepass[1024];
static char nameinst[1024];
static char namevalid[1024];
static char nameinvalid[1024];
static char nameprompt[1024];
static char namegetprog[1024];
static char namefmtfile[1024];
static char namefont[1024];
static char nameback[1024];
static char namefor[1024];
static char namegeom[1024];

static Widget ListMode;
static Widget Drawxlock;	/* the widget where is xlock show */

static pid_t numberprocess = -1;	/* PID of xlock process */

#include "modes.h"

static ToggleType toggle_table[] =
{
	{"mono       ", "turn on/off monochrome override", 0},
	{"nolock     ", "turn on/off no password required mode", 0},
	{"remote     ", "turn on/off remote host access", 0},
	{"allowroot  ", "turn on/off allow root password mode (ignored)", 0},
	{"enablesaver", "turn on/off enable X server screen saver", 0},
	{"allowaccess", "turn on/off allow new clients to connect", 0},
	{"grabmouse  ", "turn on/off grabbing of mouse and keyboard", 0},
	{"echokeys   ", "turn on/off echo '?' for each password key", 0},
    {"usefirst   ", "turn on/off using the first char typed in password", 0},
	{"verbose    ", "turn on/off verbose mode", 0},
	{"inwindow   ", "turn on/off making xlock run in a window", 0},
	{"inroot     ", "turn on/off making xlock run in the root window", 0},
	{"timeelapsed", "turn on/off clock", 0},
    {"install    ", "whether to use private colormap if needed (yes/no)", 0},
	{"use3d      ", "turn on/off 3d view", 0},
	{NULL, NULL, 0}};


static TagList programopt[] =
{
	{TAG_LABEL, "Program options", NULL, TAG_NOINIT},
	{TAG_STRING, " program to get messages from:", nameprog, TAG_INIT},
	{TAG_DONE, NULL, NULL, TAG_NOINIT}
};

static TagList messagesopt[] =
{
	{TAG_LABEL, "Messages options", NULL, TAG_NOINIT},
	{TAG_STRING, "file message to say:", namefile, TAG_INIT},
	{TAG_STRING, "message to say:", namemesg, TAG_INIT},
	{TAG_STRING, "Password prompt:", namepass, TAG_INIT},
	{TAG_STRING, "instructions:", nameinst, TAG_INIT},
	{TAG_STRING, "validating password message:", namevalid, TAG_INIT},
	{TAG_STRING, "invalid password message:", nameinvalid, TAG_INIT},
	{TAG_STRING, " Name prompt:", nameprompt, TAG_INIT},
	{TAG_STRING, "program to get messages from:", namegetprog, TAG_INIT},
	{TAG_STRING, "formatted file message to say:", namefmtfile, TAG_INIT},
	{TAG_DONE, NULL, NULL, TAG_NOINIT}
};

static TagList timeopt[] =
{
	{TAG_LABEL, "Times options", NULL, TAG_NOINIT},
	{TAG_INT, "microsecond delay updates:", &delay, TAG_INIT},
	{TAG_INT, "number of things per batch:", &things2b, TAG_INIT},
	{TAG_INT, "number of cycles per batch:", &cycle2b, TAG_INIT},
	{TAG_INT, "nice level for xlock process:", &nice2b, TAG_INIT},
{TAG_INT, "number of seconds before password times out:", &timeout, TAG_INIT},
	{TAG_INT, "number of seconds until lock:", &untillock, TAG_INIT},
	{TAG_DONE, NULL, NULL, TAG_NOINIT}
};

static TagList lookopt[] =
{
	{TAG_LABEL, "Font & colors & geometry options", NULL, TAG_NOINIT},
	{TAG_STRING, "font to use for password prompt:", namefont, TAG_INIT},
	{TAG_STRING, "background color :", nameback, TAG_INIT},
	{TAG_STRING, "foreground color :", namefor, TAG_INIT},
	{TAG_STRING, "geometry for non-full :", namegeom, TAG_INIT},
	{TAG_INT, "saturation of color ramp :", &saturation, TAG_INIT},
	{TAG_DONE, NULL, NULL, TAG_NOINIT}
};

static TagList treeDopt[] =
{
	{TAG_LABEL, "3D options", NULL, TAG_NOINIT},
	{TAG_INT, "delta3d :", &delta3d, TAG_INIT},
	{TAG_INT, "right3d :", &right3d, TAG_INIT},
	{TAG_INT, "left3d :", &left3d, TAG_INIT},
	{TAG_DONE, NULL, NULL, TAG_NOINIT}
};

static void
strcat2(char *dest, char *src1, char *src2)
{
	(void) strcat(dest, src1);
	(void) strcat(dest, src2);
	(void) strcat(dest, " ");
}

static void
strcatint2str(char *dest, char *src1, int num)
{
	char        tmp[50];

	(void) sprintf(tmp, "%d", num);
	(void) strcat(dest, src1);
	(void) strcat(dest, tmp);
	(void) strcat(dest, " ");
}

static void
getoptions(Widget w, void *data)
{
	(void) GetValues(data);
}

static void
toggleYes(Widget w, void *data)
{
	toggle_table[(int) data].value = TRUE;
}

static void
toggleNo(Widget w, void *data)
{
	toggle_table[(int) data].value = FALSE;
}

static void
Help(Widget w, void *data)
{
	GetYesNo((char *) data);
}

/*-
   Make the commande
 */
static void
launch(Widget w, void *data)
{
	int         i;
	int         index = 0;
	char        strcommand[1024];
	char        tmp[512];

	if ((index = GetCurrentListItem(ListMode)) == -1) {
		GetYesNo("Select one Mode please ... ");
		return;
	}
	(void) memset(strcommand, 0, 1023);	/* 1023 to make sure it will not core dump */
	(void) sprintf(tmp, "%s %s ", XLOCK, (char *) data);
	(void) strcat(strcommand, tmp);
	for (i = 0; i < NBTOGGLE; i++)
		if (toggle_table[i].value == TRUE) {
			(void) sprintf(tmp, "-%s ", toggle_table[i].name);
			(void) strcat(strcommand, tmp);
		}
	if (strlen(nameprog) > 0)
		strcat2(strcommand, "-program ", nameprog);
	if (strlen(namefile) > 0)
		strcat2(strcommand, "-messagefile ", namefile);
	if (strlen(namemesg) > 0)
		strcat2(strcommand, "-message ", namemesg);
	if (strlen(namepass) > 0)
		strcat2(strcommand, "-password ", namepass);
/*if (strlen(nameinst)>0) strcat2(strcommand,"%s\n", nameinst) ; */
	if (strlen(namevalid) > 0)
		strcat2(strcommand, "-validate ", namevalid);
	if (strlen(nameinvalid) > 0)
		strcat2(strcommand, "-invalid ", nameinvalid);
	if (strlen(nameprompt) > 0)
		strcat2(strcommand, "-username ", nameprompt);
/*if (strlen(namegetprog)>0) strcat2(strcommand,"%s", namegetprog) ; */
	if (strlen(namefmtfile) > 0)
		strcat2(strcommand, "-messagesfile ", namefmtfile);
	if (strlen(namefont) > 0)
		strcat2(strcommand, "-font ", namefont);
	if (strlen(nameback) > 0)
		strcat2(strcommand, "-bg ", nameback);
	if (strlen(namefor) > 0)
		strcat2(strcommand, "-fg ", namefor);

	if (delay != 0)
		strcatint2str(strcommand, "-delay ", delay);
	if (things2b != 0)
		strcatint2str(strcommand, "-batchcount ", things2b);
	if (cycle2b != 0)
		strcatint2str(strcommand, "-cycles ", cycle2b);
	if (nice2b != 0)
		strcatint2str(strcommand, "-nice ", nice2b);
	if (timeout != 0)
		strcatint2str(strcommand, "-timeout ", timeout);
	if (untillock != 0)
		strcatint2str(strcommand, "-lockdelay ", untillock);
	if (saturation != 0)
		strcatint2str(strcommand, "-saturation ", saturation);
	if (delta3d != 0)
		strcatint2str(strcommand, "-delta3d ", delta3d);
	if (right3d != 0)
		strcatint2str(strcommand, "-right3d ", right3d);
	(void) strcat2(strcommand, "-mode ", LockProcs[index]);
	(void) printf("%s\n", strcommand);
	(void) system(strcommand);
}

/*
 * quit() - Callback function for the quit button
 */
static void
quit(Widget w, void *data)
{
	int         n;

	if (GetYesNo("Quitter ???") == TRUE)
		if (numberprocess != -1) {
			(void) kill(numberprocess, SIGKILL);
			(void) wait(&n);
		}
	exit(0);
}

static void
execxlock(Widget w, char *str, int index, void *data)
{
	char        numberwidget[50];
	int         n;

	(void) sprintf(numberwidget, "%ld", XtWindow(Drawxlock));
	if (numberprocess != -1) {
		(void) kill(numberprocess, SIGKILL);
		(void) wait(&n);
	}
#ifdef VMS
#define FORK vfork
#else
#define FORK fork
#endif
	if ((numberprocess = FORK()) == -1)
		(void) fprintf(stderr, "Fork error\n");
	else if (numberprocess == 0) {
		(void) execlp(XLOCK, XLOCK, "-parent", numberwidget,
			      "-mode", str, "-geometry", WINDOW_GEOMETRY, "-delay", "100000",
			      "-nolock", "-inwindow", 0);
	}
}

/* This function sets up the display.  For any kind of a real program, 
 * you'll probably want to save the values returned by the MakeXXX calls
 * so that you have a way to refer to the display objects you have 
 * created (like if you have more than one drawing area, and want to
 * draw into both of them).
 */
static int
init_display(int argc, char **argv, void *data)
{
	Widget      w[20];
	Widget      tog[NBTOGGLE];
	Widget      togYes[NBTOGGLE];
	Widget      togNo[NBTOGGLE];
	Widget      togHelp[NBTOGGLE];
	int         i;

	argc = OpenDisplay(argc, argv);
	if (argc == FALSE)
		return argc;
	w[0] = MakeMenu("Options");
	w[1] = MakeMenuItem(w[0], "Program Option", getoptions, programopt);
	w[2] = MakeMenuItem(w[0], "Messages Options", getoptions, messagesopt);
	w[3] = MakeMenuItem(w[0], "Times Options", getoptions, timeopt);
	w[7] = MakeMenuItem(w[0], "Font Colors and Geometry options", getoptions, lookopt);
	w[6] = MakeMenuItem(w[0], "3D Options", getoptions, treeDopt);

	w[10] = MakeScrollList((char **) LockProcs, 150, 250, execxlock, NULL);
	ListMode = w[10];
	w[8] = MakeButton("Launch", launch, "");
	w[9] = MakeButton("Quit", quit, data);
	w[12] = MakeButton("Launch in window", launch, "-inwindow ");
	w[14] = MakeDrawArea(WINDOW_WIDTH, WINDOW_HEIGHT, NULL, NULL);
	Drawxlock = w[14];

	/* Create toggle  */
	for (i = 0; i < NBTOGGLE; i++) {
		tog[i] = MakeLabel(toggle_table[i].name);
		togYes[i] = MakeToggle("Yes", FALSE, NULL, toggleYes, (void *) i);
		togNo[i] = MakeToggle("No", FALSE, togYes[i], toggleNo, (void *) i);
		togHelp[i] = MakeButton("Help", Help, toggle_table[i].help);
	}

	SetWidgetPos(w[9], PLACE_RIGHT, w[14], NO_CARE, NULL);
	SetWidgetPos(w[14], PLACE_UNDER, w[0], NO_CARE, NULL);
	SetWidgetPos(w[10], PLACE_UNDER, w[14], NO_CARE, NULL);
	SetWidgetPos(w[8], PLACE_UNDER, w[10], NO_CARE, NULL);
	SetWidgetPos(w[12], PLACE_RIGHT, w[8], PLACE_UNDER, w[10]);

	SetWidgetPos(tog[0], PLACE_RIGHT, w[14], PLACE_UNDER, w[9]);
	SetWidgetPos(togYes[0], PLACE_RIGHT, tog[0], PLACE_UNDER, w[9]);
	SetWidgetPos(togNo[0], PLACE_RIGHT, togYes[0], PLACE_UNDER, w[9]);
	SetWidgetPos(togHelp[0], PLACE_RIGHT, togNo[0], PLACE_UNDER, w[9]);
	for (i = 1; i < NBTOGGLE; i++) {
		SetWidgetPos(tog[i], PLACE_RIGHT, w[14], PLACE_UNDER, tog[i - 1]);
		SetWidgetPos(togYes[i], PLACE_RIGHT, tog[i], PLACE_UNDER, tog[i - 1]);
		SetWidgetPos(togNo[i], PLACE_RIGHT, togYes[i], PLACE_UNDER, tog[i - 1]);
		SetWidgetPos(togHelp[i], PLACE_RIGHT, togNo[i], PLACE_UNDER, tog[i - 1]);
	}
	ShowDisplay();
	GetStandardColors();

	return argc;
}

int
main(int argc, char **argv)
{
	argc = init_display(argc, argv, NULL);	/* setup the display */
	if (argc == 0)
		exit(0);

	MainLoop();		/* go right into the main loop */
#ifdef VMS
	return 1;
#else
	return 0;
#endif
}
