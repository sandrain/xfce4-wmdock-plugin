/* wmdock xfce4 plugin by Andre Ellguth
 * dockapp.h
 *
 * $Id: dockapp.h 28 2012-08-12 12:31:28Z ellguth $
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
	GLUE_TL   = GLUE_T | GLUE_L,
	GLUE_TR   = GLUE_T | GLUE_R,
	GLUE_BL   = GLUE_B | GLUE_L,
	GLUE_BR   = GLUE_B | GLUE_R,
	GLUE_MAX
};

typedef struct _dockapp DockappNode;
struct _dockapp {
 GtkSocket       *s;
 GdkNativeWindow i;
 GtkWidget       *bg;
 GdkPixmap       *bgimg;
 GtkWidget       *tile;
 gchar           *name;
 gchar           *cmd;
 DockappNode     *glue[GLUE_MAX];
};

#define DOCKAPP(__dapp) ((DockappNode *) __dapp)

gboolean wmdock_startup_dockapp(const gchar *);
void wmdock_setupdnd_dockapp(DockappNode *);
void wmdock_destroy_dockapp(DockappNode *);
void wmdock_redraw_dockapp(DockappNode *);
void wmdock_free_dockapp(DockappNode *);
void wmdock_dapp_closed(GtkSocket *, DockappNode *);
DockappNode *wmdock_find_startup_dockapp(const gchar *);
GtkWidget *wmdock_create_tile_from_socket(DockappNode *);
void wmdock_set_autoposition_dockapp(DockappNode *, DockappNode *);
void wmdock_refresh_bg(GtkWidget *widget);
void wmdock_set_tile_background(DockappNode *, GdkPixbuf *);
void wmdock_update_tile_background(DockappNode *);
gboolean wmdock_is_first_dockapp(DockappNode *);
DockappNode *wmdock_get_parent_dockapp(DockappNode *);
void wmdock_dockapp_tofront(DockappNode *dapp);
void wmdock_dockapp_paneloff_handler(GtkWidget *, GdkEvent *, DockappNode *);
void wmdock_remove_anchor_dockapp(DockappNode *, DockappNode *);
void wmdock_order_dockapps(DockappNode *);
GtkWidget *wmdock_create_dummy();

#endif /* __DOCKAPP_H__ */
