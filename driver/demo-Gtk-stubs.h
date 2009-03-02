#include <gtk/gtk.h>


void
activate_menu_cb                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
lock_menu_cb                           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
kill_menu_cb                           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
restart_menu_cb                        (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
exit_menu_cb                           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
cut_menu_cb                            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
copy_menu_cb                           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
paste_menu_cb                          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
about_menu_cb                          (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
doc_menu_cb                            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
run_next_cb                            (GtkButton       *button,
                                        gpointer         user_data);

void
run_prev_cb                            (GtkButton       *button,
                                        gpointer         user_data);

void
run_this_cb                            (GtkButton       *button,
                                        gpointer         user_data);

void
apply_manual_cb                        (GtkButton       *button,
                                        gpointer         user_data);

void
prefs_ok_cb                            (GtkButton       *button,
                                        gpointer         user_data);

void
prefs_cancel_cb                        (GtkButton       *button,
                                        gpointer         user_data);

void
manual_cb                              (GtkButton       *button,
                                        gpointer         user_data);
