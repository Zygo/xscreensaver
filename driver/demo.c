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


#ifdef FORCE_ATHENA
# undef HAVE_MOTIF
# undef HAVE_GTK
# define HAVE_ATHENA 1
#endif
#ifdef FORCE_GTK
# undef HAVE_MOTIF
# undef HAVE_ATHENA
# define HAVE_GTK 1
#endif
#ifdef FORCE_MOTIF
# undef HAVE_GTK
# undef HAVE_ATHENA
# define HAVE_MOTIF 1
#endif

/* Only one, please. */
#ifdef HAVE_MOTIF
# undef HAVE_ATHENA
# undef HAVE_GTK
#endif
#ifdef HAVE_GTK
# undef HAVE_MOTIF
# undef HAVE_ATHENA
#endif
#ifdef HAVE_ATHENA
# undef HAVE_MOTIF
# undef HAVE_GTK
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

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif


#ifdef HAVE_MOTIF
# include <Xm/Xm.h>
# include <Xm/Text.h>
# include <Xm/List.h>
# include <Xm/ToggleB.h>
# include <Xm/MessageB.h>
# include <Xm/LabelG.h>
# include <Xm/RowColumn.h>

#elif defined(HAVE_ATHENA)
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

#elif defined(HAVE_GTK)
# include <gtk/gtk.h>
extern Display *gdk_display;
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

#ifdef HAVE_GTK
# define WIDGET GtkWidget *
# define POINTER gpointer
#else
# define WIDGET Widget
# define POINTER XtPointer
#endif



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

extern WIDGET demo_dialog;
extern WIDGET label1;
extern WIDGET text_line;
extern WIDGET text_activate;
extern WIDGET demo_form;
extern WIDGET demo_list;
extern WIDGET next;
extern WIDGET prev;
extern WIDGET done;
extern WIDGET restart;
extern WIDGET edit;

extern WIDGET preferences_dialog;
extern WIDGET preferences_form;
extern WIDGET prefs_done;
extern WIDGET prefs_cancel;
extern WIDGET timeout_text;
extern WIDGET cycle_text;
extern WIDGET fade_text;
extern WIDGET fade_ticks_text;
extern WIDGET lock_timeout_text;
extern WIDGET passwd_timeout_text;
extern WIDGET verbose_toggle;
extern WIDGET install_cmap_toggle;
extern WIDGET fade_toggle;
extern WIDGET unfade_toggle;
extern WIDGET lock_toggle;


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
# define disable_widget(widget) \
  XtVaSetValues((widget), XtNsensitive, False, 0)
# define widget_name(widget) XtName(widget)
# define widget_display(widget) XtDisplay(widget)
# define widget_screen(widget) XtScreen(widget)
# define CB_ARGS(a,b,c) (a,b,c)

#elif defined(HAVE_ATHENA)

# define set_toggle_button_state(toggle,state) \
  XtVaSetValues((toggle), XtNstate, (state),  0)
# define set_text_string(text_widget,string) \
  XtVaSetValues ((text_widget), XtNvalue, (string), 0)
# define add_button_callback(button,cb,arg) \
  XtAddCallback ((button), XtNcallback, (cb), (arg))
# define add_toggle_callback add_button_callback
# define add_text_callback(b,c,a) ERROR!
# define disable_widget(widget) \
  XtVaSetValues((widget), XtNsensitive, False, 0)
# define widget_name(widget) XtName(widget)
# define widget_display(widget) XtDisplay(widget)
# define widget_screen(widget) XtScreen(widget)
# define CB_ARGS(a,b,c) (a,b,c)

#elif defined(HAVE_GTK)

# define set_toggle_button_state(toggle,state) \
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(toggle),(state))
# define set_text_string(text_widget,string) \
  gtk_entry_set_text (GTK_ENTRY (text_widget), (string))
# define add_button_callback(button,cb,arg) \
  gtk_signal_connect_object (GTK_OBJECT (button), "clicked", \
                             GTK_SIGNAL_FUNC (cb), (arg))
# define add_toggle_callback(button,cb,arg) \
  gtk_signal_connect_object (GTK_OBJECT (button), "toggled", \
                             GTK_SIGNAL_FUNC (cb), (arg))
# define add_text_callback(button,cb,arg) \
  gtk_signal_connect_object (GTK_OBJECT (button), "activate", \
                             GTK_SIGNAL_FUNC (cb), (arg))
# define disable_widget(widget) \
  gtk_widget_set_sensitive (GTK_WIDGET(widget), FALSE)
# define widget_name(widget) gtk_widget_get_name(GTK_WIDGET(widget))
# define widget_display(widget) (gdk_display)
# define widget_screen(widget) (DefaultScreenOfDisplay(widget_display(widget)))
# define CB_ARGS(a,b,c) (b,a)

#endif /* HAVE_GTK */




static char *
get_text_string (WIDGET text_widget)
{
#ifdef HAVE_MOTIF
  return XmTextGetString (text_widget);
#elif defined(HAVE_ATHENA)
  char *string = 0;
  if (XtIsSubclass(text_widget, textWidgetClass))
    XtVaGetValues (text_widget, XtNstring, &string, 0);
  else if (XtIsSubclass(text_widget, dialogWidgetClass))
    XtVaGetValues (text_widget, XtNvalue, &string, 0);
  else
    string = 0;

  return string;
#elif defined(HAVE_GTK)
  return gtk_entry_get_text (GTK_ENTRY (text_widget));
#endif /* HAVE_GTK */
}


static char *
get_label_string (WIDGET label_widget)
{
#ifdef HAVE_MOTIF
  char *label = 0;
  XmString xm_label = 0;
  XtVaGetValues (label_widget, XmNlabelString, &xm_label, 0);
  if (!xm_label)
    return 0;
  XmStringGetLtoR (xm_label, XmSTRING_DEFAULT_CHARSET, &label);
  return label;
#elif defined(HAVE_ATHENA)
  char *label = 0;
  XtVaGetValues (label_widget, XtNlabel, &label, 0);
  return (label ? strdup(label) : 0);
#elif defined(HAVE_GTK)
  char *label = 0;
  gtk_label_get (GTK_LABEL (label_widget), &label);
  return label;
#endif /* HAVE_GTK */
}


static void
set_label_string (WIDGET label_widget, char *string)
{
#ifdef HAVE_MOTIF
  XmString xm_string = XmStringCreate (string, XmSTRING_DEFAULT_CHARSET);
  XtVaSetValues (label_widget, XmNlabelString, xm_string, 0);
  XmStringFree (xm_string);
#elif defined(HAVE_ATHENA)
  XtVaSetValues (label_widget, XtNlabel, string, 0);
#elif defined(HAVE_GTK)
  gtk_label_set_text (GTK_LABEL (label_widget), string);
#endif /* HAVE_GTK */
}


/* Given a label widget that has a %s in it, do the printf thing.
   If the label's string is obviously wrong, complain about resource lossage.
 */
static void
format_into_label (WIDGET label, const char *arg)
{
  char *text = get_label_string (label);
  char *buf = (char *) malloc ((text ? strlen(text) : 0) + strlen(arg) + 100);

  if (!text || !*text || !strcmp (text, widget_name (label)))
      strcpy (buf, "ERROR: RESOURCES ARE NOT INSTALLED CORRECTLY");
    else
      sprintf (buf, text, arg);

    set_label_string (label, buf);
    free (buf);

# ifndef HAVE_GTK
    XtFree (text);
# endif /* HAVE_GTK */
}


/* Why this behavior isn't automatic in *either* toolkit, I'll never know.
 */
static void
ensure_selected_item_visible (WIDGET list)
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

#elif defined(HAVE_ATHENA)
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
#elif defined(HAVE_GTK)

  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (list);
  GtkViewport *vp = GTK_VIEWPORT (GTK_BIN(scroller)->child);
  GtkList *list_widget = GTK_LIST (GTK_BIN(vp)->child);
  GList *kids;
  int nkids = 0;
  GtkWidget *selected = 0;
  int which = -1;
  GtkAdjustment *adj;
  gint parent_h, child_y, child_h, children_h, ignore;
  double ratio_t, ratio_b;

  GList *slist = list_widget->selection;
  selected = (slist ? GTK_WIDGET (slist->data) : 0);
  if (!selected)
    return;

  which = gtk_list_child_position (list_widget, GTK_WIDGET (selected));

  for (kids = gtk_container_children (GTK_CONTAINER (list_widget));
       kids; kids = kids->next)
    nkids++;

  adj = gtk_scrolled_window_get_vadjustment (scroller);                        

  gdk_window_get_geometry (GTK_WIDGET(vp)->window,
                           &ignore, &ignore, &ignore, &parent_h, &ignore);
  gdk_window_get_geometry (GTK_WIDGET(selected)->window,
                           &ignore, &child_y, &ignore, &child_h, &ignore);
  children_h = nkids * child_h;

  ratio_t = ((double) child_y) / ((double) children_h);
  ratio_b = ((double) child_y + child_h) / ((double) children_h);

  if (ratio_t < (adj->value / adj->upper) ||
      ratio_b > ((adj->value + adj->page_size) / adj->upper))
    {
      double target;

      if (ratio_t < (adj->value / adj->upper))
        {
          double ratio_w = ((double) parent_h) / ((double) children_h);
          double ratio_l = (ratio_b - ratio_t);
          target = ((ratio_t - ratio_w + ratio_l) * adj->upper);
        }
      else /* if (ratio_b > ((adj->value + adj->page_size) / adj->upper))*/
        {
          target = ratio_t * adj->upper;
        }

      if (target > adj->upper - adj->page_size)
        target = adj->upper - adj->page_size;
      if (target < 0)
        target = 0;

      gtk_adjustment_set_value (adj, target);
    }


#endif /* HAVE_GTK */
}


/* Callback for the text area:
   - note the text the user has entered;
   - change the corresponding element in `screenhacks';
   - write the .xscreensaver file;
   - tell the xscreensaver daemon to run that hack.

   (Note: in GTK, this one has a different arg list than the other callbacks.)
 */
static void
#ifdef HAVE_GTK
text_cb (WIDGET text_widget, POINTER client_data)
#else  /* !HAVE_GTK */
text_cb (WIDGET text_widget, POINTER client_data, POINTER call_data)
#endif /* !HAVE_GTK */
{
  saver_preferences *p = (saver_preferences *) client_data;
  char *new_text = get_text_string (text_widget);
  Display *dpy = widget_display (text_widget);
  Bool save = TRUE;

  int hack_number = -1;		/* 0-based */

#ifdef HAVE_ATHENA
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  hack_number = current->list_index;
#elif defined(HAVE_MOTIF)
  int *pos_list = 0;
  int pos_count = 0;
  if (XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    hack_number = pos_list[0] - 1;
  if (pos_list)
    XtFree ((char *) pos_list);
#elif defined(HAVE_GTK)
  GList *slist =
    GTK_LIST (GTK_BIN(GTK_BIN(demo_list)->child)->child)->selection;
  GtkWidget *selected = (slist ? GTK_WIDGET (slist->data) : 0);
  if (selected)
    hack_number =
      gtk_list_child_position (
                         GTK_LIST (GTK_BIN(GTK_BIN(demo_list)->child)->child),
                         GTK_WIDGET (selected));
#endif /* HAVE_GTK */

  ensure_selected_item_visible (demo_list);

  if (hack_number < 0 || hack_number >= p->screenhacks_count)
    {
      set_text_string (text_widget, "");
#ifdef HAVE_GTK
      gdk_beep();
#else  /* !HAVE_GTK */
      XBell (XtDisplay (text_widget), 0);
#endif /* !HAVE_GTK */
    }
  else
    {
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

#elif defined(HAVE_ATHENA)

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

#elif defined(HAVE_GTK)
      {
        GtkList *list_widget =
          GTK_LIST (GTK_BIN(GTK_BIN(demo_list)->child)->child);
        GList *slist = list_widget->selection;
        GtkWidget *selected = (slist ? GTK_WIDGET (slist->data) : 0);
        GtkLabel *label = (selected
                           ? GTK_LABEL (GTK_BIN (selected)->child) : 0);
        char *old_text = 0;
        gtk_label_get (label, &old_text);
        save = !!strcmp (new_text, old_text);
        if (label)
          gtk_label_set_text (label, new_text);
      }
#endif /* HAVE_GTK */

      if (save)
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


#ifdef HAVE_GTK
/* Helper for the Gtk versions of the Run Next and Run Previous buttons.
 */
static void
next_internal (GtkEntry *entry, gboolean next_p)
{
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (demo_list);
  GtkList *list_widget = GTK_LIST(GTK_BIN(GTK_BIN(scroller)->child)->child);
  GtkWidget *target = 0;
  GList *kids;
  int nkids = 0;
  int n;

  GList *slist = list_widget->selection;
  target = (slist ? GTK_WIDGET (slist->data) : 0);

  for (kids = gtk_container_children (GTK_CONTAINER (list_widget));
       kids; kids = kids->next)
    nkids++;

  if (target)
    {
      n = gtk_list_child_position (GTK_LIST (list_widget), target);
      n += (next_p ? 1 : -1);
      if (n >= nkids) n = 0;
      if (n < 0) n = nkids-1;
    }
  else if (next_p)
    n = 0;
  else
    n = nkids-1;

  gtk_list_select_item (GTK_LIST (list_widget), n);

  ensure_selected_item_visible ((WIDGET) scroller);

  run_hack (widget_display (scroller), n + 1);
}

#endif /* HAVE_GTK */



/* Callback for the Run Next button.
 */
static void
next_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
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

#elif defined(HAVE_MOTIF)

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

#elif defined(HAVE_GTK)
  next_internal (GTK_ENTRY (text_line), TRUE);
#endif /* HAVE_GTK */
}


/* Callback for the Run Previous button.
 */
static void
prev_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
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

#elif defined(HAVE_MOTIF)

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

#elif defined(HAVE_GTK)
  next_internal (GTK_ENTRY (text_line), FALSE);
#endif /* HAVE_GTK */
}


/* Callback run when a list element is double-clicked.
   (Note: in GTK, this one has a different arg list than the other callbacks.)
 */
#ifdef HAVE_GTK
static gint
select_cb (GtkWidget *button, GdkEventButton *event, gpointer client_data)
#else  /* !HAVE_GTK */
static void
select_cb (WIDGET button, POINTER client_data, POINTER call_data)
#endif /* !HAVE_GTK */
{
/*  saver_preferences *p = (saver_preferences *) client_data; */

#ifdef HAVE_ATHENA
  XawListReturnStruct *item = (XawListReturnStruct*)call_data;
  XtVaSetValues(text_line, XtNstring, item->string, 0);
  run_hack (XtDisplay (button), item->list_index + 1);

#elif defined(HAVE_MOTIF)
  XmListCallbackStruct *lcb = (XmListCallbackStruct *) call_data;
  char *string = 0;
  if (lcb->item)
    XmStringGetLtoR (lcb->item, XmSTRING_DEFAULT_CHARSET, &string);
  set_text_string (text_line, (string ? string : ""));

  if (lcb->reason == XmCR_DEFAULT_ACTION && string)
    run_hack (XtDisplay (button), lcb->item_position);

  if (string)
    XtFree (string);

#elif defined(HAVE_GTK)
  char *string = 0;
  gtk_label_get (GTK_LABEL (GTK_BIN(button)->child), &string);
  set_text_string (text_line, (string ? string : ""));

  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkViewport *vp = GTK_VIEWPORT (GTK_BIN(demo_list)->child);
      GtkList *lw = GTK_LIST (GTK_BIN(vp)->child);
      int which = gtk_list_child_position (lw, GTK_WIDGET (button));
      run_hack (gdk_display, which + 1);
    }

  return FALSE;
#endif /* HAVE_GTK */
}


static void pop_preferences_dialog (prefs_pair *pair);
static void make_preferences_dialog (prefs_pair *pair, Widget parent);

/* Callback for the Preferences button.
 */
static void
preferences_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;
#ifdef HAVE_GTK
  Widget parent = 0;
#else /* !HAVE_GTK */
  Widget parent = button;

  do {
    parent = XtParent(parent);
  } while (XtParent(parent));
#endif /* !HAVE_GTK */

  if (! preferences_dialog)
    make_preferences_dialog (pair, parent);
  *pair->b = *pair->a;
  pop_preferences_dialog (pair);
}


/* Callback for the Quit button.
 */
static void
quit_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  /* Save here?  Right now we don't need to, because we save every time
     the text field is edited, or the Preferences OK button is pressed.
  */
  exit (0);
}


/* Callback for the (now unused) Restart button.
 */
static void
restart_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  xscreensaver_command (widget_display (button), XA_RESTART, 0, False);
}


static void
pop_up_dialog_box (WIDGET dialog, WIDGET form)
{
#ifdef HAVE_ATHENA
  XtRealizeWidget (dialog);
  XtPopup (dialog, XtGrabNone);
#elif defined(HAVE_MOTIF)
  XtRealizeWidget (form);
  XtManageChild (form);
#endif /* HAVE_MOTIF */

#ifdef HAVE_GTK
  gtk_widget_show (dialog);
  gdk_window_show (GTK_WIDGET (dialog)->window);
  gdk_window_raise (GTK_WIDGET (dialog)->window);
#else /* !HAVE_GTK */
  XMapRaised (XtDisplay (dialog), XtWindow (dialog));
#endif /* !HAVE_GTK */
}


#ifdef HAVE_GTK
/* Callback for WM_DELETE_WINDOW on the main demo window.
 */
static void
destroy (GtkWidget *widget, gpointer data)
{
  gtk_main_quit ();
}

/* Callback for the "Run" button to the right of the text entry line.
 */
static void
select_button_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  gtk_signal_emit_by_name (GTK_OBJECT (text_line), "activate");
}
#endif /* HAVE_GTK */


static void
make_demo_dialog (Widget toplevel_shell, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  /* saver_preferences *p2 = pair->b; */
  Widget parent = toplevel_shell;
  char **hacks = p->screenhacks;

  create_demo_dialog (parent,
                      DefaultVisualOfScreen (widget_screen (parent)),
		      DefaultColormapOfScreen (widget_screen (parent)));

#ifdef HAVE_GTK
  gtk_window_set_title (GTK_WINDOW (demo_dialog), progclass);
  gtk_signal_connect (GTK_OBJECT (demo_dialog), "delete_event",
                      GTK_SIGNAL_FUNC (destroy), NULL);
  gtk_signal_connect (GTK_OBJECT (demo_dialog), "destroy",
                      GTK_SIGNAL_FUNC (destroy), NULL);
#endif /* HAVE_GTK */

  format_into_label (label1, short_version);
  add_button_callback (next,    next_cb,        (POINTER) p);
  add_button_callback (prev,    prev_cb,        (POINTER) p);
  add_button_callback (done,    quit_cb,        (POINTER) p);
  if (restart)
    add_button_callback(restart,restart_cb,     (POINTER) p);
  add_button_callback (edit,    preferences_cb, (POINTER) pair);

#ifdef HAVE_MOTIF
  XtAddCallback (demo_list, XmNbrowseSelectionCallback,
		 select_cb, (POINTER) p);
  XtAddCallback (demo_list, XmNdefaultActionCallback,
		 select_cb, (POINTER) p);
  XtAddCallback (text_line, XmNactivateCallback, text_cb, (POINTER) p);

  if (hacks)
    for (; *hacks; hacks++)
      {
	XmString xmstr = XmStringCreate (*hacks, XmSTRING_DEFAULT_CHARSET);
	XmListAddItem (demo_list, xmstr, 0);
	XmStringFree (xmstr);
      }

#elif defined(HAVE_ATHENA)

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

#elif defined(HAVE_GTK)
  {
    GtkList *list = GTK_LIST(GTK_BIN(GTK_BIN(demo_list)->child)->child);
    char **s;
    for (s = hacks; *s; s++)
      {
        GtkWidget *line = gtk_list_item_new_with_label (*s);
        gtk_container_add (GTK_CONTAINER (list), line);
        gtk_signal_connect (GTK_OBJECT (line), "button_press_event",
                            GTK_SIGNAL_FUNC (select_cb),
                            (POINTER) line);
        GTK_WIDGET (GTK_BIN(line)->child)->style =
          gtk_style_copy (GTK_WIDGET (text_line)->style);
        gtk_widget_show (line);
      }
    gtk_signal_connect (GTK_OBJECT (text_line), "activate",
                        GTK_SIGNAL_FUNC (text_cb),
                        (POINTER) p);
    gtk_signal_connect (GTK_OBJECT (text_activate), "clicked",
                        GTK_SIGNAL_FUNC (select_button_cb),
                        (POINTER) p);
  }
#endif /* HAVE_GTK */

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
   when not fully spelled out.  client_data is a Time* where the value goes.
 */
static void
prefs_sec_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  hack_time_text (widget_display (button), get_text_string (button),
                  (Time *) client_data, True);
}


/* Callback for text fields that hold a time that default to minutes,
   when not fully spelled out.  client_data is an Time* where the value goes.
 */
static void
prefs_min_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  hack_time_text (widget_display (button), get_text_string (button),
                  (Time *) client_data, False);
}


/* Callback for text fields that hold an integer value.
   client_data is an int* where the value goes.
 */
static void
prefs_int_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
  char *line = get_text_string (button);
  int *store = (int *) client_data;
  unsigned int value;
  char c;
  if (! *line)
    ;
  else if (sscanf (line, "%u%c", &value, &c) != 1)
#ifdef HAVE_GTK
    gdk_beep();
#else  /* !HAVE_GTK */
    XBell (XtDisplay (button), 0);
#endif /* !HAVE_GTK */
  else
    *store = value;
}


/* Callback for toggle buttons.  client_data is a Bool* where the value goes.
 */
static void
prefs_bool_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER call_data)
{
  Bool *store = (Bool *) client_data;
#ifdef HAVE_MOTIF
  *store = ((XmToggleButtonCallbackStruct *) call_data)->set;
#elif defined(HAVE_ATHENA)
  Boolean state = FALSE;
  XtVaGetValues (button, XtNstate, &state, 0);
  *store = state;
#elif defined(HAVE_GTK)
  *store = GTK_TOGGLE_BUTTON (button)->active;
#endif /* HAVE_GTK */
}


/* Callback for the Cancel button on the Preferences dialog.
 */
static void
prefs_cancel_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER ignored)
{
#ifdef HAVE_GTK
  gdk_window_hide (GTK_WIDGET (preferences_dialog)->window);
  gtk_widget_show (demo_dialog);
  gdk_window_show (GTK_WIDGET (demo_dialog)->window);
  gdk_window_raise (GTK_WIDGET (demo_dialog)->window);
#else  /* !HAVE_GTK */
  XtDestroyWidget (preferences_dialog);
  preferences_dialog = 0;
  XMapRaised (XtDisplay (demo_dialog), XtWindow (demo_dialog));
#endif /* !HAVE_GTK */
}


/* Callback for the OK button on the Preferences dialog.
 */
static void
prefs_ok_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER call_data)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;

  prefs_cancel_cb CB_ARGS(button, client_data, call_data);

#ifdef HAVE_ATHENA
  /* Athena doesn't let us put callbacks on these widgets, so run
     all the callbacks by hand when OK is pressed. */
  prefs_min_cb (timeout_text,        (POINTER) &p2->timeout,        0);
  prefs_min_cb (cycle_text,          (POINTER) &p2->cycle,          0);
  prefs_sec_cb (fade_text,           (POINTER) &p2->fade_seconds,   0);
  prefs_int_cb (fade_ticks_text,     (POINTER) &p2->fade_ticks,     0);
  prefs_min_cb (lock_timeout_text,   (POINTER) &p2->lock_timeout,   0);
  prefs_sec_cb (passwd_timeout_text, (POINTER) &p2->passwd_timeout, 0);
#elif defined(HAVE_GTK)
  /* Do it again anyway for GTK. */
  prefs_min_cb ((POINTER) &p2->timeout,        timeout_text);
  prefs_min_cb ((POINTER) &p2->cycle,          cycle_text);
  prefs_sec_cb ((POINTER) &p2->fade_seconds,   fade_text);
  prefs_int_cb ((POINTER) &p2->fade_ticks,     fade_ticks_text);
  prefs_min_cb ((POINTER) &p2->lock_timeout,   lock_timeout_text);
  prefs_sec_cb ((POINTER) &p2->passwd_timeout, passwd_timeout_text);
#endif /* HAVE_GTK */

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


#ifdef HAVE_GTK
static void
close_prefs_cb CB_ARGS(WIDGET button, POINTER client_data, POINTER call_data)
{
  prefs_cancel_cb CB_ARGS(button, client_data, call_data);
}
#endif /* HAVE_GTK */


static void
make_preferences_dialog (prefs_pair *pair, Widget parent)
{
  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;

  Screen *screen = widget_screen (parent);
  Display *dpy = widget_display (parent);

  *p2 = *p;	/* copy all slots of p into p2. */

  create_preferences_dialog (parent,
                             DefaultVisualOfScreen (screen),
                             DefaultColormapOfScreen (screen));

#ifdef HAVE_GTK
  gtk_window_set_title (GTK_WINDOW (preferences_dialog), progclass);
  gtk_signal_connect (GTK_OBJECT (preferences_dialog), "delete_event",
                      GTK_SIGNAL_FUNC (close_prefs_cb), NULL);
  gtk_signal_connect (GTK_OBJECT (preferences_dialog), "destroy",
                      GTK_SIGNAL_FUNC (close_prefs_cb), NULL);
#endif /* HAVE_GTK */

  add_button_callback (prefs_done,   prefs_ok_cb,     (POINTER) pair);
  add_button_callback (prefs_cancel, prefs_cancel_cb, 0);

#define CB(widget,type,slot) \
	add_text_callback ((widget), (type), (POINTER) (slot))
#define CBT(widget,type,slot) \
	add_toggle_callback ((widget), (type), (POINTER) (slot))

#ifndef HAVE_ATHENA
  /* When using Athena widgets, we can't set callbacks for these,
     so in that case, we run them by hand when "OK" is pressed. */
  CB (timeout_text,		prefs_min_cb,  &p2->timeout);
  CB (cycle_text,		prefs_min_cb,  &p2->cycle);
  CB (fade_text,		prefs_sec_cb,  &p2->fade_seconds);
  CB (fade_ticks_text,		prefs_int_cb,  &p2->fade_ticks);
  CB (lock_timeout_text,	prefs_min_cb,  &p2->lock_timeout);
  CB (passwd_timeout_text,	prefs_sec_cb,  &p2->passwd_timeout);

#endif /* !HAVE_ATHENA */

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
warning_dialog_dismiss_cb CB_ARGS(WIDGET button, POINTER client_data,
                                  POINTER ignored)
{
  WIDGET shell = (WIDGET) client_data;
#ifdef HAVE_GTK
  gdk_window_hide (GTK_WIDGET (shell)->window);
#else  /* !HAVE_GTK */
  XtDestroyWidget (shell);
#endif /* !HAVE_GTK */
}


static void
warning_dialog (WIDGET parent, const char *message)
{
  char *msg = strdup (message);
  char *head;

  WIDGET dialog = 0;
  WIDGET label = 0;
  WIDGET ok = 0;
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

#elif defined(HAVE_ATHENA)

  Widget form;
  dialog = XtVaCreatePopupShell("warning_dialog", transientShellWidgetClass,
				parent, 0);
  form = XtVaCreateManagedWidget("warning_form", formWidgetClass, dialog, 0);

#elif defined(HAVE_GTK)
  dialog = gtk_dialog_new ();
#endif /* HAVE_GTK */

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
#elif defined(HAVE_ATHENA)
      
      label = XtVaCreateManagedWidget (name, labelWidgetClass,
				       form,
				       XtNleft, XtChainLeft,
				       XtNright, XtChainRight,
				       XtNlabel, head,
				       (label ? XtNfromVert : XtNtop),
				       (label ? label : XtChainTop),
				       0);

#elif defined(HAVE_GTK)
      {
        char buf[255];
        label = gtk_label_new (head);
        sprintf (buf, "warning_dialog.%s.font", name);
        GTK_WIDGET (label)->style = gtk_style_copy (GTK_WIDGET (label)->style);
        GTK_WIDGET (label)->style->font =
          gdk_font_load (get_string_resource (buf, "Dialog.Label.Font"));
        /* gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5); */
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            label, TRUE, TRUE, 0);
        gtk_widget_show (label);
      }
#endif /* HAVE_GTK */

      if (s)
	head = s+1;
      else
	head = 0;
    }

#ifdef HAVE_MOTIF

  XtManageChild (container);
  XtRealizeWidget (dialog);
  XtManageChild (dialog);

#elif defined(HAVE_ATHENA)

  ok = XtVaCreateManagedWidget ("ok", commandWidgetClass, form,
				XtNleft, XtChainLeft,
				XtNbottom, XtChainBottom,
				XtNfromVert, label,
				0);

  XtRealizeWidget (dialog);
  XtPopup (dialog, XtGrabNone);

#elif defined(HAVE_GTK)
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, TRUE, TRUE, 0);

  ok = gtk_button_new_with_label (
                          get_string_resource ("warning_dialog.ok.label",
                                               "warning_dialog.Button.Label"));
  gtk_box_pack_start (GTK_BOX (label), ok, TRUE, FALSE, 0);
  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_widget_show (ok);
  gtk_widget_show (label);
  gtk_widget_show (dialog);
/*  gtk_window_set_default (GTK_WINDOW (dialog), ok);*/

  gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
                                GTK_WIDGET (preferences_dialog
                                            ? preferences_dialog
                                            : demo_dialog)->window);

  gdk_window_show (GTK_WIDGET (dialog)->window);
  gdk_window_raise (GTK_WIDGET (dialog)->window);
#endif /* HAVE_GTK */

  add_button_callback (ok, warning_dialog_dismiss_cb, (POINTER) dialog);

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
the_network_is_not_the_computer (WIDGET parent)
{
  Display *dpy = widget_display (parent);
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


/* We use this error handler so that X errors are preceeded by the name
   of the program that generated them.
 */
static int
demo_ehandler (Display *dpy, XErrorEvent *error)
{
  fprintf (stderr, "\nX error in %s:\n", progname);
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    exit (-1);
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}


#ifdef HAVE_GTK

/* We use this error handler so that Gtk/Gdk errors are preceeded by the name
   of the program that generated them; and also that we can ignore one
   particular bogus error message that Gdk madly spews.
 */
static void
g_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
               const gchar *message, gpointer user_data)
{
  /* Ignore the message "Got event for unknown window: 0x...".
     Apparently some events are coming in for the xscreensaver window
     (presumably reply events related to the ClientMessage) and Gdk
     feels the need to complain about them.  So, just suppress any
     messages that look like that one.
   */
  if (strstr (message, "unknown window"))
    return;

  fprintf (stderr, "%s: %s-%s: %s%s", blurb(), log_domain,
           (log_level == G_LOG_LEVEL_ERROR    ? "error" :
            log_level == G_LOG_LEVEL_CRITICAL ? "critical" :
            log_level == G_LOG_LEVEL_WARNING  ? "warning" :
            log_level == G_LOG_LEVEL_MESSAGE  ? "message" :
            log_level == G_LOG_LEVEL_INFO     ? "info" :
            log_level == G_LOG_LEVEL_DEBUG    ? "debug" : "???"),
           message,
           ((!*message || message[strlen(message)-1] != '\n')
            ? "\n" : ""));
}
#endif /* HAVE_GTK */



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

  progname = real_progname;

#ifdef HAVE_GTK
  /* Register our error message logger for every ``log domain'' known.
     There's no way to do this globally, so I grepped the Gtk/Gdk sources
     for all of the domains that seem to be in use.
  */
  {
    const char * const domains[] = { "Gtk", "Gdk", "GLib", "GModule",
                                     "GThread", "Gnome", "GnomeUI", 0 };
    for (i = 0; domains[i]; i++)
      g_log_set_handler (domains[i], G_LOG_LEVEL_MASK, g_log_handler, 0);
  }

  /* This is gross, but Gtk understands --display and not -display... */
  for (i = 1; i < argc; i++)
    if (argv[i][0] && argv[i][1] && 
        !strncmp(argv[i], "-display", strlen(argv[i])))
      argv[i] = "--display";

  /* Let Gtk open the X connection, then initialize Xt to use that
     same connection.  Doctor Frankenstein would be proud. */   
  gtk_init (&argc, &argv);
#endif /* HAVE_GTK */


  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";
  progname = argv[0];


#ifdef HAVE_GTK
  /* If we're using Gtk, the X connection is already open.
     Now teach Xt about it.
   */
  XtToolkitInitialize ();
  app = XtCreateApplicationContext ();
  dpy = gdk_display;
  XtAppSetFallbackResources (app, defaults);
  XtDisplayInitialize (app, dpy, progname, progclass, 0, 0, &argc, argv);
  toplevel_shell = XtAppCreateShell (progname, progclass,
                                     applicationShellWidgetClass,
                                     dpy, 0, 0);

#else  /* !HAVE_GTK */
  /* No Gtk -- open the X connection here. */
  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, &argc, argv,
				    defaults, 0, 0);
#endif /* !HAVE_GTK */

  dpy = XtDisplay (toplevel_shell);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  XSetErrorHandler (demo_ehandler);

  /* Complain about unrecognized command-line arguments.
   */
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

  /* Load the init file, which may end up consulting the X resource database
     and the site-wide app-defaults file.  Note that at this point, it's
     important that `progname' be "xscreensaver", rather than whatever
     was in argv[0].
   */
  p->db = db;
  load_init_file (p);
  *p2 = *p;

  /* Now that Xt has been initialized, and the resources have been read,
     we can set our `progname' variable to something more in line with
     reality.
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
			  (POINTER) &count);
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

#ifdef HAVE_GTK

  /* Run the Gtk event loop, and not the Xt event loop.  This means that
     if there were Xt timers or fds registered, they would never get serviced,
     and if there were any Xt widgets, they would never have events delivered.
     Fortunately, we're using Gtk for all of the UI, and only initialized
     Xt so that we could process the command line and use the X resource
     manager.
   */
  gtk_main ();

#else  /* !HAVE_GTK */

  XtAppMainLoop (app);

#endif /* !HAVE_GTK */

  exit (0);
}
