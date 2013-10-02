/* wmdock xfce4 plugin by Andre Ellguth
 * dockapp.h
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

#ifndef __DOCKAPP_H__
#define __DOCKAPP_H__

#include "wmdock.h"

enum GluePosition {
	GLUE_T    = 1,
	GLUE_B    = 2,
	GLUE_L    = 4,
	GLUE_R    = 8,
	GLUE_MAX
};

typedef struct _dockapp DockappNode;
struct _dockapp {
	GtkSocket       *s;
	GdkNativeWindow i;
	gint            width;
	gint            height;
	GtkWidget       *bg;
	GdkPixmap       *bgimg;
	GtkWidget       *evbox;
	GtkWidget       *tile;
	gchar           *name;
	gchar           *cmd;
	DockappNode     *glue[GLUE_MAX];
};

#define DOCKAPP_DUMMY_TITLE "__WMDOCK_dummy__"
#define DOCKAPP(__dapp) ((DockappNode *) __dapp)

gboolean wmdock_startup_dockapp(const gchar *);
void wmdock_setupdnd_dockapp(DockappNode *);
void wmdock_destroy_dockapp(DockappNode *);
void wmdock_redraw_dockapp(DockappNode *);
void wmdock_free_dockapp(DockappNode *);
void wmdock_dapp_closed(GtkSocket *, DockappNode *);
DockappNode *wmdock_find_startup_dockapp(const gchar *);
GtkWidget *wmdock_create_tile_from_socket(DockappNode *);
void wmdock_set_socket_postion(DockappNode *, int, int);
void wmdock_set_autoposition_dockapp(DockappNode *, DockappNode *);
void wmdock_refresh_bg(GtkWidget *widget);
void wmdock_set_tile_background(DockappNode *, GdkPixbuf *);
void wmdock_update_tile_background(DockappNode *);
DockappNode *wmdock_get_parent_dockapp(DockappNode *);
DockappNode *wmdock_get_primary_anchor_dockapp();
void wmdock_dockapp_tofront(DockappNode *dapp);
void wmdock_dockapp_event_after_handler(GtkWidget *, GdkEvent *, DockappNode *);
void wmdock_dockapp_button_press_handler(GtkWidget *, GdkEventButton *, DockappNode *);
void wmdock_dockapp_button_release_handler(GtkWidget *, GdkEventButton *, DockappNode *);
void wmdock_dockapp_motion_notify_handler(GtkWidget *, GdkEventMotion *, DockappNode *);
void wmdock_remove_anchor_dockapp(DockappNode *, DockappNode *);
void wmdock_order_dockapps(DockappNode *);
GtkWidget *wmdock_create_tile_dummy();
gint wmdock_get_default_gluepos();

#endif /* __DOCKAPP_H__ */
