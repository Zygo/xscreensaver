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

extern conf_data *load_configurator (const char *cmd_line, gboolean verbose_p);
extern char *get_configurator_command_line (conf_data *, gboolean default_p);
extern void  set_configurator_command_line (conf_data *, const char *cmd_line);
extern void free_conf_data (conf_data *);

extern const char *hack_configuration_path;

#endif /* _DEMO_GTK_CONF_H_ */
