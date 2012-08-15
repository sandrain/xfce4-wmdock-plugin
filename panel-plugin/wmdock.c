/* wmdock xfce4 plugin by Andre Ellguth
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
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/libxfce4panel.h>

#include "wmdock.h"
#include "catchwindow.h"
#include "debug.h"
#include "dnd.h"
#include "dockapp.h"
#include "misc.h"
#include "props.h"
#include "rcfile.h"

#include "tile.xpm"

#define FONT_WIDTH      4
#define MAX_WAITCNT     10000
#define WAITCNT_TIMEOUT 1000000

Atom         XfceDockAppAtom;
GtkWidget    *wmdockIcon       = NULL;
DockappNode  *dappProperties   = NULL;
GdkPixbuf    *gdkPbTileDefault = NULL;
GdkPixbuf    *gdkPbIcon        = NULL;
WmdockPlugin *wmdock           = NULL;
gchar        **rcCmds          = NULL;
gint         rcCmdcnt          = 0;
/* TODO: Set panel off to FALSE. */
gboolean     rcPanelOff        = TRUE;


static void wmdock_orientation_changed (XfcePanelPlugin *plugin, GtkOrientation orientation, gpointer user_data)
{
	xfce_hvbox_set_orientation ((XfceHVBox *) wmdock->panelBox, orientation);
	xfce_hvbox_set_orientation ((XfceHVBox *) wmdock->box, orientation);
	gtk_widget_show(GTK_WIDGET(wmdock->panelBox));
	gtk_widget_show(GTK_WIDGET(wmdock->box));
}


static gboolean wmdock_size_changed (XfcePanelPlugin *plugin, int size)
{
	if (xfce_panel_plugin_get_orientation (plugin) ==
			GTK_ORIENTATION_HORIZONTAL)  {
		gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);
	}

	if(wmdockIcon) {
		wmdock_panel_draw_wmdock_icon(TRUE);
	}

	return TRUE;
}


static void wmdock_free_data(XfcePanelPlugin *plugin)
{
	debug("wmdock.c: Called wmdock_free_data");

	g_list_foreach(wmdock->dapps, (GFunc)wmdock_destroy_dockapp, NULL);

	/* Cleanup all the dockapps. */
	g_list_foreach(wmdock->dapps, (GFunc)wmdock_free_dockapp, NULL);

	gtk_widget_destroy(GTK_WIDGET(wmdockIcon));
	gtk_widget_destroy(GTK_WIDGET(wmdock->box));
	gtk_widget_destroy(GTK_WIDGET(wmdock->panelBox));
	gtk_widget_destroy(GTK_WIDGET(wmdock->align));
	gtk_widget_destroy(GTK_WIDGET(wmdock->eventBox));
	g_list_free(wmdock->dapps);
	g_free(wmdock);

	debug("wmdock.c: wmdock_free_data() done.");
}


static WmdockPlugin *wmdock_plugin_new (XfcePanelPlugin* plugin)
{
	wmdock                     = g_new0(WmdockPlugin, 1);
	wmdock->plugin             = plugin;
	wmdock->dapps              = NULL;
	wmdock->propDispTile       = TRUE;
	wmdock->propDispPropButton = FALSE;
	wmdock->propDispAddOnlyWM  = TRUE;
	/* TODO: Set panel off to FALSE. */
	wmdock->propPanelOff       = TRUE;
	wmdock->filterList         = g_strdup(DOCKAPP_FILTER_PATTERN);
	wmdock->anchorPos          = -1;

	wmdock->eventBox = gtk_event_box_new ();
	gtk_widget_show(GTK_WIDGET(wmdock->eventBox));

	wmdock->align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

	gtk_widget_show(GTK_WIDGET(wmdock->align));

	gtk_container_add(GTK_CONTAINER(wmdock->eventBox), GTK_WIDGET(wmdock->align));

	wmdock->panelBox = xfce_hvbox_new(xfce_panel_plugin_get_orientation (plugin), FALSE, 0);
	gtk_widget_show(GTK_WIDGET(wmdock->panelBox));

	wmdock->box = xfce_hvbox_new(xfce_panel_plugin_get_orientation (plugin), FALSE, 0);

	gtk_box_pack_start(GTK_BOX(wmdock->panelBox), GTK_WIDGET(wmdock->box),
			FALSE, FALSE, 0);

	gtk_widget_show(GTK_WIDGET(wmdock->box));

	gtk_container_add (GTK_CONTAINER (wmdock->align), wmdock->panelBox);

	return wmdock;
}


static void wmdock_construct (XfcePanelPlugin *plugin)
{
	WnckScreen  *s;

	init_debug();

	s = wnck_screen_get(0);

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	XfceDockAppAtom=XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
			"_XFCE4_DOCKAPP",False);

	wmdock = wmdock_plugin_new (plugin);

	g_signal_connect(s, "window_opened", G_CALLBACK(wmdock_window_open), NULL);
	g_signal_connect (plugin, "size-changed", G_CALLBACK (wmdock_size_changed), NULL);
	g_signal_connect (plugin, "orientation-changed", G_CALLBACK (wmdock_orientation_changed), NULL);
	g_signal_connect (plugin, "free-data", G_CALLBACK (wmdock_free_data), NULL);

	gtk_container_add (GTK_CONTAINER (plugin), wmdock->eventBox);

	xfce_panel_plugin_add_action_widget (plugin, wmdock->eventBox);

	/* Setup the tile image */
	gdkPbTileDefault = gdk_pixbuf_new_from_xpm_data((const char **) tile_xpm);

	wmdock_panel_draw_wmdock_icon(FALSE);

	/* Configure plugin dialog */
	xfce_panel_plugin_menu_show_configure (plugin);
	g_signal_connect (plugin, "configure-plugin",
			G_CALLBACK (wmdock_properties_dialog), NULL);

	/* Read the config file and start the dockapps */
	wmdock_read_rc_file(plugin);

	wmdock_panel_draw_properties_button();

	g_signal_connect (plugin, "save", G_CALLBACK (wmdock_write_rc_file), NULL);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (wmdock_construct);
