/*
 * $Id$
 *
 * ROX-Filer, filer for the ROX desktop project
 * Copyright (C) 2000, Thomas Leonard, <tal197@users.sourceforge.net>.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* icon.c - shared code for the pinboard and panels */

/* TODO: Lots more duplicated code should be moved into here! */

#include "config.h"

#include <string.h>

#include <gtk/gtk.h>

#include "global.h"

#include "main.h"
#include "gui_support.h"
#include "support.h"
#include "pinboard.h"
#include "panel.h"
#include "icon.h"


/* Each entry is a GList of Icons which have the given pathname.
 * This allows us to update all necessary icons when something changes.
 */
static GHashTable *icons_hash = NULL;

/* Static prototypes */
static void rename_activate(GtkWidget *dialog);

/****************************************************************
 *			EXTERNAL INTERFACE			*
 ****************************************************************/

void icon_init(void)
{
	icons_hash = g_hash_table_new(g_str_hash, g_str_equal);
}

/* Opens a box allowing the user to change the name of a pinned icon.
 * If 'widget' is destroyed then the box will close.
 * If the user chooses OK then the callback is called once the icon's
 * name, src_path and path fields have been updated and the item field
 * restatted.
 */
void show_rename_box(GtkWidget *widget, Icon *icon, RenameFn callback)
{
	GtkWidget	*dialog, *hbox, *vbox, *label, *entry, *button;

	dialog = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Edit Icon"));
	gtk_container_set_border_width(GTK_CONTAINER(dialog), 10);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(dialog), vbox);
	
	label = gtk_label_new(_("Clicking the icon opens:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, TRUE, FALSE, 2);
	gtk_entry_set_text(GTK_ENTRY(entry), icon->src_path);
	gtk_object_set_data(GTK_OBJECT(dialog), "new_path", entry);
	gtk_signal_connect_object(GTK_OBJECT(entry), "activate",
			rename_activate, GTK_OBJECT(dialog));

	gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 2);

	label = gtk_label_new(_("The text displayed under the icon is:"));
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, TRUE, FALSE, 2);
	gtk_entry_set_text(GTK_ENTRY(entry), icon->item.leafname);
	gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	gtk_widget_grab_focus(entry);
	gtk_object_set_data(GTK_OBJECT(dialog), "new_name", entry);
	gtk_signal_connect_object(GTK_OBJECT(entry), "activate",
			rename_activate, GTK_OBJECT(dialog));
	
	gtk_signal_connect_object_while_alive(GTK_OBJECT(widget),
			"destroy",
			GTK_SIGNAL_FUNC(gtk_widget_destroy),
			GTK_OBJECT(dialog));

	gtk_object_set_data(GTK_OBJECT(dialog), "callback_icon", icon);
	gtk_object_set_data(GTK_OBJECT(dialog), "callback_fn", callback);

	gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 2);

	hbox = gtk_hbox_new(TRUE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	button = gtk_button_new_with_label(_("OK"));
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_window_set_default(GTK_WINDOW(dialog), button);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(rename_activate), GTK_OBJECT(dialog));
	
	button = gtk_button_new_with_label(_("Cancel"));
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(gtk_widget_destroy),
			GTK_OBJECT(dialog));

	gtk_widget_show_all(dialog);
}

/* Removes trailing / chars and converts a leading '~/' (if any) to
 * the user's home dir. g_free() the result.
 */
guchar *icon_convert_path(guchar *path)
{
	guchar		*retval;
	int		path_len;

	g_return_val_if_fail(path != NULL, NULL);

	path_len = strlen(path);
	while (path_len > 1 && path[path_len - 1] == '/')
		path_len--;
	
	retval = g_strndup(path, path_len);

	if (path[0] == '~' && (path[1] == '\0' || path[1] == '/'))
	{
		guchar *tmp = retval;

		retval = g_strconcat(home_dir, retval + 1, NULL);
		g_free(tmp);
	}

	return retval;
}

/* The icons_hash table allows us to convert from a path to a list
 * of icons that use that path.
 * Add this icon to the list for its path.
 */
void icon_hash_path(Icon *icon)
{
	GList	*list;

	g_return_if_fail(icon != NULL);

	/* g_print("[ hashing '%s' ]\n", icon->path); */

	list = g_hash_table_lookup(icons_hash, icon->path);
	list = g_list_prepend(list, icon);
	g_hash_table_insert(icons_hash, icon->path, list);
}

/* Remove this icon from the icons_hash table */
void icon_unhash_path(Icon *icon)
{
	GList	*list;

	g_return_if_fail(icon != NULL);

	/* g_print("[ unhashing '%s' ]\n", icon->path); */
	
	list = g_hash_table_lookup(icons_hash, icon->path);
	g_return_if_fail(list != NULL);

	list = g_list_remove(list, icon);

	/* Remove it first; the hash key may have changed address */
	g_hash_table_remove(icons_hash, icon->path);
	if (list)
		g_hash_table_insert(icons_hash,
				((Icon *) list->data)->path, list);
}

/* If path is on an icon then it may have changed... check! */
void icons_may_update(guchar *path)
{
	GList	*affected;

	/* g_print("[ icons_may_update(%s) ]\n", path); */

	affected = g_hash_table_lookup(icons_hash, path);

	while (affected)
	{
		Icon *icon = (Icon *) affected->data;

		if (icon->type == ICON_PANEL)
			panel_icon_may_update(icon);
		else
			pinboard_icon_may_update(icon);

		affected = affected->next;
	}
}

typedef struct _CheckData CheckData;
struct _CheckData {
	guchar	 *path;
	gboolean found;
};

static void check_has(gpointer key, GList *icons, CheckData *check)
{
	Icon	*icon;
	
	g_return_if_fail(icons != NULL);
	
	icon = icons->data;

	if (is_sub_dir(icon->path, check->path))
		check->found = TRUE;
}

/* Returns TRUE if any icon links to this item (or any item inside
 * this item).
 */
gboolean icons_require(guchar *path)
{
	CheckData	check;

	/* g_print("[ icons_require(%s)? ]\n", path); */

	check.path = path;
	check.found = FALSE;
	g_hash_table_foreach(icons_hash, (GHFunc) check_has, &check);

	return check.found;
}

/* Callback to update icons of a certain path */
static void update_icons(gpointer key, GList *icons, gpointer data)
{
	icons_may_update((guchar *) key);
}

/* Check all icons to see if they have been updated */
void update_all_icons(void)
{
	g_hash_table_foreach(icons_hash, (GHFunc) update_icons, NULL);
}

/****************************************************************
 *			INTERNAL FUNCTIONS			*
 ****************************************************************/

static void rename_activate(GtkWidget *dialog)
{
	GtkWidget *entry, *src;
	RenameFn callback;
	Icon	*icon;
	guchar	*new_name, *new_src;
	
	entry = gtk_object_get_data(GTK_OBJECT(dialog), "new_name");
	icon = gtk_object_get_data(GTK_OBJECT(dialog), "callback_icon");
	callback = gtk_object_get_data(GTK_OBJECT(dialog), "callback_fn");
	src = gtk_object_get_data(GTK_OBJECT(dialog), "new_path");

	g_return_if_fail(callback != NULL &&
			 entry != NULL &&
			 src != NULL &&
			 icon != NULL);

	new_name = gtk_entry_get_text(GTK_ENTRY(entry));
	new_src = gtk_entry_get_text(GTK_ENTRY(src));
	
	if (*new_name == '\0')
		report_error(PROJECT,
			_("The label must contain at least one character!"));
	else if (*new_src == '\0')
		report_error(PROJECT,
			_("The location must contain at least one character!"));
	else if (strpbrk(new_name, "<>"))
		report_error(PROJECT,
			_("Sorry, but the name must not contain < or >"));
	else
	{
		GdkFont	*font = icon->widget->style->font;
		
		g_free(icon->item.leafname);
		g_free(icon->src_path);

		icon->src_path = g_strdup(new_src);

		icon_unhash_path(icon);
		g_free(icon->path);
		icon->path = icon_convert_path(new_src);
		icon_hash_path(icon);
		
		icon->item.leafname = g_strdup(new_name);
		icon->item.name_width = gdk_string_width(font, new_name);
		dir_restat(icon->path, &icon->item, FALSE);

		callback(icon);
		gtk_widget_destroy(dialog);
	}
}
