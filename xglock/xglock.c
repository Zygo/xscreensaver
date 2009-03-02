/* ccc -I/usr/local/include -L/usr/X11R6/lib -L/usr/local/lib -lsocket -lnsl -lgtk -lgdk -lX11 -lposix4 -lm xglock.c */

#include <gtk/gtk.h>
#include "lmode.h"

void 
hello()
{
	g_print("hello world\n");
}

void 
byebye()
{
	exit(0);
}


void
destroy_window(GtkWidget * widget,
	       GtkWidget ** window)
{
	*window = NULL;
}

void
color_selection_ok(GtkWidget * w,
		   GtkColorSelectionDialog * cs)
{
	GtkColorSelection *colorsel;
	gdouble     color[4];

	colorsel = GTK_COLOR_SELECTION(cs->colorsel);

	gtk_color_selection_get_color(colorsel, color);
	gtk_color_selection_set_color(colorsel, color);
}

void
color_selection_changed(GtkWidget * w,
			GtkColorSelectionDialog * cs)
{
	GtkColorSelection *colorsel;
	gdouble     color[4];

	colorsel = GTK_COLOR_SELECTION(cs->colorsel);
	gtk_color_selection_get_color(colorsel, color);
}

void
create_color_selection()
{
	static GtkWidget *window = NULL;

	if (!window) {
		gtk_preview_set_install_cmap(TRUE);
		gtk_widget_push_visual(gtk_preview_get_visual());
		gtk_widget_push_colormap(gtk_preview_get_cmap());

		window = gtk_color_selection_dialog_new("color selection dialog");

		gtk_color_selection_set_opacity(
						       GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(window)->colorsel), TRUE);

		gtk_color_selection_set_update_policy(
							     GTK_COLOR_SELECTION(GTK_COLOR_SELECTION_DIALOG(window)->colorsel), GTK_UPDATE_CONTINUOUS);

		gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
		gtk_signal_connect(GTK_OBJECT(window), "destroy",
				   (GtkSignalFunc) destroy_window,
				   &window);

		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(window)->colorsel),
		    "color_changed", (GtkSignalFunc) color_selection_changed,
				   window);

		gtk_signal_connect(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(window)->ok_button),
			       "clicked", (GtkSignalFunc) color_selection_ok,
				   window);
		gtk_signal_connect_object(GTK_OBJECT(GTK_COLOR_SELECTION_DIALOG(window)->cancel_button),
			       "clicked", (GtkSignalFunc) gtk_widget_destroy,
					  GTK_OBJECT(window));

		gtk_widget_pop_colormap();
		gtk_widget_pop_visual();
	}
	if (!GTK_WIDGET_VISIBLE(window))
		gtk_widget_show(window);
	else
		gtk_widget_destroy(window);
}


void
file_selection_ok(GtkWidget * w,
		  GtkFileSelection * fs)
{
	g_print("%s\n", gtk_file_selection_get_filename(GTK_FILE_SELECTION(fs)));
}

void
create_file_selection()
{
	static GtkWidget *window = NULL;

	if (!window) {
		window = gtk_file_selection_new("file selection dialog");
		gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
		gtk_signal_connect(GTK_OBJECT(window), "destroy",
				   (GtkSignalFunc) destroy_window,
				   &window);

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(window)->ok_button),
				"clicked", (GtkSignalFunc) file_selection_ok,
				   window);
		gtk_signal_connect_object(GTK_OBJECT(GTK_FILE_SELECTION(window)->cancel_button),
			       "clicked", (GtkSignalFunc) gtk_widget_destroy,
					  GTK_OBJECT(window));
	}
	if (!GTK_WIDGET_VISIBLE(window))
		gtk_widget_show(window);
	else
		gtk_widget_destroy(window);
}

GtkWidget  *
create_menu(int depth)
{
	GtkWidget  *menu;
	GtkWidget  *submenu;
	GtkWidget  *menuitem;
	GSList     *group;
	char        buf[32];
	int         i, j;

	if (depth < 1)
		return NULL;

	menu = gtk_menu_new();
	submenu = NULL;
	group = NULL;

	for (i = 0, j = 1; i < 3; i++, j++) {
		sprintf(buf, "item %2d - %d", depth, j);
		menuitem = gtk_radio_menu_item_new_with_label(group, buf);
		group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menuitem));
		gtk_menu_append(GTK_MENU(menu), menuitem);
		gtk_widget_show(menuitem);

		if (depth > 0) {
			if (!submenu)
				submenu = create_menu(depth - 1);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
		}
	}

	return menu;
}

void
create_ft_and_color(GtkWidget * window)
{
	int         i;
	GtkWidget  *box0;
	GtkWidget  *box1;
	GtkWidget  *button;
	GtkWidget  *label;
	GtkWidget  *entry;
	GtkTooltips *tooltips;

	int         nb_opt = sizeof (fntcolorOpt) / sizeof (fntcolorOpt[0]);

	tooltips = gtk_tooltips_new();

	box0 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), box0);
	gtk_widget_show(box0);

	for (i = 0; i < nb_opt; i++) {
		box1 = gtk_hbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(box0), box1);
		gtk_widget_show(box1);

		label = gtk_label_new(fntcolorOpt[i].label);
		gtk_box_pack_start(GTK_BOX(box1), label, TRUE, TRUE, 0);
		gtk_widget_show(label);

		entry = gtk_entry_new();
		gtk_tooltips_set_tips(tooltips, entry, fntcolorOpt[i].desc);
		gtk_box_pack_start(GTK_BOX(box1), entry, TRUE, TRUE, 0);
		gtk_widget_show(entry);

		button = gtk_button_new_with_label("choice");
		GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
		if (i < 2)
			gtk_signal_connect(GTK_OBJECT(button), "clicked",
				      (GtkSignalFunc) create_color_selection,
					   button);
		else
			gtk_signal_connect(GTK_OBJECT(button), "clicked",
				       (GtkSignalFunc) create_file_selection,
					   button);
		gtk_tooltips_set_tips(tooltips, button, fntcolorOpt[i].desc);
		gtk_box_pack_start(GTK_BOX(box1), button, FALSE, TRUE, 0);
		gtk_widget_show(button);
	}
}
void
create_entry(GtkWidget * window)
{
	int         i;
	GtkWidget  *box0;
	GtkWidget  *box1;
	GtkWidget  *box2;
	GtkWidget  *button;
	GtkWidget  *label;
	GtkTooltips *tooltips;

	int         nb_opt = sizeof (generalOpt) / sizeof (generalOpt[0]);

	tooltips = gtk_tooltips_new();

	box0 = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), box0);
	gtk_widget_show(box0);

	box1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box0), box1);
	gtk_widget_show(box1);

	box2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box0), box2);
	gtk_widget_show(box2);

	for (i = 0; i < nb_opt; i++) {
		label = gtk_label_new(generalOpt[i].label);
		gtk_box_pack_start(GTK_BOX(box1), label, TRUE, TRUE, 0);
		gtk_widget_show(label);
	}
	for (i = 0; i < nb_opt; i++) {
		button = gtk_entry_new();
		gtk_tooltips_set_tips(tooltips, button, generalOpt[i].desc);
		gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
	}
}

void
create_check_buttons(GtkWidget * window)
{
	int         i;
	GtkWidget  *box0;
	GtkWidget  *box1;
	GtkWidget  *box2;
	GtkWidget  *button;
	GtkTooltips *tooltips;

	int         nb_opt = sizeof (BoolOpt) / sizeof (BoolOpt[0]);

	tooltips = gtk_tooltips_new();

	box0 = gtk_hbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), box0);
	gtk_widget_show(box0);

	box1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box0), box1);
	gtk_widget_show(box1);

	box2 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(box0), box2);
	gtk_widget_show(box2);

	for (i = 0; i < (nb_opt) / 2; i++) {
		button = gtk_check_button_new_with_label(BoolOpt[i].label);
		gtk_tooltips_set_tips(tooltips, button, BoolOpt[i].desc);
		gtk_box_pack_start(GTK_BOX(box1), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
	}
	for (i = (nb_opt) / 2; i < nb_opt; i++) {
		button = gtk_check_button_new_with_label(BoolOpt[i].label);
		gtk_tooltips_set_tips(tooltips, button, BoolOpt[i].desc);
		gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
		gtk_widget_show(button);
	}
}
int
main(int argc, char *argv[])
{
	GtkWidget  *window = NULL;



	GtkWidget  *box0;
	GtkWidget  *box1;
	GtkWidget  *box2;
	GtkWidget  *scrolled_win;
	GtkWidget  *list;
	GtkWidget  *list_item;
	GtkWidget  *button;
	GtkWidget  *separator;
	GtkWidget  *menubar;
	GtkWidget  *menuitem;
	GtkWidget  *menu;
	GtkWidget  *notebook;
	GtkWidget  *frame;
	GtkWidget  *label;
	GtkTooltips *tooltips;
	int         i;
	char        temp[100];

	int         nb_mode = sizeof (LockProcs) / sizeof (LockProcs[0]);

	gtk_init(&argc, &argv);
	gtk_rc_parse("simplerc");
	if (!window) {
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_set_name(window, "main window");
		gtk_window_set_title(GTK_WINDOW(window), "xlock");
		/*gtk_container_border_width (GTK_CONTAINER (window), 0); */

		tooltips = gtk_tooltips_new();

		box1 = gtk_vbox_new(FALSE, 0);
		gtk_container_add(GTK_CONTAINER(window), box1);
		gtk_widget_show(box1);


		box0 = gtk_vbox_new(FALSE, 10);
		gtk_container_border_width(GTK_CONTAINER(box0), 10);
		gtk_box_pack_start(GTK_BOX(box1), box0, TRUE, TRUE, 0);
		gtk_widget_show(box0);

		box2 = gtk_hbox_new(FALSE, 10);
		gtk_container_border_width(GTK_CONTAINER(box2), 10);
		gtk_box_pack_start(GTK_BOX(box1), box2, TRUE, TRUE, 0);
		gtk_widget_show(box2);

/* menu bar */

		menubar = gtk_menu_bar_new();
		gtk_box_pack_start(GTK_BOX(box0), menubar, FALSE, TRUE, 0);
		gtk_widget_show(menubar);

		menu = create_menu(2);

		menuitem = gtk_menu_item_new_with_label("test");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
		gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
		gtk_widget_show(menuitem);

		menuitem = gtk_menu_item_new_with_label("foo");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
		gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
		gtk_widget_show(menuitem);

		menuitem = gtk_menu_item_new_with_label("bar");
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), menu);
		gtk_menu_bar_append(GTK_MENU_BAR(menubar), menuitem);
		gtk_widget_show(menuitem);


/* notebook */
		notebook = gtk_notebook_new();
		gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
		gtk_box_pack_start(GTK_BOX(box2), notebook, TRUE, TRUE, 0);
		gtk_widget_show(notebook);

/* the list of the mode */
		/*sprintf (buffer, "Page %d", i+1); */

		frame = gtk_frame_new("Choice of the mode");
		gtk_container_border_width(GTK_CONTAINER(frame), 10);
		gtk_widget_set_usize(frame, 300, 250);

/* list */

		scrolled_win = gtk_scrolled_window_new(NULL, NULL);
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win),
				 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_container_add(GTK_CONTAINER(frame), scrolled_win);
		gtk_widget_show(scrolled_win);

		list = gtk_list_new();
		gtk_list_set_selection_mode(GTK_LIST(list), GTK_SELECTION_MULTIPLE);
		gtk_list_set_selection_mode(GTK_LIST(list), GTK_SELECTION_BROWSE);
		gtk_container_add(GTK_CONTAINER(scrolled_win), list);
		gtk_widget_show(list);

		for (i = 0; i < nb_mode; i++) {
			sprintf(temp, "%s :%s", LockProcs[i].cmdline_arg, LockProcs[i].desc);
			list_item = gtk_list_item_new_with_label(temp);
			gtk_container_add(GTK_CONTAINER(list), list_item);
			gtk_widget_show(list_item);
		}

		label = gtk_label_new("choice the mode");
		gtk_widget_show(frame);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);

/* Options */

		frame = gtk_frame_new("Options booleans");
		gtk_container_border_width(GTK_CONTAINER(frame), 10);
		gtk_widget_set_usize(frame, 200, 150);
		gtk_widget_show(frame);
		create_check_buttons(frame);
		label = gtk_label_new("Options");
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
/* General Options */

		frame = gtk_frame_new("General Options");
		gtk_container_border_width(GTK_CONTAINER(frame), 10);
		gtk_widget_set_usize(frame, 200, 150);
		gtk_widget_show(frame);
		create_entry(frame);
		label = gtk_label_new("General Options");
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);
/* Color Options */

		frame = gtk_frame_new("Color Options");
		gtk_container_border_width(GTK_CONTAINER(frame), 10);
		gtk_widget_set_usize(frame, 200, 150);
		gtk_widget_show(frame);
		create_ft_and_color(frame);
		label = gtk_label_new("Font & Color Options");
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), frame, label);



/* end of notebook */
		separator = gtk_hseparator_new();
		gtk_box_pack_start(GTK_BOX(box1), separator, FALSE, TRUE, 0);
		gtk_widget_show(separator);

		box2 = gtk_hbox_new(FALSE, 10);
		gtk_container_border_width(GTK_CONTAINER(box2), 10);
		gtk_box_pack_start(GTK_BOX(box1), box2, FALSE, TRUE, 0);
		gtk_widget_show(box2);
/* button */
		button = gtk_button_new_with_label("launch");
		GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   (GtkSignalFunc) hello,
				   list);
		gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
		gtk_widget_show(button);

		button = gtk_button_new_with_label("launch in window");
		GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
		gtk_signal_connect(GTK_OBJECT(button), "clicked",
				   (GtkSignalFunc) hello,
				   list);
		gtk_box_pack_start(GTK_BOX(box2), button, FALSE, TRUE, 0);
		gtk_widget_show(button);


		button = gtk_button_new_with_label("exit");
		gtk_tooltips_set_tips(tooltips, button, "Exit");
		gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
					  (GtkSignalFunc) byebye,
					  GTK_OBJECT(window));
		gtk_box_pack_start(GTK_BOX(box2), button, TRUE, TRUE, 0);
		GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
		gtk_widget_grab_default(button);
		gtk_widget_show(button);
	}
	if (!GTK_WIDGET_VISIBLE(window))
		gtk_widget_show(window);
	else
		gtk_widget_destroy(window);

	gtk_main();

	return 0;
}
