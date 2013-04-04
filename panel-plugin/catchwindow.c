/* wmdock xfce4 plugin by Andre Ellguth
 * Catch the window if is a dockapp.
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
#include "catchwindow.h"
#include "wmdock.h"
#include "debug.h"
#include "dockapp.h"
#include "misc.h"
#include "props.h"


static gchar *wmdock_get_dockapp_cmd(WnckWindow *w)
{
	gchar *cmd = NULL;
	int wpid = 0;
	int argc = 0;
	int fcnt, i;
	char **argv;
	FILE *procfp = NULL;
	char buf[BUF_MAX];

	XGetCommand(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
			wnck_window_get_xid(w), &argv, &argc);
	if(argc > 0) {
		argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
		argv[argc] = NULL;
		cmd = g_strjoinv (" ", argv);
		XFreeStringList(argv);
	} else {
		/* Try to get the command line from the proc fs. */
		wpid = wnck_window_get_pid (w);

		if(wpid) {
			sprintf(buf, "/proc/%d/cmdline", wpid);

			procfp = fopen(buf, "r");

			if(procfp) {
				fcnt = read(fileno(procfp), buf, BUF_MAX);

				cmd = g_malloc(fcnt+2);
				if(!cmd) return (NULL);

				for(i = 0; i < fcnt; i++) {
					if(buf[i] == 0)
						*(cmd+i) = ' ';
					else
						*(cmd+i) = buf[i];
				}
				*(cmd+(i-1)) = 0;

				fclose(procfp);
			}
		}
	}

	if(!cmd) {
		/* If nothing helps fallback to the window name. */
		cmd = g_strdup(wnck_window_get_name(w));
	}

	return(cmd);
}


void wmdock_window_open(WnckScreen *s, WnckWindow *w)
{
	int wi, he;
	XWMHints *h            = NULL;
	XWindowAttributes attr;
	DockappNode *dapp      = NULL;
	GList *_dapps          = NULL;
	gchar *cmd             = NULL;
	gboolean rcDapp        = FALSE;

	gdk_error_trap_push();
	gdk_flush();

	h = XGetWMHints(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
			wnck_window_get_xid(w));

	if(!h) return;

	if(h->initial_state == WithdrawnState ||
			h->flags == (WindowGroupHint | StateHint | IconWindowHint)
			|| has_dockapp_hint(w)) {

		debug("catchwindow.c: new wmapp open");
		debug("catchwindow.c: New dockapp %s with xid:%u pid:%u arrived sessid:%s",
				wnck_window_get_name(w), wnck_window_get_xid(w),
				wnck_window_get_pid(w), wnck_window_get_session_id(w));

		cmd = wmdock_get_dockapp_cmd(w);

		if(wmdock->propDispAddOnlyWM == TRUE &&
				comp_dockapp_with_filterlist(wnck_window_get_name(w)) == FALSE &&
				! (wmdock_find_startup_dockapp(cmd))) {
			XFree(h);
			return;
		}

		if(!cmd) {
			XFree(h);
			return;
		}
		debug("catchwindow.c: found cmd %s for window %s", cmd, wnck_window_get_name(w));

		if(rcCmds && (dapp = wmdock_find_startup_dockapp(cmd)))
			rcDapp = TRUE;

		if(rcDapp == FALSE) {
			debug("catchwindow.c: Create a new dapp window %s", wnck_window_get_name(w));

			dapp = g_new0(DockappNode, 1);
			dapp->s = GTK_SOCKET(gtk_socket_new());
		}

		if(h->initial_state == WithdrawnState && h->icon_window) {
			XUnmapWindow(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
					wnck_window_get_xid(w));
			dapp->i =h->icon_window;
		} else {
			dapp->i = wnck_window_get_xid(w);
		}

		if(!XGetWindowAttributes(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
				dapp->i, &attr)) {
			wi = DEFAULT_DOCKAPP_WIDTH;
			he = DEFAULT_DOCKAPP_HEIGHT;
		} else {
			wi = attr.width;
			he = attr.height;
		}

		if(wi > DEFAULT_DOCKAPP_WIDTH || he > DEFAULT_DOCKAPP_HEIGHT) {
			/* It seems to be no dockapp, because the width or the height of the
			 * window a greater than 64 pixels. */
			XMapWindow(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
					wnck_window_get_xid(w));
			gtk_widget_destroy(GTK_WIDGET(dapp->s));
			g_free(cmd);
			g_free(dapp);
			XFree(h);
			return;
		}

		debug("catchwindow.c: New dockapp %s width: %d height: %d", wnck_window_get_name(w), wi, he);

		gtk_widget_set_size_request(GTK_WIDGET(dapp->s), wi, he);
		wnck_window_set_skip_tasklist (w, TRUE);
		wnck_window_set_skip_pager (w, TRUE);

		/* Set this property to skip the XFCE4 session manager. */
		set_xsmp_support(w);

		dapp->name = g_strdup(wnck_window_get_name(w));
		dapp->cmd = cmd;

		if(wmdockIcon && !IS_PANELOFF(wmdock)) {
			gtk_widget_destroy(wmdockIcon);
			wmdockIcon = NULL;
		}

		/* Cleanly unmap the original window. */
		XUnmapWindow(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()), dapp->i);

		if(rcDapp == FALSE) {
			dapp->tile = wmdock_create_tile_from_socket(dapp);
			/* Setup tile background. */
			wmdock_set_tile_background(dapp, gdkPbTileDefault);

			if( ! IS_PANELOFF(wmdock) ) {
				/* Setup the DockApp in the XFCE-panel. */
				gtk_box_pack_start(GTK_BOX(wmdock->box), dapp->tile, FALSE, FALSE, 0);
			} else {
				/* Setup the DockApp for the WindowMaker like style. */
				gtk_widget_show_all(GTK_WIDGET(dapp->tile));
				wmdock_set_autoposition_dockapp(dapp,
						g_list_last(wmdock->dapps) ? g_list_last(wmdock->dapps)->data : NULL);
			}
		}

		gtk_socket_add_id(dapp->s, dapp->i);

		gtk_widget_show_all(GTK_WIDGET(dapp->tile));

		g_signal_connect(dapp->s, "plug-removed", G_CALLBACK(wmdock_dapp_closed), dapp);

		if(rcDapp == FALSE)
			wmdock->dapps=g_list_append(wmdock->dapps, dapp);

		/* Setup drag & drop for the dockapps. */
		g_list_foreach(wmdock->dapps, (GFunc) wmdock_setupdnd_dockapp, NULL);

		/* Bring all dockapps to the front if a new one shown (panel off mode). */
		g_list_foreach(wmdock->dapps, (GFunc) wmdock_dockapp_tofront, NULL);

		if( IS_PANELOFF(wmdock) ) {
			if(rcDapp == TRUE) {
				_dapps = g_list_find(wmdock->dapps, (gconstpointer) dapp);
				wmdock_set_autoposition_dockapp( dapp,
						(DOCKAPP(g_list_first(wmdock->dapps)->data) != dapp && _dapps) ?
								DOCKAPP(g_list_previous(_dapps)->data) : NULL);

				wmdock_order_dockapps(DOCKAPP(g_list_first(wmdock->dapps)->data));
			}

			/* Setup the event handler for the window. */
			g_signal_connect(dapp->tile, "event-after", G_CALLBACK(wmdock_dockapp_paneloff_handler),
					dapp);
		}
		/* Clear the noisy background. */
		wmdock_redraw_dockapp(dapp);

		wmdock_refresh_properties_dialog();
	}

	XFree(h);
}
