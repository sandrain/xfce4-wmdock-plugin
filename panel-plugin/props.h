/* wmdock xfce4 plugin by Andre Ellguth
 * Properties dialog - Header.
 *
 * Authors:
 *   Andre Ellguth <andre@ellguth.com>
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

#ifndef __PROPS_H__
#define __PROPS_H__

/* prototypes */
void wmdock_properties_dialog(XfcePanelPlugin *plugin);
void wmdock_refresh_properties_dialog();
void wmdock_panel_draw_properties_button();
void wmdock_error_dialog_response (GtkWidget  *, gint);

#endif /* __PROPS_H__ */
