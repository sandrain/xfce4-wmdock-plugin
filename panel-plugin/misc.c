/* wmdock xfce4 plugin by Andre Ellguth
 * Misc functions.
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

#include "extern.h"
#include "wmdock.h"
#include "debug.h"
#include "dockapp.h"
#include "misc.h"

#include "xfce4-wmdock-plugin.xpm"

GdkPixbuf *get_icon_from_xpm_scaled(const char **xpmData, gint width, gint height)
{
	GdkPixbuf *gdkPb = NULL;
	GdkPixbuf *gdkPbScaled = NULL;

	gdkPb = gdk_pixbuf_new_from_xpm_data (xpmData);

	gdkPbScaled = gdk_pixbuf_scale_simple(gdkPb, width, height,
			GDK_INTERP_BILINEAR);

	g_object_unref (G_OBJECT (gdkPb));

	return(gdkPbScaled);
}


void set_xsmp_support(WnckWindow *w)
{
	/* Workaround to skip the XFCE4 session manager. If the window
	 * has this X text property set, the XFCE4 session manager will not
	 * automaticly start the dockapp after startup twice. */

	XTextProperty tp;
	static Atom _XA_SM_CLIENT_ID = None;

	_XA_SM_CLIENT_ID = XInternAtom (GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()), "SM_CLIENT_ID", False);


	tp.value = (unsigned char *) strdup("SM_CLIENT_ID");
	tp.encoding = XA_STRING;
	tp.format = 8;
	tp.nitems = 1;

	XSetTextProperty(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
			wnck_window_get_xid(w),
			&tp, _XA_SM_CLIENT_ID);

	XFree((unsigned char *)tp.value);
}


gboolean comp_str_with_pattern(const gchar *str, gchar *pattern, gsize s)
{
	gboolean    r = FALSE;

	if(!str || !pattern) return FALSE;

#if (GLIB_MAJOR_VERSION >= 2 && GLIB_MINOR_VERSION >= 14)
	GRegex *regex = g_regex_new (pattern, G_REGEX_CASELESS, 0, NULL);
	if(regex) {
		r = g_regex_match (regex, str, 0, NULL);
		g_regex_unref (regex);
	}
#else
	gsize    maxsize;
	gint     i;

	maxsize = s > strlen(pattern) ? strlen(pattern) : s;

	for(i=0; i<strlen(str)&&strlen(&str[i]) >= maxsize;i++)
		if(!g_ascii_strncasecmp (&str[i], pattern, maxsize)) {
			r = TRUE;
			break;
		}
#endif

	return r;
}


gboolean comp_dockapp_with_filterlist(const gchar *name)
{
	gchar **patterns = NULL;
	gint i=0;
	gsize s=0;
	gboolean r = FALSE;

	if(!wmdock->filterList) return FALSE;

	patterns = g_strsplit (wmdock->filterList, ";", 0);
	if(!patterns) return FALSE;
	while(patterns[i]) {
		s = strlen(patterns[i]) > 256 ? 256 : strlen(patterns[i]);
		if(s > 0 &&
				(r=comp_str_with_pattern(name, patterns[i], s)) == TRUE)
			break;
		i++;
	}

	g_strfreev(patterns);
	return r;
}


gboolean has_dockapp_hint(WnckWindow *w)
{
	Atom atype;
	int afmt;
	unsigned long int nitems;
	unsigned long int naft;
	gboolean r = FALSE;
	unsigned char *dat = NULL;

	gdk_error_trap_push();
	if (XGetWindowProperty(
			GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),
			wnck_window_get_xid(w), XfceDockAppAtom, 0, 1, False,
			XA_CARDINAL, &atype, &afmt, &nitems, &naft, &dat) == Success) {
		if (nitems==1 && ((long int *) dat)[0]==1) {
			r = TRUE;
		}
		XFree(dat);
	}
	XSync(GDK_DISPLAY_XDISPLAY(get_current_gdkdisplay()),False);

	gdk_error_trap_pop();

	return (r);
}


/**
 * Returns the current GdkDisplay.
 *
 * @return current GdkDisplay
 */
GdkDisplay *get_current_gdkdisplay()
{
	if(!wmdock || !wmdock->plugin)
		return gdk_display_get_default();

	return gdk_window_get_display(gtk_widget_get_toplevel(GTK_WIDGET(wmdock->plugin))->window);
}


/**
 * Return the current GdkScreen.
 *
 * @return current GdkScreen
 */
GdkScreen *get_current_gdkscreen()
{
	if(!wmdock || !wmdock->plugin)
		return gdk_screen_get_default();

	return gdk_window_get_screen(gtk_widget_get_toplevel(GTK_WIDGET(wmdock->plugin))->window);
}

/**
 * Returns the default anchor postion for the XFCE panel.
 *
 * @return default anchor postion
 */
AnchorPostion get_default_anchor_postion()
{
	AnchorPostion anchorPos = ANCHOR_BR;

	if(!wmdock || !wmdock->plugin)
		return anchorPos;

	switch(xfce_panel_plugin_get_screen_position(wmdock->plugin)) {
	case XFCE_SCREEN_POSITION_NW_H:
	case XFCE_SCREEN_POSITION_N:
	case XFCE_SCREEN_POSITION_NE_H:
		anchorPos = ANCHOR_TR;
		break;

	case XFCE_SCREEN_POSITION_SW_H:
	case XFCE_SCREEN_POSITION_S:
	case XFCE_SCREEN_POSITION_SE_H:
		anchorPos = ANCHOR_BR;
		break;

	case XFCE_SCREEN_POSITION_NW_V:
	case XFCE_SCREEN_POSITION_W:
	case XFCE_SCREEN_POSITION_SW_V:
		anchorPos = ANCHOR_TL;
		break;

	case XFCE_SCREEN_POSITION_NE_V:
	case XFCE_SCREEN_POSITION_E:
	case XFCE_SCREEN_POSITION_SE_V:
		anchorPos = ANCHOR_TR;
		break;

	default:
		break;
	}

	return anchorPos;
}

/**
 * Function which interacts with the wmdock icon.
 *
 * @param icon The wmdock icon widget.
 */
static void wmdock_icon_pressed(GtkWidget *icon)
{
	if( IS_PANELOFF(wmdock) )
		g_list_foreach(wmdock->dapps, (GFunc) wmdock_dockapp_tofront, NULL);
}


/**
 * Function get the number of xfce4-wmdock-instances are running.
 *
 * @return int Process count of wmdock-plugin.
 */
int wmdock_get_instance_count()
{
	int count = 0;

#ifdef __linux__
	int i;
	FILE *fp = NULL;
	char buf[BUF_MAX], username[BUF_MAX];

#ifdef HAVE_CONFIG_H
	snprintf(buf, BUF_MAX, "ps -C %s -ouser=", GETTEXT_PACKAGE);
#else
	snprintf(cmd, BUF_MAX, "ps -C xfce4-wmdock-plugin -ouser=");
#endif /* HAVE_CONFIG_H */

	fp = popen(buf, "r");
	if(!fp)
		return(-1);

	strncpy(username, (const char *) g_get_user_name(), BUF_MAX);
	while(!feof(fp)) {
		buf[0] = 0;
		fgets(buf, BUF_MAX, fp);
		/* Remove all newline and carriage returns. */
		for(i = 0; i < BUF_MAX; i++)
			buf[i] = (buf[i] == 0xA || buf[i] == 0xD) ? 0 : buf[i];

		if(!strncmp(buf, username, BUF_MAX))
			count++;
	}
	pclose(fp);
#endif /* __linux__ */

	debug("misc.c: Instance count: %d", count);

	return count;
}


void wmdock_panel_draw_wmdock_icon (gboolean redraw)
{
	static GtkWidget *eventBox = NULL;

	gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm,
			xfce_panel_plugin_get_size (wmdock->plugin) - 2,
			xfce_panel_plugin_get_size (wmdock->plugin) - 2);
	if(redraw == TRUE && wmdockIcon) {
		gtk_image_set_from_pixbuf (GTK_IMAGE(wmdockIcon), gdkPbIcon);
	} else {
		if(wmdockIcon) gtk_widget_destroy(wmdockIcon);
		if(eventBox) gtk_widget_destroy(eventBox);
		eventBox = gtk_event_box_new();

		wmdockIcon = gtk_image_new_from_pixbuf (gdkPbIcon);
		gtk_container_add(GTK_CONTAINER(eventBox), wmdockIcon);

		gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(eventBox),
				FALSE, FALSE, 0);
	}
	g_object_unref (G_OBJECT (gdkPbIcon));

	if( IS_PANELOFF(wmdock) )
		g_signal_connect (G_OBJECT(eventBox), "button_press_event", G_CALLBACK (wmdock_icon_pressed), NULL);

	gtk_widget_show_all(GTK_WIDGET(eventBox));
}
