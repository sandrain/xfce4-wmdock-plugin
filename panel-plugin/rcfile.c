/* wmdock xfce4 plugin by Andre Ellguth
 * Configuration file handling.
 *
 * $Id: rcfile.c 23 2012-08-09 19:39:15Z ellguth $
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

#include "extern.h"
#include "rcfile.h"
#include "wmdock.h"
#include "debug.h"
#include "dockapp.h"
#include "props.h"


void wmdock_read_rc_file (XfcePanelPlugin *plugin)
{
	gchar     *file = NULL;
	XfceRc    *rc = NULL;
	gint      i;
	GtkWidget *gtkDlg;
	DockappNode *dapp = NULL;

	if (!(file = xfce_panel_plugin_lookup_rc_file (plugin))) return;

	rc = xfce_rc_simple_open (file, TRUE);
	g_free(file);

	if(!rc) return;

	rcCmds                     = xfce_rc_read_list_entry(rc, "cmds", ";");
	rcCmdcnt                   = xfce_rc_read_int_entry(rc, "cmdcnt", 0);
	wmdock->propDispTile       = xfce_rc_read_bool_entry (rc, "disptile", TRUE);
	wmdock->propDispPropButton = xfce_rc_read_bool_entry (rc, "disppropbtn", FALSE);
	wmdock->propDispAddOnlyWM  = xfce_rc_read_bool_entry (rc, "dispaddonlywm", TRUE);
	if(wmdock->filterList) g_free(wmdock->filterList);
	wmdock->filterList         = g_strdup(xfce_rc_read_entry (rc, "dafilter", DOCKAPP_FILTER_PATTERN));
	/* TODO: Set panel off to FALSE. */
	rcPanelOff = wmdock->propPanelOff = xfce_rc_read_bool_entry (rc, "paneloff", TRUE);
	wmdock->anchorPos          = xfce_rc_read_int_entry(rc, "anchorpos", -1);

	if(G_LIKELY(rcCmds != NULL)) {
		/* Wait 5 seconds as workaround for double XMap problems. */
		g_usleep(5 * G_USEC_PER_SEC);

		for (i = 0; i <= rcCmdcnt; i++) {
			debug("rcfile.c: config will start: %s\n", rcCmds[i]);

			if(!rcCmds[i]) continue;
			if(wmdock_startup_dockapp(rcCmds[i]) != TRUE) {
				gtkDlg = gtk_message_dialog_new(GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_MESSAGE_ERROR,
						GTK_BUTTONS_OK,
						_("Failed to start %s!"),
						rcCmds[i]);
				g_signal_connect (gtkDlg, "response", G_CALLBACK (wmdock_error_dialog_response), NULL);
				gtk_widget_show_all (gtkDlg);
			} else {
				/* Create some dummy widget entries to locate the right position on
				 * window swallow up.
				 */

				dapp = g_new0(DockappNode, 1);
				dapp->name = NULL;
				dapp->cmd = rcCmds[i];

				dapp->s = GTK_SOCKET(gtk_socket_new());
				dapp->tile = wmdock_create_tile_from_socket(dapp);

				if( ! IS_PANELOFF(wmdock) ) {
					gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile),
							FALSE, FALSE, 0);
				}

				wmdock->dapps=g_list_append(wmdock->dapps, dapp);
			}
		}
	}

	xfce_rc_close (rc);
}


void wmdock_write_rc_file (XfcePanelPlugin *plugin)
{
	gchar       *file;
	XfceRc      *rc;
	gchar       **cmds;
	DockappNode *dapp = NULL;
	gint        i;

	if (!(file = xfce_panel_plugin_save_location (plugin, TRUE))) return;

	rc = xfce_rc_simple_open (file, FALSE);
	g_free (file);

	if (!rc) return;

	if(g_list_length (wmdock->dapps) > 0) {
		cmds = g_malloc(sizeof (gchar *) * (g_list_length (wmdock->dapps) + 1));

		for(i = 0; i < g_list_length(wmdock->dapps); i++) {
			dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, i));
			if(dapp) {
				if(dapp->name && dapp->cmd)
					cmds[i] = g_strdup(dapp->cmd);
			}
		}
		/* Workaround for a xfce bug in xfce_rc_read_list_entry */
		cmds[i] = NULL;

		xfce_rc_write_list_entry(rc, "cmds", cmds, ";");

		g_strfreev(cmds);

		xfce_rc_write_int_entry (rc, "cmdcnt", g_list_length (wmdock->dapps));
		xfce_rc_write_bool_entry (rc, "disptile", wmdock->propDispTile);
		xfce_rc_write_bool_entry (rc, "disppropbtn", wmdock->propDispPropButton);
		xfce_rc_write_bool_entry (rc, "dispaddonlywm", wmdock->propDispAddOnlyWM);
		xfce_rc_write_bool_entry (rc, "paneloff", rcPanelOff);
		xfce_rc_write_entry(rc, "dafilter", wmdock->filterList);
		xfce_rc_write_int_entry (rc, "anchorpos", wmdock->anchorPos);
	}

	xfce_rc_close(rc);
}
