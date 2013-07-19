/* wmdock xfce4 plugin by Andre Ellguth
 * Dockapp handling.
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
#include "dockapp.h"
#include "wmdock.h"
#include "debug.h"
#include "dnd.h"
#include "misc.h"
#include "props.h"

#define DEFAULT_XPANEL_NAME "xfce4-panel"

static GtkTargetEntry targetList[] = {
		{ "INTEGER", 0, 0 }
};
static guint nTargets = G_N_ELEMENTS (targetList);

/**
 * Get the x coordinate child dockapp.
 *
 * @param dapp Parent dockapp.
 * @param prevDapp Child dockapp.
 * @param gluepos Orientation of the child dockapp.
 * @param x Pointer to x.
 * @param y Pointer to y.
 */
static void wmdock_dockapp_child_pos(DockappNode *prevDapp, gint gluepos, gint *x, gint *y)
{
	gint prevx, prevy;

	/* Setup the position of the first dockapp. */
	prevx = prevy = 0;

	/* Get the position of the previous DockApp if is accessable. */
	gtk_window_get_position(
			GTK_WINDOW (GTK_WIDGET (prevDapp->tile)), &prevx, &prevy);

	switch(gluepos) {
	case GLUE_T:
		*x = prevx;
		*y = prevy - DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_TL:
		*x = prevx - DEFAULT_DOCKAPP_WIDTH;
		*y = prevy - DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_TR:
		*x = prevx + DEFAULT_DOCKAPP_WIDTH;
		*y = prevy - DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_B:
		*x = prevx;
		*y = prevy + DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_BL:
		*x = prevx - DEFAULT_DOCKAPP_WIDTH;
		*y = prevy + DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_BR:
		*x = prevx + DEFAULT_DOCKAPP_WIDTH;
		*y = prevy + DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_L:
		*x = prevx - DEFAULT_DOCKAPP_WIDTH;
		*y = prevy;
		break;
	case GLUE_R:
		*x = prevx + DEFAULT_DOCKAPP_WIDTH;
		*y = prevy;
		break;
	default:
		break;
	}
}

/**
 * Calculate the next snapable postion of the moving DockApp.
 *
 * @parm dapp The moving DockApp.
 * @parm gluepos Pointer to the glue position of the determined DockApp.
 * @return The determined DockApp or NULL.
 */
static DockappNode *wmdock_get_snapable_dockapp(DockappNode *dapp, gint *gluepos)
{
	#define SNAPDELTA (DEFAULT_DOCKAPP_HEIGHT/2)-1
	gint posx, posy, gluex, gluey;
	GList *dapps;
	DockappNode *_dapp = NULL;

	gtk_window_get_position(
			GTK_WINDOW (GTK_WIDGET (dapp->tile)), &posx, &posy);

	dapps = g_list_first(wmdock->dapps);

	while(dapps) {
		if((_dapp = DOCKAPP(dapps->data))) {
			for(*gluepos = 0; *gluepos < GLUE_MAX; *gluepos=*gluepos+1) {
				if(!_dapp->glue[*gluepos] || !g_strcmp0(_dapp->glue[*gluepos]->name, DOCKAPP_DUMMY_TITLE)) {
					wmdock_dockapp_child_pos(_dapp, *gluepos, &gluex, &gluey);
					if(posx >= gluex-SNAPDELTA && posy >= gluey-SNAPDELTA &&
							posx <= gluex+SNAPDELTA && posy <= gluey+SNAPDELTA)
						return _dapp;
				}
			}
		}

		dapps = g_list_next(dapps);
	}

	return NULL;
}


/**
 * Determine the main anchor DockApp.
 *
 * @return DockappNode which is the main anchor otherwise NULL.
 */
static DockappNode *wmdock_get_primary_anchor_dockapp()
{
	gint i;
	GList *dapps1, *dapps2;
	DockappNode *dapp1 = NULL, *dapp2 = NULL;

	dapps1 = g_list_first(wmdock->dapps);

	while(dapps1) {
		if(!(dapp1 = DOCKAPP(dapps1->data)))
			continue;

		dapps2 = g_list_first(wmdock->dapps);
		while(dapps2) {
			if(!(dapp2 = DOCKAPP(dapps2->data)))
				continue;

			for(i = 0; i < GLUE_MAX; i++) {
				if(dapp2->glue[i] == dapp1)
					break;
			}
			if(dapp2->glue[i] == dapp1)
				break;

			dapps2 = g_list_next(dapps2);
		}
		/* Main anchor DockApp found. */
		if(!dapps2) {
			debug("dockapp.c: Found primary dockapp `%s'", dapp1->name);
			return(dapp1);
		}

		dapps1 = g_list_next(dapps1);
	}

	return NULL;
}


/**
 * Remove anchors of dummy DockApp.
 */
static void wmdock_remove_anchors_tile_dummy()
{
	gint i;
	GList *dapps;
	DockappNode *_dapp = NULL;

	dapps = g_list_first(wmdock->dapps);
	while(dapps) {
		if((_dapp = DOCKAPP(dapps->data))) {
			for(i = 0; i < GLUE_MAX; i++) {
				if(_dapp->glue[i] && !g_strcmp0(_dapp->glue[i]->name, DOCKAPP_DUMMY_TITLE)) {
					_dapp->glue[i] = NULL;
				}
			}
		}

		dapps = g_list_next(dapps);
	}
}


/**
 * Replace dummy DockApp with the moved DockApp.
 *
 * @param dapp Replacement Dockapp.
 * @return TRUE if dummy tile is replaced else FALSE.
 */
static gboolean wmdock_replace_tile_dummy(DockappNode *dapp)
{
	gint i;
	GList *dapps;
	DockappNode *_dapp = NULL;

	dapps = g_list_first(wmdock->dapps);
	while(dapps) {
		if((_dapp = DOCKAPP(dapps->data))) {
			for(i = 0; i < GLUE_MAX; i++) {
				if(_dapp->glue[i] && !g_strcmp0(_dapp->glue[i]->name, DOCKAPP_DUMMY_TITLE)) {
					g_list_foreach(wmdock->dapps, (GFunc) wmdock_remove_anchor_dockapp, dapp);
					_dapp->glue[i] = dapp;
					for(i = 0; i < GLUE_MAX; i++) {
						if(dapp->glue[i] == _dapp) {
							/* Remove old anchor itself. */
							dapp->glue[i] = NULL;
						}
					}
					return TRUE;
				}
			}
		}

		dapps = g_list_next(dapps);
	}

	return FALSE;
}


/**
 * Event handler for the tile in panel off mode.
 *
 * @param tile The window of the event.
 * @param ev Event informations.
 * @param dapp DockappNode of the event.
 */
void wmdock_dockapp_paneloff_handler(GtkWidget *tile, GdkEvent *ev, DockappNode *dapp)
{
	static DockappNode *dappOnMove = NULL, *dappDummy = NULL;
	DockappNode *dappSnap = NULL;
	gint gluepos;
	GdkModifierType gdkmodtype;

	debug("dockapp.c: Window event after: %d. (dapp: %s), dappOnMove: %s", ev->type, dapp->name,
			dappOnMove ? "Yes": "No");

	switch(ev->type) {
	case GDK_BUTTON_PRESS:
	case GDK_KEY_PRESS:
		dappOnMove = dapp;
		break;
	case GDK_CONFIGURE: /* Movement. */
		if(dappOnMove) {
			wmdock_remove_anchors_tile_dummy();
			dappSnap = wmdock_get_snapable_dockapp(dapp, &gluepos);
			if(dappSnap) {
				debug("dockapp.c: Snapable dockapp `%s' for dockapp `%s', glue: %d.", dappSnap->name, dapp->name, gluepos);
				if(!dappDummy) {
					dappDummy = g_new0(DockappNode, 1);
					dappDummy->name = g_strdup(DOCKAPP_DUMMY_TITLE);
					dappDummy->tile = wmdock_create_tile_dummy();
				}

				dappSnap->glue[gluepos] = dappDummy;
				wmdock_order_dockapps(dappDummy);
				gtk_widget_show_all(dappDummy->tile);
			} else if(dappDummy) {
				gtk_widget_hide(dappDummy->tile);
			}
		}

		gdk_window_get_pointer(tile->window, NULL, NULL, &gdkmodtype);
		if(!(dappOnMove && gdkmodtype && !(gdkmodtype & GDK_BUTTON1_MASK)))
			break;
		/* No break if DockApp is moved and mouse btn released. */
	case GDK_BUTTON_RELEASE:
	case GDK_KEY_RELEASE:
		debug("dockapp.c: Window event button release on `%s'.", dapp->name);
		if(wmdock_replace_tile_dummy(dapp) == TRUE) {
			debug("dockapp.c: Replaceable dummy tile found.");
			wmdock_order_dockapps(wmdock_get_primary_anchor_dockapp() ? wmdock_get_primary_anchor_dockapp() : dapp);
		} else {
			wmdock_remove_anchors_tile_dummy();
			wmdock_set_autoposition_dockapp(dapp, wmdock_get_parent_dockapp(dapp));
		}
		if(dappDummy) {
			gtk_widget_hide(dappDummy->tile);
		}
		dappOnMove = NULL;

		break;
	case GDK_FOCUS_CHANGE:
		if(ev->focus_change.in == TRUE) {
			/* `in' is true if window window got the focus. */
			g_list_foreach(wmdock->dapps, (GFunc) wmdock_dockapp_tofront, NULL);
		}
		if(dappOnMove) {
			//wmdock_set_autoposition_dockapp(dapp, wmdock_get_parent_dockapp(dapp));
		}
		break;
	case GDK_VISIBILITY_NOTIFY:
		wmdock_redraw_dockapp(dapp);
		break;
	default:
		break;
	}
}


/**
 * Creates a dummy tile without any dockapp in it.
 *
 * @return A GTK window widget.
 */
GtkWidget *wmdock_create_tile_dummy()
{
	GtkWidget *dummy = NULL;

	dummy = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(dummy), DEFAULT_DOCKAPP_WIDTH,
			DEFAULT_DOCKAPP_HEIGHT);
	gtk_container_set_border_width(GTK_CONTAINER(dummy), 0);

	/* Disable window shrinking resizing and growing. */
	gtk_window_set_policy (GTK_WINDOW(dummy), FALSE, FALSE, FALSE);
	gtk_window_set_decorated(GTK_WINDOW(dummy), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(dummy), FALSE);
	/* Window visible on all workspaces. */
	gtk_window_stick(GTK_WINDOW(dummy));
	gtk_window_set_focus_on_map(GTK_WINDOW(dummy), FALSE);
	/* Hide window from the taskbar and the pager. */
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dummy), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(dummy), TRUE);
	gtk_window_set_opacity(GTK_WINDOW(dummy), 0.6);
	gtk_widget_set_size_request(dummy, DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT);
	gtk_window_set_keep_below(GTK_WINDOW(dummy), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dummy), GDK_WINDOW_TYPE_HINT_DND);

	return (dummy);
}


/**
 * Set focus to a dockapp.
 *
 * @param dapp DockappNode to focus.
 */
void wmdock_dockapp_tofront(DockappNode *dapp) {
	if(!dapp)
		return;

	if ( IS_PANELOFF(wmdock) )
		gdk_window_raise(dapp->tile->window);
}


gboolean wmdock_startup_dockapp(const gchar *cmd)
{
	gboolean ret;
	GError *err = NULL;

	ret = xfce_exec(cmd, FALSE, FALSE, &err);

	/* Errors will be evaluate in a later version. */
	if(err) g_clear_error (&err);

	return(ret);
}


void wmdock_destroy_dockapp(DockappNode *dapp)
{
	debug("dockapp.c: Destroy dockapp %s", dapp->name);
	XDestroyWindow(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()), dapp->i);
}


void wmdock_setupdnd_dockapp(DockappNode *dapp)
{
	if( ! IS_PANELOFF(wmdock)) {
		/* Make the "well label" a DnD destination. */
		gtk_drag_dest_set (GTK_WIDGET(dapp->s), GTK_DEST_DEFAULT_MOTION, targetList,
				nTargets, GDK_ACTION_MOVE);

		gtk_drag_source_set (GTK_WIDGET(dapp->s), GDK_BUTTON1_MASK, targetList,
				nTargets, GDK_ACTION_MOVE);

		g_signal_connect (dapp->s, "drag-begin",
				G_CALLBACK (drag_begin_handl), dapp);

		g_signal_connect (dapp->s, "drag-data-get",
				G_CALLBACK (drag_data_get_handl), dapp);

#if (GTK_MAJOR_VERSION >= 2 && GTK_MINOR_VERSION >= 12)
		g_signal_connect (dapp->s, "drag-failed",
				G_CALLBACK (drag_failed_handl), dapp);
#endif

		g_signal_connect (dapp->s, "drag-data-received",
				G_CALLBACK(drag_data_received_handl), dapp);
		g_signal_connect (dapp->s, "drag-drop",
				G_CALLBACK (drag_drop_handl), dapp);

		debug("dockapp.c: Setup DnD for dockapp %s", dapp->name);
	}
}


DockappNode *wmdock_find_startup_dockapp(const gchar *compCmd)
{
	GList *dapps;
	DockappNode *dapp = NULL;

	dapps = g_list_first(wmdock->dapps);

	while(dapps) {
		dapp = DOCKAPP(dapps->data);

		if(dapp) {
			if(!dapp->name && dapp->cmd) {
				if(!g_ascii_strcasecmp(dapp->cmd, compCmd)) {
					debug("dockapp.c: found startup dockapp with cmd %s", compCmd);
					return(dapp);
				}
			}
		}

		dapps = g_list_next(dapps);
	}

	return(NULL);
}

/**
 * Removes the anchor from dockapp.
 *
 * @param anchor The anchor dockapp.
 * @param dapp Dockapp to unlink.
 */
void wmdock_remove_anchor_dockapp(DockappNode *anchor, DockappNode *dapp)
{
	gint i;

	if( ! IS_PANELOFF(wmdock) || !anchor || !dapp)
		return;

	for(i = 0; i < GLUE_MAX; i++) {
		if(anchor->glue[i] == dapp) {
			debug("Remove glue from parent: %s", anchor->name);
			anchor->glue[i] = NULL;
		}
	}
}


/**
 * Clear data of a DockappNode and reorder the other.
 *
 * @param dapp DockappNode to free.
 */
void wmdock_free_dockapp(DockappNode *dapp)
{
	gint i;
	DockappNode *_dapp = NULL;

	if( IS_PANELOFF(wmdock) ) {
		_dapp = wmdock_get_parent_dockapp(dapp);
		if(_dapp) {
			/* Remove the glue of dapp from the parent. */
			wmdock_remove_anchor_dockapp(_dapp, dapp);

			/* Cover all the glue from the closed dockapp to the parent. */
			for(i = 0; i < GLUE_MAX; i++) {
				if(dapp->glue[i]) {
					if(!_dapp->glue[i]) {
						_dapp->glue[i] = dapp->glue[i];
					} else {
						// TODO: Verify this code, maybe broken?

						/* If another glue is on the parent destroy the others. */
						wmdock_destroy_dockapp(dapp->glue[i]);
						wmdock_free_dockapp(dapp->glue[i]);
					}
				}
			}
		}
	}

	wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
	gtk_widget_destroy(GTK_WIDGET(dapp->tile));
	g_free(dapp->name);
	g_free(dapp->cmd);
	g_free(dapp);

	if(g_list_length (wmdock->dapps) == 0) {
		wmdock_panel_draw_wmdock_icon(FALSE);
	}

	if( IS_PANELOFF(wmdock) && g_list_first(wmdock->dapps))
		wmdock_order_dockapps(DOCKAPP(g_list_first(wmdock->dapps)->data));

	wmdock_refresh_properties_dialog();
}


void wmdock_dapp_closed(GtkSocket *socket, DockappNode *dapp)
{
	debug("dockapp.c: closed window signal ! (dockapp: %s)", dapp->name);
	wmdock_free_dockapp(dapp);
}


void wmdock_redraw_dockapp(DockappNode *dapp)
{
	gtk_widget_unmap (GTK_WIDGET(dapp->s));
	wmdock_set_tile_background(dapp, gdkPbTileDefault);

	debug("dockapp.c: Dockapp %s redrawed with tile %d", dapp->name, wmdock->propDispTile);

	if(dapp->bg)
		gdk_window_process_updates(dapp->bg->window, FALSE);
	gtk_widget_map (GTK_WIDGET(dapp->s));
	gtk_widget_show_all(GTK_WIDGET(dapp->s));
}


/**
 * Update the background image of the DockApp.
 *
 * @param dapp DockappNode to update.
 */
void wmdock_update_tile_background(DockappNode *dapp)
{
	gtk_widget_realize(GTK_WIDGET(dapp->s));

	if (!dapp->bgimg)
		return;

	gtk_widget_set_app_paintable(GTK_WIDGET(dapp->s), TRUE);
	gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, dapp->bgimg, FALSE);

	if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->s)) & GTK_MAPPED)
		gtk_widget_queue_draw(GTK_WIDGET(dapp->s));

}


/**
 * Get the information if the dockapp the first one.
 *
 * @param dapp DockappNode to check.
 * @return gboolean TRUE if is the first dockapp otherwise false.
 */
gboolean wmdock_is_first_dockapp(DockappNode *dapp)
{
	if(!dapp)
		return FALSE;

	if(DOCKAPP(g_list_first(wmdock->dapps)->data) == dapp)
		return TRUE;

	return FALSE;
}


/**
 * Get parent dockapp.
 *
 * @param dapp Child dockapp.
 * @return DockAppNode Parent dockapp or null.
 */
DockappNode *wmdock_get_parent_dockapp(DockappNode *dapp) {
	gint i;
	GList *dapps;
	DockappNode *_dapp;

	dapps = g_list_first(wmdock->dapps);

	while(dapps) {
		_dapp = DOCKAPP(dapps->data);

		for(i = 0; i < GLUE_MAX; i++) {
			if(_dapp->glue[i] == dapp)
				return _dapp;
		}

		dapps = g_list_next(dapps);
	}

	return NULL;
}


/**
 * This set the tile background image to the DockApp.
 *
 * @param dapp The DockApp to set.
 * @param pm The background image to set.
 */
void wmdock_set_tile_background(DockappNode *dapp, GdkPixbuf *pb)
{
	GtkFixed *fixed = NULL;
	GdkGC * gc = NULL;

	if(!(fixed = (GtkFixed *) gtk_widget_get_ancestor(GTK_WIDGET(dapp->s), GTK_TYPE_FIXED)))
		return;

	if(wmdock->propDispTile == FALSE) {
		if(!dapp->bg)
			return;

		gtk_image_clear(GTK_IMAGE(dapp->bg));
		gtk_image_set_from_pixmap(GTK_IMAGE(dapp->bg), NULL, NULL);

		gdk_pixmap_unref(dapp->bgimg);
		dapp->bgimg = NULL;
		gdk_window_clear(GTK_WIDGET(dapp->tile)->window);

		return;
	}

	debug("dockapp.c: Setup background image (wmdock_set_tile_background).");
	gtk_widget_realize(GTK_WIDGET(dapp->tile));

	dapp->bgimg = gdk_pixmap_new(GTK_WIDGET(dapp->tile)->window,
			DEFAULT_DOCKAPP_WIDTH,DEFAULT_DOCKAPP_HEIGHT, -1);

	gc = gdk_gc_new(GTK_WIDGET(dapp->tile)->window);
	gdk_draw_pixbuf(dapp->bgimg, gc,
			pb, 0, 0, 0, 0, DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT,
			GDK_RGB_DITHER_NONE, 0, 0);
	gdk_gc_unref(gc);

	gtk_image_clear(GTK_IMAGE(dapp->bg));
	gtk_image_set_from_pixmap(GTK_IMAGE(dapp->bg),dapp->bgimg,NULL);

	if(dapp->s)
		wmdock_update_tile_background(dapp);
}


GtkWidget *wmdock_create_tile_from_socket(DockappNode *dapp)
{
	GtkWidget *align = NULL;
	GtkWidget *tile = NULL;
	GtkWidget *fixed = NULL;

	if( ! IS_PANELOFF(wmdock)) {
		/* Default: Put the Dockapp in the XFCE panel. */
		debug("dockapp.c: DockApp pushed in the XFCE panel.");
		tile = fixed = gtk_fixed_new();
		gtk_container_set_border_width(GTK_CONTAINER(fixed),0);

		/* Add the background tile. */
		dapp->bg = gtk_image_new();
		gtk_fixed_put(GTK_FIXED(fixed), dapp->bg, 0, 0);

		align = gtk_alignment_new(0.5, 0.5, 0, 0);
		gtk_widget_set_size_request(GTK_WIDGET(align), DEFAULT_DOCKAPP_WIDTH,
				DEFAULT_DOCKAPP_HEIGHT);
		gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(dapp->s));
		gtk_fixed_put(GTK_FIXED(fixed), align, 0, 0);

		gtk_widget_show(align);
	} else {
		/* If propDispPanelOff is true create a separate window with the
		 * Dockapp in it. It's emulates WindowMaker much more.
		 */
		debug("dockapp.c: Setup a separate window for the DockApp.");
		fixed = gtk_fixed_new();
		gtk_container_set_border_width(GTK_CONTAINER(fixed),0);

		tile = gtk_window_new(GTK_WINDOW_TOPLEVEL);

		gtk_window_set_title(GTK_WINDOW(tile), dapp->name);
		gtk_window_set_default_size(GTK_WINDOW(tile), DEFAULT_DOCKAPP_WIDTH,
				DEFAULT_DOCKAPP_HEIGHT);
		gtk_container_set_border_width(GTK_CONTAINER(tile), 0);

		/* Disable window shrinking resizing and growing. */
		gtk_window_set_policy (GTK_WINDOW(tile), FALSE, FALSE, FALSE);
		gtk_window_set_decorated(GTK_WINDOW(tile), FALSE);
		gtk_window_set_resizable(GTK_WINDOW(tile), FALSE);
		/* Window visible on all workspaces. */
		gtk_window_stick(GTK_WINDOW(tile));
		gtk_window_set_focus_on_map(GTK_WINDOW(tile), FALSE);
		/* Hide window from the taskbar and the pager. */
		gtk_window_set_skip_taskbar_hint(GTK_WINDOW(tile), TRUE);
		gtk_window_set_skip_pager_hint(GTK_WINDOW(tile), TRUE);

		/* Add the background tile. */
		dapp->bg = gtk_image_new();
		gtk_fixed_put(GTK_FIXED(fixed), dapp->bg, 0, 0);

		/* Center Dockapp in the window. */
		align = gtk_alignment_new(0.5, 0.5, 0, 0);
		gtk_widget_set_size_request(GTK_WIDGET(align), DEFAULT_DOCKAPP_WIDTH,
				DEFAULT_DOCKAPP_HEIGHT);
		gtk_window_set_default_size(GTK_WINDOW(tile), DEFAULT_DOCKAPP_WIDTH,
				DEFAULT_DOCKAPP_HEIGHT);
		gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(dapp->s));
		/* Add the alignment container to the window. */
		gtk_fixed_put(GTK_FIXED(fixed), align, 0, 0);

		gtk_container_add(GTK_CONTAINER(tile), fixed);

		gtk_widget_show(align);
	}
	return (tile);
}


/**
 * Calculate the position of the DockApp if the PanelOff mode is active.
 *
 * @param dapp The dockapp for the move operation.
 * @param prevDapp The previous dockapp.
 */
void wmdock_set_autoposition_dockapp(DockappNode *dapp, DockappNode *prevDapp)
{
	gint panelx, panely;
	gint x, y, i, gluepos = GLUE_MAX;

	if(!IS_PANELOFF(wmdock)) {
		return;
	}

	/* Setup the position of the first dockapp. */
	panelx = panely = x = y = 0;

	/* Initial define the position of the first anchor dockapp. */
	if(wmdock->anchorPos == -1)
		wmdock->anchorPos = xfce_panel_plugin_get_screen_position(wmdock->plugin);

	gtk_window_get_position(
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (wmdock->plugin))),
			&panelx, &panely);
	if(prevDapp) {
		for(i = 0; i < GLUE_MAX; i++) {
			if(prevDapp->glue[i] == dapp) {
				gluepos = i;
				break;
			}
		}
	}

	if(gluepos != GLUE_MAX) {
		wmdock_dockapp_child_pos(prevDapp, gluepos, &x, &y);
	} else {
		/* Determine the initial dockapp position. */
		switch(wmdock->anchorPos) {
		case XFCE_SCREEN_POSITION_NW_H:
		case XFCE_SCREEN_POSITION_N:
		case XFCE_SCREEN_POSITION_NE_H:
			if(!prevDapp) {
				x = gdk_screen_width() - DEFAULT_DOCKAPP_WIDTH;
				y = panelx == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) : 0;
			} else {
				wmdock_dockapp_child_pos(prevDapp, GLUE_B, &x, &y);
				prevDapp->glue[GLUE_B] = dapp;
			}
			break;

		case XFCE_SCREEN_POSITION_SW_H:
		case XFCE_SCREEN_POSITION_S:
		case XFCE_SCREEN_POSITION_SE_H:
			if(!prevDapp) {
				x = gdk_screen_width() - DEFAULT_DOCKAPP_WIDTH;
				y = panelx == 0 ? panely - DEFAULT_DOCKAPP_HEIGHT : gdk_screen_height() - DEFAULT_DOCKAPP_HEIGHT;
			} else {
				wmdock_dockapp_child_pos(prevDapp, GLUE_T, &x, &y);
				prevDapp->glue[GLUE_T] = dapp;
			}
			break;
		case XFCE_SCREEN_POSITION_NW_V:
		case XFCE_SCREEN_POSITION_W:
		case XFCE_SCREEN_POSITION_SW_V:
			if(!prevDapp) {
				x = panely == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) : 0;
				y = 0;
			} else {
				wmdock_dockapp_child_pos(prevDapp, GLUE_B, &x, &y);
				prevDapp->glue[GLUE_B] = dapp;
			}
			break;
		case XFCE_SCREEN_POSITION_NE_V:
		case XFCE_SCREEN_POSITION_E:
		case XFCE_SCREEN_POSITION_SE_V:
			if(!prevDapp) {
				x = panely == 0 ? gdk_screen_width() - xfce_panel_plugin_get_size(wmdock->plugin) - DEFAULT_DOCKAPP_WIDTH : gdk_screen_width() - DEFAULT_DOCKAPP_WIDTH;
				y = 0;
			} else {
				wmdock_dockapp_child_pos(prevDapp, GLUE_B, &x, &y);
				prevDapp->glue[GLUE_B] = dapp;
			}
			break;
		default:
			debug("dockapp.c: Can not determine panel position x = y = 0.");
			x = y = 0;
			break;
		}
	}
	gtk_window_move(GTK_WINDOW(dapp->tile), x, y);

	debug("dockapp.c: %d, Panel posx: %d, Panel posy: %d, prevDapp: %s, movex: %d, movey: %d",
			g_list_length(wmdock->dapps), panelx, panely, prevDapp ? prevDapp->name : "NO", x, y);

}


/**
 * Function order all dockapps (panel off) starting from dapp.
 *
 * @param dappStart Dockapp to start with.
 */
void wmdock_order_dockapps(DockappNode *dapp)
{
	gint i;

	if(! IS_PANELOFF(wmdock) || !dapp)
		return;

	for(i = 0; i < GLUE_MAX; i++) {
		wmdock_set_autoposition_dockapp(dapp, wmdock_get_parent_dockapp(dapp));

		debug("dockapp.c: Order dockapp %s", dapp->name);
		/* Recurse calling wmdock_order_dockapps, to walk the hole tree. */
		if(dapp->glue[i])
			wmdock_order_dockapps(dapp->glue[i]);
	}
}
