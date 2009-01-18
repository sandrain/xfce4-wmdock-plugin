/* wmdock xfce4 plugin by Andre Ellguth
 *
 * $Id$
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

#ifndef __WMDOCK_H__
#define __WMDOCK_H__

#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

typedef struct _dockapp {
 GtkSocket       *s;
 GdkNativeWindow i;
 GtkWidget       *bg;
 GtkWidget       *tile;
 gchar           *name;
 gchar           *cmd;
} DockappNode;

typedef struct {
 XfcePanelPlugin *plugin;

 GtkWidget       *eventBox;
	
 /* Plugin specific definitions */
 GtkWidget       *align;
 GtkWidget       *box;
 GtkWidget       *panelBox;
	
 gboolean        propDispTile;
 gboolean        propDispPropButton;
 gboolean        propDispAddOnlyWM;

 GList           *dapps;
} WmdockPlugin;

#endif /* __WMDOCK_H__ */
