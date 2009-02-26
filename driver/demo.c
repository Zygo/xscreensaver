/* demo.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-1998 Jamie Zawinski <jwz@netscape.com>
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

#include <X11/Intrinsic.h>

/* We don't actually use any widget internals, but these are included
   so that gdb will have debug info for the widgets... */
#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>

#ifdef HAVE_MOTIF
# include <Xm/Xm.h>
# include <Xm/Text.h>
# include <Xm/List.h>
# include <Xm/ToggleB.h>

#else  /* HAVE_ATHENA */
  /* Athena demo code contributed by Jon A. Christopher <jac8782@tamu.edu> */
  /* Copyright 1997, with the same permissions as above. */
# include <X11/StringDefs.h>
# include <X11/Shell.h>
# include <X11/Xaw/Form.h>
# include <X11/Xaw/Box.h>
# include <X11/Xaw/List.h>
# include <X11/Xaw/Command.h>
# include <X11/Xaw/Toggle.h>
# include <X11/Xaw/Viewport.h>
# include <X11/Xaw/Dialog.h>
# include <X11/Xaw/Scrollbar.h>
#endif /* HAVE_ATHENA */

#include "xscreensaver.h"
#include "resources.h"		/* for parse_time() */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void demo_mode_hack (saver_info *si, char *);
static void demo_mode_done (saver_info *si);

extern Widget demo_dialog;
extern Widget label1;
extern Widget text_line;
extern Widget demo_form;
extern Widget demo_list;
extern Widget next, prev, done, restart, edit;

extern Widget resources_dialog;
extern Widget resources_form;
extern Widget res_done, res_cancel;
extern Widget timeout_text, cycle_text, fade_text, ticks_text;
extern Widget lock_time_text, passwd_time_text;
extern Widget verbose_toggle, cmap_toggle, fade_toggle, unfade_toggle,
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
  XtVaGetValues (text_widget, XtNvalue, &string, 0);
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


void
format_into_label (Widget label, const char *arg)
{
  char *text = get_label_string (label);
  char *buf = (char *) malloc ((text ? strlen(text) : 100) + strlen(arg) + 10);

  if (!text || !strcmp (text, XtName (label)))
      strcpy (buf, "ERROR: RESOURCES ARE NOT INSTALLED CORRECTLY");
    else
      sprintf (buf, text, arg);

    set_label_string (label, buf);
    free (buf);
    XtFree (text);
}


void
steal_focus_and_colormap (Widget dialog)
{
  Display *dpy = XtDisplay (dialog);
  Window window = XtWindow (dialog);
  Colormap cmap = 0;
  XSetInputFocus (dpy, window, RevertToParent, CurrentTime);

  XtVaGetValues (dialog, XtNcolormap, &cmap, 0);
  if (cmap)
    XInstallColormap (dpy, cmap);
}

static void
raise_screenhack_dialog (void)
{
  XMapRaised (XtDisplay (demo_dialog), XtWindow (demo_dialog));
  if (resources_dialog)
    XMapRaised (XtDisplay (resources_dialog), XtWindow (resources_dialog));
  steal_focus_and_colormap (resources_dialog ? resources_dialog : demo_dialog);
}

static void
destroy_screenhack_dialogs (saver_info *si)
{
  saver_screen_info *ssi = si->default_screen;

  if (demo_dialog) XtDestroyWidget (demo_dialog);
  if (resources_dialog) XtDestroyWidget (resources_dialog);
  demo_dialog = resources_dialog = 0;

  if (ssi->demo_cmap &&
      ssi->demo_cmap != ssi->cmap &&
      ssi->demo_cmap != DefaultColormapOfScreen (ssi->screen))
    {
      XFreeColormap (si->dpy, ssi->demo_cmap);
      ssi->demo_cmap = 0;
    }

  /* Since we installed our colormap to display the dialogs properly, put
     the old one back, so that the screensaver_window is now displayed
     properly. */
  if (ssi->cmap)
    XInstallColormap (si->dpy, ssi->cmap);
}

#ifdef HAVE_MOTIF

static void
text_cb (Widget text_widget, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;
  char *line;
  line = get_text_string (text_widget);
  demo_mode_hack (si, line);
}

#endif /* HAVE_MOTIF */


static void
select_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;

#ifdef HAVE_ATHENA
  XawListReturnStruct *item = (XawListReturnStruct*)call_data;
  demo_mode_hack (si, item->string);
  if (item->list_index >= 0)
    si->default_screen->current_hack = item->list_index;

#else  /* HAVE_MOTIF */
  XmListCallbackStruct *lcb = (XmListCallbackStruct *) call_data;
  char *string = 0;
  if (lcb->item)
    XmStringGetLtoR (lcb->item, XmSTRING_DEFAULT_CHARSET, &string);
  set_text_string (text_line, (string ? string : ""));
  if (lcb->reason == XmCR_DEFAULT_ACTION && string)
    {
      demo_mode_hack (si, string);
      if (lcb->item_position > 0)
	si->default_screen->current_hack = lcb->item_position - 1;
    }
  if (string)
    XtFree (string);
#endif /* HAVE_MOTIF */
  steal_focus_and_colormap (demo_dialog);
}


#if 0  /* configure does this now */
#ifdef HAVE_ATHENA
# if !defined(_Viewport_h)
   /* The R4 Athena libs don't have this function.  I don't know the right
      way to tell, but I note that the R5 version of Viewport.h defines
      _XawViewport_h, while the R4 version defines _Viewport_h.  So we'll
     try and key off of that... */
#  define HAVE_XawViewportSetCoordinates
# endif
#endif /* HAVE_ATHENA */
#endif /* 0 */


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


static void
next_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;

#ifdef HAVE_ATHENA
  int cnt;
  XawListReturnStruct *current = XawListShowCurrent(demo_list);
  if (current->list_index == XAW_LIST_NONE)
    XawListHighlight(demo_list,1);
  else
    {
      XtVaGetValues(demo_list,
		    XtNnumberStrings, &cnt,
		    NULL);
      if (current->list_index + 1 < cnt)
	{
	  current->list_index++;
	  XawListHighlight(demo_list, current->list_index);
	}
    }

  ensure_selected_item_visible (demo_list);
  current = XawListShowCurrent(demo_list);
  demo_mode_hack (si, current->string);

#else  /* HAVE_MOTIF */

  int *pos_list;
  int pos_count;
  if (! XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    {
      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, 1, True);
    }
  else
    {
      int pos = pos_list[0] + 1;
      if (pos > si->prefs.screenhacks_count)
	pos = 1;
      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos, True);
    }
  XtFree ((char *) pos_list);
  ensure_selected_item_visible (demo_list);
  demo_mode_hack (si, get_text_string (text_line));

#endif /* HAVE_MOTIF */
}


static void
prev_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;

#ifdef HAVE_ATHENA
  XawListReturnStruct *current=XawListShowCurrent(demo_list);
  if (current->list_index == XAW_LIST_NONE)
    XawListHighlight(demo_list,1);
  else
    {
      if (current->list_index>=1)
	{
	  current->list_index--;
	  XawListHighlight(demo_list, current->list_index);
	}
    }

  ensure_selected_item_visible (demo_list);
  current = XawListShowCurrent(demo_list);
  demo_mode_hack (si, current->string);

#else  /* HAVE_MOTIF */

  int *pos_list;
  int pos_count;
  if (! XmListGetSelectedPos (demo_list, &pos_list, &pos_count))
    {
      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, 0, True);
    }
  else
    {
      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, pos_list [0] - 1, True);
      XtFree ((char *) pos_list);
    }
  ensure_selected_item_visible (demo_list);
  demo_mode_hack (si, get_text_string (text_line));

#endif /* HAVE_MOTIF */
}


static void pop_resources_dialog (saver_info *si);
static void make_resources_dialog (saver_info *si, Widget parent);

static void
edit_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;
  saver_screen_info *ssi = si->default_screen;
  Widget parent = ssi->toplevel_shell;
  if (! resources_dialog)
    make_resources_dialog (si, parent);
  pop_resources_dialog (si);
}

static void
done_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;
  demo_mode_done (si);
}


static void
restart_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;
  demo_mode_restart_process (si);
}


void
pop_up_dialog_box (Widget dialog, Widget form, int where)
{
  /* I'm sure this is the wrong way to pop up a dialog box, but I can't
     figure out how else to do it.

     It's important that the screensaver dialogs not get decorated or
     otherwise reparented by the window manager, because they need to be
     children of the *real* root window, not the WM's virtual root, in
     order for us to guarentee that they are visible above the screensaver
     window itself.
   */
  Arg av [100];
  int ac = 0;
  Dimension sw, sh, x, y, w, h;

#ifdef HAVE_ATHENA
  XtRealizeWidget (dialog);
#else  /* HAVE_MOTIF */
  /* Motif likes us to realize the *child* of the shell... */
  XtRealizeWidget (form);
#endif /* HAVE_MOTIF */

  sw = WidthOfScreen (XtScreen (dialog));
  sh = HeightOfScreen (XtScreen (dialog));
  ac = 0;
  XtSetArg (av [ac], XtNwidth, &w); ac++;
  XtSetArg (av [ac], XtNheight, &h); ac++;
  XtGetValues (form, av, ac);

  /* for debugging -- don't ask */
  if (where >= 69)
    {
      where -= 69;
      sw = (sw * 7) / 12;
    }

  switch (where)
    {
    case 0:	/* center it in the top-right quadrant */
      x = (sw/2 + w) / 2 + (sw/2) - w;
      y = (sh/2 + h) / 2 - h;
      break;
    case 1:	/* center it in the bottom-right quadrant */
      x = (sw/2 + w) / 2 + (sw/2) - w;
      y = (sh/2 + h) / 2 + (sh/2) - h;
      break;
    case 2:	/* center it on the screen */
      x = (sw + w) / 2 - w;
      y = (sh + h) / 2 - h;
      break;
    default:
      abort ();
    }
  if (x + w > sw) x = sw - w;
  if (y + h > sh) y = sh - h;
  ac = 0;
  XtSetArg (av [ac], XtNx, x); ac++;
  XtSetArg (av [ac], XtNy, y); ac++;
  XtSetArg (av [ac], XtNoverrideRedirect, True); ac++;

#ifdef HAVE_MOTIF
  XtSetArg (av [ac], XmNdefaultPosition, False); ac++;
#endif /* HAVE_MOTIF */

  XtSetValues (dialog, av, ac);
  XtSetValues (form, av, ac);

#ifdef HAVE_ATHENA
  XtPopup (dialog, XtGrabNone);
#else  /* HAVE_MOTIF */
  XtManageChild (form);
#endif /* HAVE_MOTIF */

  steal_focus_and_colormap (dialog);
}


static void
make_screenhack_dialog (saver_info *si)
{
  saver_screen_info *ssi = si->default_screen;
  Widget parent = ssi->toplevel_shell;
  char **hacks = si->prefs.screenhacks;

  if (ssi->demo_cmap &&
      ssi->demo_cmap != ssi->cmap &&
      ssi->demo_cmap != DefaultColormapOfScreen (ssi->screen))
    {
      XFreeColormap (si->dpy, ssi->demo_cmap);
      ssi->demo_cmap = 0;
    }

  if (ssi->default_visual == DefaultVisualOfScreen (ssi->screen))
    ssi->demo_cmap = DefaultColormapOfScreen (ssi->screen);
  else
    ssi->demo_cmap = XCreateColormap (si->dpy,
				      RootWindowOfScreen (ssi->screen),
				      ssi->default_visual, AllocNone);

  create_demo_dialog (parent, ssi->default_visual, ssi->demo_cmap);
  format_into_label (label1, si->version);

  add_button_callback (next,    next_cb,    (XtPointer) si);
  add_button_callback (prev,    prev_cb,    (XtPointer) si);
  add_button_callback (done,    done_cb,    (XtPointer) si);
  add_button_callback (restart, restart_cb, (XtPointer) si);
  add_button_callback (edit,    edit_cb,    (XtPointer) si);

#ifdef HAVE_MOTIF
  XtAddCallback (demo_list, XmNbrowseSelectionCallback,
		 select_cb, (XtPointer) si);
  XtAddCallback (demo_list, XmNdefaultActionCallback,
		 select_cb, (XtPointer) si);
  XtAddCallback (text_line, XmNactivateCallback, text_cb, (XtPointer) si);

  for (; *hacks; hacks++)
    {
      XmString xmstr = XmStringCreate (*hacks, XmSTRING_DEFAULT_CHARSET);
      XmListAddItem (demo_list, xmstr, 0);
      XmStringFree (xmstr);
    }

  /* Cause the most-recently-run hack to be selected in the list.
     Do some voodoo to make it be roughly centered in the list (really,
     just make it not be within +/- 5 of the top/bottom if possible.)
   */
  if (ssi->current_hack > 0)
    {
      int i = ssi->current_hack+1;
      int top = i + 5;
      int bot = i - 5;
      if (bot < 1) bot = 1;
      if (top > si->prefs.screenhacks_count)
	top = si->prefs.screenhacks_count;

      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, bot, False);
      ensure_selected_item_visible (demo_list);

      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, top, False);
      ensure_selected_item_visible (demo_list);

      XmListDeselectAllItems(demo_list);	/* LessTif lossage */
      XmListSelectPos (demo_list, i, False);
      ensure_selected_item_visible (demo_list);
    }

#else  /* HAVE_ATHENA */

  XtVaSetValues (demo_list,
		 XtNlist, hacks,
		 XtNnumberStrings, si->prefs.screenhacks_count,
		 0);
  XtAddCallback (demo_list, XtNcallback, select_cb, si);
  if (ssi->current_hack > 0)
  XawListHighlight(demo_list, ssi->current_hack);

#endif /* HAVE_ATHENA */

  monitor_power_on (si);
  pop_up_dialog_box(demo_dialog, demo_form,
		    /* for debugging -- don't ask */
		    (si->prefs.debug_p ? 69 : 0) +
		    0);

#ifdef HAVE_ATHENA
  /* For Athena, have to do this after the dialog is managed. */
  ensure_selected_item_visible (demo_list);
#endif /* HAVE_ATHENA */
}


/* the Screensaver Parameters dialog */

static struct resources {
  int timeout, cycle, secs, ticks, lock_time, passwd_time;
  int verb, cmap, fade, unfade, lock_p;
} res;


static void 
hack_time_cb (Display *dpy, char *line, int *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      value = parse_time (line, sec_p, True);
      if (value < 0)
	/*XBell (dpy, 0)*/;
      else
	*store = value;
    }
}

static void
res_sec_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  hack_time_cb (XtDisplay (button), get_text_string (button),
		(int *) client_data, True);
}

static void
res_min_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  hack_time_cb (XtDisplay (button), get_text_string (button),
		(int *) client_data, False);
}

static void
res_int_cb (Widget button, XtPointer client_data, XtPointer call_data)
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

static void
res_bool_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  int *store = (int *) client_data;
#ifdef HAVE_MOTIF
  *store = ((XmToggleButtonCallbackStruct *) call_data)->set;
#else /* HAVE_ATHENA */
  Boolean state = FALSE;
  XtVaGetValues (button, XtNstate, &state, NULL);
  *store = state;
#endif /* HAVE_ATHENA */
}

static void
res_cancel_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  XtDestroyWidget (resources_dialog);
  resources_dialog = 0;
  raise_screenhack_dialog ();
}


static void
res_done_cb (Widget button, XtPointer client_data, XtPointer call_data)
{
  saver_info *si = (saver_info *) client_data;
  saver_preferences *p = &si->prefs;

  res_cancel_cb (button, client_data, call_data);

#ifdef HAVE_ATHENA
  /* Check all text widgets, since we don't have callbacks for these. */
  res_min_cb (timeout_text,     (XtPointer) &res.timeout,     NULL);
  res_min_cb (cycle_text,       (XtPointer) &res.cycle,       NULL);
  res_sec_cb (fade_text,        (XtPointer) &res.secs,        NULL);
  res_int_cb (ticks_text,       (XtPointer) &res.ticks,       NULL);
  res_min_cb (lock_time_text,   (XtPointer) &res.lock_time,   NULL);
  res_sec_cb (passwd_time_text, (XtPointer) &res.passwd_time, NULL);
#endif /* HAVE_ATHENA */

  /* Throttle the timeouts to minimum sane values. */
  if (res.timeout < 5) res.timeout = 5;
  if (res.cycle < 2) res.cycle = 2;
  if (res.passwd_time < 10) res.passwd_time = 10;

  p->timeout = res.timeout * 1000;
  p->cycle = res.cycle * 1000;
  p->lock_timeout = res.lock_time * 1000;
#ifndef NO_LOCKING
  p->passwd_timeout = res.passwd_time * 1000;
#endif
  p->fade_seconds = res.secs;
  p->fade_ticks = res.ticks;
  p->verbose_p = res.verb;
  p->install_cmap_p = res.cmap;
  p->fade_p = res.fade;
  p->unfade_p = res.unfade;
  p->lock_p = res.lock_p;

  if (p->debug_p && p->verbose_p)
    fprintf (stderr, "%s: parameters changed:\n\
	timeout: %d\n\tcycle:   %d\n\tlock:    %d\n\tpasswd:  %d\n\
	fade:    %d\n\tfade:    %d\n\tverbose: %d\n\tinstall: %d\n\
	fade:    %d\n\tunfade:  %d\n\tlock:    %d\n",
	     progname, p->timeout, p->cycle, p->lock_timeout,
#ifdef NO_LOCKING
	     0,
#else
	     p->passwd_timeout,
#endif
	     p->fade_seconds, p->fade_ticks, p->verbose_p, p->install_cmap_p,
	     p->fade_p, p->unfade_p, p->lock_p);


#if defined(HAVE_MIT_SAVER_EXTENSION) || defined(HAVE_SGI_SAVER_EXTENSION)
  if (p->use_mit_saver_extension || p->use_sgi_saver_extension)
    {
      /* Need to set the server timeout to the new one the user has picked.
       */
      int server_timeout, server_interval, prefer_blank, allow_exp;
      XGetScreenSaver (si->dpy, &server_timeout, &server_interval,
		       &prefer_blank, &allow_exp);
      if (server_timeout != (p->timeout / 1000))
	{
	  server_timeout = (p->timeout / 1000);
	  if (p->verbose_p)
	    fprintf (stderr,
		   "%s: configuring server for saver timeout of %d seconds.\n",
		     progname, server_timeout);
	  /* Leave all other parameters the same. */
	  XSetScreenSaver (si->dpy, server_timeout, server_interval,
			   prefer_blank, allow_exp);
	}
    }
#endif /* HAVE_MIT_SAVER_EXTENSION || HAVE_SGI_SAVER_EXTENSION */
}


static void
make_resources_dialog (saver_info *si, Widget parent)
{
  saver_screen_info *ssi = si->default_screen;

  if (ssi->demo_cmap &&
      ssi->demo_cmap != ssi->cmap &&
      ssi->demo_cmap != DefaultColormapOfScreen (ssi->screen))
    {
      XFreeColormap (si->dpy, ssi->demo_cmap);
      ssi->demo_cmap = 0;
    }

  if (ssi->default_visual == DefaultVisualOfScreen (ssi->screen))
    ssi->demo_cmap = DefaultColormapOfScreen (ssi->screen);
  else
    ssi->demo_cmap = XCreateColormap (si->dpy,
				     RootWindowOfScreen (ssi->screen),
				     ssi->default_visual, AllocNone);

  create_resources_dialog (parent, ssi->default_visual, ssi->demo_cmap);

  add_button_callback (res_done,   res_done_cb,   (XtPointer) si);
  add_button_callback (res_cancel, res_cancel_cb, (XtPointer) si);

#define CB(widget,type,slot) \
	add_text_callback ((widget), (type), (XtPointer) (slot))
#define CBT(widget,type,slot) \
	add_toggle_callback ((widget), (type), (XtPointer) (slot))

#ifdef HAVE_MOTIF
  /* When using Athena widgets, we can't set callbacks for these,
     so we'll check them all if "done" gets pressed.
   */
  CB (timeout_text,	res_min_cb,  &res.timeout);
  CB (cycle_text,	res_min_cb,  &res.cycle);
  CB (fade_text,	res_sec_cb,  &res.secs);
  CB (ticks_text,	res_int_cb,  &res.ticks);
  CB (lock_time_text,	res_min_cb,  &res.lock_time);
  CB (passwd_time_text,	res_sec_cb,  &res.passwd_time);
#endif /* HAVE_MOTIF */

  CBT (verbose_toggle,	res_bool_cb, &res.verb);
  CBT (cmap_toggle,	res_bool_cb, &res.cmap);
  CBT (fade_toggle,	res_bool_cb, &res.fade);
  CBT (unfade_toggle,	res_bool_cb, &res.unfade);
  CBT (lock_toggle,	res_bool_cb, &res.lock_p);
#undef CB
#undef CBT

  if (si->locking_disabled_p)
    {
      disable_widget (passwd_time_text);
      disable_widget (lock_time_text);
      disable_widget (lock_toggle);
    }
  if (CellsOfScreen (XtScreen (parent)) <= 2)
    {
      disable_widget (fade_text);
      disable_widget (ticks_text);
      disable_widget (cmap_toggle);
      disable_widget (fade_toggle);
      disable_widget (unfade_toggle);
    }
}


static void
fmt_time (char *buf, unsigned int s, int min_p)
{
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
/*
  if (min_p && h == 0 && s == 0)
    sprintf (buf, "%u", m);
  else if (!min_p && h == 0 && m == 0)
    sprintf (buf, "%u", s);
  else
  if (h == 0)
    sprintf (buf, "%u:%02u", m, s);
  else
*/
    sprintf (buf, "%u:%02u:%02u", h, m, s);
}

static void
pop_resources_dialog (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  char buf [100];

  res.timeout = p->timeout / 1000;
  res.cycle = p->cycle / 1000;
  res.lock_time = p->lock_timeout / 1000;
#ifndef NO_LOCKING
  res.passwd_time = p->passwd_timeout / 1000;
#endif
  res.secs = p->fade_seconds;
  res.ticks = p->fade_ticks;
  res.verb = p->verbose_p;
  res.cmap = p->install_cmap_p;
  res.fade = p->fade_p;
  res.unfade = p->unfade_p;
  res.lock_p = (p->lock_p && !si->locking_disabled_p);

  fmt_time (buf, res.timeout, 1);     set_text_string (timeout_text, buf);
  fmt_time (buf, res.cycle, 1);       set_text_string (cycle_text, buf);
  fmt_time (buf, res.lock_time, 1);   set_text_string (lock_time_text, buf);
  fmt_time (buf, res.passwd_time, 0); set_text_string (passwd_time_text, buf);
  fmt_time (buf, res.secs, 0);        set_text_string (fade_text, buf);
  sprintf (buf, "%u", res.ticks);     set_text_string (ticks_text, buf);

  set_toggle_button_state (verbose_toggle, res.verb);
  set_toggle_button_state (cmap_toggle, res.cmap);
  set_toggle_button_state (fade_toggle, res.fade);
  set_toggle_button_state (unfade_toggle, res.unfade);
  set_toggle_button_state (lock_toggle, res.lock_p);

  monitor_power_on (si);
  pop_up_dialog_box (resources_dialog, resources_form,
		     /* for debugging -- don't ask */
		     (si->prefs.debug_p ? 69 : 0) +
		     1);
}


/* The main demo-mode command loop.
 */

void
demo_mode (saver_info *si)
{
  saver_preferences *p = &si->prefs;
  si->dbox_up_p = True;
  monitor_power_on (si);
  raise_window (si, True, False, False);
  make_screenhack_dialog (si);
  while (si->demo_mode_p)
    {
      XEvent event;
      XtAppNextEvent (si->app, &event);
      switch (event.xany.type)
	{
	case 0:		/* synthetic "timeout" event */
	  break;

	case ClientMessage:
	  handle_clientmessage (si, &event, False);
	  break;

	case CreateNotify:
	  if (!p->use_xidle_extension &&
	      !p->use_mit_saver_extension &&
	      !p->use_sgi_saver_extension)
	    {
	      start_notice_events_timer (si, event.xcreatewindow.window);
#ifdef DEBUG_TIMERS
	      if (p->verbose_p)
		printf ("%s: starting notice_events_timer for 0x%X (%lu)\n",
			progname,
			(unsigned int) event.xcreatewindow.window,
			p->notice_events_timeout);
#endif /* DEBUG_TIMERS */
	    }
	  break;

	case ButtonPress:
	case ButtonRelease:
	  if (!XtWindowToWidget (si->dpy, event.xbutton.window))
	    raise_screenhack_dialog ();
	  /* fall through */

	default:
#ifdef HAVE_MIT_SAVER_EXTENSION
	  if (event.type == si->mit_saver_ext_event_number)
	    {
	      /* Get the "real" server window(s) out of the way as soon
		 as possible. */
	      int i = 0;
	      for (i = 0; i < si->nscreens; i++)
		{
		  saver_screen_info *ssi = &si->screens[i];
		  if (ssi->server_mit_saver_window &&
		      window_exists_p (si->dpy, ssi->server_mit_saver_window))
		    XUnmapWindow (si->dpy, ssi->server_mit_saver_window);
		}
	    }
	  else
#endif /* HAVE_MIT_SAVER_EXTENSION */

	  XtDispatchEvent (&event);
	  break;
	}
    }
  destroy_screenhack_dialogs (si);
  initialize_screensaver_window (si);

  si->demo_mode_p = True;  /* kludge to inhibit unfade... */
  unblank_screen (si);
  si->demo_mode_p = False;
}

static void
demo_mode_hack (saver_info *si, char *hack)
{
  if (! si->demo_mode_p) abort ();
  kill_screenhack (si);
  if (! si->demo_hack)
    blank_screen (si);
  si->demo_hack = hack;
  spawn_screenhack (si, False);
  /* raise_screenhack_dialog(); */
}

static void
demo_mode_done (saver_info *si)
{
  kill_screenhack (si);
  if (si->demo_hack)
    unblank_screen (si);
  si->demo_mode_p = False;
  si->dbox_up_p = False;
  si->demo_hack = 0;
}
