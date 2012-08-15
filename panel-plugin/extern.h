/* wmdock xfce4 plugin by Andre Ellguth
 * extern.h
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

#ifndef __EXTERN_H__
#define __EXTERN_H__

#include "wmdock.h"

extern WmdockPlugin *wmdock;
extern GdkPixbuf    *gdkPbIcon;
extern GdkPixbuf    *gdkPbTileDefault;
extern GtkWidget    *wmdockIcon;
extern Atom         XfceDockAppAtom;
extern gchar        **rcCmds;
extern gint         rcCmdcnt;
extern gboolean     rcPanelOff;

#endif /* __EXTERN_H__ */
