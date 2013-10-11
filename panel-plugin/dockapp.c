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
static DockappNode *dappOnMotion = NULL, *dappDummy = NULL;
static gint motionstartx = 0, motionstarty = 0;

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

	if(!prevDapp)
		return;

	/* Get the position of the previous DockApp if is accessable. */
	gtk_window_get_position(
			GTK_WINDOW (GTK_WIDGET (prevDapp->tile)), &prevx, &prevy);

	switch(gluepos) {
	case GLUE_T:
		*x = prevx;
		*y = prevy - DEFAULT_DOCKAPP_HEIGHT;
		break;
	case GLUE_B:
		*x = prevx;
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
	gint possible = GLUE_T | GLUE_B | GLUE_L | GLUE_R;
	gboolean prim;
	GList *dapps;
	DockappNode *_dapp = NULL;

	prim = (dapp == wmdock_get_primary_anchor_dockapp()) ? TRUE : FALSE;

	switch(wmdock->anchorPos) {
	/* Remove not possible snap positions for the dragging dockapp. */
	case ANCHOR_TR:
		possible^= (GLUE_T | GLUE_L);
		possible^= prim == TRUE ? GLUE_R : 0;
		break;
	case ANCHOR_BR:
		possible^= (GLUE_B | GLUE_R);
		possible^= prim == TRUE ? GLUE_L: 0;
		break;
	case ANCHOR_TL:
		possible^= (GLUE_T | GLUE_R);
		possible^= prim == TRUE ? GLUE_L : 0;
		break;
	case ANCHOR_BL:
		possible^= (GLUE_B | GLUE_L);
		possible^= prim == TRUE ? GLUE_R : 0;
		break;
	}

	gtk_window_get_position(
			GTK_WINDOW (GTK_WIDGET (dapp->tile)), &posx, &posy);

	dapps = g_list_first(wmdock->dapps);

	while(dapps) {
		if((_dapp = DOCKAPP(dapps->data))) {
			for(*gluepos = 0; *gluepos < GLUE_MAX; *gluepos=*gluepos+1) {
				if((!_dapp->glue[*gluepos] || !g_strcmp0(_dapp->glue[*gluepos]->name, DOCKAPP_DUMMY_TITLE))
						&& (possible & *gluepos)) {
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
 * @param dapp Replacement dockapp.
 * @return TRUE if dummy tile is replaced else FALSE.
 */
static gboolean wmdock_replace_tile_dummy(DockappNode *dapp)
{
	gint i, j;
	GList *dapps;
	DockappNode *_dapp = NULL, *parent = NULL, *_parent = NULL;

	parent = wmdock_get_parent_dockapp(dapp);
	if(!parent && wmdock_get_primary_anchor_dockapp() == dapp) {
		/* Set the nearest dockapp to parent if the current dapp is primary.
		 * The nearest is the new primary dockapp. */
		for(i = 0; i < GLUE_MAX; i++) {
			if((parent = dapp->glue[i]))
				break;
		}
	}
	debug("dockapp.c: Parent DockApp of `%s' is `%s'", dapp->name, parent ? parent->name : "<none>");

	dapps = g_list_first(wmdock->dapps);
	while(dapps) {
		if((_dapp = DOCKAPP(dapps->data))) {
			for(i = 0; i < GLUE_MAX; i++) {
				if(_dapp->glue[i] && !g_strcmp0(_dapp->glue[i]->name, DOCKAPP_DUMMY_TITLE)) {
					g_list_foreach(wmdock->dapps, (GFunc) wmdock_remove_anchor_dockapp, dapp);
					_dapp->glue[i] = dapp;
					debug("dockapp.c: Connect `%s' to `%s' with glue.", dapp->name, _dapp->name);
					for(j = 0; j < GLUE_MAX; j++) {
						if(parent) {
							if(parent == dapp->glue[j])
								dapp->glue[j] = NULL;

							/* Transfer all connected DockApps to the parent. */
							_parent = parent;
							while(_parent->glue[j] && _parent->glue[j] != dapp->glue[j]) {
								_parent = _parent->glue[j];
							}
							if(dapp->glue[j] && _parent->glue[j] == dapp->glue[j]) {
								debug("dockapp.c: Parent_Connect `%s' to `%s' with glue.", dapp->glue[j]->name, _parent->name);
								continue;
							}
							_parent->glue[j] = dapp->glue[j];
						}
						/* Remove old anchor itself or all anchors it was the first anchor. */
						dapp->glue[j] = NULL;
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
 * Event handle for the tile in panel off mode (button_press)
 *
 * @param tile The window of the event.
 * @param ev GdkEventButton.
 * @param dapp DockAppNode of the event.
 */
static void wmdock_dockapp_button_press_handler(GtkWidget *window, GdkEventButton *ev, DockappNode *dapp)
{
	debug("dockapp.c: Window button press event (dapp: `%s')", dapp->name);
	dappOnMotion = dapp;
	motionstartx = (gint) ev->x;
	motionstarty = (gint) ev->y;
	gtk_window_set_keep_above(GTK_WINDOW(dapp->tile), TRUE);
	gtk_window_set_keep_below(GTK_WINDOW(dapp->tile), FALSE);
}


/**
 * Event handle for the tile in panel off mode (button_release)
 *
 * @param tile The window of the event.
 * @param ev GdkEventButton.
 * @param dapp DockAppNode of the event.
 */
static void wmdock_dockapp_button_release_handler(GtkWidget *window, GdkEventButton *ev, DockappNode *dapp)
{
	debug("dockapp.c: Window button release event (dapp: `%s')", dapp->name);
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

	dappOnMotion = NULL;
	gtk_window_set_keep_above(GTK_WINDOW(dapp->tile), FALSE);
	gtk_window_set_keep_below(GTK_WINDOW(dapp->tile), TRUE);
}


/**
 * Event handle for the tile in panel off mode (motion_notify)
 *
 * @param tile The window of the event.
 * @param ev GdkEventButton.
 * @param dapp DockAppNode of the event.
 */
static void wmdock_dockapp_motion_notify_handler(GtkWidget *window, GdkEventMotion *ev, DockappNode *dapp)
{
	gint gluepos, x, y, posx, posy;
	DockappNode *dappSnap = NULL;
	GdkModifierType m;

	debug("dockapp.c: Window motion notify event (dapp: `%s')", dapp->name);

	gdk_window_get_pointer(dapp->tile->window, &x, &y, &m);
	if(window && dappOnMotion && (m & GDK_BUTTON1_MASK)) {
		gtk_window_get_position(GTK_WINDOW(dapp->tile), &posx, &posy);
		debug("dockapp.c: Mouse x: %d,  Mouse y: %d,  Dapp x: %d, Dapp y: %d,  Msx: %d,  Msy: %d",
				x, y, posx, posy, motionstartx, motionstarty);
		gtk_window_move(GTK_WINDOW(dapp->tile), posx - (motionstartx - x), posy - (motionstarty - y));
	}

	if(dappOnMotion == dapp) {
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
}


/* Return the translation from glue int postion to glue name.
 *
 * @param gluePos Position to be translated.
 * @return String representation of the postion.
 */
const gchar *wmdock_get_glue_name(const gint glusPos)
{
	static gchar ret[10];

	switch(glusPos) {
	case GLUE_B:
		g_strlcpy(ret, "GLUE_B", sizeof(ret));
		break;
	case GLUE_L:
		g_strlcpy(ret, "GLUE_L", sizeof(ret));
		break;
	case GLUE_R:
		g_strlcpy(ret, "GLUE_R", sizeof(ret));
		break;
	case GLUE_T:
		g_strlcpy(ret, "GLUE_T", sizeof(ret));
		break;
	default:
		return NULL;
	}

	return (ret);
}

/* Return the translation from the glue name to the postion.
 *
 * @param name The name to be translated to a number.
 * @return The position as integer. On error -1 is returned.
 */
gint wmdock_get_glue_position(gchar const *name)
{
	if(!g_ascii_strcasecmp(name, "GLUE_B"))
		return GLUE_B;
	else if(!g_ascii_strcasecmp(name, "GLUE_L"))
		return GLUE_L;
	else if(!g_ascii_strcasecmp(name, "GLUE_R"))
		return GLUE_R;
	else if(!g_ascii_strcasecmp(name, "GLUE_T"))
		return GLUE_T;

	return -1;
}


/**
 * Determine the main anchor DockApp.
 *
 * @return DockappNode which is the main anchor otherwise NULL.
 */
DockappNode *wmdock_get_primary_anchor_dockapp()
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
 * Event handler for the tile in panel off mode (event_after).
 *
 * @param tile The window of the event.
 * @param ev Event informations.
 * @param dapp DockappNode of the event.
 */
void wmdock_dockapp_event_after_handler(GtkWidget *window, GdkEvent *ev, DockappNode *dapp)
{
	debug("dockapp.c: Window event-after: %d. (dapp: `%s'), dappOnMove: %s", ev->type, dapp->name,
			dappOnMotion ? "Yes": "No");
	switch(ev->type) {
	case GDK_FOCUS_CHANGE:
		if(ev->focus_change.in == TRUE) {
			/* `in' is true if window window got the focus. */
			g_list_foreach(wmdock->dapps, (GFunc) wmdock_dockapp_tofront, NULL);
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

	if ( IS_PANELOFF(wmdock) ) {
		gtk_window_set_keep_below(GTK_WINDOW(dapp->tile), FALSE);
		gdk_window_raise(dapp->tile->window);
		gtk_window_set_keep_above(GTK_WINDOW(dapp->tile), FALSE);
		gtk_window_set_keep_below(GTK_WINDOW(dapp->tile), TRUE);
	}
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
		if((dapp = DOCKAPP(dapps->data))) {
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
			debug("Remove dockapp `%s' from the parent: `%s'", anchor->glue[i]->name, anchor->name);
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
		if((_dapp = wmdock_get_parent_dockapp(dapp))) {
			/* Remove the glue of dapp from the parent. */
			wmdock_remove_anchor_dockapp(_dapp, dapp);

			/* Cover all the glue from the closed dockapp to the parent. */
			for(i = 0; i < GLUE_MAX; i++) {
				if(dapp->glue[i]) {
					if(!_dapp->glue[i]) {
						_dapp->glue[i] = dapp->glue[i];
					} else {
						/* TODO: Verify this code, maybe broken? */

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
	gtk_widget_realize(GTK_WIDGET(dapp->evbox));

	if (!dapp->bgimg)
		return;

	gtk_widget_set_app_paintable(GTK_WIDGET(dapp->evbox), TRUE);
	gtk_widget_set_app_paintable(GTK_WIDGET(dapp->bg), TRUE);
	gdk_window_set_back_pixmap(GTK_WIDGET(dapp->evbox)->window, dapp->bgimg, FALSE);
	gdk_window_set_back_pixmap(GTK_WIDGET(dapp->bg)->window, dapp->bgimg, FALSE);

	if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->evbox)) & GTK_MAPPED)
		gtk_widget_queue_draw(GTK_WIDGET(dapp->evbox));
	if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->bg)) & GTK_MAPPED)
		gtk_widget_queue_draw(GTK_WIDGET(dapp->bg));
}


/**
 * Get parent dockapp.
 *
 * @param dapp Child dockapp.
 * @return DockAppNode Parent dockapp or null.
 */
DockappNode *wmdock_get_parent_dockapp(DockappNode *dapp)
{
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
 * Get the default glue position of the dockapps.
 *
 * @return Default glue postion.
 */
gint wmdock_get_default_gluepos()
{
	if(wmdock->anchorPos == ANCHOR_TL || wmdock->anchorPos == ANCHOR_TR)
		return (GLUE_B);
	else
		return (GLUE_T);
}


/**
 * This set the tile background image to the DockApp.
 *
 * @param dapp The DockApp to set.
 * @param pm The background image to set.
 */
void wmdock_set_tile_background(DockappNode *dapp, GdkPixbuf *pb)
{
	GdkGC * gc = NULL;

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

	debug("dockapp.c: Setup background image for dapp `%s' (wmdock_set_tile_background).", dapp->name);
	gtk_widget_realize(GTK_WIDGET(dapp->bg));

	if(!dapp->bgimg)
		dapp->bgimg = gdk_pixmap_new(GTK_WIDGET(dapp->tile)->window, DEFAULT_DOCKAPP_WIDTH,DEFAULT_DOCKAPP_HEIGHT, -1);

	gdk_window_clear(GTK_WIDGET(dapp->bg)->window);
	gc = gdk_gc_new(GTK_WIDGET(dapp->bg)->window);
	gdk_draw_pixbuf(dapp->bgimg, gc,
			pb, 0, 0, 0, 0, DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT,
			GDK_RGB_DITHER_NONE, 0, 0);
	gdk_gc_unref(gc);

	if(dapp->bg)
		wmdock_update_tile_background(dapp);

}


void wmdock_set_socket_postion(DockappNode *dapp, int x, int y)
{
	GtkFixed *fixed = NULL;

	if(!(fixed = (GtkFixed *) gtk_widget_get_ancestor(GTK_WIDGET(dapp->evbox), GTK_TYPE_FIXED)))
		return;

	gtk_widget_set_size_request(GTK_WIDGET(dapp->evbox), dapp->width, dapp->height);
	gtk_fixed_move(fixed, GTK_WIDGET(dapp->evbox), x, y);
}


GtkWidget *wmdock_create_tile_from_socket(DockappNode *dapp)
{
	GtkWidget *tile = NULL;
	GtkWidget *_fixed = NULL;
	GtkWidget *_evbox = NULL;

	tile = _fixed = gtk_fixed_new();
	gtk_container_set_border_width(GTK_CONTAINER(_fixed), 0);

	/* Create an internal eventbox to catch click events outside the socket. */
	_evbox = gtk_event_box_new();
	gtk_widget_set_size_request(GTK_WIDGET(_evbox), DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT);
	gtk_fixed_put(GTK_FIXED(_fixed), _evbox, 0, 0);

	/* Create an eventbox to catch to click and motion events inside the socket. */
	dapp->evbox = gtk_event_box_new();
	gtk_event_box_set_above_child(GTK_EVENT_BOX(dapp->evbox), FALSE);

	/* Add the GtkSocket with the dockapp fixed and centered. */
	gtk_container_add(GTK_CONTAINER(dapp->evbox), GTK_WIDGET(dapp->s));
	gtk_fixed_put(GTK_FIXED(_fixed), GTK_WIDGET(dapp->evbox),
			(DEFAULT_DOCKAPP_WIDTH - dapp->width) / 2, (DEFAULT_DOCKAPP_HEIGHT - dapp->height) / 2);

	/* Add the background tile. */
	dapp->bg = wmdock->propDispTile == TRUE ? gtk_image_new_from_pixbuf(gdkPbTileDefault) : gtk_image_new();
	gtk_widget_set_size_request(GTK_WIDGET(dapp->bg), DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT);
	gtk_container_add(GTK_CONTAINER(_evbox), GTK_WIDGET(dapp->bg));

	if( IS_PANELOFF(wmdock) ) {
		/* If propDispPanelOff is true create a separate window with the
		 * Dockapp in it. It's emulates WindowMaker much more.
		 */
		tile = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		debug("dockapp.c: Setup a separate window for the DockApp.");

		gtk_window_set_title(GTK_WINDOW(tile), dapp->name);
		gtk_window_set_default_size(GTK_WINDOW(tile), DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT);
		gtk_container_set_border_width(GTK_CONTAINER(tile), 0);
		/* To disable dragging by alt key. */
		gtk_window_set_type_hint(GTK_WINDOW(tile), GDK_WINDOW_TYPE_HINT_DOCK);
		gtk_window_set_keep_below(GTK_WINDOW(tile), TRUE);
		gtk_window_set_keep_above(GTK_WINDOW(tile), FALSE);

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

		gtk_container_add(GTK_CONTAINER(tile), _fixed);

		g_signal_connect(G_OBJECT(tile), "motion_notify_event", G_CALLBACK(wmdock_dockapp_motion_notify_handler), dapp);
		g_signal_connect(G_OBJECT(tile), "button_press_event", G_CALLBACK(wmdock_dockapp_button_press_handler), dapp);
		g_signal_connect(G_OBJECT(tile), "button_release_event", G_CALLBACK(wmdock_dockapp_button_release_handler), dapp);
	}

	gtk_widget_show(_fixed);

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
	gint panelx, panely, plugx, plugy;
	gint x, y, i, offsetx, offsety, gluepos = GLUE_MAX;
	XfceScreenPosition xfceScrPos;

	if(! IS_PANELOFF(wmdock) || !dapp )
		return;

	/* Setup the position of the first dockapp. */
	panelx = panely = plugx = plugy = x = y = 0;

	gtk_window_get_position(
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (wmdock->plugin))),
			&panelx, &panely);
	gdk_window_get_position (GDK_WINDOW (GTK_WIDGET (wmdock->plugin)->window), &plugx, &plugy);

	for(i = 0; prevDapp && i < GLUE_MAX; i++) {
		if(prevDapp->glue[i] == dapp) {
			gluepos = i;
			break;
		}
	}

	if(gluepos != GLUE_MAX) {
		/* Realign the existing dockapp. */
		wmdock_dockapp_child_pos(prevDapp, gluepos, &x, &y);
	} else {
		/* Align a new dockapp. */
		if(prevDapp) {
			/* If a parent dockapp already exists. */
			gluepos = wmdock_get_default_gluepos();

			wmdock_dockapp_child_pos(prevDapp, gluepos, &x, &y);
			prevDapp->glue[gluepos] = dapp;
		} else {
			/* Determine the initial dockapp position. */
			xfceScrPos = xfce_panel_plugin_get_screen_position(wmdock->plugin);

			offsetx = offsety = 0;
			switch(wmdock->anchorPos) {
			case ANCHOR_TL:
				if(xfceScrPos == XFCE_SCREEN_POSITION_NW_V ||
						xfceScrPos == XFCE_SCREEN_POSITION_W ||
						xfceScrPos == XFCE_SCREEN_POSITION_SW_V) {
					offsetx = panelx == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) + 1 : 0;
					offsety = 0;
				} else if (xfceScrPos == XFCE_SCREEN_POSITION_NW_H ||
						xfceScrPos == XFCE_SCREEN_POSITION_N ||
						xfceScrPos == XFCE_SCREEN_POSITION_NE_H) {
					offsetx = 0;
					offsety = panely == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) + 1 : 0;
				}

				x = 0 + offsetx;
				y = 0 + offsety;
				break;
			case ANCHOR_TR:
				if(xfceScrPos == XFCE_SCREEN_POSITION_NE_V ||
						xfceScrPos == XFCE_SCREEN_POSITION_E ||
						xfceScrPos == XFCE_SCREEN_POSITION_SE_V) {
					offsetx = xfce_panel_plugin_get_size(wmdock->plugin) + 1;
					offsety = 0;
				} else if (xfceScrPos == XFCE_SCREEN_POSITION_NW_H ||
						xfceScrPos == XFCE_SCREEN_POSITION_N ||
						xfceScrPos == XFCE_SCREEN_POSITION_NE_H) {
					offsetx = 0;
					offsety = panely == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) + 1 : 0;
				}

				x = gdk_screen_get_width(get_current_gdkscreen()) - DEFAULT_DOCKAPP_WIDTH - offsetx;
				y = 0 + offsety;
				break;
			case ANCHOR_BL:
				if(xfceScrPos == XFCE_SCREEN_POSITION_NW_V ||
						xfceScrPos == XFCE_SCREEN_POSITION_W ||
						xfceScrPos == XFCE_SCREEN_POSITION_SW_V) {
					offsetx = panelx == 0 ? xfce_panel_plugin_get_size(wmdock->plugin) + 1 : 0;
					offsety = 0;
				} else if (xfceScrPos == XFCE_SCREEN_POSITION_SW_H ||
						xfceScrPos == XFCE_SCREEN_POSITION_S ||
						xfceScrPos == XFCE_SCREEN_POSITION_SE_H) {
					offsetx = 0;
					offsety = xfce_panel_plugin_get_size(wmdock->plugin) + 1;
				}

				x = 0 + offsetx;
				y = gdk_screen_get_height(get_current_gdkscreen()) - DEFAULT_DOCKAPP_HEIGHT - offsety;
				break;
			case ANCHOR_BR:
				if(xfceScrPos == XFCE_SCREEN_POSITION_NE_V ||
						xfceScrPos == XFCE_SCREEN_POSITION_E ||
						xfceScrPos == XFCE_SCREEN_POSITION_SE_V) {
					offsetx = xfce_panel_plugin_get_size(wmdock->plugin) + 1;
					offsety = 0;
				} else if (xfceScrPos == XFCE_SCREEN_POSITION_SW_H ||
						xfceScrPos == XFCE_SCREEN_POSITION_S ||
						xfceScrPos == XFCE_SCREEN_POSITION_SE_H) {
					offsetx = 0;
					offsety = xfce_panel_plugin_get_size(wmdock->plugin) + 1;
				}

				x = gdk_screen_get_width(get_current_gdkscreen()) - DEFAULT_DOCKAPP_WIDTH - offsetx;
				y = gdk_screen_get_height(get_current_gdkscreen()) - DEFAULT_DOCKAPP_HEIGHT - offsety;
				break;
			default:
				debug("dockapp.c: Can not determine panel position x = y = 0.");
				x = y = 0;
				break;
			}
		} /* else */
	}
	gtk_window_move(GTK_WINDOW(dapp->tile), x, y);

	debug("dockapp.c: %d, Panel posx: %d, Panel posy: %d, Plug posx: %d, Plug posy: %d, prevDapp: %s, movex: %d, movey: %d",
			g_list_length(wmdock->dapps), panelx, panely, plugx, plugy, prevDapp ? prevDapp->name : "NO", x, y);

}


/**
 * Function order all dockapps (panel off) starting from dapp.
 *
 * @param dappStart Dockapp to start with.
 */
void wmdock_order_dockapps(DockappNode *dapp)
{
	gint i;

	if(! IS_PANELOFF(wmdock) || !dapp )
		return;

	for(i = 0; i < GLUE_MAX; i++) {
		wmdock_set_autoposition_dockapp(dapp, wmdock_get_parent_dockapp(dapp));

		debug("dockapp.c: Order dockapp %s", dapp->name);
		/* Recurse calling wmdock_order_dockapps, to walk the hole tree. */
		if(dapp->glue[i])
			wmdock_order_dockapps(dapp->glue[i]);
	}
}
