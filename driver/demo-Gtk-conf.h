/* demo-Gtk-conf.c --- implements the dynamic configuration dialogs.
 * xscreensaver, Copyright © 2001-2022 Jamie Zawinski <jwz@jwz.org>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.  No representations are made about the suitability of this
 * software for any purpose.  It is provided "as is" without express or 
 * implied warranty.
 */

#ifndef _DEMO_GTK_CONF_H_
#define _DEMO_GTK_CONF_H_

typedef struct {
  GtkWidget *widget;  /* the container widget with the sliders and stuff. */
  GList *parameters;  /* internal data -- hands off */
  char *progname;
  char *progclass;
  char *description;
  int year;
} conf_data;

/* Referenced by demo-Gtk.c; defined in demo-Gtk-conf.c.
 */
extern conf_data *load_configurator (const char *cmd_line,
                                     void (*changed_cb) (GtkWidget *, gpointer),
                                     gpointer changed_data,
                                     gboolean verbose_p);
extern char *get_configurator_command_line (conf_data *, gboolean default_p);
extern void  set_configurator_command_line (conf_data *, const char *cmd_line);
extern void free_conf_data (conf_data *);

/* Referenced from demo.ui and prefs.ui; defined in demo-Gtk.c.
 */
extern void quit_menu_cb (GtkAction *, gpointer user_data);
extern void about_menu_cb (GtkAction *, gpointer user_data);
extern void doc_menu_cb (GtkAction *, gpointer user_data);
extern void file_menu_cb (GtkAction *, gpointer user_data);
extern void activate_menu_cb (GtkAction *, gpointer user_data);
extern void lock_menu_cb (GtkAction *, gpointer user_data);
extern void kill_menu_cb (GtkAction *, gpointer user_data);
extern void restart_menu_cb (GtkWidget *, gpointer user_data);
extern void run_this_cb (GtkButton *, gpointer user_data);
extern void manual_cb (GtkButton *, gpointer user_data);
extern void run_next_cb (GtkButton *, gpointer user_data);
extern void run_prev_cb (GtkButton *, gpointer user_data);
extern gboolean pref_changed_cb (GtkWidget *, gpointer user_data);
extern gboolean pref_changed_event_cb (GtkWidget *, GdkEvent *, gpointer data);
extern gboolean image_text_pref_changed_event_cb (GtkWidget *, GdkEvent *,
                                                  gpointer user_data);
extern void mode_menu_item_cb (GtkWidget *, gpointer user_data);
extern void switch_page_cb (GtkNotebook *, GtkWidget *, 
                            gint page_num, gpointer user_data);
extern void browse_image_dir_cb (GtkButton *, gpointer user_data);
extern void browse_text_file_cb (GtkButton *, gpointer user_data);
extern void browse_text_program_cb (GtkButton *, gpointer user_data);
extern void settings_cb (GtkButton *, gpointer user_data);
extern void settings_adv_cb (GtkButton *, gpointer user_data);
extern void settings_std_cb (GtkButton *, gpointer user_data);
extern void settings_reset_cb (GtkButton *, gpointer user_data);
extern void settings_switch_page_cb (GtkNotebook *, GtkWidget *,
                                     gint page_num, gpointer user_data);
extern void settings_cancel_cb (GtkWidget *, gpointer user_data);
extern void settings_ok_cb (GtkWidget *, gpointer user_data);
extern void preview_theme_cb (GtkWidget *, gpointer user_data);

/* Referenced by demo-Gtk-conf.c; defined in demo-Gtk.c.
 */
extern void warning_dialog (GtkWindow *, const char *title, const char *msg);
extern gboolean file_chooser (GtkWindow *, GtkEntry *, char **retP,
                              const char *title,
                              gboolean verbose_p,
                              gboolean dir_p, gboolean program_p);

#endif /* _DEMO_GTK_CONF_H_ */
