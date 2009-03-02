#if !defined( lint ) && !defined( SABER )
static const char sccsid[] = "@(#)option.c	4.00 97/01/01 xlockmore";

#endif

/*-
 * option.c - option stuff for xmlock, the gui interface to xlock.
 *
 * Copyright (c) 1996 by Charles Vidal
 *
 * See xlock.c for copying information.
 *
 * Revision History:
 * Nov-96: written.
 *
 */

/*-
 * if you want add some options ,so it's not really difficult
 * 1: Learn Motif ... but only if you want to :)
 * 2: add a name of widget after geometry,font,program
 * 3: add a string ( char * ) after *inval=NULL,*geo=NULL
 * 4: add a callback like f_username with your new string
 * 5: copy and paste after
   geometry=XtVaCreateManagedWidget("geometry",xmPushButtonGadgetClass,options,NULL);
   XtAddCallback(geometry,XmNactivateCallback,f_geometry,NULL);
 *    change geometry by your name of widget, and f_geometry by the name
 *    of your new callback
 * 6: go to the church , call god for a miracle :)
 * GOOD LUCK
 */

#include <stdio.h>
#include <stdlib.h>
#ifdef VMS
#include <vms_x_fix.h>
#include <descrip.h>
#include <lib$routines.h>
#endif

#include <X11/cursorfont.h>
/* #include <Xm/XmAll.h> Does not work on my version of Lesstif */
#include <Xm/RowColumn.h>
#include <Xm/SelectioB.h>
#include <Xm/CascadeB.h>
#include <Xm/PushBG.h>
#include <Xm/List.h>
#include <Xm/Label.h>
#include <Xm/ToggleBG.h>


#include <Xm/MessageB.h> /* needed ? */

#include "option.h"

/*#define OPTIONS 8*/
/*#define REGULAR_OPTIONS (OPTIONS-2)*/

#define NBPROGNAME 3
#define MAXNAMES 10000
Widget      Menuoption;
static Widget Options[OPTIONS];

/* extern variables */
extern Widget toplevel;

void exitcallback(Widget w, XtPointer client_data, XtPointer call_data);


/* Widget */
static Widget PromptDialog, FontSelectionDialog, ProgramSelectionDialog;

/* strings */
char       *c_Options[OPTIONS];
extern char *r_Options[OPTIONS];

Widget Toggles[TOGGLES];

static char *prompts[OPTIONS] =
{
	(char *) "Enter the user name.",
	(char *) "Enter the password message.",
	(char *) "Enter the info message.",
	(char *) "Enter the valid message.",
	(char *) "Enter the invalid message.",
	(char *) "Enter the geometry mxn."
};

static char *prognames[NBPROGNAME] =
{
	(char *) "fortune",
	(char *) "finger",
	(char *) "echo hello world"
};
static int  loadfont = 0;

char *r_Toggles[TOGGLES] =
{
        (char *) "allowaccess",
        (char *) "allowroot",
        (char *) "debug",
        (char *) "echokeys",
        (char *) "echokeys",
        (char *) "enablesaver",
        (char *) "fullrandom",
        (char *) "grabmouse",
        (char *) "grabmouse",
        (char *) "grabserver",
        (char *) "install",
        (char *) "mousemotion",
        (char *) "nolock",
        (char *) "remote",
        (char *) "sound",
        (char *) "timeelapsed",
        (char *) "trackmouse",
        (char *) "use3d",
        (char *) "usefirst",
        (char *) "usefirst",
        (char *) "verbose",
        (char *) "wireframe",
	(char *) "mono"

};

/* string temp */
static char **whichone;


static void
managePrompt(char *s)
{
	int         ac;
	Arg         args[3];
	XmString    label_str1, label_str2;

	ac = 0;
	label_str1 = XmStringCreateSimple(s);
	XtSetArg(args[ac], XmNselectionLabelString, label_str1);
	ac++;
	if (*whichone != NULL) {
		label_str2 = XmStringCreateSimple(*whichone);
		XtSetArg(args[ac], XmNtextString, label_str2);
		ac++;
	} else {
		label_str2 = XmStringCreateSimple((char *) "");
		XtSetArg(args[ac], XmNtextString, label_str2);
		ac++;
	}

	XtSetValues(PromptDialog, args, ac);
/* PURIFY 4.0.1 on Solaris 2 reports a 71 byte memory leak on the next line. */
	XtManageChild(PromptDialog);
	XmStringFree(label_str1);
	XmStringFree(label_str2);
}

/* CallBack */
static void
f_option(Widget w, XtPointer client_data, XtPointer call_data)
{
			whichone = &c_Options[(int) client_data];
			managePrompt(prompts[(int) client_data]);
}

#if 0
/******************************
 * Callback for the font dialog
******************************/
static void
f_fontdia(Widget w, XtPointer client_data, XtPointer call_data)
{
	int         numdirnames, i;
	char      **dirnames;
	XmString    label_str;
	Widget      listtmp;
	Cursor      tmp;

	whichone = &c_Options[OPTIONS];
	if (!loadfont) {
		tmp = XCreateFontCursor(XtDisplay(toplevel), XC_watch);
		XDefineCursor(XtDisplay(toplevel), XtWindow(toplevel), tmp);
		dirnames = XListFonts(XtDisplay(toplevel), "*", MAXNAMES, &numdirnames);
		listtmp = XmSelectionBoxGetChild(FontSelectionDialog, XmDIALOG_LIST);
		for (i = 0; i < numdirnames; i++) {
			label_str = XmStringCreateSimple(dirnames[i]);
			XmListAddItemUnselected(listtmp, label_str, 0);
			XmStringFree(label_str);
		}
		tmp = XCreateFontCursor(XtDisplay(toplevel), XC_left_ptr);
		XDefineCursor(XtDisplay(toplevel), XtWindow(toplevel), tmp);
		loadfont = 1;
		XFreeFontNames(dirnames);
	}
	XtManageChild(FontSelectionDialog);
}

/******************************
 * Callback for the program dialog
******************************/
static void f_programdia(Widget w, XtPointer client_data, XtPointer call_data) {
			/*whichone = &c_Options[REGULAR_OPTIONS + 1];*/
/* PURIFY 4.0.1 on Solaris 2 reports a 71 byte memory leak on the next line. */
			XtManageChild(ProgramSelectionDialog);
	}
#endif

static void
f_Dialog(Widget w, XtPointer client_data, XtPointer call_data)
{
	static char *quoted_text;
	char       *nonquoted_text = NULL;
	XmSelectionBoxCallbackStruct *scb =
		(XmSelectionBoxCallbackStruct *) call_data;

	if (whichone != NULL)
		XtFree(*whichone);
	XmStringGetLtoR(scb->value, XmSTRING_DEFAULT_CHARSET, &nonquoted_text);
	if ((quoted_text = (char *) malloc(strlen(nonquoted_text) + 3)) ==
			NULL) {
		(void) fprintf(stderr, "Memory error\n");
		return;
	}
	(void) sprintf(quoted_text, "\"%s\"", nonquoted_text);
	(void) free((void *) nonquoted_text);
	*whichone = quoted_text;
}

/* Setup Widget */
void
setup_Option(Widget MenuBar)
{
	Arg         args[15];
	int         i, ac = 0;
	XmString    label_str;
	Widget      listtmp, pulldownmenu,exitwidget;

/** 
 ** Popup Menu File
 **/
	pulldownmenu = XmCreatePulldownMenu(MenuBar, "PopupFile", NULL, 0);
	label_str = XmStringCreateSimple((char *) "File");
	XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, MenuBar,
		XmNlabelString, label_str, XmNsubMenuId, pulldownmenu, NULL);
	XmStringFree(label_str);
	exitwidget = XtVaCreateManagedWidget("Exit", xmPushButtonGadgetClass, pulldownmenu, NULL);
	XtAddCallback(exitwidget, XmNactivateCallback, exitcallback,NULL);

	pulldownmenu = XmCreatePulldownMenu(MenuBar, "PopupOptions", NULL, 0);
	label_str = XmStringCreateSimple("Options");
	XtVaCreateManagedWidget("Options", xmCascadeButtonWidgetClass, MenuBar,
		XmNlabelString, label_str, XmNsubMenuId, pulldownmenu, NULL);
	XmStringFree(label_str);

	for (i = 0; i < OPTIONS; i++) {
		Options[i] = XtVaCreateManagedWidget(r_Options[i],
/*				xmToggleButtonGadgetClass, pulldownmenu, NULL);*/
				xmPushButtonGadgetClass, pulldownmenu, NULL);
		XtAddCallback(Options[i], XmNactivateCallback, f_option, (XtPointer) i);
	}
/* PURIFY 4.0.1 on Solaris 2 reports a 12 byte memory leak on the next line. */
	PromptDialog = XmCreatePromptDialog(toplevel, "PromptDialog", args, ac);
	XtAddCallback(PromptDialog, XmNokCallback, f_Dialog, NULL);

/* PURIFY 4.0.1 on Solaris 2 reports a 28 byte and a 12 byte memory leak on
   the next line. */
	FontSelectionDialog = XmCreateSelectionDialog(toplevel,
					     "FontSelectionDialog", NULL, 0);
	XtAddCallback(FontSelectionDialog, XmNokCallback, f_Dialog, NULL);

/* PURIFY 4.0.1 on Solaris 2 reports a 38 byte memory leak on the next line. */
	ProgramSelectionDialog = XmCreateSelectionDialog(toplevel,
					  "ProgramSelectionDialog", NULL, 0);
	XtAddCallback(ProgramSelectionDialog, XmNokCallback, f_Dialog, NULL);
	listtmp = XmSelectionBoxGetChild(ProgramSelectionDialog, XmDIALOG_LIST);
	for (i = 0; i < NBPROGNAME; i++) {
		label_str = XmStringCreateSimple(prognames[i]);
		XmListAddItemUnselected(listtmp, label_str, 0);
		XmStringFree(label_str);
	}

/**
 ** Make the menu with all options boolean
***/
	pulldownmenu = XmCreatePulldownMenu(MenuBar, "PopupSwitches", NULL, 0);
	label_str = XmStringCreateSimple("Switches");
	XtVaCreateManagedWidget("Switches", xmCascadeButtonWidgetClass, MenuBar,
		XmNlabelString, label_str, XmNsubMenuId, pulldownmenu, NULL);
	XmStringFree(label_str);

        for (i = 0; i < TOGGLES; i++) {
		Toggles[i] = XtVaCreateManagedWidget(r_Toggles[i],
				xmToggleButtonGadgetClass, pulldownmenu, NULL);
	}
}
