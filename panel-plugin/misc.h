/* wmdock xfce4 plugin by Andre Ellguth
 * misc.h
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
#ifndef __MISC_H__
#define __MISC_H__

/* Prototypes */
GdkPixbuf *get_icon_from_xpm_scaled(const char **, gint, gint);
GdkDisplay *get_current_gdkdisplay();
GdkScreen *get_current_gdkscreen();
void set_xsmp_support(WnckWindow *);
gboolean has_dockapp_hint(WnckWindow *);
gboolean comp_dockapp_with_filterlist(const gchar *);
gboolean comp_str_with_pattern(const gchar *, gchar *, gsize);
void wmdock_panel_draw_wmdock_icon (gboolean redraw);
int wmdock_get_instance_count();

#endif /* __MISC_H__ */
