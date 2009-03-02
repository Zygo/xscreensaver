/* demo-Gtk.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-2002 Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_GTK /* whole file */

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
#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>


#include <signal.h>
#include <errno.h>
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>		/* for waitpid() and associated macros */
#endif


#include <X11/Xproto.h>		/* for CARD32 */
#include <X11/Xatom.h>		/* for XA_INTEGER */
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

#include <gtk/gtk.h>

#ifdef HAVE_CRAPPLET
# include <gnome.h>
# include <capplet-widget.h>
#endif

#include <gdk/gdkx.h>

#include "version.h"
#include "prefs.h"
#include "resources.h"		/* for parse_time() */
#include "visual.h"		/* for has_writable_cells() */
#include "remote.h"		/* for xscreensaver_command() */
#include "usleep.h"

#include "logo-50.xpm"
#include "logo-180.xpm"

#include "demo-Gtk-widgets.h"
#include "demo-Gtk-support.h"
#include "demo-Gtk-conf.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>


/* from exec.c */
extern void exec_command (const char *shell, const char *command, int nice);

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;

/* The order of the items in the mode menu. */
static int mode_menu_order[] = {
  DONT_BLANK, BLANK_ONLY, ONE_HACK, RANDOM_HACKS };


typedef struct {

  char *short_version;		/* version number of this xscreensaver build */

  GtkWidget *toplevel_widget;	/* the main window */
  GtkWidget *base_widget;	/* root of our hierarchy (for name lookups) */
  GtkWidget *popup_widget;	/* the "Settings" dialog */
  conf_data *cdata;		/* private data for per-hack configuration */

  Bool debug_p;			/* whether to print diagnostics */
  Bool initializing_p;		/* flag for breaking recursion loops */
  Bool saving_p;		/* flag for breaking recursion loops */

  char *desired_preview_cmd;	/* subprocess we intend to run */
  char *running_preview_cmd;	/* subprocess we are currently running */
  pid_t running_preview_pid;	/* pid of forked subproc (might be dead) */
  Bool running_preview_error_p;	/* whether the pid died abnormally */

  Bool preview_suppressed_p;	/* flag meaning "don't launch subproc" */
  int subproc_timer_id;		/* timer to delay subproc launch */
  int subproc_check_timer_id;	/* timer to check whether it started up */
  int subproc_check_countdown;  /* how many more checks left */

  int *list_elt_to_hack_number;	/* table for sorting the hack list */
  int *hack_number_to_list_elt;	/* the inverse table */

  int _selected_list_element;	/* don't use this: call
                                   selected_list_element() instead */

  saver_preferences prefs;

} state;


/* Total fucking evilness due to the fact that it's rocket science to get
   a closure object of our own down into the various widget callbacks. */
static state *global_state_kludge;

Atom XA_VROOT;
Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO;
Atom XA_ACTIVATE, XA_BLANK, XA_LOCK, XA_RESTART, XA_EXIT;


static void populate_demo_window (state *, int list_elt);
static void populate_prefs_page (state *);
static void populate_popup_window (state *);

static Bool flush_dialog_changes_and_save (state *);
static Bool flush_popup_changes_and_save (state *);

static int maybe_reload_init_file (state *);
static void await_xscreensaver (state *);

static void schedule_preview (state *, const char *cmd);
static void kill_preview_subproc (state *);
static void schedule_preview_check (state *);



/* Some random utility functions
 */

const char *
blurb (void)
{
  time_t now = time ((time_t *) 0);
  char *ct = (char *) ctime (&now);
  static char buf[255];
  int n = strlen(progname);
  if (n > 100) n = 99;
  strncpy(buf, progname, n);
  buf[n++] = ':';
  buf[n++] = ' ';
  strncpy(buf+n, ct+11, 8);
  strcpy(buf+n+9, ": ");
  return buf;
}


static GtkWidget *
name_to_widget (state *s, const char *name)
{
  GtkWidget *w;
  if (!s) abort();
  if (!name) abort();
  if (!*name) abort();

  w = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (s->base_widget),
                                         name);
  if (w) return w;
  w = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (s->popup_widget),
                                         name);
  if (w) return w;

  fprintf (stderr, "%s: no widget \"%s\"\n", blurb(), name);
  abort();
}


/* Why this behavior isn't automatic in *either* toolkit, I'll never know.
   Takes a scroller, viewport, or list as an argument.
 */
static void
ensure_selected_item_visible (GtkWidget *widget)
{
  GtkScrolledWindow *scroller = 0;
  GtkViewport *vp = 0;
  GtkList *list_widget = 0;
  GList *slist;
  GList *kids;
  int nkids = 0;
  GtkWidget *selected = 0;
  int list_elt = -1;
  GtkAdjustment *adj;
  gint parent_h, child_y, child_h, children_h, ignore;
  double ratio_t, ratio_b;

  if (GTK_IS_SCROLLED_WINDOW (widget))
    {
      scroller = GTK_SCROLLED_WINDOW (widget);
      vp = GTK_VIEWPORT (GTK_BIN (scroller)->child);
      list_widget = GTK_LIST (GTK_BIN(vp)->child);
    }
  else if (GTK_IS_VIEWPORT (widget))
    {
      vp = GTK_VIEWPORT (widget);
      scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
      list_widget = GTK_LIST (GTK_BIN(vp)->child);
    }
  else if (GTK_IS_LIST (widget))
    {
      list_widget = GTK_LIST (widget);
      vp = GTK_VIEWPORT (GTK_WIDGET (list_widget)->parent);
      scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
    }
  else
    abort();

  slist = list_widget->selection;
  selected = (slist ? GTK_WIDGET (slist->data) : 0);
  if (!selected)
    return;

  list_elt = gtk_list_child_position (list_widget, GTK_WIDGET (selected));

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

  if (adj->upper == 0.0)  /* no items in list */
    return;

  if (ratio_t < (adj->value / adj->upper) ||
      ratio_b > ((adj->value + adj->page_size) / adj->upper))
    {
      double target;
      int slop = parent_h * 0.75; /* how much to overshoot by */

      if (ratio_t < (adj->value / adj->upper))
        {
          double ratio_w = ((double) parent_h) / ((double) children_h);
          double ratio_l = (ratio_b - ratio_t);
          target = ((ratio_t - ratio_w + ratio_l) * adj->upper);
          target += slop;
        }
      else /* if (ratio_b > ((adj->value + adj->page_size) / adj->upper))*/
        {
          target = ratio_t * adj->upper;
          target -= slop;
        }

      if (target > adj->upper - adj->page_size)
        target = adj->upper - adj->page_size;
      if (target < 0)
        target = 0;

      gtk_adjustment_set_value (adj, target);
    }
}

static void
warning_dialog_dismiss_cb (GtkWidget *widget, gpointer user_data)
{
  GtkWidget *shell = GTK_WIDGET (user_data);
  while (shell->parent)
    shell = shell->parent;
  gtk_widget_destroy (GTK_WIDGET (shell));
}


void restart_menu_cb (GtkWidget *widget, gpointer user_data);

static void warning_dialog_restart_cb (GtkWidget *widget, gpointer user_data)
{
  restart_menu_cb (widget, user_data);
  warning_dialog_dismiss_cb (widget, user_data);
}

static void
warning_dialog (GtkWidget *parent, const char *message,
                Boolean restart_button_p, int center)
{
  char *msg = strdup (message);
  char *head;

  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *label = 0;
  GtkWidget *ok = 0;
  GtkWidget *cancel = 0;
  int i = 0;

  while (parent && !parent->window)
    parent = parent->parent;

  if (!GTK_WIDGET (parent)->window) /* too early to pop up transient dialogs */
    {
      fprintf (stderr, "%s: too early for dialog?\n", progname);
      return;
    }

  head = msg;
  while (head)
    {
      char name[20];
      char *s = strchr (head, '\n');
      if (s) *s = 0;

      sprintf (name, "label%d", i++);

      {
        label = gtk_label_new (head);

        if (i == 1)
          {
            GTK_WIDGET (label)->style =
              gtk_style_copy (GTK_WIDGET (label)->style);
            GTK_WIDGET (label)->style->font =
              gdk_font_load (get_string_resource("warning_dialog.headingFont",
                                                 "Dialog.Font"));
            gtk_widget_set_style (GTK_WIDGET (label),
                                  GTK_WIDGET (label)->style);
          }

        if (center <= 0)
          gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
        gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                            label, TRUE, TRUE, 0);
        gtk_widget_show (label);
      }

      if (s)
	head = s+1;
      else
	head = 0;

      center--;
    }

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                      label, TRUE, TRUE, 0);
  gtk_widget_show (label);

  label = gtk_hbutton_box_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
                      label, TRUE, TRUE, 0);

  ok = gtk_button_new_with_label ("OK");
  gtk_container_add (GTK_CONTAINER (label), ok);

  if (restart_button_p)
    {
      cancel = gtk_button_new_with_label ("Cancel");
      gtk_container_add (GTK_CONTAINER (label), cancel);
    }

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_title (GTK_WINDOW (dialog), progclass);
  gtk_widget_show (ok);
  if (cancel)
    gtk_widget_show (cancel);
  gtk_widget_show (label);
  gtk_widget_show (dialog);
/*  gtk_window_set_default (GTK_WINDOW (dialog), ok);*/

  if (restart_button_p)
    {
      gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                                 GTK_SIGNAL_FUNC (warning_dialog_restart_cb),
                                 (gpointer) dialog);
      gtk_signal_connect_object (GTK_OBJECT (cancel), "clicked",
                                 GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                                 (gpointer) dialog);
    }
  else
    {
      gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                                 GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                                 (gpointer) dialog);
    }

  gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
                                GTK_WIDGET (parent)->window);

  gdk_window_show (GTK_WIDGET (dialog)->window);
  gdk_window_raise (GTK_WIDGET (dialog)->window);

  free (msg);
}


static void
run_cmd (state *s, Atom command, int arg)
{
  char *err = 0;
  int status;

  flush_dialog_changes_and_save (s);
  status = xscreensaver_command (GDK_DISPLAY(), command, arg, False, &err);
  if (status < 0)
    {
      char buf [255];
      if (err)
        sprintf (buf, "Error:\n\n%s", err);
      else
        strcpy (buf, "Unknown error!");
      warning_dialog (s->toplevel_widget, buf, False, 100);
    }
  if (err) free (err);
}


static void
run_hack (state *s, int list_elt, Bool report_errors_p)
{
  int hack_number;
  if (list_elt < 0) return;
  hack_number = s->list_elt_to_hack_number[list_elt];

  flush_dialog_changes_and_save (s);
  schedule_preview (s, 0);
  if (report_errors_p)
    run_cmd (s, XA_DEMO, hack_number + 1);
  else
    {
      char *s = 0;
      xscreensaver_command (GDK_DISPLAY(), XA_DEMO, hack_number + 1,
                            False, &s);
      if (s) free (s);
    }
}



/* Button callbacks
 */

void
exit_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  flush_dialog_changes_and_save (s);
  kill_preview_subproc (s);
  gtk_main_quit ();
}

static void
wm_toplevel_close_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  state *s = (state *) data;
  flush_dialog_changes_and_save (s);
  gtk_main_quit ();
}


void
about_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  char msg [2048];
  char *vers = strdup (screensaver_id + 4);
  char *s;
  char copy[1024];
  char *desc = "For updates, check http://www.jwz.org/xscreensaver/";

  s = strchr (vers, ',');
  *s = 0;
  s += 2;

  sprintf(copy, "Copyright \251 1991-2002 %s", s);

  sprintf (msg, "%s\n\n%s", copy, desc);

  /* I can't make gnome_about_new() work here -- it starts dying in
     gdk_imlib_get_visual() under gnome_about_new().  If this worked,
     then this might be the thing to do:

     #ifdef HAVE_CRAPPLET
     {
       const gchar *auth[] = { 0 };
       GtkWidget *about = gnome_about_new (progclass, vers, "", auth, desc,
                                           "xscreensaver.xpm");
       gtk_widget_show (about);
     }
     #else / * GTK but not GNOME * /
      ...
   */
  {
    GdkColormap *colormap;
    GdkPixmap *gdkpixmap;
    GdkBitmap *mask;

    GtkWidget *dialog = gtk_dialog_new ();
    GtkWidget *hbox, *icon, *vbox, *label1, *label2, *hb, *ok;
    GtkWidget *parent = GTK_WIDGET (menuitem);
    while (parent->parent)
      parent = parent->parent;

    hbox = gtk_hbox_new (FALSE, 20);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
                        hbox, TRUE, TRUE, 0);

    colormap = gtk_widget_get_colormap (parent);
    gdkpixmap =
      gdk_pixmap_colormap_create_from_xpm_d (NULL, colormap, &mask, NULL,
                                             (gchar **) logo_180_xpm);
    icon = gtk_pixmap_new (gdkpixmap, mask);
    gtk_misc_set_padding (GTK_MISC (icon), 10, 10);

    gtk_box_pack_start (GTK_BOX (hbox), icon, FALSE, FALSE, 0);

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);

    label1 = gtk_label_new (vers);
    gtk_box_pack_start (GTK_BOX (vbox), label1, TRUE, TRUE, 0);
    gtk_label_set_justify (GTK_LABEL (label1), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label1), 0.0, 0.75);

    GTK_WIDGET (label1)->style = gtk_style_copy (GTK_WIDGET (label1)->style);
    GTK_WIDGET (label1)->style->font =
      gdk_font_load (get_string_resource ("about.headingFont","Dialog.Font"));
    gtk_widget_set_style (GTK_WIDGET (label1), GTK_WIDGET (label1)->style);

    label2 = gtk_label_new (msg);
    gtk_box_pack_start (GTK_BOX (vbox), label2, TRUE, TRUE, 0);
    gtk_label_set_justify (GTK_LABEL (label2), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC (label2), 0.0, 0.25);

    GTK_WIDGET (label2)->style = gtk_style_copy (GTK_WIDGET (label2)->style);
    GTK_WIDGET (label2)->style->font =
      gdk_font_load (get_string_resource ("about.bodyFont","Dialog.Font"));
    gtk_widget_set_style (GTK_WIDGET (label2), GTK_WIDGET (label2)->style);

    hb = gtk_hbutton_box_new ();

    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->action_area),
                        hb, TRUE, TRUE, 0);

    ok = gtk_button_new_with_label ("OK");
    gtk_container_add (GTK_CONTAINER (hb), ok);

    gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
    gtk_window_set_title (GTK_WINDOW (dialog), progclass);

    gtk_widget_show (hbox);
    gtk_widget_show (icon);
    gtk_widget_show (vbox);
    gtk_widget_show (label1);
    gtk_widget_show (label2);
    gtk_widget_show (hb);
    gtk_widget_show (ok);
    gtk_widget_show (dialog);

    gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                               GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                               (gpointer) dialog);
    gdk_window_set_transient_for (GTK_WIDGET (dialog)->window,
                                  GTK_WIDGET (parent)->window);
    gdk_window_show (GTK_WIDGET (dialog)->window);
    gdk_window_raise (GTK_WIDGET (dialog)->window);
  }
}


void
doc_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  char *help_command;

  if (!p->help_url || !*p->help_url)
    {
      warning_dialog (s->toplevel_widget,
                      "Error:\n\n"
                      "No Help URL has been specified.\n", False, 100);
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
activate_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_ACTIVATE, 0);
}


void
lock_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_LOCK, 0);
}


void
kill_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  run_cmd (s, XA_EXIT, 0);
}


void
restart_menu_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  flush_dialog_changes_and_save (s);
  xscreensaver_command (GDK_DISPLAY(), XA_EXIT, 0, False, NULL);
  sleep (1);
  system ("xscreensaver -nosplash &");

  await_xscreensaver (s);
}

static void
await_xscreensaver (state *s)
{
  int countdown = 5;

  Display *dpy = GDK_DISPLAY();
  /*  GtkWidget *dialog = 0;*/
  char *rversion = 0;

  while (!rversion && (--countdown > 0))
    {
      /* Check for the version of the running xscreensaver... */
      server_xscreensaver_version (dpy, &rversion, 0, 0);

      /* If it's not there yet, wait a second... */
      if (!rversion)
        sleep (1);
    }

/*  if (dialog) gtk_widget_destroy (dialog);*/

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

      warning_dialog (s->toplevel_widget, buf, False, 1);
    }
}


static int
selected_list_element (state *s)
{
  return s->_selected_list_element;
}


static int
demo_write_init_file (state *s, saver_preferences *p)
{

#if 0
  /* #### try to figure out why shit keeps getting reordered... */
  if (strcmp (s->prefs.screenhacks[0]->name, "DNA Lounge Slideshow"))
    abort();
#endif

  if (!write_init_file (p, s->short_version, False))
    {
      if (s->debug_p)
        fprintf (stderr, "%s: wrote %s\n", blurb(), init_file_name());
      return 0;
    }
  else
    {
      const char *f = init_file_name();
      if (!f || !*f)
        warning_dialog (s->toplevel_widget,
                        "Error:\n\nCouldn't determine init file name!\n",
                        False, 100);
      else
        {
          char *b = (char *) malloc (strlen(f) + 1024);
          sprintf (b, "Error:\n\nCouldn't write %s\n", f);
          warning_dialog (s->toplevel_widget, b, False, 100);
          free (b);
        }
      return -1;
    }
}


void
run_this_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  int list_elt = selected_list_element (s);
  if (list_elt < 0) return;
  if (!flush_dialog_changes_and_save (s))
    run_hack (s, list_elt, True);
}


void
manual_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  GtkList *list_widget = GTK_LIST (name_to_widget (s, "list"));
  int list_elt = selected_list_element (s);
  int hack_number;
  char *name, *name2, *cmd, *str;
  if (list_elt < 0) return;
  hack_number = s->list_elt_to_hack_number[list_elt];

  flush_dialog_changes_and_save (s);
  ensure_selected_item_visible (GTK_WIDGET (list_widget));

  name = strdup (p->screenhacks[hack_number]->command);
  name2 = name;
  while (isspace (*name2)) name2++;
  str = name2;
  while (*str && !isspace (*str)) str++;
  *str = 0;
  str = strrchr (name2, '/');
  if (str) name = str+1;

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
      warning_dialog (GTK_WIDGET (button),
                      "Error:\n\nno `manualCommand' resource set.",
                      False, 100);
    }

  free (name);
}


static void
force_list_select_item (state *s, GtkList *list, int list_elt, Bool scroll_p)
{
  GtkWidget *parent = name_to_widget (s, "scroller");
  Bool was = GTK_WIDGET_IS_SENSITIVE (parent);

  if (!was) gtk_widget_set_sensitive (parent, True);
  gtk_list_select_item (GTK_LIST (list), list_elt);
  if (scroll_p) ensure_selected_item_visible (GTK_WIDGET (list));
  if (!was) gtk_widget_set_sensitive (parent, False);
}


void
run_next_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  Bool ops = s->preview_suppressed_p;

  GtkList *list_widget =
    GTK_LIST (name_to_widget (s, "list"));
  int list_elt = selected_list_element (s);

  if (list_elt < 0)
    list_elt = 0;
  else
    list_elt++;

  if (list_elt >= p->screenhacks_count)
    list_elt = 0;

  s->preview_suppressed_p = True;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, GTK_LIST (list_widget), list_elt, True);
  populate_demo_window (s, list_elt);
  run_hack (s, list_elt, False);

  s->preview_suppressed_p = ops;
}


void
run_prev_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  Bool ops = s->preview_suppressed_p;

  GtkList *list_widget =
    GTK_LIST (name_to_widget (s, "list"));
  int list_elt = selected_list_element (s);

  if (list_elt < 0)
    list_elt = p->screenhacks_count - 1;
  else
    list_elt--;

  if (list_elt < 0)
    list_elt = p->screenhacks_count - 1;

  s->preview_suppressed_p = True;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, GTK_LIST (list_widget), list_elt, True);
  populate_demo_window (s, list_elt);
  run_hack (s, list_elt, False);

  s->preview_suppressed_p = ops;
}


/* Writes the given settings into prefs.
   Returns true if there was a change, False otherwise.
   command and/or visual may be 0, or enabled_p may be -1, meaning "no change".
 */
static Bool
flush_changes (state *s,
               int list_elt,
               int enabled_p,
               const char *command,
               const char *visual)
{
  saver_preferences *p = &s->prefs;
  Bool changed = False;
  screenhack *hack;
  int hack_number;
  if (list_elt < 0 || list_elt >= p->screenhacks_count)
    abort();

  hack_number = s->list_elt_to_hack_number[list_elt];
  hack = p->screenhacks[hack_number];

  if (enabled_p != -1 &&
      enabled_p != hack->enabled_p)
    {
      hack->enabled_p = enabled_p;
      changed = True;
      if (s->debug_p)
        fprintf (stderr, "%s: \"%s\": enabled => %d\n",
                 blurb(), hack->name, enabled_p);
    }

  if (command)
    {
      if (!hack->command || !!strcmp (command, hack->command))
        {
          if (hack->command) free (hack->command);
          hack->command = strdup (command);
          changed = True;
          if (s->debug_p)
            fprintf (stderr, "%s: \"%s\": command => \"%s\"\n",
                     blurb(), hack->name, command);
        }
    }

  if (visual)
    {
      const char *ov = hack->visual;
      if (!ov || !*ov) ov = "any";
      if (!*visual) visual = "any";
      if (!!strcasecmp (visual, ov))
        {
          if (hack->visual) free (hack->visual);
          hack->visual = strdup (visual);
          changed = True;
          if (s->debug_p)
            fprintf (stderr, "%s: \"%s\": visual => \"%s\"\n",
                     blurb(), hack->name, visual);
        }
    }

  return changed;
}


/* Helper for the text fields that contain time specifications:
   this parses the text, and does error checking.
 */
static void 
hack_time_text (state *s, const char *line, Time *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      if (!sec_p || strchr (line, ':'))
        value = parse_time ((char *) line, sec_p, True);
      else
        {
          char c;
          if (sscanf (line, "%u%c", &value, &c) != 1)
            value = -1;
          if (!sec_p)
            value *= 60;
        }

      value *= 1000;	/* Time measures in microseconds */
      if (value < 0)
	{
	  char b[255];
	  sprintf (b,
		   "Error:\n\n"
		   "Unparsable time format: \"%s\"\n",
		   line);
	  warning_dialog (s->toplevel_widget, b, False, 100);
	}
      else
	*store = value;
    }
}


static Bool
directory_p (const char *path)
{
  struct stat st;
  if (!path || !*path)
    return False;
  else if (stat (path, &st))
    return False;
  else if (!S_ISDIR (st.st_mode))
    return False;
  else
    return True;
}

static char *
normalize_directory (const char *path)
{
  int L;
  char *p2, *s;
  if (!path) return 0;
  L = strlen (path);;
  p2 = (char *) malloc (L + 2);
  strcpy (p2, path);
  if (p2[L-1] == '/')  /* remove trailing slash */
    p2[--L] = 0;

  for (s = p2; s && *s; s++)
    {
      if (*s == '/' &&
          (!strncmp (s, "/../", 4) ||			/* delete "XYZ/../" */
           !strncmp (s, "/..\000", 4)))			/* delete "XYZ/..$" */
        {
          char *s0 = s;
          while (s0 > p2 && s0[-1] != '/')
            s0--;
          if (s0 > p2)
            {
              s0--;
              s += 3;
              strcpy (s0, s);
              s = s0-1;
            }
        }
      else if (*s == '/' && !strncmp (s, "/./", 3))	/* delete "/./" */
        strcpy (s, s+2), s--;
      else if (*s == '/' && !strncmp (s, "/.\000", 3))	/* delete "/.$" */
        *s = 0, s--;
    }

  for (s = p2; s && *s; s++)		/* normalize consecutive slashes */
    while (s[0] == '/' && s[1] == '/')
      strcpy (s, s+1);

  /* and strip trailing whitespace for good measure. */
  L = strlen(p2);
  while (isspace(p2[L-1]))
    p2[--L] = 0;

  return p2;
}


/* Flush out any changes made in the main dialog window (where changes
   take place immediately: clicking on a checkbox causes the init file
   to be written right away.)
 */
static Bool
flush_dialog_changes_and_save (state *s)
{
  saver_preferences *p = &s->prefs;
  saver_preferences P2, *p2 = &P2;
  GtkList *list_widget = GTK_LIST (name_to_widget (s, "list"));
  GList *kids = gtk_container_children (GTK_CONTAINER (list_widget));
  Bool changed = False;
  GtkWidget *w;
  int i;

  if (s->saving_p) return False;
  s->saving_p = True;

  *p2 = *p;

  /* Flush any checkbox changes in the list down into the prefs struct.
   */
  for (i = 0; kids; kids = kids->next, i++)
    {
      GtkWidget *line = GTK_WIDGET (kids->data);
      GtkWidget *line_hbox = GTK_WIDGET (GTK_BIN (line)->child);
      GtkWidget *line_check =
        GTK_WIDGET (gtk_container_children (GTK_CONTAINER (line_hbox))->data);
      Bool checked =
        gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (line_check));

      if (flush_changes (s, i, (checked ? 1 : 0), 0, 0))
        changed = True;
    }


  /* Flush the non-hack-specific settings down into the prefs struct.
   */

# define SECONDS(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    hack_time_text (s, gtk_entry_get_text (GTK_ENTRY (w)), (FIELD), True)

# define MINUTES(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    hack_time_text (s, gtk_entry_get_text (GTK_ENTRY (w)), (FIELD), False)

# define CHECKBOX(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    (FIELD) = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w))

# define PATHNAME(FIELD,NAME) \
    w = name_to_widget (s, (NAME)); \
    (FIELD) = normalize_directory (gtk_entry_get_text (GTK_ENTRY (w)))

  MINUTES  (&p2->timeout,         "timeout_spinbutton");
  MINUTES  (&p2->cycle,           "cycle_spinbutton");
  CHECKBOX (p2->lock_p,           "lock_button");
  MINUTES  (&p2->lock_timeout,    "lock_spinbutton");

  CHECKBOX (p2->dpms_enabled_p,  "dpms_button");
  MINUTES  (&p2->dpms_standby,    "dpms_standby_spinbutton");
  MINUTES  (&p2->dpms_suspend,    "dpms_suspend_spinbutton");
  MINUTES  (&p2->dpms_off,        "dpms_off_spinbutton");

  CHECKBOX (p2->grab_desktop_p,   "grab_desk_button");
  CHECKBOX (p2->grab_video_p,     "grab_video_button");
  CHECKBOX (p2->random_image_p,   "grab_image_button");
  PATHNAME (p2->image_directory,  "image_text");

  CHECKBOX (p2->verbose_p,        "verbose_button");
  CHECKBOX (p2->capture_stderr_p, "capture_button");
  CHECKBOX (p2->splash_p,         "splash_button");

  CHECKBOX (p2->install_cmap_p,   "install_button");
  CHECKBOX (p2->fade_p,           "fade_button");
  CHECKBOX (p2->unfade_p,         "unfade_button");
  SECONDS  (&p2->fade_seconds,    "fade_spinbutton");

# undef SECONDS
# undef MINUTES
# undef CHECKBOX
# undef PATHNAME

  /* Warn if the image directory doesn't exist.
   */
  if (p2->image_directory &&
      *p2->image_directory &&
      !directory_p (p2->image_directory))
    {
      char b[255];
      sprintf (b, "Error:\n\n" "Directory does not exist: \"%s\"\n",
               p2->image_directory);
      warning_dialog (s->toplevel_widget, b, False, 100);
    }


  /* Map the mode menu to `saver_mode' enum values. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));
    GtkMenu *menu = GTK_MENU (gtk_option_menu_get_menu (opt));
    GtkWidget *selected = gtk_menu_get_active (menu);
    GList *kids = gtk_container_children (GTK_CONTAINER (menu));
    int menu_elt = g_list_index (kids, (gpointer) selected);
    if (menu_elt < 0 || menu_elt >= countof(mode_menu_order)) abort();
    p2->mode = mode_menu_order[menu_elt];
  }

  if (p2->mode == ONE_HACK)
    {
      int list_elt = selected_list_element (s);
      p2->selected_hack = (list_elt >= 0
                           ? s->list_elt_to_hack_number[list_elt]
                           : -1);
    }

# define COPY(field, name) \
  if (p->field != p2->field) { \
    changed = True; \
    if (s->debug_p) \
      fprintf (stderr, "%s: %s => %d\n", blurb(), name, p2->field); \
  } \
  p->field = p2->field

  COPY(mode,             "mode");
  COPY(selected_hack,    "selected_hack");

  COPY(timeout,        "timeout");
  COPY(cycle,          "cycle");
  COPY(lock_p,         "lock_p");
  COPY(lock_timeout,   "lock_timeout");

  COPY(dpms_enabled_p, "dpms_enabled_p");
  COPY(dpms_standby,   "dpms_standby");
  COPY(dpms_suspend,   "dpms_suspend");
  COPY(dpms_off,       "dpms_off");

  COPY(verbose_p,        "verbose_p");
  COPY(capture_stderr_p, "capture_stderr_p");
  COPY(splash_p,         "splash_p");

  COPY(install_cmap_p,   "install_cmap_p");
  COPY(fade_p,           "fade_p");
  COPY(unfade_p,         "unfade_p");
  COPY(fade_seconds,     "fade_seconds");

  COPY(grab_desktop_p, "grab_desktop_p");
  COPY(grab_video_p,   "grab_video_p");
  COPY(random_image_p, "random_image_p");

# undef COPY

  if (!p->image_directory ||
      !p2->image_directory ||
      strcmp(p->image_directory, p2->image_directory))
    {
      changed = True;
      if (s->debug_p)
        fprintf (stderr, "%s: image_directory => \"%s\"\n",
                 blurb(), p2->image_directory);
    }
  if (p->image_directory && p->image_directory != p2->image_directory)
    free (p->image_directory);
  p->image_directory = p2->image_directory;
  p2->image_directory = 0;

  populate_prefs_page (s);

  if (changed)
    {
      Display *dpy = GDK_DISPLAY();
      Bool enabled_p = (p->dpms_enabled_p && p->mode != DONT_BLANK);
      sync_server_dpms_settings (dpy, enabled_p,
                                 p->dpms_standby / 1000,
                                 p->dpms_suspend / 1000,
                                 p->dpms_off / 1000,
                                 False);

      changed = demo_write_init_file (s, p);
    }

  s->saving_p = False;
  return changed;
}


/* Flush out any changes made in the popup dialog box (where changes
   take place only when the OK button is clicked.)
 */
static Bool
flush_popup_changes_and_save (state *s)
{
  Bool changed = False;
  saver_preferences *p = &s->prefs;
  int list_elt = selected_list_element (s);

  GtkEntry *cmd = GTK_ENTRY (name_to_widget (s, "cmd_text"));
  GtkCombo *vis = GTK_COMBO (name_to_widget (s, "visual_combo"));

  const char *visual = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (vis)->entry));
  const char *command = gtk_entry_get_text (cmd);

  char c;
  unsigned long id;

  if (s->saving_p) return False;
  s->saving_p = True;

  if (list_elt < 0)
    goto DONE;

  if (maybe_reload_init_file (s) != 0)
    {
      changed = True;
      goto DONE;
    }

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
      gdk_beep ();				  /* unparsable */
      visual = "";
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry), "Any");
    }

  changed = flush_changes (s, list_elt, -1, command, visual);
  if (changed)
    {
      changed = demo_write_init_file (s, p);

      /* Do this to re-launch the hack if (and only if) the command line
         has changed. */
      populate_demo_window (s, selected_list_element (s));
    }

 DONE:
  s->saving_p = False;
  return changed;
}




void
pref_changed_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  if (! s->initializing_p)
    {
      s->initializing_p = True;
      flush_dialog_changes_and_save (s);
      s->initializing_p = False;
    }
}


/* Callback on menu items in the "mode" options menu.
 */
void
mode_menu_item_cb (GtkWidget *widget, gpointer user_data)
{
  state *s = (state *) user_data;
  saver_preferences *p = &s->prefs;
  GtkList *list = GTK_LIST (name_to_widget (s, "list"));
  int list_elt;

  GList *menu_items = gtk_container_children (GTK_CONTAINER (widget->parent));
  int menu_index = 0;
  saver_mode new_mode;
  int old_selected = p->selected_hack;

  while (menu_items)
    {
      if (menu_items->data == widget)
        break;
      menu_index++;
      menu_items = menu_items->next;
    }
  if (!menu_items) abort();

  new_mode = mode_menu_order[menu_index];

  /* Keep the same list element displayed as before; except if we're
     switching *to* "one screensaver" mode from any other mode, scroll
     to and select "the one".
   */
  list_elt = -1;
  if (new_mode == ONE_HACK)
    list_elt = (p->selected_hack >= 0
                ? s->hack_number_to_list_elt[p->selected_hack]
                : -1);

  if (list_elt < 0)
    list_elt = selected_list_element (s);

  {
    saver_mode old_mode = p->mode;
    p->mode = new_mode;
    populate_demo_window (s, list_elt);
    force_list_select_item (s, list, list_elt, True);
    p->mode = old_mode;  /* put it back, so the init file gets written */
  }

  pref_changed_cb (widget, user_data);

  if (old_selected != p->selected_hack)
    abort();    /* dammit, not again... */
}


void
switch_page_cb (GtkNotebook *notebook, GtkNotebookPage *page,
                gint page_num, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  pref_changed_cb (GTK_WIDGET (notebook), user_data);

  /* If we're switching to page 0, schedule the current hack to be run.
     Otherwise, schedule it to stop. */
  if (page_num == 0)
    populate_demo_window (s, selected_list_element (s));
  else
    schedule_preview (s, 0);
}


static time_t last_doubleclick_time = 0;   /* FMH!  This is to suppress the
                                              list_select_cb that comes in
                                              *after* we've double-clicked.
                                            */

static gint
list_doubleclick_cb (GtkWidget *button, GdkEventButton *event,
                     gpointer data)
{
  state *s = (state *) data;
  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkList *list = GTK_LIST (name_to_widget (s, "list"));
      int list_elt = gtk_list_child_position (list, GTK_WIDGET (button));

      last_doubleclick_time = time ((time_t *) 0);

      if (list_elt >= 0)
        run_hack (s, list_elt, True);
    }

  return FALSE;
}


static void
list_select_cb (GtkList *list, GtkWidget *child, gpointer data)
{
  state *s = (state *) data;
  time_t now = time ((time_t *) 0);

  if (now >= last_doubleclick_time + 2)
    {
      int list_elt = gtk_list_child_position (list, GTK_WIDGET (child));
      populate_demo_window (s, list_elt);
      flush_dialog_changes_and_save (s);
    }
}

static void
list_unselect_cb (GtkList *list, GtkWidget *child, gpointer data)
{
  state *s = (state *) data;
  populate_demo_window (s, -1);
  flush_dialog_changes_and_save (s);
}


/* Called when the checkboxes that are in the left column of the
   scrolling list are clicked.  This both populates the right pane
   (just as clicking on the label (really, listitem) does) and
   also syncs this checkbox with  the right pane Enabled checkbox.
 */
static void
list_checkbox_cb (GtkWidget *cb, gpointer data)
{
  state *s = (state *) data;

  GtkWidget *line_hbox = GTK_WIDGET (cb)->parent;
  GtkWidget *line = GTK_WIDGET (line_hbox)->parent;

  GtkList *list = GTK_LIST (GTK_WIDGET (line)->parent);
  GtkViewport *vp = GTK_VIEWPORT (GTK_WIDGET (list)->parent);
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
  GtkAdjustment *adj;
  double scroll_top;

  int list_elt = gtk_list_child_position (list, line);

  /* remember previous scroll position of the top of the list */
  adj = gtk_scrolled_window_get_vadjustment (scroller);
  scroll_top = adj->value;

  flush_dialog_changes_and_save (s);
  force_list_select_item (s, list, list_elt, False);
  populate_demo_window (s, list_elt);
  
  /* restore the previous scroll position of the top of the list.
     this is weak, but I don't really know why it's moving... */
  gtk_adjustment_set_value (adj, scroll_top);
}


typedef struct {
  state *state;
  GtkFileSelection *widget;
} file_selection_data;



static void
store_image_directory (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  state *s = fsd->state;
  GtkFileSelection *selector = fsd->widget;
  GtkWidget *top = s->toplevel_widget;
  saver_preferences *p = &s->prefs;
  char *path = gtk_file_selection_get_filename (selector);

  if (p->image_directory && !strcmp(p->image_directory, path))
    return;  /* no change */

  if (!directory_p (path))
    {
      char b[255];
      sprintf (b, "Error:\n\n" "Directory does not exist: \"%s\"\n", path);
      warning_dialog (GTK_WIDGET (top), b, False, 100);
      return;
    }

  if (p->image_directory) free (p->image_directory);
  p->image_directory = normalize_directory (path);

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  demo_write_init_file (s, p);
}


static void
browse_image_dir_cancel (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  gtk_widget_hide (GTK_WIDGET (fsd->widget));
}

static void
browse_image_dir_ok (GtkWidget *button, gpointer user_data)
{
  browse_image_dir_cancel (button, user_data);
  store_image_directory (button, user_data);
}

static void
browse_image_dir_close (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  browse_image_dir_cancel (widget, user_data);
}


void
browse_image_dir_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  saver_preferences *p = &s->prefs;
  static file_selection_data *fsd = 0;

  GtkFileSelection *selector = GTK_FILE_SELECTION(
    gtk_file_selection_new ("Please select the image directory."));

  if (!fsd)
    fsd = (file_selection_data *) malloc (sizeof (*fsd));  

  fsd->widget = selector;
  fsd->state = s;

  if (p->image_directory && *p->image_directory)
    gtk_file_selection_set_filename (selector, p->image_directory);

  gtk_signal_connect (GTK_OBJECT (selector->ok_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_ok),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC (browse_image_dir_cancel),
                      (gpointer *) fsd);
  gtk_signal_connect (GTK_OBJECT (selector), "delete_event",
                      GTK_SIGNAL_FUNC (browse_image_dir_close),
                      (gpointer *) fsd);

  gtk_widget_set_sensitive (GTK_WIDGET (selector->file_list), False);

  gtk_window_set_modal (GTK_WINDOW (selector), True);
  gtk_widget_show (GTK_WIDGET (selector));
}


void
settings_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  int list_elt = selected_list_element (s);

  populate_demo_window (s, list_elt);   /* reset the widget */
  populate_popup_window (s);		/* create UI on popup window */
  gtk_widget_show (s->popup_widget);
}

static void
settings_sync_cmd_text (state *s)
{
# ifdef HAVE_XML
  GtkWidget *cmd = GTK_WIDGET (name_to_widget (s, "cmd_text"));
  char *cmd_line = get_configurator_command_line (s->cdata);
  gtk_entry_set_text (GTK_ENTRY (cmd), cmd_line);
  gtk_entry_set_position (GTK_ENTRY (cmd), strlen (cmd_line));
  free (cmd_line);
# endif /* HAVE_XML */
}

void
settings_adv_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook =
    GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));

  settings_sync_cmd_text (s);
  gtk_notebook_set_page (notebook, 1);
}

void
settings_std_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook =
    GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));

  /* Re-create UI to reflect the in-progress command-line settings. */
  populate_popup_window (s);

  gtk_notebook_set_page (notebook, 0);
}

void
settings_switch_page_cb (GtkNotebook *notebook, GtkNotebookPage *page,
                         gint page_num, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkWidget *adv = name_to_widget (s, "adv_button");
  GtkWidget *std = name_to_widget (s, "std_button");

  if (page_num == 0)
    {
      gtk_widget_show (adv);
      gtk_widget_hide (std);
    }
  else if (page_num == 1)
    {
      gtk_widget_hide (adv);
      gtk_widget_show (std);
    }
  else
    abort();
}



void
settings_cancel_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  gtk_widget_hide (s->popup_widget);
}

void
settings_ok_cb (GtkButton *button, gpointer user_data)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  GtkNotebook *notebook = GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));
  int page = gtk_notebook_get_current_page (notebook);

  if (page == 0)
    /* Regenerate the command-line from the widget contents before saving.
       But don't do this if we're looking at the command-line page already,
       or we will blow away what they typed... */
    settings_sync_cmd_text (s);

  flush_popup_changes_and_save (s);
  gtk_widget_hide (s->popup_widget);
}

static void
wm_popup_close_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  state *s = (state *) data;
  settings_cancel_cb (0, (gpointer) s);
}



/* Populating the various widgets
 */


/* Returns the number of the last hack run by the server.
 */
static int
server_current_hack (void)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  CARD32 *data = 0;
  Display *dpy = GDK_DISPLAY();
  int hack_number = -1;

  if (XGetWindowProperty (dpy, RootWindow (dpy, 0), /* always screen #0 */
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          (unsigned char **) &data)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && data)
    hack_number = (int) data[2] - 1;

  if (data) free (data);

  return hack_number;
}


/* Finds the number of the last hack to run, and makes that item be
   selected by default.
 */
static void
scroll_to_current_hack (state *s)
{
  saver_preferences *p = &s->prefs;
  int hack_number;

  if (p->mode == ONE_HACK)
    hack_number = p->selected_hack;
  else
    hack_number = server_current_hack ();

  if (hack_number >= 0 && hack_number < p->screenhacks_count)
    {
      int list_elt = s->hack_number_to_list_elt[hack_number];
      GtkList *list = GTK_LIST (name_to_widget (s, "list"));
      force_list_select_item (s, list, list_elt, True);
      populate_demo_window (s, list_elt);
    }
}


static void
populate_hack_list (state *s)
{
  saver_preferences *p = &s->prefs;
  GtkList *list = GTK_LIST (name_to_widget (s, "list"));
  int i;
  for (i = 0; i < p->screenhacks_count; i++)
    {
      screenhack *hack = p->screenhacks[s->list_elt_to_hack_number[i]];

      /* A GtkList must contain only GtkListItems, but those can contain
         an arbitrary widget.  We add an Hbox, and inside that, a Checkbox
         and a Label.  We handle single and double click events on the
         line itself, for clicking on the text, but the interior checkbox
         also handles its own events.
       */
      GtkWidget *line;
      GtkWidget *line_hbox;
      GtkWidget *line_check;
      GtkWidget *line_label;

      char *pretty_name = (hack->name
                           ? strdup (hack->name)
                           : make_hack_name (hack->command));

      line = gtk_list_item_new ();
      line_hbox = gtk_hbox_new (FALSE, 0);
      line_check = gtk_check_button_new ();
      line_label = gtk_label_new (pretty_name);

      gtk_container_add (GTK_CONTAINER (line), line_hbox);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_check, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_label, FALSE, FALSE, 0);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (line_check),
                                    hack->enabled_p);
      gtk_label_set_justify (GTK_LABEL (line_label), GTK_JUSTIFY_LEFT);

      gtk_widget_show (line_check);
      gtk_widget_show (line_label);
      gtk_widget_show (line_hbox);
      gtk_widget_show (line);

      free (pretty_name);

      gtk_container_add (GTK_CONTAINER (list), line);
      gtk_signal_connect (GTK_OBJECT (line), "button_press_event",
                          GTK_SIGNAL_FUNC (list_doubleclick_cb),
                          (gpointer) s);

      gtk_signal_connect (GTK_OBJECT (line_check), "toggled",
                          GTK_SIGNAL_FUNC (list_checkbox_cb),
                          (gpointer) s);

#if 0 /* #### */
      GTK_WIDGET (GTK_BIN(line)->child)->style =
        gtk_style_copy (GTK_WIDGET (text_line)->style);
#endif
      gtk_widget_show (line);
    }

  gtk_signal_connect (GTK_OBJECT (list), "select_child",
                      GTK_SIGNAL_FUNC (list_select_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (list), "unselect_child",
                      GTK_SIGNAL_FUNC (list_unselect_cb),
                      (gpointer) s);
}


static void
update_list_sensitivity (state *s)
{
  saver_preferences *p = &s->prefs;
  Bool sensitive = (p->mode == RANDOM_HACKS || p->mode == ONE_HACK);
  Bool checkable = (p->mode == RANDOM_HACKS);
  Bool blankable = (p->mode != DONT_BLANK);

  GtkWidget *head     = name_to_widget (s, "col_head_hbox");
  GtkWidget *use      = name_to_widget (s, "use_col_frame");
  GtkWidget *scroller = name_to_widget (s, "scroller");
  GtkWidget *buttons  = name_to_widget (s, "next_prev_hbox");
  GtkWidget *blanker  = name_to_widget (s, "blanking_table");

  GtkList *list = GTK_LIST (name_to_widget (s, "list"));
  GList *kids   = gtk_container_children (GTK_CONTAINER (list));

  gtk_widget_set_sensitive (GTK_WIDGET (head),     sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (scroller), sensitive);
  gtk_widget_set_sensitive (GTK_WIDGET (buttons),  sensitive);

  gtk_widget_set_sensitive (GTK_WIDGET (blanker),  blankable);

  if (checkable)
    gtk_widget_show (use);   /* the "Use" column header */
  else
    gtk_widget_hide (use);

  while (kids)
    {
      GtkBin *line = GTK_BIN (kids->data);
      GtkContainer *line_hbox = GTK_CONTAINER (line->child);
      GtkWidget *line_check =
        GTK_WIDGET (gtk_container_children (line_hbox)->data);
      
      if (checkable)
        gtk_widget_show (line_check);
      else
        gtk_widget_hide (line_check);

      kids = kids->next;
    }
}


static void
populate_prefs_page (state *s)
{
  saver_preferences *p = &s->prefs;
  char str[100];

  /* The file supports timeouts of less than a minute, but the GUI does
     not, so throttle the values to be at least one minute (since "0" is
     a bad rounding choice...)
   */
# define THROTTLE(NAME) if (p->NAME != 0 && p->NAME < 60000) p->NAME = 60000
  THROTTLE (timeout);
  THROTTLE (cycle);
  THROTTLE (passwd_timeout);
# undef THROTTLE

# define FMT_MINUTES(NAME,N) \
    sprintf (str, "%d", (((N / 1000) + 59) / 60)); \
    gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, (NAME))), str)

# define FMT_SECONDS(NAME,N) \
    sprintf (str, "%d", ((N) / 1000)); \
    gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, (NAME))), str)

  FMT_MINUTES ("timeout_spinbutton",      p->timeout);
  FMT_MINUTES ("cycle_spinbutton",        p->cycle);
  FMT_MINUTES ("lock_spinbutton",         p->lock_timeout);
  FMT_MINUTES ("dpms_standby_spinbutton", p->dpms_standby);
  FMT_MINUTES ("dpms_suspend_spinbutton", p->dpms_suspend);
  FMT_MINUTES ("dpms_off_spinbutton",     p->dpms_off);
  FMT_SECONDS ("fade_spinbutton",         p->fade_seconds);

# undef FMT_MINUTES
# undef FMT_SECONDS

# define TOGGLE_ACTIVE(NAME,ACTIVEP) \
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (name_to_widget (s,(NAME))),\
                                (ACTIVEP))

  TOGGLE_ACTIVE ("lock_button",       p->lock_p);
  TOGGLE_ACTIVE ("verbose_button",    p->verbose_p);
  TOGGLE_ACTIVE ("capture_button",    p->capture_stderr_p);
  TOGGLE_ACTIVE ("splash_button",     p->splash_p);
  TOGGLE_ACTIVE ("dpms_button",       p->dpms_enabled_p);
  TOGGLE_ACTIVE ("grab_desk_button",  p->grab_desktop_p);
  TOGGLE_ACTIVE ("grab_video_button", p->grab_video_p);
  TOGGLE_ACTIVE ("grab_image_button", p->random_image_p);
  TOGGLE_ACTIVE ("install_button",    p->install_cmap_p);
  TOGGLE_ACTIVE ("fade_button",       p->fade_p);
  TOGGLE_ACTIVE ("unfade_button",     p->unfade_p);

# undef TOGGLE_ACTIVE

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (s, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  gtk_widget_set_sensitive (name_to_widget (s, "image_text"),
                            p->random_image_p);
  gtk_widget_set_sensitive (name_to_widget (s, "image_browse_button"),
                            p->random_image_p);

  /* Map the `saver_mode' enum to mode menu to values. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));

    int i;
    for (i = 0; i < countof(mode_menu_order); i++)
      if (mode_menu_order[i] == p->mode)
        break;
    gtk_option_menu_set_history (opt, i);
    update_list_sensitivity (s);
  }

  {
    Bool found_any_writable_cells = False;
    Bool dpms_supported = False;

    Display *dpy = GDK_DISPLAY();
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

#ifdef HAVE_DPMS_EXTENSION
    {
      int op = 0, event = 0, error = 0;
      if (XQueryExtension (dpy, "DPMS", &op, &event, &error))
        dpms_supported = True;
    }
#endif /* HAVE_DPMS_EXTENSION */


# define SENSITIZE(NAME,SENSITIVEP) \
    gtk_widget_set_sensitive (name_to_widget (s, (NAME)), (SENSITIVEP))

    /* Blanking and Locking
     */
    SENSITIZE ("lock_spinbutton", p->lock_p);
    SENSITIZE ("lock_mlabel",     p->lock_p);

    /* DPMS
     */
    SENSITIZE ("dpms_frame",              dpms_supported);
    SENSITIZE ("dpms_button",             dpms_supported);
    SENSITIZE ("dpms_standby_label",      dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_standby_mlabel",     dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_standby_spinbutton", dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_label",      dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_mlabel",     dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_suspend_spinbutton", dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_label",          dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_mlabel",         dpms_supported && p->dpms_enabled_p);
    SENSITIZE ("dpms_off_spinbutton",     dpms_supported && p->dpms_enabled_p);

    /* Colormaps
     */
    SENSITIZE ("cmap_frame",      found_any_writable_cells);
    SENSITIZE ("install_button",  found_any_writable_cells);
    SENSITIZE ("fade_button",     found_any_writable_cells);
    SENSITIZE ("unfade_button",   found_any_writable_cells);

    SENSITIZE ("fade_label",      (found_any_writable_cells &&
                                   (p->fade_p || p->unfade_p)));
    SENSITIZE ("fade_spinbutton", (found_any_writable_cells &&
                                   (p->fade_p || p->unfade_p)));

# undef SENSITIZE
  }
}


static void
populate_popup_window (state *s)
{
  saver_preferences *p = &s->prefs;
  GtkWidget *parent = name_to_widget (s, "settings_vbox");
  GtkLabel *doc = GTK_LABEL (name_to_widget (s, "doc"));
  int list_elt = selected_list_element (s);
  int hack_number = (list_elt >= 0 && list_elt < p->screenhacks_count
                     ? s->list_elt_to_hack_number[list_elt]
                     : -1);
  screenhack *hack = (hack_number >= 0 ? p->screenhacks[hack_number] : 0);
  char *doc_string = 0;

  /* #### not in Gtk 1.2
  gtk_label_set_selectable (doc);
   */

# ifdef HAVE_XML
  if (s->cdata)
    {
      free_conf_data (s->cdata);
      s->cdata = 0;
    }

  if (hack)
    {
      GtkWidget *cmd = GTK_WIDGET (name_to_widget (s, "cmd_text"));
      const char *cmd_line = gtk_entry_get_text (GTK_ENTRY (cmd));
      s->cdata = load_configurator (cmd_line, s->debug_p);
      if (s->cdata && s->cdata->widget)
        gtk_box_pack_start (GTK_BOX (parent), s->cdata->widget, TRUE, TRUE, 0);
    }

  doc_string = (s->cdata
                ? s->cdata->description
                : 0);
# else  /* !HAVE_XML */
  doc_string = "Descriptions not available: no XML support compiled in.";
# endif /* !HAVE_XML */

  gtk_label_set_text (doc, (doc_string
                            ? doc_string
                            : "No description available."));
}


static void
sensitize_demo_widgets (state *s, Bool sensitive_p)
{
  const char *names1[] = { "demo", "settings" };
  const char *names2[] = { "cmd_label", "cmd_text", "manual",
                           "visual", "visual_combo" };
  int i;
  for (i = 0; i < countof(names1); i++)
    {
      GtkWidget *w = name_to_widget (s, names1[i]);
      gtk_widget_set_sensitive (GTK_WIDGET(w), sensitive_p);
    }
  for (i = 0; i < countof(names2); i++)
    {
      GtkWidget *w = name_to_widget (s, names2[i]);
      gtk_widget_set_sensitive (GTK_WIDGET(w), sensitive_p);
    }
}


/* Even though we've given these text fields a maximum number of characters,
   their default size is still about 30 characters wide -- so measure out
   a string in their font, and resize them to just fit that.
 */
static void
fix_text_entry_sizes (state *s)
{
  const char * const spinbuttons[] = {
    "timeout_spinbutton", "cycle_spinbutton", "lock_spinbutton",
    "dpms_standby_spinbutton", "dpms_suspend_spinbutton",
    "dpms_off_spinbutton",
    "-fade_spinbutton" };
  int i;
  int width = 0;
  GtkWidget *w;

  for (i = 0; i < countof(spinbuttons); i++)
    {
      const char *n = spinbuttons[i];
      int cols = 4;
      while (*n == '-') n++, cols--;
      w = GTK_WIDGET (name_to_widget (s, n));
      width = gdk_text_width (w->style->font, "MMMMMMMM", cols);
      gtk_widget_set_usize (w, width, -2);
    }

  /* Now fix the width of the combo box.
   */
  w = GTK_WIDGET (name_to_widget (s, "visual_combo"));
  w = GTK_COMBO (w)->entry;
  width = gdk_string_width (w->style->font, "PseudoColor___");
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the width of the file entry text.
   */
  w = GTK_WIDGET (name_to_widget (s, "image_text"));
  width = gdk_string_width (w->style->font, "mmmmmmmmmmmmmm");
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the width of the command line text.
   */
  w = GTK_WIDGET (name_to_widget (s, "cmd_text"));
  width = gdk_string_width (w->style->font, "mmmmmmmmmmmmmmmmmmmm");
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the height of the list.
   */
  {
    int lines = 10;
    int height;
    int leading = 3;  /* approximate is ok... */
    int border = 2;
    w = GTK_WIDGET (name_to_widget (s, "list"));
    height = w->style->font->ascent + w->style->font->descent;
    height += leading;
    height *= lines;
    height += border * 2;
    w = GTK_WIDGET (name_to_widget (s, "scroller"));
    gtk_widget_set_usize (w, -2, height);
  }
}




/* Pixmaps for the up and down arrow buttons (yeah, this is sleazy...)
 */

static char *up_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None",
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
  "               ",

  /* Need these here because gdk_pixmap_create_from_xpm_d() walks off
     the end of the array (Gtk 1.2.5.) */
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
};

static char *down_arrow_xpm[] = {
  "15 15 4 1",
  " 	c None",
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
  "       @       ",

  /* Need these here because gdk_pixmap_create_from_xpm_d() walks off
     the end of the array (Gtk 1.2.5.) */
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000",
  "\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000"
};

static void
pixmapify_button (state *s, int down_p)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkWidget *pixmapwid;
  GtkStyle *style;
  GtkWidget *w;

  w = GTK_WIDGET (name_to_widget (s, (down_p ? "next" : "prev")));
  style = gtk_widget_get_style (w);
  mask = 0;
  pixmap = gdk_pixmap_create_from_xpm_d (w->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         (down_p
                                          ? (gchar **) down_arrow_xpm
                                          : (gchar **) up_arrow_xpm));
  pixmapwid = gtk_pixmap_new (pixmap, mask);
  gtk_widget_show (pixmapwid);
  gtk_container_remove (GTK_CONTAINER (w), GTK_BIN (w)->child);
  gtk_container_add (GTK_CONTAINER (w), pixmapwid);
}

static void
map_next_button_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  pixmapify_button (s, 1);
}

static void
map_prev_button_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  pixmapify_button (s, 0);
}



/* Work around a Gtk bug that causes label widgets to wrap text too early.
 */

static void
you_are_not_a_unique_or_beautiful_snowflake (GtkWidget *label,
                                             GtkAllocation *allocation,
					     void *foo)
{
  GtkRequisition req;
  GtkWidgetAuxInfo *aux_info;

  aux_info = gtk_object_get_data (GTK_OBJECT (label), "gtk-aux-info");

  aux_info->width = allocation->width;
  aux_info->height = -2;
  aux_info->x = -1;
  aux_info->y = -1;

  gtk_widget_size_request (label, &req);
}


/* Feel the love.  Thanks to Nat Friedman for finding this workaround.
 */
static void
eschew_gtk_lossage (GtkLabel *label)
{
  GtkWidgetAuxInfo *aux_info = g_new0 (GtkWidgetAuxInfo, 1);
  aux_info->width = GTK_WIDGET (label)->allocation.width;
  aux_info->height = -2;
  aux_info->x = -1;
  aux_info->y = -1;

  gtk_object_set_data (GTK_OBJECT (label), "gtk-aux-info", aux_info);

  gtk_signal_connect (GTK_OBJECT (label), "size_allocate",
                      you_are_not_a_unique_or_beautiful_snowflake,
                      0);

  gtk_widget_set_usize (GTK_WIDGET (label), -2, -2);

  gtk_widget_queue_resize (GTK_WIDGET (label));
}


static void
populate_demo_window (state *s, int list_elt)
{
  saver_preferences *p = &s->prefs;
  screenhack *hack;
  char *pretty_name;
  GtkFrame *frame1 = GTK_FRAME (name_to_widget (s, "preview_frame"));
  GtkFrame *frame2 = GTK_FRAME (name_to_widget (s, "doc_frame"));
  GtkEntry *cmd    = GTK_ENTRY (name_to_widget (s, "cmd_text"));
  GtkCombo *vis    = GTK_COMBO (name_to_widget (s, "visual_combo"));
  GtkWidget *list  = GTK_WIDGET (name_to_widget (s, "list"));

  if (p->mode == BLANK_ONLY)
    {
      hack = 0;
      pretty_name = strdup ("Blank Screen");
      schedule_preview (s, 0);
    }
  else if (p->mode == DONT_BLANK)
    {
      hack = 0;
      pretty_name = strdup ("Screen Saver Disabled");
      schedule_preview (s, 0);
    }
  else
    {
      int hack_number = (list_elt >= 0 && list_elt < p->screenhacks_count
                         ? s->list_elt_to_hack_number[list_elt]
                         : -1);
      hack = (hack_number >= 0 ? p->screenhacks[hack_number] : 0);

      pretty_name = (hack
                     ? (hack->name
                        ? strdup (hack->name)
                        : make_hack_name (hack->command))
                     : 0);

      if (hack)
        schedule_preview (s, hack->command);
      else
        schedule_preview (s, 0);
    }

  if (!pretty_name)
    pretty_name = strdup ("Preview");

  gtk_frame_set_label (frame1, pretty_name);
  gtk_frame_set_label (frame2, pretty_name);

  gtk_entry_set_text (cmd, (hack ? hack->command : ""));
  gtk_entry_set_position (cmd, 0);

  {
    char title[255];
    sprintf (title, "%s: %.100s Settings",
             progclass, (pretty_name ? pretty_name : "???"));
    gtk_window_set_title (GTK_WINDOW (s->popup_widget), title);
  }

  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry),
                      (hack
                       ? (hack->visual && *hack->visual
                          ? hack->visual
                          : "Any")
                       : ""));

  sensitize_demo_widgets (s, (hack ? True : False));

  if (pretty_name) free (pretty_name);

  ensure_selected_item_visible (list);

  s->_selected_list_element = list_elt;
}


static void
widget_deleter (GtkWidget *widget, gpointer data)
{
  /* #### Well, I want to destroy these widgets, but if I do that, they get
     referenced again, and eventually I get a SEGV.  So instead of
     destroying them, I'll just hide them, and leak a bunch of memory
     every time the disk file changes.  Go go go Gtk!

     #### Ok, that's a lie, I get a crash even if I just hide the widget
     and don't ever delete it.  Fuck!
   */
#if 0
  gtk_widget_destroy (widget);
#else
  gtk_widget_hide (widget);
#endif
}


static char **sort_hack_cmp_names_kludge;
static int
sort_hack_cmp (const void *a, const void *b)
{
  if (a == b)
    return 0;
  else
    return strcmp (sort_hack_cmp_names_kludge[*(int *) a],
                   sort_hack_cmp_names_kludge[*(int *) b]);
}


static void
initialize_sort_map (state *s)
{
  saver_preferences *p = &s->prefs;
  int i;

  if (s->list_elt_to_hack_number) free (s->list_elt_to_hack_number);
  if (s->hack_number_to_list_elt) free (s->hack_number_to_list_elt);

  s->list_elt_to_hack_number = (int *)
    calloc (sizeof(int), p->screenhacks_count + 1);
  s->hack_number_to_list_elt = (int *)
    calloc (sizeof(int), p->screenhacks_count + 1);

  /* Initialize table to 1:1 mapping */
  for (i = 0; i < p->screenhacks_count; i++)
    s->list_elt_to_hack_number[i] = i;

  /* Generate list of names (once)
   */
  sort_hack_cmp_names_kludge = (char **)
    calloc (sizeof(char *), p->screenhacks_count);
  for (i = 0; i < p->screenhacks_count; i++)
    {
      screenhack *hack = p->screenhacks[i];
      char *name = (hack->name && *hack->name
                    ? strdup (hack->name)
                    : make_hack_name (hack->command));
      char *str;
      for (str = name; *str; str++)
        *str = tolower(*str);
      sort_hack_cmp_names_kludge[i] = name;
    }

  /* Sort alphabetically
   */
  qsort (s->list_elt_to_hack_number,
         p->screenhacks_count,
         sizeof(*s->list_elt_to_hack_number),
         sort_hack_cmp);

  /* Free names
   */
  for (i = 0; i < p->screenhacks_count; i++)
    free (sort_hack_cmp_names_kludge[i]);
  free (sort_hack_cmp_names_kludge);
  sort_hack_cmp_names_kludge = 0;

  /* Build inverse table */
  for (i = 0; i < p->screenhacks_count; i++)
    s->hack_number_to_list_elt[s->list_elt_to_hack_number[i]] = i;
}


static int
maybe_reload_init_file (state *s)
{
  saver_preferences *p = &s->prefs;
  int status = 0;

  static Bool reentrant_lock = False;
  if (reentrant_lock) return 0;
  reentrant_lock = True;

  if (init_file_changed_p (p))
    {
      const char *f = init_file_name();
      char *b;
      int list_elt;
      GtkList *list;

      if (!f || !*f) return 0;
      b = (char *) malloc (strlen(f) + 1024);
      sprintf (b,
               "Warning:\n\n"
               "file \"%s\" has changed, reloading.\n",
               f);
      warning_dialog (s->toplevel_widget, b, False, 100);
      free (b);

      load_init_file (p);
      initialize_sort_map (s);

      list_elt = selected_list_element (s);
      list = GTK_LIST (name_to_widget (s, "list"));
      gtk_container_foreach (GTK_CONTAINER (list), widget_deleter, NULL);
      populate_hack_list (s);
      force_list_select_item (s, list, list_elt, True);
      populate_prefs_page (s);
      populate_demo_window (s, list_elt);
      ensure_selected_item_visible (GTK_WIDGET (list));

      status = 1;
    }

  reentrant_lock = False;
  return status;
}



/* Making the preview window have the right X visual (so that GL works.)
 */

static Visual *get_best_gl_visual (state *);

static GdkVisual *
x_visual_to_gdk_visual (Visual *xv)
{
  GList *gvs = gdk_list_visuals();
  if (!xv) return gdk_visual_get_system();
  for (; gvs; gvs = gvs->next)
    {
      GdkVisual *gv = (GdkVisual *) gvs->data;
      if (xv == GDK_VISUAL_XVISUAL (gv))
        return gv;
    }
  fprintf (stderr, "%s: couldn't convert X Visual 0x%lx to a GdkVisual\n",
           blurb(), (unsigned long) xv->visualid);
  abort();
}

static void
clear_preview_window (state *s)
{
  GtkWidget *p;
  GdkWindow *window;

  if (!s->toplevel_widget) return;  /* very early */
  p = name_to_widget (s, "preview");
  window = p->window;

  if (!window) return;

  /* Flush the widget background down into the window, in case a subproc
     has changed it. */
  gdk_window_set_background (window, &p->style->bg[GTK_STATE_NORMAL]);
  gdk_window_clear (window);

  if (s->running_preview_error_p)
    {
      const char * const lines[] = { "No Preview", "Available" };
      int lh = p->style->font->ascent + p->style->font->descent;
      int y, i;
      gint w, h;
      gdk_window_get_size (window, &w, &h);
      y = (h - (lh * countof(lines))) / 2;
      y += p->style->font->ascent;
      for (i = 0; i < countof(lines); i++)
        {
          int sw = gdk_string_width (p->style->font, lines[i]);
          int x = (w - sw) / 2;
          gdk_draw_string (window, p->style->font,
                           p->style->fg_gc[GTK_STATE_NORMAL],
                           x, y, lines[i]);
          y += lh;
        }
    }

  gdk_flush ();

  /* Is there a GDK way of doing this? */
  XSync (GDK_DISPLAY(), False);
}


static void
fix_preview_visual (state *s)
{
  GtkWidget *widget = name_to_widget (s, "preview");
  Visual *xvisual = get_best_gl_visual (s);
  GdkVisual *visual = x_visual_to_gdk_visual (xvisual);
  GdkVisual *dvisual = gdk_visual_get_system();
  GdkColormap *cmap = (visual == dvisual
                       ? gdk_colormap_get_system ()
                       : gdk_colormap_new (visual, False));

  if (s->debug_p)
    fprintf (stderr, "%s: using %s visual 0x%lx\n", blurb(),
             (visual == dvisual ? "default" : "non-default"),
             (xvisual ? (unsigned long) xvisual->visualid : 0L));

  if (!GTK_WIDGET_REALIZED (widget) ||
      gtk_widget_get_visual (widget) != visual)
    {
      gtk_widget_unrealize (widget);
      gtk_widget_set_visual (widget, visual);
      gtk_widget_set_colormap (widget, cmap);
      gtk_widget_realize (widget);
    }

  /* Set the Widget colors to be white-on-black. */
  {
    GdkWindow *window = widget->window;
    GtkStyle *style = gtk_style_copy (widget->style);
    GdkColormap *cmap = gtk_widget_get_colormap (widget);
    GdkColor *fg = &style->fg[GTK_STATE_NORMAL];
    GdkColor *bg = &style->bg[GTK_STATE_NORMAL];
    GdkGC *fgc = gdk_gc_new(window);
    GdkGC *bgc = gdk_gc_new(window);
    if (!gdk_color_white (cmap, fg)) abort();
    if (!gdk_color_black (cmap, bg)) abort();
    gdk_gc_set_foreground (fgc, fg);
    gdk_gc_set_background (fgc, bg);
    gdk_gc_set_foreground (bgc, bg);
    gdk_gc_set_background (bgc, fg);
    style->fg_gc[GTK_STATE_NORMAL] = fgc;
    style->bg_gc[GTK_STATE_NORMAL] = fgc;
    gtk_widget_set_style (widget, style);
  }

  gtk_widget_show (widget);
}


/* Subprocesses
 */

static char *
subproc_pretty_name (state *s)
{
  if (s->running_preview_cmd)
    {
      char *ps = strdup (s->running_preview_cmd);
      char *ss = strchr (ps, ' ');
      if (ss) *ss = 0;
      ss = strrchr (ps, '/');
      if (ss) *ss = 0;
      else ss = ps;
      return ss;
    }
  else
    return strdup ("???");
}


static void
reap_zombies (state *s)
{
  int wait_status = 0;
  pid_t pid;
  while ((pid = waitpid (-1, &wait_status, WNOHANG|WUNTRACED)) > 0)
    {
      if (s->debug_p)
        {
          if (pid == s->running_preview_pid)
            {
              char *ss = subproc_pretty_name (s);
              fprintf (stderr, "%s: pid %lu (%s) died\n", blurb(), pid, ss);
              free (ss);
            }
          else
            fprintf (stderr, "%s: pid %lu died\n", blurb(), pid);
        }
    }
}


/* Mostly lifted from driver/subprocs.c */
static Visual *
get_best_gl_visual (state *s)
{
  Display *dpy = GDK_DISPLAY();
  pid_t forked;
  int fds [2];
  int in, out;
  char buf[1024];

  char *av[10];
  int ac = 0;

  av[ac++] = "xscreensaver-gl-helper";
  av[ac] = 0;

  if (pipe (fds))
    {
      perror ("error creating pipe:");
      return 0;
    }

  in = fds [0];
  out = fds [1];

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        exit (1);
      }
    case 0:
      {
        int stdout_fd = 1;

        close (in);  /* don't need this one */
        close (ConnectionNumber (dpy));		/* close display fd */

        if (dup2 (out, stdout_fd) < 0)		/* pipe stdout */
          {
            perror ("could not dup() a new stdout:");
            return 0;
          }

        execvp (av[0], av);			/* shouldn't return. */

        if (errno != ENOENT)
          {
            /* Ignore "no such file or directory" errors, unless verbose.
               Issue all other exec errors, though. */
            sprintf (buf, "%s: running %s", blurb(), av[0]);
            perror (buf);
          }
        exit (1);                               /* exits fork */
        break;
      }
    default:
      {
        int result = 0;
        int wait_status = 0;

        FILE *f = fdopen (in, "r");
        unsigned long v = 0;
        char c;

        close (out);  /* don't need this one */

        *buf = 0;
        fgets (buf, sizeof(buf)-1, f);
        fclose (f);

        /* Wait for the child to die. */
        waitpid (-1, &wait_status, 0);

        if (1 == sscanf (buf, "0x%x %c", &v, &c))
          result = (int) v;

        if (result == 0)
          {
            if (s->debug_p)
              fprintf (stderr, "%s: %s did not report a GL visual!\n",
                       blurb(), av[0]);
            return 0;
          }
        else
          {
            Visual *v = id_to_visual (DefaultScreenOfDisplay (dpy), result);
            if (s->debug_p)
              fprintf (stderr, "%s: %s says the GL visual is 0x%X.\n",
                       blurb(), av[0], result);
            if (!v) abort();
            return v;
          }
      }
    }

  abort();
}


static void
kill_preview_subproc (state *s)
{
  s->running_preview_error_p = False;

  reap_zombies (s);
  clear_preview_window (s);

  if (s->subproc_check_timer_id)
    {
      gtk_timeout_remove (s->subproc_check_timer_id);
      s->subproc_check_timer_id = 0;
      s->subproc_check_countdown = 0;
    }

  if (s->running_preview_pid)
    {
      int status = kill (s->running_preview_pid, SIGTERM);
      char *ss = subproc_pretty_name (s);

      if (status < 0)
        {
          if (errno == ESRCH)
            {
              if (s->debug_p)
                fprintf (stderr, "%s: pid %lu (%s) was already dead.\n",
                         blurb(), s->running_preview_pid, ss);
            }
          else
            {
              char buf [1024];
              sprintf (buf, "%s: couldn't kill pid %lu (%s)",
                       blurb(), s->running_preview_pid, ss);
              perror (buf);
            }
        }
      else if (s->debug_p)
        fprintf (stderr, "%s: killed pid %lu (%s)\n", blurb(),
                 s->running_preview_pid, ss);

      free (ss);
      s->running_preview_pid = 0;
      if (s->running_preview_cmd) free (s->running_preview_cmd);
      s->running_preview_cmd = 0;
    }

  reap_zombies (s);
}


/* Immediately and unconditionally launches the given process,
   after appending the -window-id option; sets running_preview_pid.
 */
static void
launch_preview_subproc (state *s)
{
  saver_preferences *p = &s->prefs;
  Window id;
  char *new_cmd = 0;
  pid_t forked;
  const char *cmd = s->desired_preview_cmd;

  GtkWidget *pr = name_to_widget (s, "preview");
  GdkWindow *window = pr->window;

  s->running_preview_error_p = False;

  if (s->preview_suppressed_p)
    {
      kill_preview_subproc (s);
      goto DONE;
    }

  new_cmd = malloc (strlen (cmd) + 40);

  id = (window ? GDK_WINDOW_XWINDOW (window) : 0);
  if (id == 0)
    {
      /* No window id?  No command to run. */
      free (new_cmd);
      new_cmd = 0;
    }
  else
    {
      strcpy (new_cmd, cmd);
      sprintf (new_cmd + strlen (new_cmd), " -window-id 0x%X", id);
    }

  kill_preview_subproc (s);
  if (! new_cmd)
    {
      s->running_preview_error_p = True;
      clear_preview_window (s);
      goto DONE;
    }

  switch ((int) (forked = fork ()))
    {
    case -1:
      {
        char buf[255];
        sprintf (buf, "%s: couldn't fork", blurb());
        perror (buf);
        s->running_preview_error_p = True;
        goto DONE;
        break;
      }
    case 0:
      {
        close (ConnectionNumber (GDK_DISPLAY()));

        usleep (250000);  /* pause for 1/4th second before launching, to give
                             the previous program time to die and flush its X
                             buffer, so we don't get leftover turds on the
                             window. */

        exec_command (p->shell, new_cmd, p->nice_inferior);
        /* Don't bother printing an error message when we are unable to
           exec subprocesses; we handle that by polling the pid later. */
        exit (1);  /* exits child fork */
        break;

      default:

        if (s->running_preview_cmd) free (s->running_preview_cmd);
        s->running_preview_cmd = strdup (s->desired_preview_cmd);
        s->running_preview_pid = forked;

        if (s->debug_p)
          {
            char *ss = subproc_pretty_name (s);
            fprintf (stderr, "%s: forked %lu (%s)\n", blurb(), forked, ss);
            free (ss);
          }
        break;
      }
    }

  schedule_preview_check (s);

 DONE:
  if (new_cmd) free (new_cmd);
  new_cmd = 0;
}


/* Modify $DISPLAY and $PATH for the benefit of subprocesses.
 */
static void
hack_environment (state *s)
{
  static const char *def_path =
# ifdef DEFAULT_PATH_PREFIX
    DEFAULT_PATH_PREFIX;
# else
    "";
# endif

  Display *dpy = GDK_DISPLAY();
  const char *odpy = DisplayString (dpy);
  char *ndpy = (char *) malloc(strlen(odpy) + 20);
  strcpy (ndpy, "DISPLAY=");
  strcat (ndpy, odpy);
  if (putenv (ndpy))
    abort ();

  if (s->debug_p)
    fprintf (stderr, "%s: %s\n", blurb(), ndpy);

  /* don't free(ndpy) -- some implementations of putenv (BSD 4.4, glibc
     2.0) copy the argument, but some (libc4,5, glibc 2.1.2) do not.
     So we must leak it (and/or the previous setting).  Yay.
   */

  if (def_path && *def_path)
    {
      const char *opath = getenv("PATH");
      char *npath = (char *) malloc(strlen(def_path) + strlen(opath) + 20);
      strcpy (npath, "PATH=");
      strcat (npath, def_path);
      strcat (npath, ":");
      strcat (npath, opath);

      if (putenv (npath))
	abort ();
      /* do not free(npath) -- see above */

      if (s->debug_p)
        fprintf (stderr, "%s: added \"%s\" to $PATH\n", blurb(), def_path);
    }
}


/* Called from a timer:
   Launches the currently-chosen subprocess, if it's not already running.
   If there's a different process running, kills it.
 */
static int
update_subproc_timer (gpointer data)
{
  state *s = (state *) data;
  if (! s->desired_preview_cmd)
    kill_preview_subproc (s);
  else if (!s->running_preview_cmd ||
           !!strcmp (s->desired_preview_cmd, s->running_preview_cmd))
    launch_preview_subproc (s);

  s->subproc_timer_id = 0;
  return FALSE;  /* do not re-execute timer */
}


/* Call this when you think you might want a preview process running.
   It will set a timer that will actually launch that program a second
   from now, if you haven't changed your mind (to avoid double-click
   spazzing, etc.)  `cmd' may be null meaning "no process".
 */
static void
schedule_preview (state *s, const char *cmd)
{
  int delay = 1000 * 0.5;   /* 1/2 second hysteresis */

  if (s->debug_p)
    {
      if (cmd)
        fprintf (stderr, "%s: scheduling preview \"%s\"\n", blurb(), cmd);
      else
        fprintf (stderr, "%s: scheduling preview death\n", blurb());
    }

  if (s->desired_preview_cmd) free (s->desired_preview_cmd);
  s->desired_preview_cmd = (cmd ? strdup (cmd) : 0);

  if (s->subproc_timer_id)
    gtk_timeout_remove (s->subproc_timer_id);
  s->subproc_timer_id = gtk_timeout_add (delay, update_subproc_timer, s);
}


/* Called from a timer:
   Checks to see if the subproc that should be running, actually is.
 */
static int
check_subproc_timer (gpointer data)
{
  state *s = (state *) data;
  Bool again_p = True;

  if (s->running_preview_error_p ||   /* already dead */
      s->running_preview_pid <= 0)
    {
      again_p = False;
    }
  else
    {
      int status;
      reap_zombies (s);
      status = kill (s->running_preview_pid, 0);
      if (status < 0 && errno == ESRCH)
        s->running_preview_error_p = True;

      if (s->debug_p)
        {
          char *ss = subproc_pretty_name (s);
          fprintf (stderr, "%s: timer: pid %lu (%s) is %s\n", blurb(),
                   s->running_preview_pid, ss,
                   (s->running_preview_error_p ? "dead" : "alive"));
          free (ss);
        }

      if (s->running_preview_error_p)
        {
          clear_preview_window (s);
          again_p = False;
        }
    }

  /* Otherwise, it's currently alive.  We might be checking again, or we
     might be satisfied. */

  if (--s->subproc_check_countdown <= 0)
    again_p = False;

  if (again_p)
    return TRUE;     /* re-execute timer */
  else
    {
      s->subproc_check_timer_id = 0;
      s->subproc_check_countdown = 0;
      return FALSE;  /* do not re-execute timer */
    }
}


/* Call this just after launching a subprocess.
   This sets a timer that will, five times a second for two seconds,
   check whether the program is still running.  The assumption here
   is that if the process didn't stay up for more than a couple of
   seconds, then either the program doesn't exist, or it doesn't
   take a -window-id argument.
 */
static void
schedule_preview_check (state *s)
{
  int seconds = 2;
  int ticks = 5;

  if (s->debug_p)
    fprintf (stderr, "%s: scheduling check\n", blurb());

  if (s->subproc_check_timer_id)
    gtk_timeout_remove (s->subproc_check_timer_id);
  s->subproc_check_timer_id =
    gtk_timeout_add (1000 / ticks,
                     check_subproc_timer, (gpointer) s);
  s->subproc_check_countdown = ticks * seconds;
}


static Bool
screen_blanked_p (void)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  CARD32 *data = 0;
  Display *dpy = GDK_DISPLAY();
  Bool blanked_p = False;

  if (XGetWindowProperty (dpy, RootWindow (dpy, 0), /* always screen #0 */
                          XA_SCREENSAVER_STATUS,
                          0, 3, False, XA_INTEGER,
                          &type, &format, &nitems, &bytesafter,
                          (unsigned char **) &data)
      == Success
      && type == XA_INTEGER
      && nitems >= 3
      && data)
    blanked_p = (data[0] == XA_BLANK || data[0] == XA_LOCK);

  if (data) free (data);

  return blanked_p;
}

/* Wake up every now and then and see if the screen is blanked.
   If it is, kill off the small-window demo -- no point in wasting
   cycles by running two screensavers at once...
 */
static int
check_blanked_timer (gpointer data)
{
  state *s = (state *) data;
  Bool blanked_p = screen_blanked_p ();
  if (blanked_p && s->running_preview_pid)
    {
      if (s->debug_p)
        fprintf (stderr, "%s: screen is blanked: killing preview\n", blurb());
      kill_preview_subproc (s);
    }

  return True;  /* re-execute timer */
}


/* Setting window manager icon
 */

static void
init_icon (GdkWindow *window)
{
  GdkBitmap *mask = 0;
  GdkColor transp;
  GdkPixmap *pixmap =
    gdk_pixmap_create_from_xpm_d (window, &mask, &transp,
                                  (gchar **) logo_50_xpm);
  if (pixmap)
    gdk_window_set_icon (window, 0, pixmap, mask);
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
the_network_is_not_the_computer (state *s)
{
  Display *dpy = GDK_DISPLAY();
  char *rversion = 0, *ruser = 0, *rhost = 0;
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
               "on display \"%s\".  Launch it now?",
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
	      "You should either re-run %s as \"%s\", or re-run\n"
	      "xscreensaver as \"%s\".\n"
              "\n"
              "Restart the xscreensaver daemon now?\n",
	      blurb(), luser, lhost,
	      d,
	      (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	      blurb(),
	      blurb(), (ruser ? ruser : "???"),
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
               "Restart the daemon on \"%s\" as \"%s\" now?\n",
	       blurb(), luser, lhost,
	       d,
	       (ruser ? ruser : "???"), (rhost ? rhost : "???"),
	       luser,
	       blurb(),
               lhost, luser);
    }
  else if (!!strcmp (rversion, s->short_version))
    {
      /* Warn that the version numbers don't match.
       */
      sprintf (msg,
	       "Warning:\n\n"
	       "This is %s version %s.\n"
	       "But the xscreensaver managing display \"%s\"\n"
	       "is version %s.  This could cause problems.\n"
              "\n"
              "Restart the xscreensaver daemon now?\n",
	       blurb(), s->short_version,
	       d,
	       rversion);
    }


  if (*msg)
    warning_dialog (s->toplevel_widget, msg, True, 1);

  if (rversion) free (rversion);
  if (ruser) free (ruser);
  if (rhost) free (rhost);
  free (msg);
}


/* We use this error handler so that X errors are preceeded by the name
   of the program that generated them.
 */
static int
demo_ehandler (Display *dpy, XErrorEvent *error)
{
  state *s = global_state_kludge;  /* I hate C so much... */
  fprintf (stderr, "\nX error in %s:\n", blurb());
  if (XmuPrintDefaultErrorMessage (dpy, error, stderr))
    {
      kill_preview_subproc (s);
      exit (-1);
    }
  else
    fprintf (stderr, " (nonfatal.)\n");
  return 0;
}


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

  fprintf (stderr, "%s: %s-%s: %s%s", blurb(),
           (log_domain ? log_domain : progclass),
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


static char *defaults[] = {
#include "XScreenSaver_ad.h"
 0
};

#if 0
#ifdef HAVE_CRAPPLET
static struct poptOption crapplet_options[] = {
  {NULL, '\0', 0, NULL, 0}
};
#endif /* HAVE_CRAPPLET */
#endif /* 0 */

const char *usage = "[--display dpy] [--prefs]"
# ifdef HAVE_CRAPPLET
                    " [--crapplet]"
# endif
                    " [--debug]";


static void
map_popup_window_cb (GtkWidget *w, gpointer user_data)
{
  state *s = (state *) user_data;
  Boolean oi = s->initializing_p;
  GtkLabel *label = GTK_LABEL (name_to_widget (s, "doc"));
  s->initializing_p = True;
  eschew_gtk_lossage (label);
  s->initializing_p = oi;
}


#if 0
static void
print_widget_tree (GtkWidget *w, int depth)
{
  int i;
  for (i = 0; i < depth; i++)
    fprintf (stderr, "  ");
  fprintf (stderr, "%s\n", gtk_widget_get_name (w));

  if (GTK_IS_LIST (w))
    {
      for (i = 0; i < depth+1; i++)
        fprintf (stderr, "  ");
      fprintf (stderr, "...list kids...\n");
    }
  else if (GTK_IS_CONTAINER (w))
    {
      GList *kids = gtk_container_children (GTK_CONTAINER (w));
      while (kids)
        {
          print_widget_tree (GTK_WIDGET (kids->data), depth+1);
          kids = kids->next;
        }
    }
}
#endif /* 0 */

static int
delayed_scroll_kludge (gpointer data)
{
  state *s = (state *) data;
  GtkWidget *w = GTK_WIDGET (name_to_widget (s, "list"));
  ensure_selected_item_visible (w);

  /* Oh, this is just fucking lovely, too. */
  w = GTK_WIDGET (name_to_widget (s, "preview"));
  gtk_widget_hide (w);
  gtk_widget_show (w);

  return FALSE;  /* do not re-execute timer */
}


int
main (int argc, char **argv)
{
  XtAppContext app;
  state S, *s;
  saver_preferences *p;
  Bool prefs = False;
  int i;
  Display *dpy;
  Widget toplevel_shell;
  char *real_progname = argv[0];
  char window_title[255];
  Bool crapplet_p = False;
  char *str;

  str = strrchr (real_progname, '/');
  if (str) real_progname = str+1;

  s = &S;
  memset (s, 0, sizeof(*s));
  s->initializing_p = True;
  p = &s->prefs;

  global_state_kludge = s;  /* I hate C so much... */

  progname = real_progname;

  s->short_version = (char *) malloc (5);
  memcpy (s->short_version, screensaver_id + 17, 4);
  s->short_version [4] = 0;


  /* Register our error message logger for every ``log domain'' known.
     There's no way to do this globally, so I grepped the Gtk/Gdk sources
     for all of the domains that seem to be in use.
  */
  {
    const char * const domains[] = { 0,
                                     "Gtk", "Gdk", "GLib", "GModule",
                                     "GThread", "Gnome", "GnomeUI" };
    for (i = 0; i < countof(domains); i++)
      g_log_set_handler (domains[i], G_LOG_LEVEL_MASK, g_log_handler, 0);
  }

#ifdef DEFAULT_ICONDIR  /* from -D on compile line */
  {
    const char *dir = DEFAULT_ICONDIR;
    if (*dir) add_pixmap_directory (dir);
  }
#endif

  /* This is gross, but Gtk understands --display and not -display...
   */
  for (i = 1; i < argc; i++)
    if (argv[i][0] && argv[i][1] && 
        !strncmp(argv[i], "-display", strlen(argv[i])))
      argv[i] = "--display";


  /* We need to parse this arg really early... Sigh. */
  for (i = 1; i < argc; i++)
    if (argv[i] &&
        (!strcmp(argv[i], "--crapplet") ||
         !strcmp(argv[i], "--capplet")))
      {
# ifdef HAVE_CRAPPLET
        int j;
        crapplet_p = True;
        for (j = i; j < argc; j++)  /* remove it from the list */
          argv[j] = argv[j+1];
        argc--;

# else  /* !HAVE_CRAPPLET */
        fprintf (stderr, "%s: not compiled with --crapplet support\n",
                 real_progname);
        fprintf (stderr, "%s: %s\n", real_progname, usage);
        exit (1);
# endif /* !HAVE_CRAPPLET */
      }
  else if (argv[i] &&
           (!strcmp(argv[i], "--debug") ||
            !strcmp(argv[i], "-debug") ||
            !strcmp(argv[i], "-d")))
    {
      int j;
      s->debug_p = True;
      for (j = i; j < argc; j++)  /* remove it from the list */
        argv[j] = argv[j+1];
      argc--;
    }

  /* Let Gtk open the X connection, then initialize Xt to use that
     same connection.  Doctor Frankenstein would be proud.
   */
# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    {
      GnomeClient *client;
      GnomeClientFlags flags = 0;

      int init_results = gnome_capplet_init ("screensaver-properties",
                                             s->short_version,
                                             argc, argv, NULL, 0, NULL);
      /* init_results is:
         0 upon successful initialization;
         1 if --init-session-settings was passed on the cmdline;
         2 if --ignore was passed on the cmdline;
        -1 on error.

         So the 1 signifies just to init the settings, and quit, basically.
         (Meaning launch the xscreensaver daemon.)
       */

      if (init_results < 0)
        {
#  if 0
          g_error ("An initialization error occurred while "
                   "starting xscreensaver-capplet.\n");
#  else  /* !0 */
          fprintf (stderr, "%s: gnome_capplet_init failed: %d\n",
                   real_progname, init_results);
          exit (1);
#  endif /* !0 */
        }

      client = gnome_master_client ();

      if (client)
        flags = gnome_client_get_flags (client);

      if (flags & GNOME_CLIENT_IS_CONNECTED)
        {
          int token =
            gnome_startup_acquire_token ("GNOME_SCREENSAVER_PROPERTIES",
                                         gnome_client_get_id (client));
          if (token)
            {
              char *session_args[20];
              int i = 0;
              session_args[i++] = real_progname;
              session_args[i++] = "--capplet";
              session_args[i++] = "--init-session-settings";
              session_args[i] = 0;
              gnome_client_set_priority (client, 20);
              gnome_client_set_restart_style (client, GNOME_RESTART_ANYWAY);
              gnome_client_set_restart_command (client, i, session_args);
            }
          else
            {
              gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);
            }

          gnome_client_flush (client);
        }

      if (init_results == 1)
	{
	  system ("xscreensaver -nosplash &");
	  return 0;
	}

    }
  else
# endif /* HAVE_CRAPPLET */
    {
      gtk_init (&argc, &argv);
    }


  /* We must read exactly the same resources as xscreensaver.
     That means we must have both the same progclass *and* progname,
     at least as far as the resource database is concerned.  So,
     put "xscreensaver" in argv[0] while initializing Xt.
   */
  argv[0] = "xscreensaver";
  progname = argv[0];


  /* Teach Xt to use the Display that Gtk/Gdk have already opened.
   */
  XtToolkitInitialize ();
  app = XtCreateApplicationContext ();
  dpy = GDK_DISPLAY();
  XtAppSetFallbackResources (app, defaults);
  XtDisplayInitialize (app, dpy, progname, progclass, 0, 0, &argc, argv);
  toplevel_shell = XtAppCreateShell (progname, progclass,
                                     applicationShellWidgetClass,
                                     dpy, 0, 0);

  dpy = XtDisplay (toplevel_shell);
  db = XtDatabase (dpy);
  XtGetApplicationNameAndClass (dpy, &progname, &progclass);
  XSetErrorHandler (demo_ehandler);

  /* Let's just ignore these.  They seem to confuse Irix Gtk... */
  signal (SIGPIPE, SIG_IGN);

  /* After doing Xt-style command-line processing, complain about any
     unrecognized command-line arguments.
   */
  for (i = 1; i < argc; i++)
    {
      char *str = argv[i];
      if (str[0] == '-' && str[1] == '-')
	str++;
      if (!strcmp (str, "-prefs"))
	prefs = True;
      else if (crapplet_p)
        /* There are lots of random args that we don't care about when we're
           started as a crapplet, so just ignore unknown args in that case. */
        ;
      else
	{
	  fprintf (stderr, "%s: unknown option: %s\n", real_progname, argv[i]);
          fprintf (stderr, "%s: %s\n", real_progname, usage);
          exit (1);
	}
    }

  /* Load the init file, which may end up consulting the X resource database
     and the site-wide app-defaults file.  Note that at this point, it's
     important that `progname' be "xscreensaver", rather than whatever
     was in argv[0].
   */
  p->db = db;
  load_init_file (p);
  initialize_sort_map (s);

  /* Now that Xt has been initialized, and the resources have been read,
     we can set our `progname' variable to something more in line with
     reality.
   */
  progname = real_progname;


#if 0
  /* Print out all the resources we read. */
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
  s->base_widget     = create_xscreensaver_demo ();
  s->popup_widget    = create_xscreensaver_settings_dialog ();
  s->toplevel_widget = s->base_widget;


  /* Set the main window's title. */
  {
    char *v = (char *) strdup(strchr(screensaver_id, ' '));
    char *s1, *s2, *s3, *s4;
    s1 = (char *) strchr(v,  ' '); s1++;
    s2 = (char *) strchr(s1, ' ');
    s3 = (char *) strchr(v,  '('); s3++;
    s4 = (char *) strchr(s3, ')');
    *s2 = 0;
    *s4 = 0;
    sprintf (window_title, "%.50s %.50s, %.50s", progclass, s1, s3);
    gtk_window_set_title (GTK_WINDOW (s->toplevel_widget), window_title);
    gtk_window_set_title (GTK_WINDOW (s->popup_widget),    window_title);
    free (v);
  }

  /* Adjust the (invisible) notebooks on the popup dialog... */
  {
    GtkNotebook *notebook =
      GTK_NOTEBOOK (name_to_widget (s, "opt_notebook"));
    GtkWidget *std = GTK_WIDGET (name_to_widget (s, "std_button"));
    int page = 0;

# ifdef HAVE_XML
    gtk_widget_hide (std);
# else  /* !HAVE_XML */
    /* Make the advanced page be the only one available. */
    gtk_widget_set_sensitive (std, False);
    std = GTK_WIDGET (name_to_widget (s, "adv_button"));
    gtk_widget_hide (std);
    page = 1;
# endif /* !HAVE_XML */

    gtk_notebook_set_page (notebook, page);
    gtk_notebook_set_show_tabs (notebook, False);
  }

  /* Various other widget initializations...
   */
  gtk_signal_connect (GTK_OBJECT (s->toplevel_widget), "delete_event",
                      GTK_SIGNAL_FUNC (wm_toplevel_close_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (s->popup_widget), "delete_event",
                      GTK_SIGNAL_FUNC (wm_popup_close_cb),
                      (gpointer) s);

  populate_hack_list (s);
  populate_prefs_page (s);
  sensitize_demo_widgets (s, False);
  fix_text_entry_sizes (s);
  scroll_to_current_hack (s);

  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "cancel_button")),
                      "map", GTK_SIGNAL_FUNC(map_popup_window_cb),
                      (gpointer) s);

  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "prev")),
                      "map", GTK_SIGNAL_FUNC(map_prev_button_cb),
                      (gpointer) s);
  gtk_signal_connect (GTK_OBJECT (name_to_widget (s, "next")),
                      "map", GTK_SIGNAL_FUNC(map_next_button_cb),
                      (gpointer) s);


  /* Hook up callbacks to the items on the mode menu. */
  {
    GtkOptionMenu *opt = GTK_OPTION_MENU (name_to_widget (s, "mode_menu"));
    GtkMenu *menu = GTK_MENU (gtk_option_menu_get_menu (opt));
    GList *kids = gtk_container_children (GTK_CONTAINER (menu));
    for (; kids; kids = kids->next)
      gtk_signal_connect (GTK_OBJECT (kids->data), "activate",
                          GTK_SIGNAL_FUNC (mode_menu_item_cb),
                          (gpointer) s);
  }


  /* Handle the -prefs command-line argument. */
  if (prefs)
    {
      GtkNotebook *notebook =
        GTK_NOTEBOOK (name_to_widget (s, "notebook"));
      gtk_notebook_set_page (notebook, 1);
    }

# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    {
      GtkWidget *capplet;
      GtkWidget *outer_vbox;

      gtk_widget_hide (s->toplevel_widget);

      capplet = capplet_widget_new ();

      /* Make there be a "Close" button instead of "OK" and "Cancel" */
# ifdef HAVE_CRAPPLET_IMMEDIATE
      capplet_widget_changes_are_immediate (CAPPLET_WIDGET (capplet));
# endif /* HAVE_CRAPPLET_IMMEDIATE */

      /* In crapplet-mode, take off the menubar. */
      gtk_widget_hide (name_to_widget (s, "menubar"));

      /* Reparent our top-level container to be a child of the capplet
         window.
       */
      outer_vbox = GTK_BIN (s->toplevel_widget)->child;
      gtk_widget_ref (outer_vbox);
      gtk_container_remove (GTK_CONTAINER (s->toplevel_widget),
                            outer_vbox);
      GTK_OBJECT_SET_FLAGS (outer_vbox, GTK_FLOATING);
      gtk_container_add (GTK_CONTAINER (capplet), outer_vbox);

      /* Find the window above us, and set the title and close handler. */
      {
        GtkWidget *window = capplet;
        while (window && !GTK_IS_WINDOW (window))
          window = window->parent;
        if (window)
          {
            gtk_window_set_title (GTK_WINDOW (window), window_title);
            gtk_signal_connect (GTK_OBJECT (window), "delete_event",
                                GTK_SIGNAL_FUNC (wm_toplevel_close_cb),
                                (gpointer) s);
          }
      }

      s->toplevel_widget = capplet;
    }
# endif /* HAVE_CRAPPLET */


  gtk_widget_show (s->toplevel_widget);
  init_icon (GTK_WIDGET (s->toplevel_widget)->window);  /* after `show' */
  hack_environment (s);
  fix_preview_visual (s);

  /* Realize page zero, so that we can diddle the scrollbar when the
     user tabs back to it -- otherwise, the current hack isn't scrolled
     to the first time they tab back there, when started with "-prefs".
     (Though it is if they then tab away, and back again.)

     #### Bah!  This doesn't work.  Gtk eats my ass!  Someone who
     #### understands this crap, explain to me how to make this work.
  */
  gtk_widget_realize (name_to_widget (s, "demos_table"));


  gtk_timeout_add (60 * 1000, check_blanked_timer, s);


  /* Issue any warnings about the running xscreensaver daemon. */
  the_network_is_not_the_computer (s);


  /* Run the Gtk event loop, and not the Xt event loop.  This means that
     if there were Xt timers or fds registered, they would never get serviced,
     and if there were any Xt widgets, they would never have events delivered.
     Fortunately, we're using Gtk for all of the UI, and only initialized
     Xt so that we could process the command line and use the X resource
     manager.
   */
  s->initializing_p = False;

  /* This totally sucks -- set a timer that whacks the scrollbar 0.5 seconds
     after we start up.  Otherwise, it always appears scrolled to the top
     when in crapplet-mode. */
  gtk_timeout_add (500, delayed_scroll_kludge, s);


#if 0
  /* Load every configurator in turn, to scan them for errors all at once. */
  {
    int i;
    for (i = 0; i < p->screenhacks_count; i++)
      {
        screenhack *hack = p->screenhacks[s->hack_number_to_list_elt[i]];
        conf_data *d = load_configurator (hack->command, False);
        if (d) free_conf_data (d);
      }
  }
#endif


# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    capplet_gtk_main ();
  else
# endif /* HAVE_CRAPPLET */
    gtk_main ();

  kill_preview_subproc (s);
  exit (0);
}

#endif /* HAVE_GTK -- whole file */
