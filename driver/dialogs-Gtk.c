/* dialogs-Gtk.c --- Gtk widgets for demo, options, and password dialogs.
 * xscreensaver, Copyright (c) 1999 Jamie Zawinski <jwz@jwz.org>
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

#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include "resources.h"

#include <stdio.h>

GtkWidget *preferences_dialog;
GtkWidget *preferences_form;
GtkWidget *timeout_text;
GtkWidget *cycle_text;
GtkWidget *fade_text;
GtkWidget *fade_ticks_text;
GtkWidget *lock_timeout_text;
GtkWidget *passwd_timeout_text;
GtkWidget *verbose_toggle;
GtkWidget *install_cmap_toggle;
GtkWidget *fade_toggle;
GtkWidget *unfade_toggle;
GtkWidget *lock_toggle;
GtkWidget *prefs_done;
GtkWidget *prefs_cancel;

GtkWidget *demo_dialog;
GtkWidget *demo_form;
GtkWidget *label1;
GtkWidget *label2;
GtkWidget *demo_list;
GtkWidget *text_line;
GtkWidget *text_activate;
GtkWidget *next;
GtkWidget *prev;
GtkWidget *edit;
GtkWidget *done;
GtkWidget *restart;  /* #### */

/* This is a Gtk program that uses Xrm for localization and preferences --
   may god forgive me for what I have unleashed. */
static char *
STR (char *resource)
{
  return (get_string_resource (resource, resource));
}


void
create_preferences_dialog (void *ignore1, void *ignore2, void *ignore3)
{
  GtkWidget *window, *box1, *box2, *box3;
  GtkWidget *label;
  GtkWidget *timeout_label;
  GtkWidget *cycle_label;
  GtkWidget *faded_label;
  GtkWidget *fade_ticks_label;
  GtkWidget *lock_timeout_label;
  GtkWidget *passwd_timeout_label;
  int entry_width;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  box1 = gtk_vbox_new (FALSE, 6);
  box2 = gtk_table_new (6, 3, FALSE);
  box3 = gtk_hbutton_box_new ();

  /* Create the labels. */
  label = gtk_label_new (STR("preferences_dialog.label1.label"));
  timeout_label = gtk_label_new (STR("preferences_dialog.timeout.label"));
  cycle_label = gtk_label_new (STR("preferences_dialog.cycle.label"));
  faded_label = gtk_label_new (STR("preferences_dialog.fade.label"));
  fade_ticks_label = gtk_label_new (STR("preferences_dialog.ticks.label"));
  lock_timeout_label = gtk_label_new(STR("preferences_dialog.lockTime.label"));
  passwd_timeout_label =
    gtk_label_new (STR("preferences_dialog.passwdTime.label"));

  /* Make the labels right-justify. */
  gtk_misc_set_alignment (GTK_MISC (timeout_label), 1.0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (cycle_label), 1.0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (faded_label), 1.0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (fade_ticks_label), 1.0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (lock_timeout_label), 1.0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (passwd_timeout_label), 1.0, 0.5);

  prefs_done =
    gtk_button_new_with_label (STR("preferences_dialog.done.label"));
  prefs_cancel =
    gtk_button_new_with_label (STR("preferences_dialog.cancel.label"));

  /* Create the text-entry widgets. */
  timeout_text = gtk_entry_new_with_max_length (8);
  cycle_text = gtk_entry_new_with_max_length (8);
  fade_text = gtk_entry_new_with_max_length (8);
  fade_ticks_text = gtk_entry_new_with_max_length (8);
  lock_timeout_text = gtk_entry_new_with_max_length (8);
  passwd_timeout_text = gtk_entry_new_with_max_length (8);

  /* Set their sizes. */
  entry_width = gdk_text_width (GTK_WIDGET (timeout_text)->style->font,
				"00:00:00 ", 9);
  gtk_widget_set_usize (GTK_WIDGET (timeout_text), entry_width, -2);
  gtk_widget_set_usize (GTK_WIDGET (cycle_text), entry_width, -2);
  gtk_widget_set_usize (GTK_WIDGET (fade_text), entry_width, -2);
  gtk_widget_set_usize (GTK_WIDGET (fade_ticks_text), entry_width, -2);
  gtk_widget_set_usize (GTK_WIDGET (lock_timeout_text), entry_width, -2);
  gtk_widget_set_usize (GTK_WIDGET (passwd_timeout_text), entry_width, -2);

  verbose_toggle = gtk_check_button_new_with_label
    (STR("preferences_dialog.buttonbox.verbose.label"));
  install_cmap_toggle = gtk_check_button_new_with_label
    (STR("preferences_dialog.buttonbox.cmap.label"));
  fade_toggle = gtk_check_button_new_with_label
    (STR("preferences_dialog.buttonbox.fade.label"));
  unfade_toggle = gtk_check_button_new_with_label
    (STR("preferences_dialog.buttonbox.unfade.label"));
  lock_toggle = gtk_check_button_new_with_label
    (STR("preferences_dialog.buttonbox.lock.label"));

  gtk_box_pack_start (GTK_BOX(box1), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box1), box2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box1), box3, FALSE, FALSE, 0);

# define FROB(widget, x, y) \
    gtk_table_attach_defaults (GTK_TABLE(box2), widget, x, x+1, y, y+1)

  FROB (timeout_label,		0, 0);
  FROB (cycle_label,		0, 1);
  FROB (faded_label,		0, 2);
  FROB (fade_ticks_label,	0, 3);
  FROB (lock_timeout_label,	0, 4);
  FROB (passwd_timeout_label,	0, 5);

  FROB (timeout_text,		1, 0);
  FROB (cycle_text,		1, 1);
  FROB (fade_text,		1, 2);
  FROB (fade_ticks_text,	1, 3);
  FROB (lock_timeout_text,	1, 4);
  FROB (passwd_timeout_text,	1, 5);

  FROB (verbose_toggle,		2, 0);
  FROB (install_cmap_toggle,	2, 1);
  FROB (fade_toggle,		2, 2);
  FROB (unfade_toggle,		2, 3);
  FROB (lock_toggle,		2, 4);
# undef FROB
  gtk_table_set_col_spacings (GTK_TABLE(box2), 10);

  gtk_box_pack_start (GTK_BOX(box3), prefs_done, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box3), prefs_cancel, TRUE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (window), box1);

  gtk_widget_show_all (window);

  preferences_dialog = window;
  preferences_form = window;
}


void
create_demo_dialog (void *ignore1, void *ignore2, void *ignore3)
{
  GtkWidget *window, *box1, *box2, *box3, *list;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  /* Set the minimum size. */
  gtk_widget_set_usize (GTK_WIDGET (window), 1, 1);

  /* Set the default size. */
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 300);

  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  box1 = gtk_vbox_new (FALSE, 6);
  box2 = gtk_hbutton_box_new ();
  box3 = gtk_hbox_new (FALSE, 6);
  list = gtk_list_new ();

  label1 = gtk_label_new (STR("label1.label"));
  label2 = gtk_label_new (STR("label2.label"));

  demo_list = gtk_scrolled_window_new (0, 0);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (demo_list),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (demo_list),
                                         list);
  gtk_widget_show (list);

  text_line = gtk_entry_new ();
  gtk_entry_set_editable (GTK_ENTRY (text_line), TRUE);
  text_activate = gtk_button_new_with_label (STR("demo_dialog.run.label"));

  GTK_WIDGET (text_line)->style =
    gtk_style_copy (GTK_WIDGET (text_line)->style);
  GTK_WIDGET (text_line)->style->font =
    gdk_font_load (STR("demo_dialog.Text.font"));

  next = gtk_button_new_with_label (STR("demo_dialog.next.label"));
  prev = gtk_button_new_with_label (STR("demo_dialog.prev.label"));
  edit = gtk_button_new_with_label (STR("demo_dialog.edit.label"));
  done = gtk_button_new_with_label (STR("demo_dialog.done.label"));

  gtk_widget_show (box1);
  gtk_widget_show (box2);
  gtk_widget_show (box3);
  gtk_widget_show (label1);
  gtk_widget_show (label2);
  gtk_widget_show (demo_list);
  gtk_widget_show (text_activate);
  gtk_widget_show (text_line);
  gtk_widget_show (next);
  gtk_widget_show (prev);
  gtk_widget_show (edit);
  gtk_widget_show (done);

  gtk_box_pack_start (GTK_BOX(box3), text_line, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX(box3), text_activate, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX(box1), label1, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box1), label2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box1), demo_list, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX(box1), box3, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box1), box2, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX(box2), next, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box2), prev, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box2), edit, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX(box2), done, TRUE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (window), box1);

  demo_dialog = window;
  demo_form = window;
}
