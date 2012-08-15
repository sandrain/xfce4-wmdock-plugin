/* wmdock xfce4 plugin by Andre Ellguth
 * Drag & Drop functions.
 *
 * $Id: dnd.c 23 2012-08-09 19:39:15Z ellguth $
 *
 * Authors:
 *   Andre Ellguth <ellguth@ibh.de>
 *
 * License:
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this package; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "extern.h"
#include "wmdock.h"
#include "debug.h"
#include "dockapp.h"
#include "misc.h"
#include "dnd.h"

#include "xfce4-wmdock-plugin.xpm"

#define _BYTE 8

void drag_begin_handl (GtkWidget *widget, GdkDragContext *context,
		gpointer dapp)
{
	gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm,
			DEFAULT_DOCKAPP_WIDTH/2,
			DEFAULT_DOCKAPP_HEIGHT/2);

	gtk_drag_set_icon_pixbuf (context, gdkPbIcon, 0, 0);

	g_object_unref (G_OBJECT(gdkPbIcon));
}

#if (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 12)
gboolean drag_failed_handl(GtkWidget *widget, GdkDragContext *context,
		GtkDragResult result, gpointer dapp)
{
	GtkWidget *gtkDlg;

	if(result == GTK_DRAG_RESULT_NO_TARGET && dapp) {
		gtkDlg = gtk_message_dialog_new(GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (wmdock->plugin))),
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				GTK_MESSAGE_QUESTION,
				GTK_BUTTONS_YES_NO,
				_("Do you want remove the dockapp \"%s\"?"),
				((DockappNode *) dapp)->name);

		if(gtk_dialog_run (GTK_DIALOG(gtkDlg)) == GTK_RESPONSE_YES)
			wmdock_destroy_dockapp((DockappNode *) dapp);

		gtk_widget_destroy (GTK_WIDGET(gtkDlg));
	}
	debug("dnd.c: Drag failed of dockapp %s", ((DockappNode *) dapp)->name);

	return TRUE;
}
#endif


gboolean drag_drop_handl (GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, guint time, gpointer dapp)
{
	gboolean        is_valid_drop_site;
	GdkAtom         target_type;

	is_valid_drop_site = TRUE;

	if (context-> targets)
	{
		target_type = GDK_POINTER_TO_ATOM
				(g_list_nth_data (context-> targets, 0));

		gtk_drag_get_data (widget,context, target_type, time);
	}

	else
	{
		is_valid_drop_site = FALSE;
	}

	return  is_valid_drop_site;
}



void drag_data_received_handl (GtkWidget *widget,
		GdkDragContext *context, gint x, gint y,
		GtkSelectionData *selection_data,
		guint target_type, guint time,
		gpointer dapp)
{
	glong *_idata;
	gboolean dnd_success = FALSE;
	GList *dappsSrc = NULL;
	GList *dappsDst = NULL;

	if(target_type == 0) {
		_idata = (glong*) selection_data-> data;
		debug("dnd.c: DnD integer received: %ld", *_idata);

		dnd_success = TRUE;

		if(dapp) {
			dappsSrc = g_list_nth(wmdock->dapps, *_idata);
			dappsDst = g_list_find(wmdock->dapps, (DockappNode *) dapp);

			if(dappsSrc->data != dappsDst->data) {
				debug("dnd.c: DnD src dockapp name: %s",
						DOCKAPP(dappsSrc->data)->name);
				debug("dnd.c: DnD dst dockapp name: %s",
						DOCKAPP(dapp)->name);

				dappsDst->data = dappsSrc->data;
				dappsSrc->data = dapp;

				debug("dnd.c: DnD src index: %d",
						g_list_index (wmdock->dapps, dappsSrc->data));
				debug("dnd.c: DnD dst index: %d",
						g_list_index (wmdock->dapps, dappsDst->data));

				gtk_box_reorder_child(GTK_BOX(wmdock->box),
						GTK_WIDGET(DOCKAPP(dappsSrc->data)->tile),
						g_list_index (wmdock->dapps, dappsSrc->data));
				gtk_box_reorder_child(GTK_BOX(wmdock->box),
						GTK_WIDGET(DOCKAPP(dappsDst->data)->tile),
						g_list_index (wmdock->dapps, dappsDst->data));

				g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
			}
		}

	}

	gtk_drag_finish (context, dnd_success, FALSE, time);

}



void drag_data_get_handl (GtkWidget *widget, GdkDragContext *context,
		GtkSelectionData *selection_data,
		guint target_type, guint time,
		gpointer dapp)
{
	gint index;

	if(target_type == 0 && dapp) {
		index = g_list_index (wmdock->dapps, (DockappNode *) dapp);

		gtk_selection_data_set (selection_data, selection_data->target,
				sizeof(index) * _BYTE,
				(guchar*) &index, sizeof (index));

		debug("dnd.c: DnD Integer sent: %ld", index);
	}
}
