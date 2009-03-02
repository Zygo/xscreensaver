/* demo-Xm.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-2001 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_MOTIF /* whole file */

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

#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xatom.h>		/* for XA_INTEGER */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* We don't actually use any widget internals, but these are included
   so that gdb will have debug info for the widgets... */
#include <X11/IntrinsicP.h>
#include <X11/ShellP.h>

#ifdef HAVE_XPM
# include <X11/xpm.h>
#endif /* HAVE_XPM */

#ifdef HAVE_XMU
# ifndef VMS
#  include <X11/Xmu/Error.h>
# else /* VMS */
#  include <Xmu/Error.h>
# endif
#else
# include "xmu.h"
#endif



#include <Xm/Xm.h>
#include <Xm/List.h>
#include <Xm/PushB.h>
#include <Xm/LabelG.h>
#include <Xm/RowColumn.h>
#include <Xm/MessageB.h>

#ifdef HAVE_XMCOMBOBOX		/* a Motif 2.0 widget */
# include <Xm/ComboBox.h>
# ifndef XmNtextField		/* Lesstif 0.89.4 bug */
#  undef HAVE_XMCOMBOBOX
# endif
# if (XmVersion < 2001)		/* Lesstif has two personalities these days */
#  undef HAVE_XMCOMBOBOX
# endif
#endif /* HAVE_XMCOMBOBOX */

#include "version.h"
#include "prefs.h"
#include "resources.h"		/* for parse_time() */
#include "visual.h"		/* for has_writable_cells() */
#include "remote.h"		/* for xscreensaver_command() */
#include "usleep.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;

typedef struct {
  saver_preferences *a, *b;
} prefs_pair;

static void *global_prefs_pair;  /* I hate C so much... */

char *blurb (void) { return progname; }

extern Widget create_xscreensaver_demo (Widget parent);
extern const char *visual_menu[];


static char *short_version = 0;

Atom XA_VROOT;
Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO;
Atom XA_ACTIVATE, XA_BLANK, XA_LOCK, XA_RESTART, XA_EXIT;


static void populate_demo_window (Widget toplevel,
                                  int which, prefs_pair *pair);
static void populate_prefs_page (Widget top, prefs_pair *pair);
static int apply_changes_and_save (Widget widget);
static int maybe_reload_init_file (Widget widget, prefs_pair *pair);
static void await_xscreensaver (Widget widget);


/* Some random utility functions
 */

static Widget 
name_to_widget (Widget widget, const char *name)
{
  Widget parent;
  char name2[255];
  name2[0] = '*';
  strcpy (name2+1, name);
  
  while ((parent = XtParent (widget)))
    widget = parent;
  return XtNameToWidget (widget, name2);
}



/* Why this behavior isn't automatic in *either* toolkit, I'll never know.
   Takes a scroller, viewport, or list as an argument.
 */
static void
ensure_selected_item_visible (Widget list)
{
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
}


static void
warning_dialog_dismiss_cb  (Widget button, XtPointer client_data,
                            XtPointer user_data)
{
  Widget shell = (Widget) client_data;
  XtDestroyWidget (shell);
}


static void
warning_dialog (Widget parent, const char *message, int center)
{
  char *msg = strdup (message);
  char *head;

  Widget dialog = 0;
  Widget label = 0;
  Widget ok = 0;
  int i = 0;

  Widget w;
  Widget container;
  XmString xmstr;
  Arg av[10];
  int ac = 0;

  ac = 0;
  dialog = XmCreateWarningDialog (parent, "warning", av, ac);

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

  head = msg;
  while (head)
    {
      char name[20];
      char *s = strchr (head, '\n');
      if (s) *s = 0;

      sprintf (name, "label%d", i++);

      xmstr = XmStringCreate (head, XmSTRING_DEFAULT_CHARSET);
      ac = 0;
      XtSetArg (av[ac], XmNlabelString, xmstr); ac++;
      XtSetArg (av[ac], XmNmarginHeight, 0); ac++;
      label = XmCreateLabelGadget (container, name, av, ac);
      XtManageChild (label);
      XmStringFree (xmstr);

      if (s)
	head = s+1;
      else
	head = 0;

      center--;
    }

  XtManageChild (container);
  XtRealizeWidget (dialog);
  XtManageChild (dialog);

  XtAddCallback (ok, XmNactivateCallback, warning_dialog_dismiss_cb, dialog);

  free (msg);
}


static void
run_cmd (Widget widget, Atom command, int arg)
{
  char *err = 0;
  int status;

  apply_changes_and_save (widget);
  status = xscreensaver_command (XtDisplay (widget),
                                 command, arg, False, &err);
  if (status < 0)
    {
      char buf [255];
      if (err)
        sprintf (buf, "Error:\n\n%s", err);
      else
        strcpy (buf, "Unknown error!");
      warning_dialog (widget, buf, 100);
    }
  if (err) free (err);
}


static void
run_hack (Widget widget, int which, Bool report_errors_p)
{
  if (which < 0) return;
  apply_changes_and_save (widget);
  if (report_errors_p)
    run_cmd (widget, XA_DEMO, which + 1);
  else
    {
      char *s = 0;
      xscreensaver_command (XtDisplay (widget), XA_DEMO, which + 1, False, &s);
      if (s) free (s);
    }
}



/* Button callbacks
 */

void
exit_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  apply_changes_and_save (XtParent (button));
  exit (0);
}

#if 0
static void
wm_close_cb (Widget widget, GdkEvent *event, XtPointer data)
{
  apply_changes_and_save (XtParent (button));
  exit (0);
}
#endif

void
cut_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  /* #### */
  warning_dialog (XtParent (button),
                  "Error:\n\n"
                  "cut unimplemented\n", 1);
}


void
copy_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  /* #### */
  warning_dialog (XtParent (button),
                  "Error:\n\n"
                  "copy unimplemented\n", 1);
}


void
paste_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  /* #### */
  warning_dialog (XtParent (button),
                  "Error:\n\n"
                  "paste unimplemented\n", 1);
}


void
about_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  char buf [2048];
  char *s = strdup (screensaver_id + 4);
  char *s2;

  s2 = strchr (s, ',');
  *s2 = 0;
  s2 += 2;

  sprintf (buf, "%s\n%s\n\n"
           "For updates, check http://www.jwz.org/xscreensaver/",
           s, s2);
  free (s);

  warning_dialog (XtParent (button), buf, 100);
}


void
doc_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;

  saver_preferences *p =  pair->a;
  char *help_command;

  if (!p->help_url || !*p->help_url)
    {
      warning_dialog (XtParent (button),
                      "Error:\n\n"
                      "No Help URL has been specified.\n", 100);
      return;
    }

  help_command = (char *) malloc (strlen (p->load_url_command) +
				  (strlen (p->help_url) * 2) + 20);
  strcpy (help_command, "( ");
  sprintf (help_command + strlen(help_command),
           p->load_url_command, p->help_url, p->help_url);
  strcat (help_command, " ) &");
  system (help_command);
  free (help_command);
}


void
activate_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  run_cmd (XtParent (button), XA_ACTIVATE, 0);
}


void
lock_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  run_cmd (XtParent (button), XA_LOCK, 0);
}


void
kill_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  run_cmd (XtParent (button), XA_EXIT, 0);
}


void
restart_menu_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
#if 0
  run_cmd (XtParent (button), XA_RESTART, 0);
#else
  button = XtParent (button);
  apply_changes_and_save (button);
  xscreensaver_command (XtDisplay (button), XA_EXIT, 0, False, NULL);
  sleep (1);
  system ("xscreensaver -nosplash &");
#endif

  await_xscreensaver (button);
}

static void
await_xscreensaver (Widget widget)
{
  int countdown = 5;

  Display *dpy = XtDisplay (widget);
  char *rversion = 0;

  while (!rversion && (--countdown > 0))
    {
      /* Check for the version of the running xscreensaver... */
      server_xscreensaver_version (dpy, &rversion, 0, 0);

      /* If it's not there yet, wait a second... */
      sleep (1);
    }

  if (rversion)
    {
      /* Got it. */
      free (rversion);
    }
  else
    {
      /* Timed out, no screensaver running. */

      char buf [1024];
      Bool root_p = (geteuid () == 0);
      
      strcpy (buf, 
              "Error:\n\n"
              "The xscreensaver daemon did not start up properly.\n"
              "\n");

      if (root_p)
        strcat (buf,
            "You are running as root.  This usually means that xscreensaver\n"
            "was unable to contact your X server because access control is\n"
            "turned on.  Try running this command:\n"
            "\n"
            "                        xhost +localhost\n"
            "\n"
            "and then selecting `File / Restart Daemon'.\n"
            "\n"
            "Note that turning off access control will allow anyone logged\n"
            "on to this machine to access your screen, which might be\n"
            "considered a security problem.  Please read the xscreensaver\n"
            "manual and FAQ for more information.\n"
            "\n"
            "You shouldn't run X as root. Instead, you should log in as a\n"
            "normal user, and `su' as necessary.");
      else
        strcat (buf, "Please check your $PATH and permissions.");

      warning_dialog (XtParent (widget), buf, 1);
    }
}


static int _selected_hack_number = -1;

static int
selected_hack_number (Widget toplevel)
{
  return _selected_hack_number;
}


static int
demo_write_init_file (Widget widget, saver_preferences *p)
{
  if (!write_init_file (p, short_version, False))
    return 0;
  else
    {
      const char *f = init_file_name();
      if (!f || !*f)
        warning_dialog (widget,
                        "Error:\n\nCouldn't determine init file name!\n",
                        100);
      else
        {
          char *b = (char *) malloc (strlen(f) + 1024);
          sprintf (b, "Error:\n\nCouldn't write %s\n", f);
          warning_dialog (widget, b, 100);
          free (b);
        }
      return -1;
    }
}


static int
apply_changes_and_save (Widget widget)
{
  prefs_pair *pair = global_prefs_pair;
  saver_preferences *p =  pair->a;
  Widget list_widget = name_to_widget (widget, "list");
  int which = selected_hack_number (widget);

  Widget cmd = name_to_widget (widget, "cmdText");
  Widget enabled = name_to_widget (widget, "enabled");

  Widget vis = name_to_widget (widget, "combo");
# ifdef HAVE_XMCOMBOBOX
  Widget text = 0;
# else /* !HAVE_XMCOMBOBOX */
  Widget menu = 0, *kids = 0, selected_item = 0;
  Cardinal nkids = 0;
  int i = 0;
# endif /* !HAVE_XMCOMBOBOX */

  Bool enabled_p = False;
  const char *visual = 0;
  const char *command = 0;
  
  char c;
  unsigned long id;

  if (which < 0) return -1;

# ifdef HAVE_XMCOMBOBOX
  XtVaGetValues (vis, XmNtextField, &text, 0);
  if (!text)
    /* If we can't get at the text field of this combo box, we're screwed. */
    abort();
  XtVaGetValues (text, XmNvalue, &visual, 0);

# else /* !HAVE_XMCOMBOBOX */
  XtVaGetValues (vis, XmNsubMenuId, &menu, 0);
  XtVaGetValues (menu, XmNnumChildren, &nkids, XmNchildren, &kids, 0);
  XtVaGetValues (menu, XmNmenuHistory, &selected_item, 0);
  if (selected_item)
    for (i = 0; i < nkids; i++)
      if (kids[i] == selected_item)
        break;

  visual = visual_menu[i];
# endif /* !HAVE_XMCOMBOBOX */

  XtVaGetValues (enabled, XmNset, &enabled_p, 0);
  XtVaGetValues (cmd, XtNvalue, &command, 0);

  if (maybe_reload_init_file (widget, pair) != 0)
    return 1;

  /* Sanity-check and canonicalize whatever the user typed into the combo box.
   */
  if      (!strcasecmp (visual, ""))                   visual = "";
  else if (!strcasecmp (visual, "any"))                visual = "";
  else if (!strcasecmp (visual, "default"))            visual = "Default";
  else if (!strcasecmp (visual, "default-n"))          visual = "Default-N";
  else if (!strcasecmp (visual, "default-i"))          visual = "Default-I";
  else if (!strcasecmp (visual, "best"))               visual = "Best";
  else if (!strcasecmp (visual, "mono"))               visual = "Mono";
  else if (!strcasecmp (visual, "monochrome"))         visual = "Mono";
  else if (!strcasecmp (visual, "gray"))               visual = "Gray";
  else if (!strcasecmp (visual, "grey"))               visual = "Gray";
  else if (!strcasecmp (visual, "color"))              visual = "Color";
  else if (!strcasecmp (visual, "gl"))                 visual = "GL";
  else if (!strcasecmp (visual, "staticgray"))         visual = "StaticGray";
  else if (!strcasecmp (visual, "staticcolor"))        visual = "StaticColor";
  else if (!strcasecmp (visual, "truecolor"))          visual = "TrueColor";
  else if (!strcasecmp (visual, "grayscale"))          visual = "GrayScale";
  else if (!strcasecmp (visual, "greyscale"))          visual = "GrayScale";
  else if (!strcasecmp (visual, "pseudocolor"))        visual = "PseudoColor";
  else if (!strcasecmp (visual, "directcolor"))        visual = "DirectColor";
  else if (1 == sscanf (visual, " %ld %c", &id, &c))   ;
  else if (1 == sscanf (visual, " 0x%lx %c", &id, &c)) ;
  else
    {
      XBell (XtDisplay (widget), 0);			  /* unparsable */
      visual = "";
      /* #### gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry), "Any");*/
    }

  ensure_selected_item_visible (list_widget);

  if (!p->screenhacks[which]->visual)
    p->screenhacks[which]->visual = strdup ("");
  if (!p->screenhacks[which]->command)
    p->screenhacks[which]->command = strdup ("");

  if (p->screenhacks[which]->enabled_p != enabled_p ||
      !!strcasecmp (p->screenhacks[which]->visual, visual) ||
      !!strcasecmp (p->screenhacks[which]->command, command))
    {
      /* Something was changed -- store results into the struct,
         and write the file.
       */
      free (p->screenhacks[which]->visual);
      free (p->screenhacks[which]->command);
      p->screenhacks[which]->visual = strdup (visual);
      p->screenhacks[which]->command = strdup (command);
      p->screenhacks[which]->enabled_p = enabled_p;

      return demo_write_init_file (widget, p);
    }

  /* No changes made */
  return 0;
}

void
run_this_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  int which = selected_hack_number (XtParent (button));
  if (which < 0) return;
  if (0 == apply_changes_and_save (XtParent (button)))
    run_hack (XtParent (button), which, True);
}


void
manual_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  saver_preferences *p =  pair->a;
  Widget list_widget = name_to_widget (button, "list");
  int which = selected_hack_number (button);
  char *name, *name2, *cmd, *s;
  if (which < 0) return;
  apply_changes_and_save (button);
  ensure_selected_item_visible (list_widget);

  name = strdup (p->screenhacks[which]->command);
  name2 = name;
  while (isspace (*name2)) name2++;
  s = name2;
  while (*s && !isspace (*s)) s++;
  *s = 0;
  s = strrchr (name2, '/');
  if (s) name = s+1;

  cmd = get_string_resource ("manualCommand", "ManualCommand");
  if (cmd)
    {
      char *cmd2 = (char *) malloc (strlen (cmd) + strlen (name2) + 100);
      strcpy (cmd2, "( ");
      sprintf (cmd2 + strlen (cmd2),
               cmd,
               name2, name2, name2, name2);
      strcat (cmd2, " ) &");
      system (cmd2);
      free (cmd2);
    }
  else
    {
      warning_dialog (XtParent (button),
                      "Error:\n\nno `manualCommand' resource set.",
                      100);
    }

  free (name);
}


void
run_next_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  saver_preferences *p =  pair->a;

  Widget list_widget = name_to_widget (button, "list");
  int which = selected_hack_number (button);

  button = XtParent (button);

  if (which < 0)
    which = 0;
  else
    which++;

  if (which >= p->screenhacks_count)
    which = 0;

  apply_changes_and_save (button);

  XmListDeselectAllItems (list_widget);	/* LessTif lossage */
  XmListSelectPos (list_widget, which+1, True);

  ensure_selected_item_visible (list_widget);
  populate_demo_window (button, which, pair);
  run_hack (button, which, False);
}


void
run_prev_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;
  saver_preferences *p =  pair->a;

  Widget list_widget = name_to_widget (button, "list");
  int which = selected_hack_number (button);

  button = XtParent (button);

  if (which < 0)
    which = p->screenhacks_count - 1;
  else
    which--;

  if (which < 0)
    which = p->screenhacks_count - 1;

  apply_changes_and_save (button);

  XmListDeselectAllItems (list_widget);	/* LessTif lossage */
  XmListSelectPos (list_widget, which+1, True);

  ensure_selected_item_visible (list_widget);
  populate_demo_window (button, which, pair);
  run_hack (button, which, False);
}


/* Helper for the text fields that contain time specifications:
   this parses the text, and does error checking.
 */
static void 
hack_time_text (Widget button, const char *line, Time *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      value = parse_time ((char *) line, sec_p, True);
      value *= 1000;	/* Time measures in microseconds */
      if (value < 0)
	{
	  char b[255];
	  sprintf (b,
		   "Error:\n\n"
		   "Unparsable time format: \"%s\"\n",
		   line);
	  warning_dialog (XtParent (button), b, 100);
	}
      else
	*store = value;
    }
}


void
prefs_ok_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;

  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;
  Bool changed = False;
  char *v = 0;

  button = XtParent (button);

# define SECONDS(field, name) \
  v = 0; \
  XtVaGetValues (name_to_widget (button, (name)), XtNvalue, &v, 0); \
  hack_time_text (button, v, (field), True)

# define MINUTES(field, name) \
  v = 0; \
  XtVaGetValues (name_to_widget (button, (name)), XtNvalue, &v, 0); \
  hack_time_text (button, v, (field), False)

# define INTEGER(field, name) do { \
    unsigned int value; \
    char c; \
    XtVaGetValues (name_to_widget (button, (name)), XtNvalue, &v, 0); \
    if (! *v) \
      ; \
    else if (sscanf (v, "%u%c", &value, &c) != 1) \
      { \
	char b[255]; \
	sprintf (b, "Error:\n\n" "Not an integer: \"%s\"\n", v); \
	warning_dialog (XtParent (button), b, 100); \
      } \
   else \
     *(field) = value; \
  } while(0)

# define CHECKBOX(field, name) \
  XtVaGetValues (name_to_widget (button, (name)), XmNset, &field, 0)

  MINUTES (&p2->timeout,        "timeoutText");
  MINUTES (&p2->cycle,          "cycleText");
  SECONDS (&p2->fade_seconds,   "fadeSecondsText");
  INTEGER (&p2->fade_ticks,     "fadeTicksText");
  MINUTES (&p2->lock_timeout,   "lockText");
  SECONDS (&p2->passwd_timeout, "passwdText");
  CHECKBOX (p2->verbose_p,      "verboseToggle");
  CHECKBOX (p2->install_cmap_p, "cmapToggle");
  CHECKBOX (p2->fade_p,         "fadeToggle");
  CHECKBOX (p2->unfade_p,       "unfadeToggle");
  CHECKBOX (p2->lock_p,         "lockToggle");

# undef SECONDS
# undef MINUTES
# undef INTEGER
# undef CHECKBOX

# define COPY(field) \
  if (p->field != p2->field) changed = True; \
  p->field = p2->field

  COPY(timeout);
  COPY(cycle);
  COPY(lock_timeout);
  COPY(passwd_timeout);
  COPY(fade_seconds);
  COPY(fade_ticks);
  COPY(verbose_p);
  COPY(install_cmap_p);
  COPY(fade_p);
  COPY(unfade_p);
  COPY(lock_p);
# undef COPY

  populate_prefs_page (button, pair);

  if (changed)
    demo_write_init_file (button, p);
}


void
prefs_cancel_cb (Widget button, XtPointer client_data, XtPointer ignored)
{
  prefs_pair *pair = (prefs_pair *) client_data;

  *pair->b = *pair->a;
  populate_prefs_page (XtParent (button), pair);
}


static void
list_select_cb (Widget list, XtPointer client_data, XtPointer call_data)
{
  prefs_pair *pair = (prefs_pair *) client_data;

  XmListCallbackStruct *lcb = (XmListCallbackStruct *) call_data;
  int which = lcb->item_position - 1;

  apply_changes_and_save (list);
  populate_demo_window (list, which, pair);

  if (lcb->reason == XmCR_DEFAULT_ACTION && which >= 0)
    run_hack (list, which, True);
}


/* Populating the various widgets
 */


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


/* Finds the number of the last hack to run, and makes that item be
   selected by default.
 */
static void
scroll_to_current_hack (Widget toplevel, prefs_pair *pair)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  CARD32 *data = 0;
  Display *dpy = XtDisplay (toplevel);
  int which = 0;
  Widget list;

  if (XGetWindowProperty (dpy, RootWindow (dpy, 0), /* always screen #0 */
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          (unsigned char **) &data)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && data)
    which = (int) data[2] - 1;

  if (data) free (data);

  if (which < 0)
    return;

  list = name_to_widget (toplevel, "list");
  apply_changes_and_save (toplevel);

  XmListDeselectAllItems (list);	/* LessTif lossage */
  XmListSelectPos (list, which+1, True);

  ensure_selected_item_visible (list);
  populate_demo_window (toplevel, which, pair);
}



static void
populate_hack_list (Widget toplevel, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  Widget list = name_to_widget (toplevel, "list");
  screenhack **hacks = p->screenhacks;
  screenhack **h;

  for (h = hacks; *h; h++)
    {
      char *pretty_name = (h[0]->name
                           ? strdup (h[0]->name)
                           : make_hack_name (h[0]->command));

      XmString xmstr = XmStringCreate (pretty_name, XmSTRING_DEFAULT_CHARSET);
      XmListAddItem (list, xmstr, 0);
      XmStringFree (xmstr);
    }

  XtAddCallback (list, XmNbrowseSelectionCallback, list_select_cb, pair);
  XtAddCallback (list, XmNdefaultActionCallback,   list_select_cb, pair);
}


static void
populate_prefs_page (Widget top, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  char s[100];

  format_time (s, p->timeout);
  XtVaSetValues (name_to_widget (top, "timeoutText"),     XmNvalue, s, 0);
  format_time (s, p->cycle);
  XtVaSetValues (name_to_widget (top, "cycleText"),       XmNvalue, s, 0);
  format_time (s, p->lock_timeout);
  XtVaSetValues (name_to_widget (top, "lockText"),        XmNvalue, s, 0);
  format_time (s, p->passwd_timeout);
  XtVaSetValues (name_to_widget (top, "passwdText"),      XmNvalue, s, 0);
  format_time (s, p->fade_seconds);
  XtVaSetValues (name_to_widget (top, "fadeSecondsText"), XmNvalue, s, 0);
  sprintf (s, "%u", p->fade_ticks);
  XtVaSetValues (name_to_widget (top, "fadeTicksText"),   XmNvalue, s, 0);

  XtVaSetValues (name_to_widget (top, "verboseToggle"),
                 XmNset, p->verbose_p, 0);
  XtVaSetValues (name_to_widget (top, "cmapToggle"),
                 XmNset, p->install_cmap_p, 0);
  XtVaSetValues (name_to_widget (top, "fadeToggle"),
                 XmNset, p->fade_p, 0);
  XtVaSetValues (name_to_widget (top, "unfadeToggle"),
                 XmNset, p->unfade_p, 0);
  XtVaSetValues (name_to_widget (top, "lockToggle"),
                 XmNset, p->lock_p, 0);


  {
    Bool found_any_writable_cells = False;
    Display *dpy = XtDisplay (top);
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

#ifdef HAVE_XF86VMODE_GAMMA
    found_any_writable_cells = True;  /* if we can gamma fade, go for it */
#endif

    XtVaSetValues (name_to_widget (top, "fadeSecondsLabel"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "fadeTicksLabel"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "fadeSecondsText"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "fadeTicksText"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "cmapToggle"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "fadeToggle"), XtNsensitive,
                           found_any_writable_cells, 0);
    XtVaSetValues (name_to_widget (top, "unfadeToggle"), XtNsensitive,
                           found_any_writable_cells, 0);
  }
}


static void
sensitize_demo_widgets (Widget toplevel, Bool sensitive_p)
{
  const char *names[] = { "cmdLabel", "cmdText", "enabled",
                          "visLabel", "combo", "demo", "man" };
  int i;
  for (i = 0; i < sizeof(names)/countof(*names); i++)
    {
      Widget w = name_to_widget (toplevel, names[i]);
      XtVaSetValues (w, XtNsensitive, sensitive_p, 0);
    }

  /* I don't know how to handle these yet... */
  {
    const char *names2[] = { "cut", "copy", "paste" };
    for (i = 0; i < sizeof(names2)/countof(*names2); i++)
      {
        Widget w = name_to_widget (toplevel, names2[i]);
        XtVaSetValues (w, XtNsensitive, FALSE, 0);
      }
  }
}



/* Pixmaps for the up and down arrow buttons (yeah, this is sleazy...)
 */

#ifdef HAVE_XPM

static char *up_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None s background",
  "-	c #FFFFFF",
  "+	c #D6D6D6",
  "@	c #000000",

  "       @       ",
  "       @       ",
  "      -+@      ",
  "      -+@      ",
  "     -+++@     ",
  "     -+++@     ",
  "    -+++++@    ",
  "    -+++++@    ",
  "   -+++++++@   ",
  "   -+++++++@   ",
  "  -+++++++++@  ",
  "  -+++++++++@  ",
  " -+++++++++++@ ",
  " @@@@@@@@@@@@@ ",
  "               "
};

static char *down_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None s background",
  "-	c #FFFFFF",
  "+	c #D6D6D6",
  "@	c #000000",

  "               ",
  " ------------- ",
  " -+++++++++++@ ",
  "  -+++++++++@  ",
  "  -+++++++++@  ",
  "   -+++++++@   ",
  "   -+++++++@   ",
  "    -+++++@    ",
  "    -+++++@    ",
  "     -+++@     ",
  "     -+++@     ",
  "      -+@      ",
  "      -+@      ",
  "       @       ",
  "       @       "
};

#endif /* HAVE_XPM */


static void
pixmapify_buttons (Widget toplevel)
{
#ifdef HAVE_XPM

  Display *dpy = XtDisplay (toplevel);
  Window window = XtWindow (toplevel);
  XWindowAttributes xgwa;
  XpmAttributes xpmattrs;
  Pixmap up_pixmap = 0, down_pixmap = 0;
  int result;
  Widget up = name_to_widget (toplevel, "up");
  Widget dn = name_to_widget (toplevel, "down");
# ifdef XpmColorSymbols
  XColor xc;
  XpmColorSymbol symbols[2];
  char color[20];
# endif

  XGetWindowAttributes (dpy, window, &xgwa);

  xpmattrs.valuemask = 0;

# ifdef XpmColorSymbols
  symbols[0].name = "background";
  symbols[0].pixel = 0;
  symbols[1].name = 0;
  XtVaGetValues (up, XmNbackground, &xc, 0);
  XQueryColor (dpy, xgwa.colormap, &xc);
  sprintf (color, "#%04X%04X%04X", xc.red, xc.green, xc.blue);
  symbols[0].value = color;
  symbols[0].pixel = xc.pixel;

  xpmattrs.valuemask |= XpmColorSymbols;
  xpmattrs.colorsymbols = symbols;
  xpmattrs.numsymbols = 1;
# endif

# ifdef XpmCloseness
  xpmattrs.valuemask |= XpmCloseness;
  xpmattrs.closeness = 40000;
# endif
# ifdef XpmVisual
  xpmattrs.valuemask |= XpmVisual;
  xpmattrs.visual = xgwa.visual;
# endif
# ifdef XpmDepth
  xpmattrs.valuemask |= XpmDepth;
  xpmattrs.depth = xgwa.depth;
# endif
# ifdef XpmColormap
  xpmattrs.valuemask |= XpmColormap;
  xpmattrs.colormap = xgwa.colormap;
# endif

  result = XpmCreatePixmapFromData(dpy, window, up_arrow_xpm,
                                   &up_pixmap, 0 /* mask */, &xpmattrs);
  if (!up_pixmap || (result != XpmSuccess && result != XpmColorError))
    {
      fprintf (stderr, "%s: Can't load pixmaps\n", progname);
      return;
    }

  result = XpmCreatePixmapFromData(dpy, window, down_arrow_xpm,
                                   &down_pixmap, 0 /* mask */, &xpmattrs);
  if (!down_pixmap || (result != XpmSuccess && result != XpmColorError))
    {
      fprintf (stderr, "%s: Can't load pixmaps\n", progname);
      return;
    }

  XtVaSetValues (up, XmNlabelType, XmPIXMAP, XmNlabelPixmap, up_pixmap, 0);
  XtVaSetValues (dn, XmNlabelType, XmPIXMAP, XmNlabelPixmap, down_pixmap, 0);

#endif /* HAVE_XPM */
}



char *
get_hack_blurb (screenhack *hack)
{
  char *doc_string;
  char *prog_name = strdup (hack->command);
  char *pretty_name = (hack->name
                       ? strdup (hack->name)
                       : make_hack_name (hack->command));
  char doc_name[255], doc_class[255];
  char *s, *s2;

  for (s = prog_name; *s && !isspace(*s); s++)
    ;
  *s = 0;
  s = strrchr (prog_name, '/');
  if (s) strcpy (prog_name, s+1);

  sprintf (doc_name,  "hacks.%s.documentation", pretty_name);
  sprintf (doc_class, "hacks.%s.documentation", prog_name);
  free (prog_name);
  free (pretty_name);

  doc_string = get_string_resource (doc_name, doc_class);
  if (doc_string)
    {
      for (s = doc_string; *s; s++)
        {
          if (*s == '\n')
            {
              /* skip over whitespace at beginning of line */
              s++;
              while (*s && (*s == ' ' || *s == '\t'))
                s++;
            }
          else if (*s == ' ' || *s == '\t')
            {
              /* compress all other horizontal whitespace. */
              *s = ' ';
              s++;
              for (s2 = s; *s2 && (*s2 == ' ' || *s2 == '\t'); s2++)
                ;
              if (s2 > s) strcpy (s, s2);
              s--;
            }
        }

      while (*s && isspace (*s))      /* Strip trailing whitespace */
        *(--s) = 0;

      /* Delete whitespace at end of each line. */
      for (; s > doc_string; s--)
        if (*s == '\n' && (s[-1] == ' ' || s[-1] == '\t'))
          {
            for (s2 = s-1;
                 s2 > doc_string && (*s2 == ' ' || *s2 == '\t');
                 s2--)
              ;
            s2++;
            if (s2 < s) strcpy (s2, s);
            s = s2;
          }
      
      /* Delete leading blank lines. */
      for (s = doc_string; *s == '\n'; s++)
        ;
      if (s > doc_string) strcpy (doc_string, s);
    }
  else
    {
      static int doc_installed = 0;
      if (doc_installed == 0)
        {
          if (get_boolean_resource ("hacks.documentation.isInstalled",
                                    "hacks.documentation.isInstalled"))
            doc_installed = 1;
          else
            doc_installed = -1;
        }

      if (doc_installed < 0)
        doc_string =
          strdup ("Error:\n\n"
                  "The documentation strings do not appear to be "
                  "installed.  This is probably because there is "
                  "an \"XScreenSaver\" app-defaults file installed "
                  "that is from an older version of the program. "
                  "To fix this problem, delete that file, or "
                  "install a current version (either will work.)");
      else
        doc_string = strdup ("");
    }

  return doc_string;
}


static void
populate_demo_window (Widget toplevel, int which, prefs_pair *pair)
{
  saver_preferences *p = pair->a;
  screenhack *hack = (which >= 0 ? p->screenhacks[which] : 0);
  Widget frameL = name_to_widget (toplevel, "frameLabel");
  Widget doc = name_to_widget (toplevel, "doc");
  Widget cmd = name_to_widget (toplevel, "cmdText");
  Widget enabled = name_to_widget (toplevel, "enabled");
  Widget vis = name_to_widget (toplevel, "combo");
  int i = 0;

  char *pretty_name = (hack
                       ? (hack->name
                          ? strdup (hack->name)
                          : make_hack_name (hack->command))
                       : 0);
  char *doc_string = hack ? get_hack_blurb (hack) : 0;

  XmString xmstr;

  xmstr = XmStringCreate (pretty_name, XmSTRING_DEFAULT_CHARSET);
  XtVaSetValues (frameL, XmNlabelString, xmstr, 0);
  XmStringFree (xmstr);

  XtVaSetValues (doc, XmNvalue, doc_string, 0);
  XtVaSetValues (cmd, XmNvalue, (hack ? hack->command : ""), 0);

  XtVaSetValues (enabled, XmNset, (hack ? hack->enabled_p : False), 0);

  i = 0;
  if (hack && hack->visual && *hack->visual)
    for (i = 0; visual_menu[i]; i++)
      if (!strcasecmp (hack->visual, visual_menu[i]))
        break;
  if (!visual_menu[i]) i = -1;

  {
# ifdef HAVE_XMCOMBOBOX
    Widget text = 0;
    XtVaGetValues (vis, XmNtextField, &text, 0);
    XtVaSetValues (vis, XmNselectedPosition, i, 0);
    if (i < 0)
      XtVaSetValues (text, XmNvalue, hack->visual, 0);
# else /* !HAVE_XMCOMBOBOX */
    Cardinal nkids;
    Widget *kids;
    Widget menu;

    XtVaGetValues (vis, XmNsubMenuId, &menu, 0);
    if (!menu) abort ();
    XtVaGetValues (menu, XmNnumChildren, &nkids, XmNchildren, &kids, 0);
    if (!kids) abort();
    if (i < nkids)
      XtVaSetValues (vis, XmNmenuHistory, kids[i], 0);
# endif /* !HAVE_XMCOMBOBOX */
  }

  sensitize_demo_widgets (toplevel, (hack ? True : False));

  if (pretty_name) free (pretty_name);
  if (doc_string) free (doc_string);

  _selected_hack_number = which;
}



static int
maybe_reload_init_file (Widget widget, prefs_pair *pair)
{
  int status = 0;
  saver_preferences *p =  pair->a;

  static Bool reentrant_lock = False;
  if (reentrant_lock) return 0;
  reentrant_lock = True;

  if (init_file_changed_p (p))
    {
      const char *f = init_file_name();
      char *b;
      int which;
      Widget list;

      if (!f || !*f) return 0;
      b = (char *) malloc (strlen(f) + 1024);
      sprintf (b,
               "Warning:\n\n"
               "file \"%s\" has changed, reloading.\n",
               f);
      warning_dialog (widget, b, 100);
      free (b);

      load_init_file (p);

      which = selected_hack_number (widget);
      list = name_to_widget (widget, "list");

      XtVaSetValues (list, XmNitemCount, 0, 0);

      populate_hack_list (widget, pair);

      XmListDeselectAllItems (list);	/* LessTif lossage */
      XmListSelectPos (list, which+1, True);

      populate_prefs_page (widget, pair);
      populate_demo_window (widget, which, pair);
      ensure_selected_item_visible (list);

      status = 1;
    }

  reentrant_lock = False;
  return status;
}



/* Attach all callback functions to widgets
 */

static void
add_callbacks (Widget toplevel, prefs_pair *pair)
{
  Widget w;

# define CB(NAME,FN) \
  w = name_to_widget (toplevel, (NAME)); \
  XtAddCallback (w, XmNactivateCallback, (FN), pair)

  CB ("blank",   activate_menu_cb);
  CB ("lock",    lock_menu_cb);
  CB ("kill",    kill_menu_cb);
  CB ("restart", restart_menu_cb);
  CB ("exit",    exit_menu_cb);

  CB ("cut",     cut_menu_cb);
  CB ("copy",    copy_menu_cb);
  CB ("paste",   paste_menu_cb);

  CB ("about",   about_menu_cb);
  CB ("docMenu", doc_menu_cb);

  CB ("down",    run_next_cb);
  CB ("up",      run_prev_cb);
  CB ("demo",    run_this_cb);
  CB ("man",     manual_cb);

  CB ("preferencesForm.Cancel",  prefs_cancel_cb);
  CB ("preferencesForm.OK",      prefs_ok_cb);

# undef CB
}


static void
sanity_check_resources (Widget toplevel)
{
  const char *names[] = { "demoTab", "optionsTab", "cmdLabel", "visLabel",
                          "enabled", "demo", "man", "timeoutLabel",
                          "cycleLabel", "fadeSecondsLabel", "fadeTicksLabel",
                          "lockLabel", "passwdLabel" };
  int i;
  for (i = 0; i < sizeof(names)/countof(*names); i++)
    {
      Widget w = name_to_widget (toplevel, names[i]);
      const char *name = XtName(w);
      XmString xm = 0;
      char *label = 0;
      XtVaGetValues (w, XmNlabelString, &xm, 0);
      if (xm) XmStringGetLtoR (xm, XmSTRING_DEFAULT_CHARSET, &label);
      if (w && (!label || !strcmp (name, label)))
        {
          xm = XmStringCreate ("ERROR", XmSTRING_DEFAULT_CHARSET);
          XtVaSetValues (w, XmNlabelString, xm, 0);
        }
    }
}

/* Set certain buttons to be the same size (the max of the set.)
 */
static void
hack_button_sizes (Widget toplevel)
{
  Widget demo = name_to_widget (toplevel, "demo");
  Widget man  = name_to_widget (toplevel, "man");
  Widget ok   = name_to_widget (toplevel, "OK");
  Widget can  = name_to_widget (toplevel, "Cancel");
  Widget up   = name_to_widget (toplevel, "up");
  Widget down = name_to_widget (toplevel, "down");
  Dimension w1, w2;

  XtVaGetValues (demo, XmNwidth, &w1, 0);
  XtVaGetValues (man,  XmNwidth, &w2, 0);
  XtVaSetValues ((w1 > w2 ? man : demo), XmNwidth, (w1 > w2 ? w1 : w2), 0);

  XtVaGetValues (ok,   XmNwidth, &w1, 0);
  XtVaGetValues (can,  XmNwidth, &w2, 0);
  XtVaSetValues ((w1 > w2 ? can : ok), XmNwidth, (w1 > w2 ? w1 : w2), 0);

  XtVaGetValues (up,   XmNwidth, &w1, 0);
  XtVaGetValues (down, XmNwidth, &w2, 0);
  XtVaSetValues ((w1 > w2 ? down : up), XmNwidth, (w1 > w2 ? w1 : w2), 0);
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
			       1024));
  *msg = 0;

  if (!rversion || !*rversion)
    {
      sprintf (msg,
	       "Warning:\n\n"
               "The XScreenSaver daemon doesn't seem to be running\n"
               "on display \"%s\".  You can launch it by selecting\n"
               "`Restart Daemon' from the File menu, or by typing\n"
               "\"xscreensaver &\" in a shell.",
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
	      "xscreensaver as \"%s\" (which you can do by\n"
              "selecting `Restart Daemon' from the File menu.)\n",
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
	       "%s won't work right.\n"
               "\n"
               "You can restart the daemon on \"%s\" as \"%s\" by\n"
               "selecting `Restart Daemon' from the File menu.)",
	       progname, luser, lhost,
	       d,
	       (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	       luser,
	       progname,
               lhost, luser);
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
    warning_dialog (parent, msg, 1);

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
  Widget toplevel_shell, dialog;
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

  global_prefs_pair = pair;

  progname = real_progname;

  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";
  progname = argv[0];


  toplevel_shell = XtAppInitialize (&app, progclass, 0, 0, &argc, argv,
				    defaults, 0, 0);

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


#if 0
  {
    XrmName name = { 0 };
    XrmClass class = { 0 };
    int count = 0;
    XrmEnumerateDatabase (db, &name, &class, XrmEnumAllLevels, mapper,
			  (POINTER) &count);
  }
#endif


  /* Intern the atoms that xscreensaver_command() needs.
   */
  XA_VROOT = XInternAtom (dpy, "__SWM_VROOT", False);
  XA_SCREENSAVER = XInternAtom (dpy, "SCREENSAVER", False);
  XA_SCREENSAVER_VERSION = XInternAtom (dpy, "_SCREENSAVER_VERSION",False);
  XA_SCREENSAVER_STATUS = XInternAtom (dpy, "_SCREENSAVER_STATUS", False);
  XA_SCREENSAVER_ID = XInternAtom (dpy, "_SCREENSAVER_ID", False);
  XA_SCREENSAVER_RESPONSE = XInternAtom (dpy, "_SCREENSAVER_RESPONSE", False);
  XA_SELECT = XInternAtom (dpy, "SELECT", False);
  XA_DEMO = XInternAtom (dpy, "DEMO", False);
  XA_ACTIVATE = XInternAtom (dpy, "ACTIVATE", False);
  XA_BLANK = XInternAtom (dpy, "BLANK", False);
  XA_LOCK = XInternAtom (dpy, "LOCK", False);
  XA_EXIT = XInternAtom (dpy, "EXIT", False);
  XA_RESTART = XInternAtom (dpy, "RESTART", False);

  /* Create the window and all its widgets.
   */
  dialog = create_xscreensaver_demo (toplevel_shell);

  /* Set the window's title. */
  {
    char title[255];
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    sprintf (title, "%.50s %.50s, %.50s", progclass, s1, s3);
    XtVaSetValues (toplevel_shell, XtNtitle, title, 0);
    free (v);
  }

  sanity_check_resources (toplevel_shell);
  add_callbacks (toplevel_shell, pair);
  populate_hack_list (toplevel_shell, pair);
  populate_prefs_page (toplevel_shell, pair);
  sensitize_demo_widgets (toplevel_shell, False);
  scroll_to_current_hack (toplevel_shell, pair);

  XtManageChild (dialog);
  XtRealizeWidget(toplevel_shell);

  /* The next few calls must come after XtRealizeWidget(). */
  pixmapify_buttons (toplevel_shell);
  hack_button_sizes (toplevel_shell);
  ensure_selected_item_visible (name_to_widget (toplevel_shell, "list"));

  XSync (dpy, False);
  XtVaSetValues (toplevel_shell, XmNallowShellResize, False, 0);


  /* Handle the -prefs command-line argument. */
  if (prefs)
    {
      Widget tabber = name_to_widget (toplevel_shell, "folder");
      Widget this_tab = name_to_widget (toplevel_shell, "optionsTab");
      Widget this_page = name_to_widget (toplevel_shell, "preferencesForm");
      Widget *kids = 0;
      Cardinal nkids = 0;
      if (!tabber) abort();
  
      XtVaGetValues (tabber, XmNnumChildren, &nkids, XmNchildren, &kids, 0);
      if (!kids) abort();
      if (nkids > 0)
        XtUnmanageChildren (kids, nkids);

      XtManageChild (this_page);

      XmProcessTraversal (this_tab, XmTRAVERSE_CURRENT);
    }

  /* Issue any warnings about the running xscreensaver daemon. */
  the_network_is_not_the_computer (toplevel_shell);


  XtAppMainLoop (app);
  exit (0);
}

#endif /* HAVE_MOTIF -- whole file */
