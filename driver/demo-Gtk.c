/* demo-Gtk.c --- implements the interactive demo-mode and options dialogs.
 * xscreensaver, Copyright (c) 1993-1999 Jamie Zawinski <jwz@jwz.org>
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

extern Display *gdk_display;

#include "version.h"
#include "prefs.h"
#include "resources.h"		/* for parse_time() */
#include "visual.h"		/* for has_writable_cells() */
#include "remote.h"		/* for xscreensaver_command() */
#include "usleep.h"

#include "demo-Gtk-widgets.h"

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


/* Some random utility functions
 */

static GtkWidget *
name_to_widget (GtkWidget *widget, const char *name)
{
  while (1)
    {
      GtkWidget *parent = (GTK_IS_MENU (widget)
                           ? gtk_menu_get_attach_widget (GTK_MENU (widget))
                           : widget->parent);
      if (parent)
        widget = parent;
      else
        break;
    }
  return (GtkWidget *) gtk_object_get_data (GTK_OBJECT (widget), name);
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
warning_dialog_dismiss_cb (GtkButton *button, gpointer user_data)
{
  GtkWidget *shell = GTK_WIDGET (user_data);
  while (shell->parent)
    shell = shell->parent;
  gtk_widget_destroy (GTK_WIDGET (shell));
}


static void
warning_dialog (GtkWidget *parent, const char *message, int center)
{
  char *msg = strdup (message);
  char *head;

  GtkWidget *dialog = gtk_dialog_new ();
  GtkWidget *label = 0;
  GtkWidget *ok = 0;
  int i = 0;

  while (parent->parent)
    parent = parent->parent;

  head = msg;
  while (head)
    {
      char name[20];
      char *s = strchr (head, '\n');
      if (s) *s = 0;

      sprintf (name, "label%d", i++);

      {
        char buf[255];
        label = gtk_label_new (head);
        sprintf (buf, "warning_dialog.%s.font", name);
        GTK_WIDGET (label)->style = gtk_style_copy (GTK_WIDGET (label)->style);
        GTK_WIDGET (label)->style->font =
          gdk_font_load (get_string_resource (buf, "Dialog.Label.Font"));
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

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 10);
  gtk_window_set_title (GTK_WINDOW (dialog), progclass);
  gtk_widget_show (ok);
  gtk_widget_show (label);
  gtk_widget_show (dialog);
/*  gtk_window_set_default (GTK_WINDOW (dialog), ok);*/

  gtk_signal_connect_object (GTK_OBJECT (ok), "clicked",
                             GTK_SIGNAL_FUNC (warning_dialog_dismiss_cb),
                             (gpointer) dialog);
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
  status = xscreensaver_command (gdk_display, command, arg, False, &err);
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
run_hack (GtkWidget *widget, int which, Bool report_errors_p)
{
  if (which < 0) return;
  apply_changes_and_save (widget);
  if (report_errors_p)
    run_cmd (widget, XA_DEMO, which + 1);
  else
    {
      char *s = 0;
      xscreensaver_command (gdk_display, XA_DEMO, which + 1, False, &s);
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
                  "cut unimplemented\n", 1);
}


void
copy_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  /* #### */
  warning_dialog (GTK_WIDGET (menuitem),
                  "Error:\n\n"
                  "copy unimplemented\n", 1);
}


void
paste_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
  /* #### */
  warning_dialog (GTK_WIDGET (menuitem),
                  "Error:\n\n"
                  "paste unimplemented\n", 1);
}


void
about_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
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

  warning_dialog (GTK_WIDGET (menuitem), buf, 100);
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
restart_menu_cb (GtkMenuItem *menuitem, gpointer user_data)
{
#if 0
  run_cmd (GTK_WIDGET (menuitem), XA_RESTART, 0);
#else
  apply_changes_and_save (GTK_WIDGET (menuitem));
  xscreensaver_command (gdk_display, XA_EXIT, 0, False, NULL);
  sleep (1);
  system ("xscreensaver -nosplash &");
#endif
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
apply_changes_and_save (GtkWidget *widget)
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
                      100);
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
hack_time_text (const char *line, Time *store, Bool sec_p)
{
  if (*line)
    {
      int value;
      value = parse_time ((char *) line, sec_p, True);
      value *= 1000;	/* Time measures in microseconds */
      if (value < 0)
        /* gdk_beep () */;
      else
	*store = value;
    }
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
  hack_time_text (gtk_entry_get_text (\
                    GTK_ENTRY (name_to_widget (GTK_WIDGET(button), (name)))), \
                  (field), \
                  True)

# define MINUTES(field, name) \
  hack_time_text (gtk_entry_get_text (\
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
     gdk_beep(); \
   else \
     *(field) = value; \
  } while(0)

# define CHECKBOX(field, name) \
  field = gtk_toggle_button_get_active (\
             GTK_TOGGLE_BUTTON (name_to_widget (GTK_WIDGET(button), (name))))

  MINUTES (&p2->timeout,        "timeout_text");
  MINUTES (&p2->cycle,          "cycle_text");
  SECONDS (&p2->fade_seconds,   "fade_text");
  INTEGER (&p2->fade_ticks,     "ticks_text");
  MINUTES (&p2->lock_timeout,   "lock_text");
  SECONDS (&p2->passwd_timeout, "pass_text");
  CHECKBOX (p2->verbose_p,      "verbose_button");
  CHECKBOX (p2->install_cmap_p, "install_button");
  CHECKBOX (p2->fade_p,         "fade_button");
  CHECKBOX (p2->unfade_p,       "unfade_button");
  CHECKBOX (p2->lock_p,         "lock_button");

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

  populate_prefs_page (GTK_WIDGET (button), pair);

  if (changed)
    demo_write_init_file (GTK_WIDGET (button), p);
}


void
prefs_cancel_cb (GtkButton *button, gpointer user_data)
{
  /* prefs_pair *pair = (prefs_pair *) client_data; */
  prefs_pair *pair = global_prefs_pair;  /* I hate C so much... */

  *pair->b = *pair->a;
  populate_prefs_page (GTK_WIDGET (button), pair);
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


static char *
make_pretty_name (const char *shell_command)
{
  char *s = strdup (shell_command);
  char *s2;
  char res_name[255];

  for (s2 = s; *s2; s2++)	/* truncate at first whitespace */
    if (isspace (*s2))
      {
        *s2 = 0;
        break;
      }

  s2 = strrchr (s, '/');	/* if pathname, take last component */
  if (s2)
    {
      s2 = strdup (s2+1);
      free (s);
      s = s2;
    }

  if (strlen (s) > 50)		/* 51 is hereby defined as "unreasonable" */
    s[50] = 0;

  sprintf (res_name, "hacks.%s.name", s);		/* resource? */
  s2 = get_string_resource (res_name, res_name);
  if (s2)
    return s2;

  for (s2 = s; *s2; s2++)	/* if it has any capitals, return it */
    if (*s2 >= 'A' && *s2 <= 'Z')
      return s;

  if (s[0] >= 'a' && s[0] <= 'z')			/* else cap it */
    s[0] -= 'a'-'A';
  if (s[0] == 'X' && s[1] >= 'a' && s[1] <= 'z')	/* (magic leading X) */
    s[1] -= 'a'-'A';
  return s;
}


/* Finds the number of the last hack to run, and makes that item be
   selected by default.
 */
static void
scroll_to_current_hack (GtkWidget *toplevel, prefs_pair *pair)
{
  Atom type;
  int format;
  unsigned long nitems, bytesafter;
  CARD32 *data = 0;
  Display *dpy = gdk_display;
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
  gtk_list_select_item (list, which);
  ensure_selected_item_visible (GTK_WIDGET (list));
  populate_demo_window (toplevel, which, pair);
}



static void
populate_hack_list (GtkWidget *toplevel, prefs_pair *pair)
{
  saver_preferences *p =  pair->a;
  GtkList *list = GTK_LIST (name_to_widget (toplevel, "list"));
  screenhack **hacks = p->screenhacks;
  screenhack **h;

  for (h = hacks; *h; h++)
    {
      GtkWidget *line;
      char *pretty_name = (h[0]->name
                           ? strdup (h[0]->name)
                           : make_pretty_name (h[0]->command));

      line = gtk_list_item_new_with_label (pretty_name);
      free (pretty_name);

      gtk_container_add (GTK_CONTAINER (list), line);
      gtk_signal_connect (GTK_OBJECT (line), "button_press_event",
                          GTK_SIGNAL_FUNC (list_doubleclick_cb),
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
  format_time (s, p->passwd_timeout);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "pass_text")), s);
  format_time (s, p->fade_seconds);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "fade_text")), s);
  sprintf (s, "%u", p->fade_ticks);
  gtk_entry_set_text (GTK_ENTRY (name_to_widget (top, "ticks_text")), s);

  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "verbose_button")),
                   p->verbose_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "install_button")),
                   p->install_cmap_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "fade_button")),
                   p->fade_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "unfade_button")),
                   p->unfade_p);
  gtk_toggle_button_set_active (
                   GTK_TOGGLE_BUTTON (name_to_widget (top, "lock_button")),
                   p->lock_p);


  {
    Bool found_any_writable_cells = False;
    Display *dpy = gdk_display;
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

    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "fade_label")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "ticks_label")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "fade_text")),
                           found_any_writable_cells);
    gtk_widget_set_sensitive (
                           GTK_WIDGET (name_to_widget (top, "ticks_text")),
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
  const char *names[] = { "timeout_text", "cycle_text", "fade_text",
                          "ticks_text", "lock_text", "pass_text" };
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
pixmapify_buttons (GtkWidget *toplevel)
{
  GdkPixmap *pixmap;
  GdkBitmap *mask;
  GtkWidget *pixmapwid;
  GtkStyle *style;
  GtkWidget *w;

  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel), "next"));
  style = gtk_widget_get_style (w);
  mask = 0;
  pixmap = gdk_pixmap_create_from_xpm_d (w->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         (gchar **) down_arrow_xpm);
  pixmapwid = gtk_pixmap_new (pixmap, mask);
  gtk_widget_show (pixmapwid);
  gtk_container_remove (GTK_CONTAINER (w), GTK_BIN (w)->child);
  gtk_container_add (GTK_CONTAINER (w), pixmapwid);

  w = GTK_WIDGET (name_to_widget (GTK_WIDGET (toplevel), "prev"));
  style = gtk_widget_get_style (w);
  mask = 0;
  pixmap = gdk_pixmap_create_from_xpm_d (w->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         (gchar **) up_arrow_xpm);
  pixmapwid = gtk_pixmap_new (pixmap, mask);
  gtk_widget_show (pixmapwid);
  gtk_container_remove (GTK_CONTAINER (w), GTK_BIN (w)->child);
  gtk_container_add (GTK_CONTAINER (w), pixmapwid);
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
                       : make_pretty_name (hack->command));
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
  screenhack *hack = (which >= 0 ? p->screenhacks[which] : 0);
  GtkFrame *frame = GTK_FRAME (name_to_widget (toplevel, "frame"));
  GtkLabel *doc = GTK_LABEL (name_to_widget (toplevel, "doc"));
  GtkEntry *cmd = GTK_ENTRY (name_to_widget (toplevel, "cmd_text"));
  GtkToggleButton *enabled =
    GTK_TOGGLE_BUTTON (name_to_widget (toplevel, "enabled"));
  GtkCombo *vis = GTK_COMBO (name_to_widget (toplevel, "visual_combo"));

  char *pretty_name = (hack
                       ? (hack->name
                          ? strdup (hack->name)
                          : make_pretty_name (hack->command))
                       : 0);
  char *doc_string = hack ? get_hack_blurb (hack) : 0;

  gtk_frame_set_label (frame, (pretty_name ? pretty_name : ""));
  gtk_label_set_text (doc, (doc_string ? doc_string : ""));
  gtk_entry_set_text (cmd, (hack ? hack->command : ""));
  gtk_entry_set_position (cmd, 0);
  gtk_toggle_button_set_active (enabled, (hack ? hack->enabled_p : False));
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
      warning_dialog (widget, b, 100);
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
  Display *dpy = gdk_display;
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

  /* Let Gtk open the X connection, then initialize Xt to use that
     same connection.  Doctor Frankenstein would be proud. */   
  gtk_init (&argc, &argv);


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
  dpy = gdk_display;
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
      else
	{
	  fprintf (stderr, "usage: %s [ -display dpy-string ] [ -prefs ]\n",
		   real_progname);
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
  gtk_widget_show (gtk_window);

  /* The next three calls must come after gtk_widget_show(). */
  pixmapify_buttons (gtk_window);
  eschew_gtk_lossage (gtk_window);
  ensure_selected_item_visible (GTK_WIDGET(name_to_widget(gtk_window,"list")));

  /* Handle the -prefs command-line argument. */
  if (prefs)
    {
      GtkNotebook *notebook =
        GTK_NOTEBOOK (name_to_widget (gtk_window, "notebook"));
      gtk_notebook_set_page (notebook, 1);
    }

  /* Issue any warnings about the running xscreensaver daemon. */
  the_network_is_not_the_computer (gtk_window);

  /* Run the Gtk event loop, and not the Xt event loop.  This means that
     if there were Xt timers or fds registered, they would never get serviced,
     and if there were any Xt widgets, they would never have events delivered.
     Fortunately, we're using Gtk for all of the UI, and only initialized
     Xt so that we could process the command line and use the X resource
     manager.
   */
  gtk_main ();
  exit (0);
}

#endif /* HAVE_GTK -- whole file */
