/* demo.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_ATHENA_KLUDGE	/* don't ask */
# undef HAVE_MOTIF
# define HAVE_ATHENA 1
#endif

#include <stdlib.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef VMS
# include <pwd.h>		/* for getpwuid() */
#else /* VMS */
# include "vms-pwd.h"
#endif /* VMS */

#ifdef HAVE_UNAME
# include <sys/utsname.h>	/* for uname() */
#endif /* HAVE_UNAME */

#include <stdio.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* We don't actually use any widget internals, but these are included
   so that gdb will have debug info for the widgets... */
#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>

#ifdef HAVE_MOTIF
# include <Xm/Xm.h>
# include <Xm/Text.h>
# include <Xm/List.h>
# include <Xm/ToggleB.h>
# include <Xm/MessageB.h>
# include <Xm/LabelG.h>
# include <Xm/RowColumn.h>

#else  /* HAVE_ATHENA */
  /* Athena demo code contributed by Jon A. Christopher <jac8782@tamu.edu> */
  /* Copyright 1997, with the same permissions as above. */
# include <X11/Shell.h>
# include <X11/Xaw/Form.h>
# include <X11/Xaw/Box.h>
# include <X11/Xaw/List.h>
# include <X11/Xaw/Command.h>
# include <X11/Xaw/Toggle.h>
# include <X11/Xaw/Viewport.h>
# include <X11/Xaw/Dialog.h>
# include <X11/Xaw/Scrollbar.h>
# include <X11/Xaw/Text.h>
#endif /* HAVE_ATHENA */

#include "version.h"
#include "prefs.h"
#include "resources.h"		/* for parse_time() */
#include "visual.h"		/* for has_writable_cells() */
#include "remote.h"		/* for xscreensaver_command() */
#include "usleep.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>


char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;

typedef struct {
  saver_preferences *a, *b;
} prefs_pair;


char *blurb (void) { return progname; }

static void run_hack (Display *dpy, int n);

#ifdef HAVE_ATHENA
static saver_preferences *global_prefs_kludge = 0;    /* I hate C so much... */
#endif /* HAVE_ATHENA */

static char *short_version = 0;

Atom XA_VROOT;
Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
Atom XA_SCREENSAVER_TIME, XA_SCREENSAVER_ID, XA_SELECT, XA_DEMO, XA_RESTART;

extern void create_demo_dialog (Widget, Visual *, Colormap);
extern void create_preferences_dialog (Widget, Visual *, Colormap);

extern Widget demo_dialog;
extern Widget label1;
extern Widget text_line;
extern Widget demo_form;
extern Widget demo_list;
extern Widget next, prev, done, restart, edit;

extern Widget preferences_dialog;
extern Widget preferences_form;
extern Widget prefs_done, prefs_cancel;
extern Widget timeout_text, cycle_text, fade_text, fade_ticks_text;
extern Widget lock_timeout_text, passwd_timeout_text;
extern Widget verbose_toggle, install_cmap_toggle, fade_toggle, unfade_toggle,
  lock_toggle;


#ifdef HAVE_MOTIF

# define set_toggle_button_state(toggle,state) \
  XmToggleButtonSetState ((toggle), (state), True)
# define set_text_string(text_widget,string) \
  XmTextSetString ((text_widget), (string))
# define add_button_callback(button,cb,arg) \
  XtAddCallback ((button), XmNactivateCallback, (cb), (arg))
# define add_toggle_callback(button,cb,arg) \
  XtAddCallback ((button), XmNvalueChangedCallback, (cb), (arg))
# define add_text_callback add_toggle_callback

#else  /* HAVE_ATHENA */

# define set_toggle_button_state(toggle,state) \
  XtVaSetValues((toggle), XtNstate, (state),  0)
# define set_text_string(text_widget,string) \
  XtVaSetValues ((text_widget), XtNvalue, (string), 0)
# define add_button_callback(button,cb,arg) \
  XtAddCallback ((button), XtNcallback, (cb), (arg))
# define add_toggle_callback add_button_callback
# define add_text_callback(b,c,a) ERROR!

#endif /* HAVE_ATHENA */


#define disable_widget(widget) \
  XtVaSetValues((widget), XtNsensitive, False, 0)


static char *
get_text_string (Widget text_widget)
{
#ifdef HAVE_MOTIF
  return XmTextGetString (text_widget);
#else  /* HAVE_ATHENA */
  char *string = 0;
  if (XtIsSubclass(text_widget, textWidgetClass))
    XtVaGetValues (text_widget, XtNstring, &string, 0);
  else if (XtIsSubclass(text_widget, dialogWidgetClass))
    XtVaGetValues (text_widget, XtNvalue, &string, 0);
  else
    string = 0;

  return string;
#endif /* HAVE_ATHENA */
}

static char *
get_label_string (Widget label_widget)
{
#ifdef HAVE_MOTIF
  char *label = 0;
  XmString xm_label = 0;
  XtVaGetValues (label_widget, XmNlabelString, &xm_label, 0);
  if (!xm_label)
    return 0;
  XmStringGetLtoR (xm_label, XmSTRING_DEFAULT_CHARSET, &label);
  return label;
#else  /* HAVE_ATHENA */
  char *label = 0;
  XtVaGetValues (label_widget, XtNlabel, &label, 0);
  return (label ? strdup(label) : 0);
#endif /* HAVE_ATHENA */
}


static void
set_label_string (Widget label_widget, char *string)
{
#ifdef HAVE_MOTIF
  XmString xm_string = XmStringCreate (string, XmSTRING_DEFAULT_CHARSET);
  XtVaSetValues (label_widget, XmNlabelString, xm_string, 0);
  XmStringFree (xm_string);
#else  /* HAVE_ATHENA */
  XtVaSetValues (label_widget, XtNlabel, string, 0);
#endif /* HAVE_ATHENA */
}


static void
format_into_label (Widget label, const char *arg)
{
  char *text = get_label_string (label);
  char *buf = (char *) malloc ((text ? strlen(text) : 0) + strlen(arg) + 100);

  if (!text || !strcmp (text, XtName (label)))
      strcpy (buf, "ERROR: RESOURCES ARE NOT INSTALLED CORRECTLY");
    else
      sprintf (buf, text, arg);

    set_label_string (label, buf);
    free (buf);
    XtFree (text);
}


/* Why this behavior isn't automatic in *either* toolkit, I'll never know.
 */
static void
ensure_selected_item_visible (Widget list)
{
#ifdef HAVE_MOTIF
  int *pos_list = 0;
  int pos_count = 0;
  if (XmListGetSelectedPos (list, &pos_list, &pos_count) && pos_count > 0)
    {
      int top = -2;
      int visible = 0;
      XtVaGetValues (list,
		     XmNtopItemPosition, &top,
		     XmNvisibleItemCount, &visible,
		     0);
      if (pos_list[0] >= top + visible)
	{
	  int pos = pos_list[0] - visible + 1;
	  if (pos < 0) pos = 0;
	  XmListSetPos (list, pos);
	}
      else if (pos_list[0] < top)
	{
	  XmListSetPos (list, pos_list[0]);
	}
    }
  if (pos_list)
    XtFree ((char *) pos_list);

#else  /* HAVE_ATHENA */
# ifdef HAVE_XawViewportSetCoordinates

  int margin = 16;	/* should be line height or something. */
  int count = 0;
  int pos;
  Dimension list_h = 0, vp_h = 0;
  Dimension top_margin = 4;  /* I don't know where this value comes from */
  Position vp_x = 0, vp_y = 0, current_y;
  double cratio;
  Widget viewport = XtParent(demo_list);
  Widget sb = (viewport ? XtNameToWidget(viewport, "*vertical") : 0);
  float sb_top = 0, sb_size = 0;
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  if (!current || !sb) return;

  XtVaGetValues(demo_list,
		XtNnumberStrings, &count,
		XtNheight, &list_h,
		0);
  if (count < 2 || list_h < 10) return;

  XtVaGetValues(viewport, XtNheight, &vp_h, XtNx, &vp_x, XtNy, &vp_y, 0);
  if (vp_h < 10) return;

  XtVaGetValues(sb, XtNtopOfThumb, &sb_top, XtNshown, &sb_size, 0);
  if (sb_size <= 0) return;

  pos = current->list_index;
  cratio = ((double) pos)  / ((double) count);
  current_y = (cratio * list_h);

  if (cratio < sb_top ||
      cratio > sb_top + sb_size)
    {
      if (cratio < sb_top)
	current_y -= (vp_h - margin - margin);
      else
	current_y -= margin;

      if ((long)current_y >= (long) list_h)
	current_y = (Position) ((long)list_h - (long)vp_h);

      if ((long)current_y < (long)top_margin)
	current_y = (Position)top_margin;

      XawViewportSetCoordinates (viewport, vp_x, current_y);
    }
# endif /* HAVE_XawViewportSetCoordinates */
#endif /* HAVE_ATHENA */
}


/* Callback for the text area:
   - note the text the user has entered;
   - change the corresponding element in `screenhacks';
   - write the .xscreensaver file;
   - tell the xscreensaver daemon to run that hack.
 */
static void
text_cb (Widget text_widget, XtPointer client_data, XtPointer call_data)
{
  Display *dpy = XtDisplay (text_widget);
  saver_preferences *p = (saver_preferences *) client_data;
  char *new_text = get_text_string (text_widget);

  int hack_number = -1;		/* 0-based */

#ifdef HAVE_ATHENA
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  hack_number = current->list_index;
#else  /* HAVE_MOTIF */
  int *pos_list = 0;
  int pos_count = 0;
  if (XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    hack_number = pos_list[0] - 1;
  if (pos_list)
    XtFree ((char *) pos_list);
#endif /* HAVE_MOTIF */

  ensure_selected_item_visible (demo_list);

  if (hack_number < 0 || hack_number >= p->screenhacks_count)
    {
      set_text_string (text_widget, "");
      XBell (dpy, 0);
    }
  else
    {
fprintf(stderr, "%d:\nold: %s\nnew: %s\n",
	hack_number, p->screenhacks [hack_number], new_text);

      if (p->screenhacks [hack_number])
	free (p->screenhacks [hack_number]);
      p->screenhacks [hack_number] = strdup (new_text);

#ifdef HAVE_MOTIF

      XmListDeselectAllItems (demo_list);
      {
	XmString xmstr = XmStringCreate (new_text, XmSTRING_DEFAULT_CHARSET);
	XmListReplaceItemsPos (demo_list, &xmstr, 1, hack_number+1);
	XmStringFree (xmstr);
      }
      XmListSelectPos (demo_list, hack_number+1, True);

#else  /* HAVE_ATHENA */

      {
	Widget vp = XtParent(demo_list);
	Widget sb = (vp ? XtNameToWidget(vp, "*vertical") : 0);
	Dimension list_h = 0;
	Position vp_x = 0, vp_y = 0;
	float sb_top = 0;

	XawListUnhighlight (demo_list);

	XtVaGetValues (vp, XtNx, &vp_x, 0);
	XtVaGetValues (sb, XtNtopOfThumb, &sb_top, 0);
	XtVaGetValues (demo_list, XtNheight, &list_h, 0);
	vp_y = (sb_top * list_h);
	XtVaSetValues (demo_list,
		       XtNlist, p->screenhacks,
		       XtNnumberStrings, p->screenhacks_count,
		       0);
	XawViewportSetCoordinates (vp, vp_x, vp_y);
	XawListHighlight (demo_list, hack_number);
      }

#endif /* HAVE_ATHENA */

      write_init_file (p, short_version);
      XSync (dpy, False);
      usleep (500000);		/* give the disk time to settle down */

      run_hack (dpy, hack_number+1);
    }
}


#ifdef HAVE_ATHENA
/* Bend over backwards to make hitting Return in the text field do the
   right thing. 
   */
static void text_enter (Widget w, XEvent *event, String *av, Cardinal *ac)
{
  text_cb (w, global_prefs_kludge, 0);	  /* I hate C so much... */
}

static XtActionsRec actions[] = {{"done",      text_enter}
			        };
static char translations[] = ("<Key>Return:	done()\n"
			      "<Key>Linefeed:	done()\n"
			      "Ctrl<Key>M:	done()\n"
			      "Ctrl<Key>J:	done()\n");
#endif /* HAVE_ATHENA */


/* Callback for the Run Next button.
 */
static void
next_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
#ifdef HAVE_ATHENA
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  int cnt;
  XtVaGetValues (demo_list, XtNnumberStrings, &cnt, 0);
  if (current->list_index == XAW_LIST_NONE ||
      current->list_index + 1 >= cnt)
    current->list_index = 0;
  else
    current->list_index++;
  XawListHighlight(demo_list, current->list_index);

  ensure_selected_item_visible (demo_list);
  current = XawListShowCurrent(demo_list);
  XtVaSetValues(text_line, XtNstring, current->string, 0);

  run_hack (XtDisplay (button), current->list_index + 1);

#else  /* HAVE_MOTIF */

  saver_preferences *p = (saver_preferences *) client_data;
  int *pos_list = 0;
  int pos_count = 0;
  int pos;
  if (! XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    {
      pos = 1;
      XmListDeselectAllItems (demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos, True);
    }
  else
    {
      pos = pos_list[0] + 1;
      if (pos > p->screenhacks_count)
	pos = 1;
      XmListDeselectAllItems (demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos, True);
    }
     
  ensure_selected_item_visible (demo_list);
  run_hack (XtDisplay (button), pos);
  if (pos_list)
    XtFree ((char *) pos_list);

#endif /* HAVE_MOTIF */
}


/* Callback for the Run Previous button.
 */
static void
prev_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
#ifdef HAVE_ATHENA
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  int cnt;
  XtVaGetValues (demo_list, XtNnumberStrings, &cnt, 0);
  if (current->list_index == XAW_LIST_NONE ||
      current->list_index <= 0)
    current->list_index = cnt-1;
  else
    current->list_index--;
  XawListHighlight(demo_list, current->list_index);

  ensure_selected_item_visible (demo_list);
  current = XawListShowCurrent(demo_list);
  XtVaSetValues(text_line, XtNstring, current->string, 0);

  run_hack (XtDisplay (button), current->list_index + 1);

#else  /* HAVE_MOTIF */

  saver_preferences *p = (saver_preferences *) client_data;
  int *pos_list = 0;
  int pos_count = 0;
  int pos;
  if (! XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    {
      pos = p->screenhacks_count;
      XmListDeselectAllItems (demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos, True);
    }
  else
    {
      pos = pos_list[0] - 1;
      if (pos == 0)
	pos = p->screenhacks_count;
      XmListDeselectAllItems (demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos, True);
    }
     
  ensure_selected_item_visible (demo_list);
  run_hack (XtDisplay (button), pos);
  if (pos_list)
    XtFree ((char *) pos_list);

#endif /* HAVE_MOTIF */
}


/* Callback run when a list element is double-clicked.
 */
static void
select_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
/*  saver_preferences *p = (saver_preferences *) client_data; */

#ifdef HAVE_ATHENA
  XawListReturnStruct *item = (XawListReturnStruct*)call_data;
  XtVaSetValues(text_line, XtNstring, item->string, 0);
  run_hack (XtDisplay (button), item->list_index + 1);

#else  /* HAVE_MOTIF */
  XmListCallbackStruct *lcb = (XmListCallbackStruct *) call_data;
  char *string = 0;
  if (lcb->item)
    XmStringGetLtoR (lcb->item, XmSTRING_DEFAULT_CHARSET, &string);
  set_text_string (text_line, (string ? string : ""));

  if (lcb->reason == XmCR_DEFAULT_ACTION && string)
    run_hack (XtDisplay (button), lcb->item_position);

  if (string)
    XtFree (string);
#endif /* HAVE_MOTIF */
}



static void pop_preferences_dialog (prefs_pair *pair);
static void make_preferences_dialog (prefs_pair *pair, Widget parent);

/* Callback for the Preferences button.
 */
static void
preferences_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  Widget parent = button;

  do {
    parent = XtParent(parent);
  } while (XtParent(parent));

  if (! preferences_dialog)
    make_preferences_dialog (pair, parent);
  pop_preferences_dialog (pair);
}

/* Callback for the Quit button.
 */
static void
quit_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  /* Save here?  Right now we don't need to, because we save every time
     the text field is edited, or the Preferences OK button is pressed.
  */
  exit (0);
}


/* Callback for the (now unused) Restart button.
 */
static void
restart_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  xscreensaver_command (XtDisplay (button), XA_RESTART, 0, False);
}


static void
pop_up_dialog_box (Widget dialog, Widget form)
{
#ifdef HAVE_ATHENA
  XtRealizeWidget (dialog);
  XtPopup (dialog, XtGrabNone);
#else  /* HAVE_MOTIF */
  XtRealizeWidget (form);
  XtManageChild (form);
#endif /* HAVE_MOTIF */
  XMapRaised (XtDisplay (dialog), XtWindow (dialog));
}


static void
make_demo_dialog (Widget toplevel_shell, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  /* saver_preferences *p2 = pair->b; */

  Widget parent = toplevel_shell;
  char **hacks = p->screenhacks;

  create_demo_dialog (parent,
		      DefaultVisualOfScreen (XtScreen (parent)),
		      DefaultColormapOfScreen (XtScreen (parent)));
  format_into_label (label1, short_version);

  add_button_callback (next,    next_cb,        (XtPointer) p);
  add_button_callback (prev,    prev_cb,        (XtPointer) p);
  add_button_callback (done,    quit_cb,        (XtPointer) p);
  if (restart)
    add_button_callback(restart,restart_cb,     (XtPointer) p);
  add_button_callback (edit,    preferences_cb, (XtPointer) pair);

#ifdef HAVE_MOTIF
  XtAddCallback (demo_list, XmNbrowseSelectionCallback,
		 select_cb, (XtPointer) p);
  XtAddCallback (demo_list, XmNdefaultActionCallback,
		 select_cb, (XtPointer) p);
  XtAddCallback (text_line, XmNactivateCallback, text_cb, (XtPointer) p);

  if (hacks)
    for (; *hacks; hacks++)
      {
	XmString xmstr = XmStringCreate (*hacks, XmSTRING_DEFAULT_CHARSET);
	XmListAddItem (demo_list, xmstr, 0);
	XmStringFree (xmstr);
      }

#else  /* HAVE_ATHENA */

  /* Hook up the text line. */

  XtAppAddActions(XtWidgetToApplicationContext(text_line),
		  actions, XtNumber(actions));
  XtOverrideTranslations(text_line, XtParseTranslationTable(translations));


  /* Must realize the widget before populating the list, or the dialog
     will be as wide as the longest string.
  */
  XtRealizeWidget (demo_dialog);

  XtVaSetValues (demo_list,
		 XtNlist, hacks,
		 XtNnumberStrings, p->screenhacks_count,
		 0);
  XtAddCallback (demo_list, XtNcallback, select_cb, p);

  /* Now that we've populated the list, make sure that the list is as
     wide as the dialog itself.
  */
  {
    Widget viewport = XtParent(demo_list);
    Widget subform = XtParent(viewport);
    Widget box = XtNameToWidget(demo_dialog, "*box");
    Widget label1 = XtNameToWidget(demo_dialog, "*label1");
    Widget label2 = XtNameToWidget(demo_dialog, "*label2");
    Dimension x=0, y=0, w=0, h=0, bw=0, w2=0;
    XtVaGetValues(subform,
		  XtNwidth, &w, XtNheight, &h, XtNborderWidth, &bw, 0);
    XtVaGetValues(box, XtNwidth, &w2, 0);
    if (w2 != w)
      XtResizeWidget(subform, w2, h, bw);

    /* Why isn't the viewport getting centered? */
    XtVaGetValues(viewport,
		  XtNx, &x, XtNy, &y, XtNheight, &h, XtNborderWidth, &bw, 0);
    XtConfigureWidget(viewport, x, y, w2-x-x, h, bw);

    /* And the text line, too. */
    XtVaGetValues(text_line,
		  XtNwidth, &w, XtNheight, &h, XtNborderWidth, &bw, 0);
    XtVaGetValues(viewport, XtNwidth, &w2, 0);
    if (w2 != w)
      XtResizeWidget(text_line, w2, h, bw);

    /* And the labels too. */
    XtVaGetValues(label1,
		  XtNwidth, &w, XtNheight, &h, XtNborderWidth, &bw, 0);
    if (w2 != w)
      XtResizeWidget(label1, w2, h, bw);

    XtVaGetValues(label2,
		  XtNwidth, &w, XtNheight, &h, XtNborderWidth, &bw, 0);
    if (w2 != w)
      XtResizeWidget(label2, w2, h, bw);

  }

#endif /* HAVE_ATHENA */

  pop_up_dialog_box(demo_dialog, demo_form);

#ifdef HAVE_ATHENA
  /* For Athena, have to do this after the dialog is managed. */
  ensure_selected_item_visible (demo_list);
#endif /* HAVE_ATHENA */
}


/* the Preferences dialog
 */

/* Helper for the text fields that contain time specifications:
   this parses the text, and does error checking.
 */
static void 
hack_time_text (Display *dpy, char *line, Time *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      value = parse_time (line, sec_p, True);
      value *= 1000;	/* Time measures in microseconds */
      if (value < 0)
	/*XBell (dpy, 0)*/;
      else
	*store = value;
    }
}


/* Callback for text fields that hold a time that default to seconds,
   when not fully spelled out.  client_data is an Time* where the value goes.
 */
static void
prefs_sec_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  hack_time_text (XtDisplay (button), get_text_string (button),
		(Time *) client_data, True);
}


/* Callback for text fields that hold a time that default to minutes,
   when not fully spelled out.  client_data is an Time* where the value goes.
 */
static void
prefs_min_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  hack_time_text (XtDisplay (button), get_text_string (button),
		(Time *) client_data, False);
}


/* Callback for text fields that hold an integer value.
   client_data is an int* where the value goes.
 */
static void
prefs_int_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  char *line = get_text_string (button);
  int *store = (int *) client_data;
  unsigned int value;
  char c;
  if (! *line)
    ;
  else if (sscanf (line, "%u%c", &value, &c) != 1)
    XBell (XtDisplay (button), 0);
  else
    *store = value;
}

/* Callback for toggle buttons.  client_data is an Bool* where the value goes.
 */
static void
prefs_bool_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  Bool *store = (Bool *) client_data;
#ifdef HAVE_MOTIF
  *store = ((XmToggleButtonCallbackStruct *) call_data)->set;
#else /* HAVE_ATHENA */
  Boolean state = FALSE;
  XtVaGetValues (button, XtNstate, &state, 0);
  *store = state;
#endif /* HAVE_ATHENA */
}


/* Callback for the Cancel button on the Preferences dialog.
 */
static void
prefs_cancel_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  XtDestroyWidget (preferences_dialog);
  preferences_dialog = 0;
  XMapRaised (XtDisplay (demo_dialog), XtWindow (demo_dialog));
}


/* Callback for the OK button on the Preferences dialog.
 */
static void
prefs_ok_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;

  prefs_cancel_cb (button, 0, call_data);

#ifdef HAVE_ATHENA
  /* Athena doesn't let us put callbacks on these widgets, so run
     all the callbacks by hand when OK is pressed. */
  prefs_min_cb (timeout_text,        (XtPointer) &p2->timeout,        0);
  prefs_min_cb (cycle_text,          (XtPointer) &p2->cycle,          0);
  prefs_sec_cb (fade_text,           (XtPointer) &p2->fade_seconds,   0);
  prefs_int_cb (fade_ticks_text,     (XtPointer) &p2->fade_ticks,     0);
  prefs_min_cb (lock_timeout_text,   (XtPointer) &p2->lock_timeout,   0);
  prefs_sec_cb (passwd_timeout_text, (XtPointer) &p2->passwd_timeout, 0);
#endif /* HAVE_ATHENA */

  p->timeout	    = p2->timeout;
  p->cycle	    = p2->cycle;
  p->lock_timeout   = p2->lock_timeout;
  p->passwd_timeout = p2->passwd_timeout;
  p->fade_seconds   = p2->fade_seconds;
  p->fade_ticks	    = p2->fade_ticks;
  p->verbose_p	    = p2->verbose_p;
  p->install_cmap_p = p2->install_cmap_p;
  p->fade_p	    = p2->fade_p;
  p->unfade_p	    = p2->unfade_p;
  p->lock_p	    = p2->lock_p;

  write_init_file (p, short_version);
}


static void
make_preferences_dialog (prefs_pair *pair, Widget parent)
{
  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;

  Screen *screen = XtScreen (parent);
  Display *dpy = XtDisplay (parent);

  *p2 = *p;	/* copy all slots of p into p2. */

  create_preferences_dialog (parent,
			     DefaultVisualOfScreen (screen),
			     DefaultColormapOfScreen (screen));

  add_button_callback (prefs_done,   prefs_ok_cb,     (XtPointer) pair);
  add_button_callback (prefs_cancel, prefs_cancel_cb, 0);

#define CB(widget,type,slot) \
	add_text_callback ((widget), (type), (XtPointer) (slot))
#define CBT(widget,type,slot) \
	add_toggle_callback ((widget), (type), (XtPointer) (slot))

#ifdef HAVE_MOTIF
  /* When using Athena widgets, we can't set callbacks for these,
     so in that case, we run them by hand when "OK" is pressed. */
  CB (timeout_text,		prefs_min_cb,  &p2->timeout);
  CB (cycle_text,		prefs_min_cb,  &p2->cycle);
  CB (fade_text,		prefs_sec_cb,  &p2->fade_seconds);
  CB (fade_ticks_text,		prefs_int_cb,  &p2->fade_ticks);
  CB (lock_timeout_text,	prefs_min_cb,  &p2->lock_timeout);
  CB (passwd_timeout_text,	prefs_sec_cb,  &p2->passwd_timeout);
#endif /* HAVE_MOTIF */

  CBT (verbose_toggle,		prefs_bool_cb, &p2->verbose_p);
  CBT (install_cmap_toggle,	prefs_bool_cb, &p2->install_cmap_p);
  CBT (fade_toggle,		prefs_bool_cb, &p2->fade_p);
  CBT (unfade_toggle,		prefs_bool_cb, &p2->unfade_p);
  CBT (lock_toggle,		prefs_bool_cb, &p2->lock_p);
#undef CB
#undef CBT

  {
    Bool found_any_writable_cells = False;
    int nscreens = ScreenCount(dpy);
    int i;
    for (i = 0; i < nscreens; i++)
      {
	Screen *s = ScreenOfDisplay (dpy, i);
	if (has_writable_cells (s, DefaultVisualOfScreen (s)))
	  {
	    found_any_writable_cells = True;
	    break;
	  }
      }

    if (! found_any_writable_cells)	/* fading isn't possible */
      {
	disable_widget (fade_text);
	disable_widget (fade_ticks_text);
	disable_widget (install_cmap_toggle);
	disable_widget (fade_toggle);
	disable_widget (unfade_toggle);
      }
  }
}


/* Formats a `Time' into "H:MM:SS".  (Time is microseconds.)
 */
static void
format_time (char *buf, Time time)
{
  int s = time / 1000;
  unsigned int h = 0, m = 0;
  if (s >= 60)
    {
      m += (s / 60);
      s %= 60;
    }
  if (m >= 60)
    {
      h += (m / 60);
      m %= 60;
    }
  sprintf (buf, "%u:%02u:%02u", h, m, s);
}


static void
pop_preferences_dialog (prefs_pair *pair)
{
  /* saver_preferences *p =  pair->a; */
  saver_preferences *p2 = pair->b;
  char s[100];

  format_time (s, p2->timeout);        set_text_string(timeout_text, s);
  format_time (s, p2->cycle);          set_text_string(cycle_text, s);
  format_time (s, p2->lock_timeout);   set_text_string(lock_timeout_text, s);
  format_time (s, p2->passwd_timeout); set_text_string(passwd_timeout_text, s);
  format_time (s, p2->fade_seconds);   set_text_string(fade_text, s);
  sprintf (s, "%u", p2->fade_ticks);   set_text_string(fade_ticks_text, s);

  set_toggle_button_state (verbose_toggle,	p2->verbose_p);
  set_toggle_button_state (install_cmap_toggle, p2->install_cmap_p);
  set_toggle_button_state (fade_toggle,		p2->fade_p);
  set_toggle_button_state (unfade_toggle,	p2->unfade_p);
  set_toggle_button_state (lock_toggle,		p2->lock_p);

  pop_up_dialog_box (preferences_dialog, preferences_form);
}


static void
run_hack (Display *dpy, int n)
{
  if (n <= 0) abort();
  xscreensaver_command (dpy, XA_DEMO, n, False);
}


static void
warning_dialog_dismiss_cb (Widget button, XtPointer client_data,
			   XtPointer call_data)
{
  Widget shell = (Widget) client_data;
  XtDestroyWidget (shell);
}

static void
warning_dialog (Widget parent, const char *message)
{
  char *msg = strdup (message);
  char *head;

  Widget dialog = 0;
  Widget label = 0;
  Widget ok = 0;
  int i = 0;

#ifdef HAVE_MOTIF

  Widget w;
  Widget container;
  XmString xmstr;
  Arg av[10];
  int ac = 0;

  ac = 0;
  dialog = XmCreateWarningDialog (parent, "versionWarning", av, ac);

  w = XmMessageBoxGetChild (dialog, XmDIALOG_MESSAGE_LABEL);
  if (w) XtUnmanageChild (w);
  w = XmMessageBoxGetChild (dialog, XmDIALOG_CANCEL_BUTTON);
  if (w) XtUnmanageChild (w);
  w = XmMessageBoxGetChild (dialog, XmDIALOG_HELP_BUTTON);
  if (w) XtUnmanageChild (w);

  ok = XmMessageBoxGetChild (dialog, XmDIALOG_OK_BUTTON);

  ac = 0;
  XtSetArg (av[ac], XmNnumColumns, 1); ac++;
  XtSetArg (av[ac], XmNorientation, XmVERTICAL); ac++;
  XtSetArg (av[ac], XmNpacking, XmPACK_COLUMN); ac++;
  XtSetArg (av[ac], XmNrowColumnType, XmWORK_AREA); ac++;
  XtSetArg (av[ac], XmNspacing, 0); ac++;
  container = XmCreateRowColumn (dialog, "container", av, ac);

#else  /* HAVE_ATHENA */

  Widget form;
  dialog = XtVaCreatePopupShell("warning_dialog", transientShellWidgetClass,
				parent, 0);
  form = XtVaCreateManagedWidget("warning_form", formWidgetClass, dialog, 0);

#endif /* HAVE_ATHENA */

  head = msg;
  while (head)
    {
      char name[20];
      char *s = strchr (head, '\n');
      if (s) *s = 0;

      sprintf (name, "label%d", i++);

#ifdef HAVE_MOTIF
      xmstr = XmStringCreate (head, XmSTRING_DEFAULT_CHARSET);
      ac = 0;
      XtSetArg (av[ac], XmNlabelString, xmstr); ac++;
      label = XmCreateLabelGadget (container, name, av, ac);
      XtManageChild (label);
      XmStringFree (xmstr);
#else  /* HAVE_ATHENA */
      
      label = XtVaCreateManagedWidget (name, labelWidgetClass,
				       form,
				       XtNleft, XtChainLeft,
				       XtNright, XtChainRight,
				       XtNlabel, head,
				       (label ? XtNfromVert : XtNtop),
				       (label ? label : XtChainTop),
				       0);

#endif /* HAVE_ATHENA */

      if (s)
	head = s+1;
      else
	head = 0;
    }

#ifdef HAVE_MOTIF

  XtManageChild (container);
  XtRealizeWidget (dialog);
  XtManageChild (dialog);

#else  /* HAVE_ATHENA */

  ok = XtVaCreateManagedWidget ("ok", commandWidgetClass, form,
				XtNleft, XtChainLeft,
				XtNbottom, XtChainBottom,
				XtNfromVert, label,
				0);

  XtRealizeWidget (dialog);
  XtPopup (dialog, XtGrabNone);

#endif /* HAVE_ATHENA */

  add_button_callback (ok, warning_dialog_dismiss_cb, dialog);

  free (msg);
}



/* The main demo-mode command loop.
 */

#if 0
static Bool
mapper (XrmDatabase *db, XrmBindingList bindings, XrmQuarkList quarks,
	XrmRepresentation *type, XrmValue *value, XPointer closure)
{
  int i;
  for (i = 0; quarks[i]; i++)
    {
      if (bindings[i] == XrmBindTightly)
	fprintf (stderr, (i == 0 ? "" : "."));
      else if (bindings[i] == XrmBindLoosely)
	fprintf (stderr, "*");
      else
	fprintf (stderr, " ??? ");
      fprintf(stderr, "%s", XrmQuarkToString (quarks[i]));
    }

  fprintf (stderr, ": %s\n", (char *) value->addr);

  return False;
}
#endif

static void
the_network_is_not_the_computer (Widget parent)
{
  Display *dpy = XtDisplay (parent);
  char *rversion, *ruser, *rhost;
  char *luser, *lhost;
  char *msg = 0;
  struct passwd *p = getpwuid (getuid ());
  const char *d = DisplayString (dpy);

# if defined(HAVE_UNAME)
  struct utsname uts;
  if (uname (&uts) < 0)
    lhost = "<UNKNOWN>";
  else
    lhost = uts.nodename;
# elif defined(VMS)
  strcpy (lhost, getenv("SYS$NODE"));
# else  /* !HAVE_UNAME && !VMS */
  strcat (lhost, "<UNKNOWN>");
# endif /* !HAVE_UNAME && !VMS */

  if (p && p->pw_name)
    luser = p->pw_name;
  else
    luser = "???";

  server_xscreensaver_version (dpy, &rversion, &ruser, &rhost);

  /* Make a buffer that's big enough for a number of copies of all the
     strings, plus some. */
  msg = (char *) malloc (10 * ((rversion ? strlen(rversion) : 0) +
			       (ruser ? strlen(ruser) : 0) +
			       (rhost ? strlen(rhost) : 0) +
			       strlen(lhost) +
			       strlen(luser) +
			       strlen(d) +
			       30));
  *msg = 0;

  if (!rversion || !*rversion)
    {
      sprintf (msg,
	       "Warning:\n\n"
	       "xscreensaver doesn't seem to be running on display \"%s\".",
	       d);
    }
  else if (p && ruser && *ruser && !!strcmp (ruser, p->pw_name))
    {
      /* Warn that the two processes are running as different users.
       */
      sprintf(msg,
	       "Warning:\n\n"
	      "%s is running as user \"%s\" on host \"%s\".\n"
	      "But the xscreensaver managing display \"%s\"\n"
	      "is running as user \"%s\" on host \"%s\".\n"
	      "\n"
	      "Since they are different users, they won't be reading/writing\n"
	      "the same ~/.xscreensaver file, so %s isn't\n"
	      "going to work right.\n"
	      "\n"
	      "Either re-run %s as \"%s\", or re-run\n"
	      "xscreensaver as \"%s\".\n",
	      progname, luser, lhost,
	      d,
	      (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	      progname,
	      progname, (ruser ? ruser : "???"),
	      luser);
    }
  else if (rhost && *rhost && !!strcmp (rhost, lhost))
    {
      /* Warn that the two processes are running on different hosts.
       */
      sprintf (msg,
	       "Warning:\n\n"
	       "%s is running as user \"%s\" on host \"%s\".\n"
	       "But the xscreensaver managing display \"%s\"\n"
	       "is running as user \"%s\" on host \"%s\".\n"
	       "\n"
	       "If those two machines don't share a file system (that is,\n"
	       "if they don't see the same ~%s/.xscreensaver file) then\n"
	       "%s won't work right.",
	       progname, luser, lhost,
	       d,
	       (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	       luser,
	       progname);
    }
  else if (!!strcmp (rversion, short_version))
    {
      /* Warn that the version numbers don't match.
       */
      sprintf (msg,
	       "Warning:\n\n"
	       "This is %s version %s.\n"
	       "But the xscreensaver managing display \"%s\"\n"
	       "is version %s.  This could cause problems.",
	       progname, short_version,
	       d,
	       rversion);
    }


  if (*msg)
    warning_dialog (parent, msg);

  free (msg);
}


static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

int
main (int argc, char **argv)
{
  XtAppContext app;
  prefs_pair Pair, *pair;
  saver_preferences P, P2, *p, *p2;
  Bool prefs = False;
  int i;
  Display *dpy;
  Widget toplevel_shell;
  char *real_progname = argv[0];
  char *s;

  s = strrchr (real_progname, '/');
  if (s) real_progname = s+1;

  p = &P;
  p2 = &P2;
  pair = &Pair;
  pair->a = p;
  pair->b = p2;
  memset (p,  0, sizeof (*p));
  memset (p2, 0, sizeof (*p2));

  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";

  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, &argc, argv,
				    defaults, 0, 0);
  dpy = XtDisplay (toplevel_shell);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);

  for (i = 1; i < argc; i++)
    {
      char *s = argv[i];
      if (s[0] == '-' && s[1] == '-')
	s++;
      if (!strcmp (s, "-prefs"))
	prefs = True;
      else
	{
	  fprintf (stderr, "usage: %s [ -display dpy-string ] [ -prefs ]\n",
		   real_progname);
	  exit (1);
	}
    }

  short_version = (char *) malloc (5);
  memcpy (short_version, screensaver_id + 17, 4);
  short_version [4] = 0;

  p->db = db;
  p->fading_possible_p = True;
  load_init_file (p);
  *p2 = *p;


  /* Now that Xt has been initialized, we can set our `progname' variable
     to something that makes more sense (like our "real" argv[0].)
   */
  progname = real_progname;


#ifdef HAVE_ATHENA
  global_prefs_kludge = p;	/* I hate C so much... */
#endif /* HAVE_ATHENA */

#if 0
  {
    XrmName name = { 0 };
    XrmClass class = { 0 };
    int count = 0;
    XrmEnumerateDatabase (db, &name, &class, XrmEnumAllLevels, mapper,
			  (XtPointer) &count);
  }
#endif


  XA_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_TIME = XInternAtom (dpy, "_SCREENSAVER_TIME", False);
  XA_SCREENSAVER_ID = XInternAtom (dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (dpy, "_SCREENSAVER_RESPONSE", False);
  XA_SELECT = XInternAtom (dpy, "SELECT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);

  make_demo_dialog (toplevel_shell, pair);

  if (prefs)
    {
      make_preferences_dialog (pair, toplevel_shell);
      pop_preferences_dialog (pair);
    }

  the_network_is_not_the_computer (preferences_dialog
				   ? preferences_dialog
				   : demo_dialog);

  XtAppMainLoop(app);
  exit (0);
}
