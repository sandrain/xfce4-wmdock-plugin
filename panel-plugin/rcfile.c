/* wmdock xfce4 plugin by Andre Ellguth
 * Configuration file handling.
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
#include "misc.h"
#include "props.h"


void wmdock_read_rc_file (XfcePanelPlugin *plugin)
{
	gchar     *file = NULL;
	XfceRc    *rc = NULL;
	gint      i = 0, j = 0, gluePos = 0;
	gint64    n = 0;
	gchar     *glueName = NULL;
	GtkWidget *gtkDlg;
	DockappNode *dapp = NULL;
	DockappNode **launched = NULL;
	gchar     **glueList = NULL;
	gchar     **glueInfo = NULL;

	if (!(file = xfce_panel_plugin_lookup_rc_file (plugin))) return;

	rc = xfce_rc_simple_open (file, TRUE);
	g_free(file);
	if(!rc)
		return;

	rcCmds                     = xfce_rc_read_list_entry(rc, RCKEY_CMDLIST, RC_LIST_DELIMITER);
	wmdock->propDispTile       = xfce_rc_read_bool_entry (rc, RCKEY_DISPTILE, TRUE);
	wmdock->propDispPropButton = xfce_rc_read_bool_entry (rc, RCKEY_DISPPROPBTN, FALSE);
	wmdock->propDispAddOnlyWM  = xfce_rc_read_bool_entry (rc, RCKEY_DISPADDONLYWM, TRUE);
	if(wmdock->filterList) g_free(wmdock->filterList);
	wmdock->filterList         = g_strdup(xfce_rc_read_entry (rc, RCKEY_DAFILTER, DOCKAPP_FILTER_PATTERN));
	/* TODO: Set panel off to FALSE. */
	rcPanelOff                 = wmdock->propPanelOff = xfce_rc_read_bool_entry (rc, RCKEY_PANELOFF, TRUE);
	wmdock->propPanelOffIgnoreOffset = xfce_rc_read_bool_entry (rc, RCKEY_PANELOFFIGNOREOFFSET, FALSE);
	wmdock->propPanelOffKeepAbove    = xfce_rc_read_bool_entry (rc, RCKEY_PANELOFFKEEPABOVE, FALSE);
	glueList                   = IS_PANELOFF(wmdock) ? xfce_rc_read_list_entry(rc, RCKEY_GLUELIST, RC_LIST_DELIMITER) : NULL;
	wmdock->anchorPos          = xfce_rc_read_int_entry(rc, RCKEY_ANCHORPOS, -1);
	xfce_rc_close (rc);

	if(G_LIKELY(rcCmds != NULL)) {
		if(!(launched = g_malloc0(sizeof (DockappNode *) * (g_strv_length(rcCmds)))))
				return;

		/* Wait 1 seconds as workaround for double XMap problems. */
		g_usleep(1 * G_USEC_PER_SEC);
		for (i = 0; rcCmds[i]; i++) {
			debug("rcfile.c: Setup `%s'\n", rcCmds[i]);

			if(wmdock_startup_dockapp(rcCmds[i]) != TRUE) {
				launched[i] = NULL;
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

				wmdock->dapps = g_list_append(wmdock->dapps, dapp);
				launched[i] = dapp;

				if( ! IS_PANELOFF(wmdock) ) {
					gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile), FALSE, FALSE, 0);
				} else  {
					/* If is possible, restore the old position of the dockapps in panel off mode. */
					if(! wmdock_get_parent_dockapp(dapp) ) {
						if(g_list_previous(g_list_last(wmdock->dapps))) {
							DOCKAPP(((GList *) g_list_previous(g_list_last(wmdock->dapps)))->data)->glue[wmdock_get_default_gluepos()] = dapp;
						}
					}
				}
			}
		}

		if( IS_PANELOFF( wmdock ) && g_strv_length(rcCmds) == g_strv_length(glueList) ) {
			for (i = 0; glueList[i]; i++) {
				if(!launched[i])
					continue;

				/* Cleanup the default anchors. */
				memset(launched[i]->glue, '\0', sizeof(DockappNode *) * GLUE_MAX);

				if(glueList[i][0] == '\0' || !(glueInfo = g_strsplit(glueList[i], RC_GLUE_DELIMITER, 0)))
					continue;

				for (j = 0; glueInfo[j]; j++) {
					n = g_ascii_strtoll(glueInfo[j], &glueName, 10);
					if(n > G_MAXINT || n < 0 || n > g_strv_length(rcCmds)-1 || glueName == glueInfo[j] || glueName[0] != ':')
						continue;
					if((gluePos = wmdock_get_glue_position(&glueName[1])) == -1)
						continue;

					launched[i]->glue[gluePos] = launched[(gint) n];
					debug("rcfile.c: Restored panel off position. (`%s', %s = %d)", launched[i]->cmd, &glueName[1], gluePos);
				}
				g_strfreev(glueInfo);
			}

			g_strfreev(glueList);
		}

		g_free(launched);
	} /* rcCmds != NULL */
}


void wmdock_write_rc_file (XfcePanelPlugin *plugin)
{
	gchar       *file = NULL, *str = NULL;
	XfceRc      *rc;
	gchar       **cmdList = NULL;
	gchar       **glueList = NULL;
	gchar       buf[BUF_MAX];
	GList       *dapps;
	DockappNode *dapp = NULL;
	gint        i = 0, gluePos = 0, n = 0;

	if (!(file = xfce_panel_plugin_save_location (plugin, TRUE))) return;

	rc = xfce_rc_simple_open (file, FALSE);
	g_free (file);

	if (!rc)
		return;

	if(g_list_length (wmdock->dapps) > 0) {
		cmdList = g_malloc0(sizeof (gchar *) * (g_list_length (wmdock->dapps) + 1));
		if ( IS_PANELOFF(wmdock) )
			glueList = g_malloc0(sizeof (gchar *) * (g_list_length (wmdock->dapps) + 1));

		for(dapps = g_list_first(wmdock->dapps) ; dapps; dapps = g_list_next(dapps)) {
			dapp = DOCKAPP(dapps->data);
			if((i = g_list_index(wmdock->dapps, (gconstpointer) dapp)) == -1)
				continue;
			cmdList[i] = dapp->cmd ? g_strdup(dapp->cmd) : NULL;
			if( IS_PANELOFF(wmdock) ) {
				buf[0] = '\0';
				for(gluePos = 0; gluePos < GLUE_MAX; gluePos++) {
					if(dapp->glue[gluePos] && (n = g_list_index(wmdock->dapps, (gconstpointer) dapp->glue[gluePos])) != -1) {
						/* ChildIndex1(n):position,ChildIndex2:postion,... */
						if(strnlen((const char *) buf, sizeof(buf)-1) > 0)
							str = g_strdup_printf("%s%d:%s", RC_GLUE_DELIMITER, n, wmdock_get_glue_name(gluePos));
						else
							str = g_strdup_printf("%d:%s", n, wmdock_get_glue_name(gluePos));
						g_strlcat(buf, str, sizeof(buf));
						g_free(str);
					}
				}
				glueList[i] = g_strdup(buf);
			}
		}

		xfce_rc_write_list_entry(rc, RCKEY_CMDLIST, cmdList, RC_LIST_DELIMITER);
		g_strfreev(cmdList);

		if( IS_PANELOFF(wmdock) ) {
			xfce_rc_write_list_entry(rc, RCKEY_GLUELIST, glueList, RC_LIST_DELIMITER);
			g_strfreev(glueList);
		} else if ( ! IS_PANELOFF(wmdock) || rcPanelOff == FALSE ) {
			xfce_rc_delete_entry(rc, RCKEY_GLUELIST, TRUE);
		}

		xfce_rc_write_bool_entry (rc, RCKEY_DISPTILE, wmdock->propDispTile);
		xfce_rc_write_bool_entry (rc, RCKEY_DISPPROPBTN, wmdock->propDispPropButton);
		xfce_rc_write_bool_entry (rc, RCKEY_DISPADDONLYWM, wmdock->propDispAddOnlyWM);
		xfce_rc_write_bool_entry (rc, RCKEY_PANELOFF, rcPanelOff);
		xfce_rc_write_bool_entry (rc, RCKEY_PANELOFFIGNOREOFFSET, wmdock->propPanelOffIgnoreOffset);
		xfce_rc_write_bool_entry (rc, RCKEY_PANELOFFKEEPABOVE, wmdock->propPanelOffKeepAbove);
		xfce_rc_write_int_entry (rc, RCKEY_ANCHORPOS, wmdock->anchorPos);
		xfce_rc_write_entry(rc, RCKEY_DAFILTER, wmdock->filterList);
	}

	xfce_rc_close(rc);
}
