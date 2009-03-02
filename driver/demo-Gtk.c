/* demo-Gtk.c --- implements the interactive demo-mode and options dialogs.
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
#include <sys/stat.h>

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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#undef countof
#define countof(x) (sizeof((x))/sizeof((*x)))


char *progname = 0;
char *progclass = "XScreenSaver";
XrmDatabase db;

static Bool crapplet_p = False;
static Bool initializing_p;
static GtkWidget *toplevel_widget;

typedef struct {
  saver_preferences *a, *b;
} prefs_pair;

static void *global_prefs_pair;  /* I hate C so much... */

char *blurb (void) { return progname; }

static char *short_version = 0;

Atom XA_VROOT;
Atom XA_SCREENSAVER, XA_SCREENSAVER_RESPONSE, XA_SCREENSAVER_VERSION;
Atom XA_SCREENSAVER_ID, XA_SCREENSAVER_STATUS, XA_SELECT, XA_DEMO;
Atom XA_ACTIVATE, XA_BLANK, XA_LOCK, XA_RESTART, XA_EXIT;


static void populate_demo_window (GtkWidget *toplevel,
                                  int which, prefs_pair *pair);
static void populate_prefs_page (GtkWidget *top, prefs_pair *pair);
static int apply_changes_and_save (GtkWidget *widget);
static int maybe_reload_init_file (GtkWidget *widget, prefs_pair *pair);
static void await_xscreensaver (GtkWidget *widget);


/* Some random utility functions
 */

static GtkWidget *
name_to_widget (GtkWidget *widget, const char *name)
{
  return (GtkWidget *) gtk_object_get_data (GTK_OBJECT(toplevel_widget), name);
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
  int which = -1;
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

  while (parent->parent)
    parent = parent->parent;

  if (!GTK_WIDGET (parent)->window) /* too early to pop up transient dialogs */
    return;

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
run_cmd (GtkWidget *widget, Atom command, int arg)
{
  char *err = 0;
  int status;

  apply_changes_and_save (widget);
  status = xscreensaver_command (GDK_DISPLAY(), command, arg, False, &err);
  if (status < 0)
    {
      char buf [255];
      if (err)
        sprintf (buf, "Error:\n\n%s", err);
      else
        strcpy (buf, "Unknown error!");
      warning_dialog (widget, buf, False, 100);
    }
  if (err) free (err);
}


static void
run_hack (GtkWidget *widget, int which, Bool report_errors_p)
{
  if (which < 0) return;
  apply_changes_and_save (widget);
  if (report_errors_p)
    run_cmd (widget, XA_DEMO, which + 1);
  else
    {
      char *s = 0;
      xscreensaver_command (GDK_DISPLAY(), XA_DEMO, which + 1, False, &s);
      if (s) free (s);
    }
}



/* Button callbacks
 */

void
exit_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  apply_changes_and_save (GTK_WIDGET (menuitem));
  gtk_main_quit ();
}

static void
wm_close_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
  apply_changes_and_save (widget);
  gtk_main_quit ();
}


void
cut_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  /* #### */
  warning_dialog (GTK_WIDGET (menuitem),
                  "Error:\n\n"
                  "cut unimplemented\n", False, 1);
}


void
copy_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  /* #### */
  warning_dialog (GTK_WIDGET (menuitem),
                  "Error:\n\n"
                  "copy unimplemented\n", False, 1);
}


void
paste_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  /* #### */
  warning_dialog (GTK_WIDGET (menuitem),
                  "Error:\n\n"
                  "paste unimplemented\n", False, 1);
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

  sprintf(copy, "Copyright \251 1991-2001 %s", s);

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
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  saver_preferences *p =  pair->a;
  char *help_command;

  if (!p->help_url || !*p->help_url)
    {
      warning_dialog (GTK_WIDGET (menuitem),
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
  run_cmd (GTK_WIDGET (menuitem), XA_ACTIVATE, 0);
}


void
lock_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  run_cmd (GTK_WIDGET (menuitem), XA_LOCK, 0);
}


void
kill_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  run_cmd (GTK_WIDGET (menuitem), XA_EXIT, 0);
}


void
restart_menu_cb (GtkWidget *widget, gpointer user_data)
{
#if 0
  run_cmd (GTK_WIDGET (widget), XA_RESTART, 0);
#else
  apply_changes_and_save (GTK_WIDGET (widget));
  xscreensaver_command (GDK_DISPLAY(), XA_EXIT, 0, False, NULL);
  sleep (1);
  system ("xscreensaver -nosplash &");
#endif

  await_xscreensaver (GTK_WIDGET (widget));
}

static void
await_xscreensaver (GtkWidget *widget)
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

      warning_dialog (widget, buf, False, 1);
    }
}


static int _selected_hack_number = -1;

static int
selected_hack_number (GtkWidget *toplevel)
{
#if 0
  GtkViewport *vp = GTK_VIEWPORT (name_to_widget (toplevel, "viewport"));
  GtkList *list_widget = GTK_LIST (GTK_BIN(vp)->child);
  GList *slist = list_widget->selection;
  GtkWidget *selected = (slist ? GTK_WIDGET (slist->data) : 0);
  int which = (selected
               ? gtk_list_child_position (list_widget, GTK_WIDGET (selected))
               : -1);
  return which;
#else
  return _selected_hack_number;
#endif
}


static int
demo_write_init_file (GtkWidget *widget, saver_preferences *p)
{
  if (!write_init_file (p, short_version, False))
    return 0;
  else
    {
      const char *f = init_file_name();
      if (!f || !*f)
        warning_dialog (widget,
                        "Error:\n\nCouldn't determine init file name!\n",
                        False, 100);
      else
        {
          char *b = (char *) malloc (strlen(f) + 1024);
          sprintf (b, "Error:\n\nCouldn't write %s\n", f);
          warning_dialog (widget, b, False, 100);
          free (b);
        }
      return -1;
    }
}


static int
apply_changes_and_save_1 (GtkWidget *widget)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */
  saver_preferences *p =  pair->a;
  GtkList *list_widget =
    GTK_LIST (name_to_widget (widget, "list"));
  int which = selected_hack_number (widget);

  GtkEntry *cmd = GTK_ENTRY (name_to_widget (widget, "cmd_text"));
  GtkToggleButton *enabled =
    GTK_TOGGLE_BUTTON (name_to_widget (widget, "enabled"));
  GtkCombo *vis = GTK_COMBO (name_to_widget (widget, "visual_combo"));

  Bool enabled_p = gtk_toggle_button_get_active (enabled);
  const char *visual = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (vis)->entry));
  const char *command = gtk_entry_get_text (cmd);
  
  char c;
  unsigned long id;

  if (which < 0) return -1;

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
      gdk_beep ();				  /* unparsable */
      visual = "";
      gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry), "Any");
    }

  ensure_selected_item_visible (GTK_WIDGET (list_widget));

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

void prefs_ok_cb (GtkButton *button, gpointer user_data);

static int
apply_changes_and_save (GtkWidget *widget)
{
  prefs_ok_cb ((GtkButton *) widget, 0);
  return apply_changes_and_save_1 (widget);
}


void
run_this_cb (GtkButton *button, gpointer user_data)
{
  int which = selected_hack_number (GTK_WIDGET (button));
  if (which < 0) return;
  if (0 == apply_changes_and_save (GTK_WIDGET (button)))
    run_hack (GTK_WIDGET (button), which, True);
}


void
manual_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */
  saver_preferences *p =  pair->a;
  GtkList *list_widget =
    GTK_LIST (name_to_widget (GTK_WIDGET (button), "list"));
  int which = selected_hack_number (GTK_WIDGET (button));
  char *name, *name2, *cmd, *s;
  if (which < 0) return;
  apply_changes_and_save (GTK_WIDGET (button));
  ensure_selected_item_visible (GTK_WIDGET (list_widget));

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
      warning_dialog (GTK_WIDGET (button),
                      "Error:\n\nno `manualCommand' resource set.",
                      False, 100);
    }

  free (name);
}


void
run_next_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */
  saver_preferences *p =  pair->a;

  GtkList *list_widget =
    GTK_LIST (name_to_widget (GTK_WIDGET (button), "list"));
  int which = selected_hack_number (GTK_WIDGET (button));

  if (which < 0)
    which = 0;
  else
    which++;

  if (which >= p->screenhacks_count)
    which = 0;

  apply_changes_and_save (GTK_WIDGET (button));
  gtk_list_select_item (GTK_LIST (list_widget), which);
  ensure_selected_item_visible (GTK_WIDGET (list_widget));
  populate_demo_window (GTK_WIDGET (button), which, pair);
  run_hack (GTK_WIDGET (button), which, False);
}


void
run_prev_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */
  saver_preferences *p =  pair->a;

  GtkList *list_widget =
    GTK_LIST (name_to_widget (GTK_WIDGET (button), "list"));
  int which = selected_hack_number (GTK_WIDGET (button));

  if (which < 0)
    which = p->screenhacks_count - 1;
  else
    which--;

  if (which < 0)
    which = p->screenhacks_count - 1;

  apply_changes_and_save (GTK_WIDGET (button));
  gtk_list_select_item (GTK_LIST (list_widget), which);
  ensure_selected_item_visible (GTK_WIDGET (list_widget));
  populate_demo_window (GTK_WIDGET (button), which, pair);
  run_hack (GTK_WIDGET (button), which, False);
}


/* Helper for the text fields that contain time specifications:
   this parses the text, and does error checking.
 */
static void 
hack_time_text (GtkWidget *widget, const char *line, Time *store, Bool sec_p)
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
	  warning_dialog (widget, b, False, 100);
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

  return p2;
}


void
prefs_ok_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  saver_preferences *p =  pair->a;
  saver_preferences *p2 = pair->b;
  Bool changed = False;

# define SECONDS(field, name) \
  hack_time_text (GTK_WIDGET(button), gtk_entry_get_text (\
                    GTK_ENTRY (name_to_widget (GTK_WIDGET(button), (name)))), \
                  (field), \
                  True)

# define MINUTES(field, name) \
  hack_time_text (GTK_WIDGET(button), gtk_entry_get_text (\
                    GTK_ENTRY (name_to_widget (GTK_WIDGET(button), (name)))), \
                  (field), \
                  False)

# define INTEGER(field, name) do { \
    char *line = gtk_entry_get_text (\
                    GTK_ENTRY (name_to_widget (GTK_WIDGET(button), (name)))); \
    unsigned int value; \
    char c; \
    if (! *line) \
      ; \
    else if (sscanf (line, "%u%c", &value, &c) != 1) \
      { \
	char b[255]; \
	sprintf (b, "Error:\n\n" "Not an integer: \"%s\"\n", line); \
	warning_dialog (GTK_WIDGET (button), b, False, 100); \
      } \
   else \
     *(field) = value; \
  } while(0)

# define PATHNAME(field, name) do { \
    char *line = gtk_entry_get_text (\
                    GTK_ENTRY (name_to_widget (GTK_WIDGET(button), (name)))); \
    if (! *line) \
      ; \
    else if (!directory_p (line)) \
      { \
	char b[255]; \
	sprintf (b, "Error:\n\n" "Directory does not exist: \"%s\"\n", line); \
	warning_dialog (GTK_WIDGET (button), b, False, 100); \
        if ((field)) free ((field)); \
        (field) = strdup(line); \
      } \
   else { \
     if ((field)) free ((field)); \
     (field) = strdup(line); \
    } \
  } while(0)

# define CHECKBOX(field, name) \
  field = gtk_toggle_button_get_active (\
             GTK_TOGGLE_BUTTON (name_to_widget (GTK_WIDGET(button), (name))))

  MINUTES (&p2->timeout,          "timeout_text");
  MINUTES (&p2->cycle,            "cycle_text");
  CHECKBOX (p2->lock_p,           "lock_button");
  MINUTES (&p2->lock_timeout,     "lock_text");

  CHECKBOX (p2->dpms_enabled_p,   "dpms_button");
  MINUTES (&p2->dpms_standby,     "dpms_standby_text");
  MINUTES (&p2->dpms_suspend,     "dpms_suspend_text");
  MINUTES (&p2->dpms_off,         "dpms_off_text");

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
  SECONDS (&p2->fade_seconds,     "fade_text");

# undef SECONDS
# undef MINUTES
# undef INTEGER
# undef PATHNAME
# undef CHECKBOX

# define COPY(field) \
  if (p->field != p2->field) changed = True; \
  p->field = p2->field

  COPY(timeout);
  COPY(cycle);
  COPY(lock_p);
  COPY(lock_timeout);

  COPY(dpms_enabled_p);
  COPY(dpms_standby);
  COPY(dpms_suspend);
  COPY(dpms_off);

  COPY (grab_desktop_p);
  COPY (grab_video_p);
  COPY (random_image_p);

  if (!p->image_directory ||
      !p2->image_directory ||
      strcmp(p->image_directory, p2->image_directory))
    changed = True;
  if (p->image_directory && p->image_directory != p2->image_directory)
    free (p->image_directory);
  p->image_directory = normalize_directory (p2->image_directory);
  if (p2->image_directory) free (p2->image_directory);
  p2->image_directory = 0;

  COPY(verbose_p);
  COPY(capture_stderr_p);
  COPY(splash_p);

  COPY(install_cmap_p);
  COPY(fade_p);
  COPY(unfade_p);
  COPY(fade_seconds);
# undef COPY

  populate_prefs_page (GTK_WIDGET (button), pair);

  if (changed)
    {
      Display *dpy = GDK_DISPLAY();
      sync_server_dpms_settings (dpy, p->dpms_enabled_p,
                                 p->dpms_standby / 1000,
                                 p->dpms_suspend / 1000,
                                 p->dpms_off / 1000,
                                 False);

      demo_write_init_file (GTK_WIDGET (button), p);
    }
}


void
prefs_cancel_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  *pair->b = *pair->a;
  populate_prefs_page (GTK_WIDGET (button), pair);
}


void
pref_changed_cb (GtkButton *button, gpointer user_data)
{
  if (! initializing_p)
    apply_changes_and_save (GTK_WIDGET (button));
}


static gint
list_doubleclick_cb (GtkWidget *button, GdkEventButton *event,
                     gpointer client_data)
{
  if (event->type == GDK_2BUTTON_PRESS)
    {
      GtkList *list = GTK_LIST (name_to_widget (button, "list"));
      int which = gtk_list_child_position (list, GTK_WIDGET (button));

      if (which >= 0)
        run_hack (GTK_WIDGET (button), which, True);
    }

  return FALSE;
}


static void
list_select_cb (GtkList *list, GtkWidget *child)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  int which = gtk_list_child_position (list, GTK_WIDGET (child));
  apply_changes_and_save (GTK_WIDGET (list));
  populate_demo_window (GTK_WIDGET (list), which, pair);
}

static void
list_unselect_cb (GtkList *list, GtkWidget *child)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  apply_changes_and_save (GTK_WIDGET (list));
  populate_demo_window (GTK_WIDGET (list), -1, pair);
}


static int updating_enabled_cb = 0;  /* kludge to make sure that enabled_cb
                                        is only run by user action, not by
                                        program action. */

/* Called when the checkboxes that are in the left column of the
   scrolling list are clicked.  This both populates the right pane
   (just as clicking on the label (really, listitem) does) and
   also syncs this checkbox with  the right pane Enabled checkbox.
 */
static void
list_checkbox_cb (GtkWidget *cb, gpointer client_data)
{
  prefs_pair *pair = (prefs_pair *) client_data;

  GtkWidget *line_hbox = GTK_WIDGET (cb)->parent;
  GtkWidget *line = GTK_WIDGET (line_hbox)->parent;

  GtkList *list = GTK_LIST (GTK_WIDGET (line)->parent);
  GtkViewport *vp = GTK_VIEWPORT (GTK_WIDGET (list)->parent);
  GtkScrolledWindow *scroller = GTK_SCROLLED_WINDOW (GTK_WIDGET (vp)->parent);
  GtkAdjustment *adj;
  double scroll_top;

  GtkToggleButton *enabled =
    GTK_TOGGLE_BUTTON (name_to_widget (cb, "enabled"));

  int which = gtk_list_child_position (list, line);

  /* remember previous scroll position of the top of the list */
  adj = gtk_scrolled_window_get_vadjustment (scroller);
  scroll_top = adj->value;

  apply_changes_and_save (GTK_WIDGET (list));
  gtk_list_select_item (list, which);
  /* ensure_selected_item_visible (GTK_WIDGET (list)); */
  populate_demo_window (GTK_WIDGET (list), which, pair);
  
  updating_enabled_cb++;
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (enabled),
                                GTK_TOGGLE_BUTTON (cb)->active);
  updating_enabled_cb--;

  /* restore the previous scroll position of the top of the list.
     this is weak, but I don't really know why it's moving... */
  gtk_adjustment_set_value (adj, scroll_top);
}


/* Called when the right pane Enabled checkbox is clicked.  This syncs
   the corresponding checkbox inside the scrolling list to the state
   of this checkbox.
 */
void
enabled_cb (GtkWidget *cb, gpointer client_data)
{
  int which = selected_hack_number (cb);
  
  if (updating_enabled_cb) return;

  if (which != -1)
    {
      GtkList *list = GTK_LIST (name_to_widget (cb, "list"));
      GList *kids = GTK_LIST (list)->children;
      GtkWidget *line = GTK_WIDGET (g_list_nth_data (kids, which));
      GtkWidget *line_hbox = GTK_WIDGET (GTK_BIN (line)->child);
      GtkWidget *line_check =
        GTK_WIDGET (gtk_container_children (GTK_CONTAINER (line_hbox))->data);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (line_check),
                                    GTK_TOGGLE_BUTTON (cb)->active);
    }
}



typedef struct {
  prefs_pair *pair;
  GtkFileSelection *widget;
} file_selection_data;



static void
store_image_directory (GtkWidget *button, gpointer user_data)
{
  file_selection_data *fsd = (file_selection_data *) user_data;
  prefs_pair *pair = fsd->pair;
  GtkFileSelection *selector = fsd->widget;
  GtkWidget *top = toplevel_widget;
  saver_preferences *p = pair->a;
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

  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  demo_write_init_file (GTK_WIDGET (top), p);
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
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */
  saver_preferences *p = pair->a;
  static file_selection_data *fsd = 0;

  GtkFileSelection *selector = GTK_FILE_SELECTION(
    gtk_file_selection_new ("Please select the image directory."));

  if (!fsd)
    fsd = (file_selection_data *) malloc (sizeof (*fsd));  

  fsd->widget = selector;
  fsd->pair = pair;

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
scroll_to_current_hack (GtkWidget *toplevel, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  CARD32 *data = 0;
  Display *dpy = GDK_DISPLAY();
  int which = 0;
  GtkList *list;

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

  list = GTK_LIST (name_to_widget (toplevel, "list"));
  apply_changes_and_save (toplevel);
  if (which < p->screenhacks_count)
    {
      gtk_list_select_item (list, which);
      ensure_selected_item_visible (GTK_WIDGET (list));
      populate_demo_window (toplevel, which, pair);
    }
}



static void
populate_hack_list (GtkWidget *toplevel, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  GtkList *list = GTK_LIST (name_to_widget (toplevel, "list"));
  screenhack **hacks = p->screenhacks;
  screenhack **h;

  for (h = hacks; h && *h; h++)
    {
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

      char *pretty_name = (h[0]->name
                           ? strdup (h[0]->name)
                           : make_hack_name (h[0]->command));

      line = gtk_list_item_new ();
      line_hbox = gtk_hbox_new (FALSE, 0);
      line_check = gtk_check_button_new ();
      line_label = gtk_label_new (pretty_name);

      gtk_container_add (GTK_CONTAINER (line), line_hbox);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_check, FALSE, FALSE, 0);
      gtk_box_pack_start (GTK_BOX (line_hbox), line_label, FALSE, FALSE, 0);

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (line_check),
                                    h[0]->enabled_p);
      gtk_label_set_justify (GTK_LABEL (line_label), GTK_JUSTIFY_LEFT);

      gtk_widget_show (line_check);
      gtk_widget_show (line_label);
      gtk_widget_show (line_hbox);
      gtk_widget_show (line);

      free (pretty_name);

      gtk_container_add (GTK_CONTAINER (list), line);
      gtk_signal_connect (GTK_OBJECT (line), "button_press_event",
                          GTK_SIGNAL_FUNC (list_doubleclick_cb),
                          (gpointer) pair);

      gtk_signal_connect (GTK_OBJECT (line_check), "toggled",
                          GTK_SIGNAL_FUNC (list_checkbox_cb),
                          (gpointer) pair);

#if 0 /* #### */
      GTK_WIDGET (GTK_BIN(line)->child)->style =
        gtk_style_copy (GTK_WIDGET (text_line)->style);
#endif
      gtk_widget_show (line);
    }

  gtk_signal_connect (GTK_OBJECT (list), "select_child",
                      GTK_SIGNAL_FUNC (list_select_cb),
                      (gpointer) pair);
  gtk_signal_connect (GTK_OBJECT (list), "unselect_child",
                      GTK_SIGNAL_FUNC (list_unselect_cb),
                      (gpointer) pair);
}


static void
populate_prefs_page (GtkWidget *top, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  char s[100];

  format_time (s, p->timeout);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "timeout_text")), s);
  format_time (s, p->cycle);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "cycle_text")), s);
  format_time (s, p->lock_timeout);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "lock_text")), s);

  format_time (s, p->dpms_standby);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "dpms_standby_text")),s);
  format_time (s, p->dpms_suspend);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "dpms_suspend_text")),s);
  format_time (s, p->dpms_off);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "dpms_off_text")), s);

  format_time (s, p->fade_seconds);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "fade_text")), s);

  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "lock_button")),
                   p->lock_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "verbose_button")),
                   p->verbose_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "capture_button")),
                   p->capture_stderr_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "splash_button")),
                   p->splash_p);

  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "dpms_button")),
                   p->dpms_enabled_p);

  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top,"grab_desk_button")),
                   p->grab_desktop_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget(top,"grab_video_button")),
                   p->grab_video_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget(top,"grab_image_button")),
                   p->random_image_p);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "image_text")),
                      (p->image_directory ? p->image_directory : ""));
  gtk_widget_set_sensitive (GTK_WIDGET (name_to_widget (top, "image_text")),
                            p->random_image_p);
  gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top,"image_browse_button")),
                            p->random_image_p);

  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "install_button")),
                   p->install_cmap_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "fade_button")),
                   p->fade_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "unfade_button")),
                   p->unfade_p);


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


    /* Blanking and Locking
     */
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "lock_label")),
                           p->lock_p);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "lock_text")),
                           p->lock_p);

    /* DPMS
     */
    gtk_widget_set_sensitive (
                      GTK_WIDGET (name_to_widget (top, "dpms_frame")),
                      dpms_supported);
    gtk_widget_set_sensitive (
                      GTK_WIDGET (name_to_widget (top, "dpms_button")),
                      dpms_supported);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_standby_label")),
                       dpms_supported && p->dpms_enabled_p);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_standby_text")),
                       dpms_supported && p->dpms_enabled_p);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_suspend_label")),
                       dpms_supported && p->dpms_enabled_p);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_suspend_text")),
                       dpms_supported && p->dpms_enabled_p);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_off_label")),
                       dpms_supported && p->dpms_enabled_p);
    gtk_widget_set_sensitive (
                       GTK_WIDGET (name_to_widget (top, "dpms_off_text")),
                       dpms_supported && p->dpms_enabled_p);

    /* Colormaps
     */
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "cmap_frame")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "install_button")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "fade_button")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "unfade_button")),
                           found_any_writable_cells);

    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "fade_label")),
                           (found_any_writable_cells &&
                            (p->fade_p || p->unfade_p)));
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "fade_text")),
                           (found_any_writable_cells &&
                            (p->fade_p || p->unfade_p)));
  }

}


static void
sensitize_demo_widgets (GtkWidget *toplevel, Bool sensitive_p)
{
  const char *names[] = { "cmd_label", "cmd_text", "enabled",
                          "visual", "visual_combo",
                          "demo", "manual" };
  int i;
  for (i = 0; i < countof(names); i++)
    {
      GtkWidget *w = name_to_widget (toplevel, names[i]);
      gtk_widget_set_sensitive (GTK_WIDGET(w), sensitive_p);
    }

  /* I don't know how to handle these yet... */
  {
    const char *names2[] = { "cut_menu", "copy_menu", "paste_menu" };
    for (i = 0; i < countof(names2); i++)
      {
        GtkWidget *w = name_to_widget (toplevel, names2[i]);
        gtk_widget_set_sensitive (GTK_WIDGET(w), False);
      }
  }
}


/* Even though we've given these text fields a maximum number of characters,
   their default size is still about 30 characters wide -- so measure out
   a string in their font, and resize them to just fit that.
 */
static void
fix_text_entry_sizes (GtkWidget *toplevel)
{
  const char *names[] = { "timeout_text", "cycle_text", "lock_text",
                          "dpms_standby_text", "dpms_suspend_text",
                          "dpms_off_text", "fade_text" };
  int i;
  int width = 0;
  GtkWidget *w;

  for (i = 0; i < countof(names); i++)
    {
      w = GTK_WIDGET (name_to_widget (toplevel, names[i]));
      if (width == 0)
        width = gdk_text_width (w->style->font, "00:00:00_", 9);
      gtk_widget_set_usize (w, width, -2);
    }

  /* Now fix the size of the combo box.
   */
  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel), "visual_combo"));
  w = GTK_COMBO (w)->entry;
  width = gdk_text_width (w->style->font, "PseudoColor___", 14);
  gtk_widget_set_usize (w, width, -2);

  /* Now fix the size of the file entry text.
   */
  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel), "image_text"));
  width = gdk_text_width (w->style->font, "MMMMMMMMMMMMMM", 14);
  gtk_widget_set_usize (w, width, -2);

#if 0
  /* Now fix the size of the list.
   */
  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel), "list"));
  width = gdk_text_width (w->style->font, "nnnnnnnnnnnnnnnnnnnnnn", 22);
  gtk_widget_set_usize (w, width, -2);
#endif
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
pixmapify_button (GtkWidget *toplevel, int down_p)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkWidget *pixmapwid;
  GtkStyle *style;
  GtkWidget *w;

  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel),
                                  (down_p ? "next" : "prev")));
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
  pixmapify_button (w, 1);
}

static void
map_prev_button_cb (GtkWidget *w, gpointer user_data)
{
  pixmapify_button (w, 0);
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
eschew_gtk_lossage (GtkWidget *toplevel)
{
  GtkWidgetAuxInfo *aux_info;
  GtkWidget *label = GTK_WIDGET (name_to_widget (toplevel, "doc"));

  aux_info = g_new0 (GtkWidgetAuxInfo, 1);
  aux_info->width = label->allocation.width;
  aux_info->height = -2;
  aux_info->x = -1;
  aux_info->y = -1;

  gtk_object_set_data (GTK_OBJECT (label), "gtk-aux-info", aux_info);

  gtk_signal_connect (GTK_OBJECT (label), "size_allocate",
		      you_are_not_a_unique_or_beautiful_snowflake, NULL);

  gtk_widget_queue_resize (label); 
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
populate_demo_window (GtkWidget *toplevel, int which, prefs_pair *pair)
{
  saver_preferences *p = pair->a;
  screenhack *hack = (which >= 0 && which < p->screenhacks_count
		      ? p->screenhacks[which] : 0);
  GtkFrame *frame = GTK_FRAME (name_to_widget (toplevel, "frame"));
  GtkLabel *doc = GTK_LABEL (name_to_widget (toplevel, "doc"));
  GtkEntry *cmd = GTK_ENTRY (name_to_widget (toplevel, "cmd_text"));
  GtkToggleButton *enabled =
    GTK_TOGGLE_BUTTON (name_to_widget (toplevel, "enabled"));
  GtkCombo *vis = GTK_COMBO (name_to_widget (toplevel, "visual_combo"));

  char *pretty_name = (hack
                       ? (hack->name
                          ? strdup (hack->name)
                          : make_hack_name (hack->command))
                       : 0);
  char *doc_string = hack ? get_hack_blurb (hack) : 0;

  gtk_frame_set_label (frame, (pretty_name ? pretty_name : ""));
  gtk_label_set_text (doc, (doc_string ? doc_string : ""));
  gtk_entry_set_text (cmd, (hack ? hack->command : ""));
  gtk_entry_set_position (cmd, 0);

  updating_enabled_cb++;
  gtk_toggle_button_set_active (enabled, (hack ? hack->enabled_p : False));
  updating_enabled_cb--;

  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (vis)->entry),
                      (hack
                       ? (hack->visual && *hack->visual
                          ? hack->visual
                          : "Any")
                       : ""));

  gtk_container_resize_children (GTK_CONTAINER (GTK_WIDGET (doc)->parent));

  sensitize_demo_widgets (toplevel, (hack ? True : False));

  if (pretty_name) free (pretty_name);
  if (doc_string) free (doc_string);

  _selected_hack_number = which;
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


static int
maybe_reload_init_file (GtkWidget *widget, prefs_pair *pair)
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
      GtkList *list;

      if (!f || !*f) return 0;
      b = (char *) malloc (strlen(f) + 1024);
      sprintf (b,
               "Warning:\n\n"
               "file \"%s\" has changed, reloading.\n",
               f);
      warning_dialog (widget, b, False, 100);
      free (b);

      load_init_file (p);

      which = selected_hack_number (widget);
      list = GTK_LIST (name_to_widget (widget, "list"));
      gtk_container_foreach (GTK_CONTAINER (list), widget_deleter, NULL);
      populate_hack_list (widget, pair);
      gtk_list_select_item (list, which);
      populate_prefs_page (widget, pair);
      populate_demo_window (widget, which, pair);
      ensure_selected_item_visible (GTK_WIDGET (list));

      status = 1;
    }

  reentrant_lock = False;
  return status;
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
the_network_is_not_the_computer (GtkWidget *parent)
{
  Display *dpy = GDK_DISPLAY();
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
               "Restart the daemon on \"%s\" as \"%s\" now?\n",
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
	       "is version %s.  This could cause problems.\n"
              "\n"
              "Restart the xscreensaver daemon now?\n",
	       progname, short_version,
	       d,
	       rversion);
    }


  if (*msg)
    warning_dialog (parent, msg, True, 1);

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

#define USAGE() \
  fprintf (stderr, "usage: %s [ -display dpy-string ] [ -prefs ]\n", \
           real_progname)


static void
map_window_cb (GtkWidget *w, gpointer user_data)
{
  Boolean oi = initializing_p;
  initializing_p = True;
  eschew_gtk_lossage (w);
  ensure_selected_item_visible (GTK_WIDGET(name_to_widget(w, "list")));
  initializing_p = oi;
}


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
  GtkWidget *gtk_window;
  char *real_progname = argv[0];
  char *s;

  initializing_p = True;

  s = strrchr (real_progname, '/');
  if (s) real_progname = s+1;

  p = &P;
  p2 = &P2;
  pair = &Pair;
  pair->a = p;
  pair->b = p2;
  memset (p,  0, sizeof (*p));
  memset (p2, 0, sizeof (*p2));

  global_prefs_pair = pair;  /* I hate C so much... */

  progname = real_progname;

  short_version = (char *) malloc (5);
  memcpy (short_version, screensaver_id + 17, 4);
  short_version [4] = 0;


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
        USAGE ();
        exit (1);
# endif /* !HAVE_CRAPPLET */
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
                                             short_version,
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


  /* After doing Xt-style command-line processing, complain about any
     unrecognized command-line arguments.
   */
  for (i = 1; i < argc; i++)
    {
      char *s = argv[i];
      if (s[0] == '-' && s[1] == '-')
	s++;
      if (!strcmp (s, "-prefs"))
	prefs = True;
      else if (crapplet_p)
        /* There are lots of random args that we don't care about when we're
           started as a crapplet, so just ignore unknown args in that case. */
        ;
      else
	{
	  fprintf (stderr, "%s: unknown option: %s\n", real_progname, argv[i]);
          USAGE ();
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
  *p2 = *p;

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
  gtk_window = create_xscreensaver_demo ();
  toplevel_widget = gtk_window;

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
    gtk_window_set_title (GTK_WINDOW (gtk_window), title);
    free (v);
  }

  /* Various other widget initializations...
   */
  gtk_signal_connect (GTK_OBJECT (gtk_window), "delete_event",
                      GTK_SIGNAL_FUNC (wm_close_cb), NULL);

  populate_hack_list (gtk_window, pair);
  populate_prefs_page (gtk_window, pair);
  sensitize_demo_widgets (gtk_window, False);
  fix_text_entry_sizes (gtk_window);
  scroll_to_current_hack (gtk_window, pair);

  gtk_signal_connect (
              GTK_OBJECT (name_to_widget (GTK_WIDGET (gtk_window), "list")),
              "map", GTK_SIGNAL_FUNC(map_window_cb), 0);
  gtk_signal_connect (
              GTK_OBJECT (name_to_widget (GTK_WIDGET (gtk_window), "prev")),
              "map", GTK_SIGNAL_FUNC(map_prev_button_cb), 0);
  gtk_signal_connect (
              GTK_OBJECT (name_to_widget (GTK_WIDGET (gtk_window), "next")),
              "map", GTK_SIGNAL_FUNC(map_next_button_cb), 0);


  /* Handle the -prefs command-line argument. */
  if (prefs)
    {
      GtkNotebook *notebook =
        GTK_NOTEBOOK (name_to_widget (gtk_window, "notebook"));
      gtk_notebook_set_page (notebook, 1);
    }

# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    {
      GtkWidget *capplet;
      GtkWidget *top_vbox;

      capplet = capplet_widget_new ();

      top_vbox = GTK_BIN (gtk_window)->child;

      gtk_widget_ref (top_vbox);
      gtk_container_remove (GTK_CONTAINER (gtk_window), top_vbox);
      GTK_OBJECT_SET_FLAGS (top_vbox, GTK_FLOATING);

      /* In crapplet-mode, take off the menubar. */
      gtk_widget_hide (name_to_widget (gtk_window, "menubar"));

      gtk_container_add (GTK_CONTAINER (capplet), top_vbox);
      gtk_widget_show (capplet);
      gtk_widget_hide (gtk_window);

      /* Hook up the Control Center's redundant Help button, too. */
      gtk_signal_connect (GTK_OBJECT (capplet), "help",
                          GTK_SIGNAL_FUNC (doc_menu_cb), 0);

      /* Issue any warnings about the running xscreensaver daemon. */
      the_network_is_not_the_computer (top_vbox);
    }
  else
# endif /* HAVE_CRAPPLET */
    {
      gtk_widget_show (gtk_window);
      init_icon (GTK_WIDGET(gtk_window)->window);

      /* Issue any warnings about the running xscreensaver daemon. */
      the_network_is_not_the_computer (gtk_window);
    }

  /* Run the Gtk event loop, and not the Xt event loop.  This means that
     if there were Xt timers or fds registered, they would never get serviced,
     and if there were any Xt widgets, they would never have events delivered.
     Fortunately, we're using Gtk for all of the UI, and only initialized
     Xt so that we could process the command line and use the X resource
     manager.
   */
  initializing_p = False;

# ifdef HAVE_CRAPPLET
  if (crapplet_p)
    capplet_gtk_main ();
  else
# endif /* HAVE_CRAPPLET */
    gtk_main ();

  exit (0);
}

#endif /* HAVE_GTK -- whole file */
