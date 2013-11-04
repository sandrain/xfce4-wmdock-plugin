/* wmdock xfce4 plugin by Andre Ellguth
 * Properties dialog.
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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/libxfce4panel.h>

#include "extern.h"
#include "props.h"
#include "wmdock.h"
#include "debug.h"
#include "dockapp.h"
#include "misc.h"

#include "xfce4-wmdock-plugin.xpm"

/* Properties dialog */
static struct {
	GtkWidget *dlg; /* Dialogs */
	GtkWidget *vbox, *vbox2, *vboxGeneral, *vboxDetect, *vboxPanelOffOpts; /* Vertical boxes */
	GtkWidget *hbox, *hboxPanelOffOpts; /* Horizontal boxes */
	GtkWidget *frmGeneral, *frmDetect, *frmPanelOffOpts; /* Frames */
	GtkWidget *lblSel, *lblCmd, *lblPanelOffPlacement, *lblPanelOffHint; /* Labels */
	GtkWidget *chkDispTile, *chkPropButton, *chkAddOnlyWM, *chkPanelOff, *chkPanelOffIgnoreOffset, *chkPanelOffKeepAbove, *chkPanelOffFreePositioning; /* Check boxes */
	GtkWidget *radioPanelOffTL, *radioPanelOffTR, *radioPanelOffBL, *radioPanelOffBR; /* Radio buttons */
	GtkWidget *imageContainer, *container; /* Misc. containers */
	GtkWidget *tblPanelOff; /* Layout tables */
	GtkWidget *imageTile, *image; /* Images */
	GtkWidget *txtCmd, *txtPatterns; /* Text boxes */
	GtkWidget *cbx; /* Combo boxes */
	GtkWidget *btnMoveUp, *btnMoveDown; /* Misc. buttons */
} prop;

static GtkWidget *btnProperties = NULL;


static void wmdock_set_prop_paneloff(gboolean enabled)
{
	gtk_widget_set_sensitive(GTK_WIDGET(prop.radioPanelOffBL), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.radioPanelOffBR), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.radioPanelOffTL), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.radioPanelOffTR), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.chkPanelOffKeepAbove), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.chkPanelOffFreePositioning), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.chkPanelOffIgnoreOffset), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.lblPanelOffPlacement), enabled);
	gtk_widget_set_sensitive(GTK_WIDGET(prop.lblPanelOffHint), enabled);
}


static void wmdock_properties_fillcmbx(DockappNode *dapp, GtkWidget *gtkComboBox)
{

	if(gtkComboBox) {
		debug("props.c: %s append to list", dapp->name);
		gtk_combo_box_append_text (GTK_COMBO_BOX(gtkComboBox), dapp->name);
	}
}


static void wmdock_properties_chkdisptile(GtkToggleButton *gtkChkDispTile, gpointer user_data)
{
	wmdock->propDispTile = gtk_toggle_button_get_active(gtkChkDispTile);

	g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
	gtk_widget_show_all(GTK_WIDGET(wmdock->box));

	wmdock_msg_dialog(GTK_MESSAGE_INFO, _("Changes maybe not work properly until you restart XFCE!"));
}


static void wmdock_properties_chkpropbtn(GtkToggleButton *gtkChkPropButton, gpointer user_data)
{
	wmdock->propDispPropButton = gtk_toggle_button_get_active(gtkChkPropButton);

	if(wmdock->propDispPropButton == FALSE) {
		if(btnProperties) gtk_widget_destroy(btnProperties);
		btnProperties = NULL;
	}
	else
		wmdock_panel_draw_properties_button ();
}


static void wmdock_properties_chkaddonlywm(GtkToggleButton *gtkChkAddOnlyWM, gpointer user_data)
{
	wmdock->propDispAddOnlyWM = gtk_toggle_button_get_active(gtkChkAddOnlyWM);
	gtk_widget_set_sensitive (GTK_WIDGET (prop.txtPatterns),
			wmdock->propDispAddOnlyWM);
}


static void wmdock_properties_chkpaneloffignoreoffset(GtkToggleButton *gtkChkPanelOffIgnoreOffset, gpointer user_data)
{
	wmdock->propPanelOffIgnoreOffset = gtk_toggle_button_get_active(gtkChkPanelOffIgnoreOffset);
	wmdock_order_dockapps(wmdock_get_primary_anchor_dockapp());
}


static void wmdock_properties_chkpaneloffkeepabove(GtkToggleButton *gtkChkPanelOffKeepAbove, gpointer user_data)
{
	wmdock->propPanelOffKeepAbove = gtk_toggle_button_get_active(gtkChkPanelOffKeepAbove);
	g_list_foreach(wmdock->dapps, (GFunc) wmdock_dockapp_tofront, NULL);
}


static void wmdock_properties_chkpanelofffreepositioning(GtkToggleButton *gtkChkPanelOffFreePositioning, gpointer user_data)
{
	wmdock->propPanelOffFreePositioning = gtk_toggle_button_get_active(gtkChkPanelOffFreePositioning);
	wmdock->panelOffFpY = wmdock->panelOffFpX = G_MININT;
	wmdock_order_dockapps(wmdock_get_primary_anchor_dockapp());
}


static void wmdock_properties_radiopaneloff(GtkRadioButton *gtkRadioPanelOff, gpointer user_data)
{
	gint _anchorPos = wmdock->anchorPos;

	if(gtkRadioPanelOff == GTK_RADIO_BUTTON(prop.radioPanelOffTL))
		_anchorPos = ANCHOR_TL;
	else if(gtkRadioPanelOff == GTK_RADIO_BUTTON(prop.radioPanelOffTR))
		_anchorPos = ANCHOR_TR;
	else if(gtkRadioPanelOff == GTK_RADIO_BUTTON(prop.radioPanelOffBL))
		_anchorPos = ANCHOR_BL;
	else if(gtkRadioPanelOff == GTK_RADIO_BUTTON(prop.radioPanelOffBR))
		_anchorPos = ANCHOR_BR;

	wmdock->panelOffFpX = wmdock->panelOffFpY = G_MININT;
	wmdock_set_new_anchorpos(_anchorPos);
	wmdock->anchorPos = _anchorPos;
	wmdock_order_dockapps(wmdock_get_primary_anchor_dockapp());
}


static void wmdock_properties_chkpaneloff(GtkToggleButton *gtkChkPanelOff, gpointer user_data)
{
	if((rcPanelOff = gtk_toggle_button_get_active(gtkChkPanelOff)) == TRUE) {
		wmdock->anchorPos = get_default_anchor_postion();
		wmdock_set_prop_paneloff(TRUE);
	} else {
		wmdock_set_prop_paneloff(FALSE);
	}

	if(g_list_length(wmdock->dapps)) {
		wmdock_msg_dialog(GTK_MESSAGE_INFO, _("Changes will take effect when you restart XFCE!"));
	} else {
		/* If no dockapp is started enable/disable panel off mode. */
		wmdock->propPanelOff = rcPanelOff;
	}
}


static gboolean wmdock_properties_refresh_dapp_icon()
{
	GdkPixmap *pm = NULL;
	DockappNode *dapp = NULL;
	gboolean ret;

	if(prop.dlg && prop.image && prop.cbx) {
		dapp = DOCKAPP(g_list_nth_data(wmdock->dapps,
				gtk_combo_box_get_active(GTK_COMBO_BOX(prop.cbx))));
		if(dapp) {
			pm = gdk_pixmap_foreign_new (dapp->i);
			if(pm) {
				gtk_image_set_from_pixmap (GTK_IMAGE(prop.image), pm, NULL);
				gtk_widget_show(prop.image);
				g_object_unref (G_OBJECT(pm));
			}
			else {
				gtk_image_set_from_pixbuf (GTK_IMAGE(prop.image), gdkPbTileDefault);
				gtk_widget_show(prop.image);
				/* Check if the window is gone. */
				if(!wnck_window_get (dapp->i)) {
					ret = FALSE;
					wmdock_dapp_closed(dapp->s, dapp);
				}
			}
		}

		ret = TRUE;
	} else {
		ret = FALSE;
		debug("props.c: wmdock_properties_refresh_dapp_icon status changed to FALSE");
	}

	return (ret);
}


static void wmdock_properties_changed (GtkWidget *gtkComboBox, GtkWidget *gtkTxtCmd)
{
	DockappNode *dapp = NULL;


	dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox))));
	if(dapp) {
		debug("props.c: changed, %s selected:", dapp->name);

		gtk_entry_set_text(GTK_ENTRY(gtkTxtCmd), dapp->cmd);

		wmdock_properties_refresh_dapp_icon();
	}
}


static void wmdock_properties_moveup (GtkWidget *gtkBtnMoveUp, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;

	pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));

	if(g_list_length(wmdock->dapps) > 1 && pos > 0) {
		dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, pos));

		if(dapp) {
			wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
			wmdock->dapps = g_list_insert(wmdock->dapps, dapp, pos - 1);
			gtk_combo_box_remove_text(GTK_COMBO_BOX(gtkComboBox), pos);
			gtk_combo_box_insert_text(GTK_COMBO_BOX(gtkComboBox), pos - 1, dapp->name);
			gtk_combo_box_set_active(GTK_COMBO_BOX(gtkComboBox), pos - 1);
			gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile), pos - 1);

			g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
			debug("prop.c: Move up `%s' (now at pos: %d)", dapp->name, pos - 1);
		}
	}
}


static void wmdock_properties_movedown (GtkWidget *gtkBtnMoveDown, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;

	pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));

	if(g_list_length(wmdock->dapps) > 1 && pos < g_list_length(wmdock->dapps) - 1) {
		dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, pos));

		if(dapp) {
			wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
			wmdock->dapps = g_list_insert(wmdock->dapps, dapp, pos + 1);
			gtk_combo_box_remove_text(GTK_COMBO_BOX(gtkComboBox), pos);
			gtk_combo_box_insert_text(GTK_COMBO_BOX(gtkComboBox), pos + 1, dapp->name);
			gtk_combo_box_set_active(GTK_COMBO_BOX(gtkComboBox), pos + 1);
			gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile), pos + 1);

			g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
			debug("prop.c: Move down `%s' (now at pos: %d)", dapp->name, pos + 1);
		}
	}
}


static void wmdock_properties_savecmd (GtkWidget *gtkTxtCmd, GdkEventKey *event, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;
	gchar *_cmd = NULL;

	pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));

	dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, pos));
	if(dapp) {
		if((_cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkTxtCmd))))) {
			g_free(dapp->cmd);
			dapp->cmd = _cmd;
		}
	}
}


static void wmdock_properties_dialog_response (GtkWidget  *gtkDlg, gint response)
{
	DockappNode *dapp = NULL;
	gint pos;

	if(!gtkDlg) return;

	switch(response) {
	case GTK_RESPONSE_NO: /* Remove dockapp */
		if(g_list_length(wmdock->dapps) > 0 && prop.cbx) {
			pos = gtk_combo_box_get_active(GTK_COMBO_BOX(prop.cbx));

			dapp = DOCKAPP(g_list_nth_data(wmdock->dapps, pos));
			if(dapp)
				wmdock_destroy_dockapp(dapp);
		}
		break;

	default:
		/* Backup the value of the dockapp filter. */
		if(wmdock->propDispAddOnlyWM) {
			if(wmdock->filterList) g_free(wmdock->filterList);
			wmdock->filterList = g_strdup(gtk_entry_get_text(GTK_ENTRY(prop.txtPatterns)));
		}

		xfce_panel_plugin_unblock_menu (wmdock->plugin);
		gtk_widget_destroy (gtkDlg);

		debug("props.c: properties dlg closed");

		prop.dlg = prop.cbx = prop.txtCmd = prop.image = NULL;
		break;
	}
}


static void wmdock_properties_dialog_called_from_widget(GtkWidget *widget, XfcePanelPlugin *plugin)
{
	wmdock_properties_dialog(plugin);
}


void wmdock_panel_draw_properties_button ()
{
	if(!btnProperties && wmdock->propDispPropButton == TRUE) {
		btnProperties = xfce_create_panel_button();
		gtk_box_pack_start(GTK_BOX(wmdock->panelBox),
				btnProperties, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(wmdock->panelBox), btnProperties, 0);

		g_signal_connect (G_OBJECT(btnProperties), "pressed",
				G_CALLBACK (wmdock_properties_dialog_called_from_widget),
				wmdock->plugin);

		gtk_widget_show(GTK_WIDGET(btnProperties));
	}
}


void wmdock_error_dialog_response (GtkWidget  *gtkDlg, gint response)
{
	gtk_widget_destroy (gtkDlg);
}


void wmdock_refresh_properties_dialog()
{
	gint pos = 0;

	if(!prop.dlg || !prop.cbx || !prop.txtCmd) return;

	/* Cleanup the old list */
	while((pos = gtk_combo_box_get_active (GTK_COMBO_BOX(prop.cbx)))
			!= -1) {
		gtk_combo_box_remove_text(GTK_COMBO_BOX(prop.cbx), pos);
		gtk_combo_box_set_active (GTK_COMBO_BOX(prop.cbx), 0);
	}

	gtk_combo_box_popdown (GTK_COMBO_BOX(prop.cbx));
	if(g_list_length(wmdock->dapps) > 0) {
		gtk_widget_set_sensitive (prop.txtCmd, TRUE);

		g_list_foreach(wmdock->dapps, (GFunc) wmdock_properties_fillcmbx, prop.cbx);


	} else {
		gtk_combo_box_append_text (GTK_COMBO_BOX(prop.cbx),
				_("No dockapp is running!"));

		gtk_widget_set_state(prop.txtCmd, GTK_STATE_INSENSITIVE);
		gtk_entry_set_text(GTK_ENTRY(prop.txtCmd), "");

		gdkPbIcon = gdk_pixbuf_new_from_xpm_data((const char**)
				xfce4_wmdock_plugin_xpm);

		gtk_image_set_from_pixbuf (GTK_IMAGE(prop.image), gdkPbIcon);

		g_object_unref (G_OBJECT (gdkPbIcon));
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(prop.cbx), 0);

	gtk_widget_show (prop.image);
	gtk_widget_show (prop.cbx);
	gtk_widget_show (prop.txtCmd);
}


void wmdock_properties_dialog(XfcePanelPlugin *plugin)
{
	if(prop.dlg) return; /* Return if properties dialog is already open. */

	memset(&prop, 0, sizeof(prop));
	xfce_panel_plugin_block_menu (plugin);

	/* Create the configure dialog. */
	prop.dlg = xfce_titled_dialog_new_with_buttons (_("WMdock"),
			GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
			GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
			_("Remove dockapp"), GTK_RESPONSE_NO,
			GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
			NULL);

	gtk_window_set_position   (GTK_WINDOW (prop.dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name  (GTK_WINDOW (prop.dlg), "xfce4-settings");

	g_signal_connect (prop.dlg, "response",
			G_CALLBACK (wmdock_properties_dialog_response),
			NULL);

	/* Create the layout containers. */
	prop.hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (prop.hbox), 4);

	prop.frmGeneral = gtk_frame_new(_("General settings"));
	prop.frmDetect = gtk_frame_new(_("Dockapp detection"));
	prop.frmPanelOffOpts = gtk_frame_new(_("Mode settings"));
	prop.vboxGeneral = gtk_vbox_new(FALSE, 6);
	prop.vboxDetect = gtk_vbox_new(FALSE, 6);
	prop.vboxPanelOffOpts = gtk_vbox_new(FALSE, 6);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.frmGeneral, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.frmDetect, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.frmPanelOffOpts, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (prop.vboxGeneral), 4);
	gtk_container_set_border_width (GTK_CONTAINER (prop.vboxDetect), 4);
	gtk_container_set_border_width (GTK_CONTAINER (prop.vboxPanelOffOpts), 4);

	prop.vbox = gtk_vbox_new(FALSE, 4);
	prop.vbox2 = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (prop.vbox), 2);
	gtk_container_set_border_width (GTK_CONTAINER (prop.vbox2), 2);
	gtk_box_pack_start (GTK_BOX (prop.hbox), prop.vbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (prop.hbox), prop.vbox2, FALSE, FALSE, 0);

	prop.imageContainer = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_widget_set_size_request(GTK_WIDGET(prop.imageContainer), DEFAULT_DOCKAPP_WIDTH, DEFAULT_DOCKAPP_HEIGHT);
	prop.container =  gtk_fixed_new();

	prop.hboxPanelOffOpts = gtk_hbox_new(FALSE, 4);

	/* Create the GTK widget objects. */
	gdkPbIcon = gdk_pixbuf_new_from_xpm_data((const char**) xfce4_wmdock_plugin_xpm);

	prop.imageTile = gtk_image_new_from_pixbuf(gdkPbTileDefault);
	prop.image = gtk_image_new_from_pixbuf (gdkPbIcon);

	g_object_unref (G_OBJECT (gdkPbIcon));

	gtk_container_add(GTK_CONTAINER(prop.imageContainer), prop.image);
	gtk_container_add(GTK_CONTAINER(prop.container), prop.imageTile);
	gtk_container_add(GTK_CONTAINER(prop.container), prop.imageContainer);

	gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.container), FALSE, FALSE, 0);

	prop.btnMoveUp = xfce_arrow_button_new (GTK_ARROW_UP);
	prop.btnMoveDown = xfce_arrow_button_new (GTK_ARROW_DOWN);

	if(!IS_PANELOFF(wmdock)) {
		gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.btnMoveUp), FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.btnMoveDown), FALSE, FALSE, 0);
	}

	prop.lblSel = gtk_label_new (_("Select dockapp to configure:"));
	gtk_misc_set_alignment (GTK_MISC (prop.lblSel), 0, 0);
	gtk_box_pack_start (GTK_BOX(prop.vbox2), prop.lblSel, FALSE, FALSE, 0);

	/* Create the dockapp chooser combobox */
	prop.cbx = gtk_combo_box_new_text();

	gtk_box_pack_start (GTK_BOX (prop.vbox2), prop.cbx, FALSE, TRUE, 0);

	prop.lblCmd = gtk_label_new (_("Shell command:"));
	gtk_misc_set_alignment (GTK_MISC (prop.lblCmd), 0, 0);
	gtk_box_pack_start (GTK_BOX(prop.vbox2), prop.lblCmd, FALSE, FALSE, 0);
	prop.txtCmd = gtk_entry_new();
	if(g_list_length(wmdock->dapps) > 0) {
		gtk_editable_set_editable(GTK_EDITABLE(prop.txtCmd), TRUE);
	} else {
		gtk_editable_set_editable(GTK_EDITABLE(prop.txtCmd), FALSE);
	}
	gtk_box_pack_start (GTK_BOX(prop.vbox2), prop.txtCmd, FALSE, FALSE, 0);

	prop.chkDispTile                = gtk_check_button_new_with_label(_("Display tile in the background."));
	prop.chkPropButton              = gtk_check_button_new_with_label(_("Display a separate WMdock properties button in the panel."));
	prop.chkAddOnlyWM               = gtk_check_button_new_with_label(_("Add only dockapps which start with pattern in list. (e.g.: ^wm;^as)"));
	prop.chkPanelOff                = gtk_check_button_new_with_label(_("Display dockapps in separate windows and not in the panel."));
	prop.chkPanelOffIgnoreOffset    = gtk_check_button_new_with_label(_("Don't use panel size as offset for the first dockapp."));
	prop.chkPanelOffKeepAbove       = gtk_check_button_new_with_label(_("Keep dockapp windows on top."));
	prop.chkPanelOffFreePositioning = gtk_check_button_new_with_label(_("Enable free positioning of the first dockapp on the screen."));
	prop.txtPatterns    = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(prop.txtPatterns), wmdock->filterList);
	gtk_widget_set_sensitive (GTK_WIDGET (prop.txtPatterns), wmdock->propDispAddOnlyWM);

	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkDispTile, wmdock->propDispTile);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPropButton, wmdock->propDispPropButton);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkAddOnlyWM, wmdock->propDispAddOnlyWM);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPanelOff, rcPanelOff);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPanelOffIgnoreOffset, wmdock->propPanelOffIgnoreOffset);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPanelOffKeepAbove, wmdock->propPanelOffKeepAbove);
	gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPanelOffFreePositioning, wmdock->propPanelOffFreePositioning);

	gtk_container_add(GTK_CONTAINER(prop.frmGeneral), prop.vboxGeneral);
	gtk_container_add(GTK_CONTAINER(prop.frmDetect), prop.vboxDetect);
	gtk_container_add(GTK_CONTAINER(prop.frmPanelOffOpts), prop.vboxPanelOffOpts);

	gtk_box_pack_start (GTK_BOX(prop.vboxGeneral), prop.frmPanelOffOpts, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(prop.vboxGeneral), prop.chkDispTile, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(prop.vboxGeneral), prop.chkPropButton, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(prop.vboxDetect), prop.chkAddOnlyWM, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(prop.vboxDetect), prop.txtPatterns, FALSE, FALSE, 0);

	/* Setup panel off mode options frame. */
	gtk_box_pack_start (GTK_BOX(prop.vboxPanelOffOpts), prop.chkPanelOff, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (prop.vboxPanelOffOpts), prop.chkPanelOffKeepAbove, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (prop.vboxPanelOffOpts), prop.chkPanelOffIgnoreOffset, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (prop.vboxPanelOffOpts), prop.chkPanelOffFreePositioning, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(prop.vboxPanelOffOpts), prop.hboxPanelOffOpts, FALSE, FALSE, 0);
	prop.lblPanelOffHint = gtk_label_new (_("Hint: Is the first dockapp covered by a XFCE panel, please try to move the plugin\nto this panel to correct this problem."));
	gtk_box_pack_start (GTK_BOX(prop.vboxPanelOffOpts), prop.lblPanelOffHint, FALSE, FALSE, 0);

	prop.tblPanelOff = gtk_table_new(2, 2, TRUE);
	prop.lblPanelOffPlacement = gtk_label_new (_("Placement:"));
	gtk_misc_set_alignment (GTK_MISC (prop.lblPanelOffPlacement), 0, 0);
	prop.radioPanelOffTL = gtk_radio_button_new_with_label(NULL, _("Top left"));
	prop.radioPanelOffTR = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(prop.radioPanelOffTL), _("Top right"));
	prop.radioPanelOffBL = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(prop.radioPanelOffTL), _("Bottom left"));
	prop.radioPanelOffBR = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(prop.radioPanelOffTL), _("Bottom right"));

	gtk_box_pack_start (GTK_BOX (prop.hboxPanelOffOpts), prop.lblPanelOffPlacement, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (prop.hboxPanelOffOpts), prop.tblPanelOff, FALSE, FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(prop.tblPanelOff), prop.radioPanelOffTL, 0, 1, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(prop.tblPanelOff), prop.radioPanelOffTR, 1, 2, 0, 1);
	gtk_table_attach_defaults(GTK_TABLE(prop.tblPanelOff), prop.radioPanelOffBL, 0, 1, 1, 2);
	gtk_table_attach_defaults(GTK_TABLE(prop.tblPanelOff), prop.radioPanelOffBR, 1, 2, 1, 2);

	switch(wmdock->anchorPos) {
	case ANCHOR_TL:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prop.radioPanelOffTL), TRUE);
		break;
	case ANCHOR_TR:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prop.radioPanelOffTR), TRUE);
		break;
	case ANCHOR_BL:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prop.radioPanelOffBL), TRUE);
		break;
	default:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prop.radioPanelOffBR), TRUE);
		break;
	}

	if ( rcPanelOff == FALSE ) {
		/* Disable advanced panel options is the panel used. */
		wmdock_set_prop_paneloff(FALSE);
	} else {
		wmdock_set_prop_paneloff(TRUE);
	}

	/* Fill the dockapp chooser with entries. */
	wmdock_refresh_properties_dialog();

	/* Connect some signals to the dialog widgets */
	g_signal_connect(G_OBJECT(prop.cbx), "changed", G_CALLBACK(wmdock_properties_changed), prop.txtCmd);
	g_signal_connect(G_OBJECT(prop.txtCmd), "key-release-event", G_CALLBACK(wmdock_properties_savecmd), prop.cbx);
	g_signal_connect(G_OBJECT(prop.btnMoveUp), "pressed", G_CALLBACK(wmdock_properties_moveup), prop.cbx);
	g_signal_connect(G_OBJECT(prop.btnMoveDown), "pressed", G_CALLBACK(wmdock_properties_movedown), prop.cbx);
	g_signal_connect(G_OBJECT(prop.chkDispTile), "toggled", G_CALLBACK(wmdock_properties_chkdisptile), NULL);
	g_signal_connect(G_OBJECT(prop.chkPanelOff), "toggled", G_CALLBACK(wmdock_properties_chkpaneloff), NULL);
	g_signal_connect(G_OBJECT(prop.chkPanelOffIgnoreOffset), "toggled", G_CALLBACK(wmdock_properties_chkpaneloffignoreoffset), NULL);
	g_signal_connect(G_OBJECT(prop.chkPanelOffKeepAbove), "toggled", G_CALLBACK(wmdock_properties_chkpaneloffkeepabove), NULL);
	g_signal_connect(G_OBJECT(prop.chkPanelOffFreePositioning), "toggled", G_CALLBACK(wmdock_properties_chkpanelofffreepositioning), NULL);
	g_signal_connect(G_OBJECT(prop.radioPanelOffTL), "toggled", G_CALLBACK(wmdock_properties_radiopaneloff), NULL);
	g_signal_connect(G_OBJECT(prop.radioPanelOffTR), "toggled", G_CALLBACK(wmdock_properties_radiopaneloff), NULL);
	g_signal_connect(G_OBJECT(prop.radioPanelOffBL), "toggled", G_CALLBACK(wmdock_properties_radiopaneloff), NULL);
	g_signal_connect(G_OBJECT(prop.radioPanelOffBR), "toggled", G_CALLBACK(wmdock_properties_radiopaneloff), NULL);
	g_signal_connect(G_OBJECT(prop.chkPropButton), "toggled", G_CALLBACK(wmdock_properties_chkpropbtn), NULL);
	g_signal_connect(G_OBJECT(prop.chkAddOnlyWM), "toggled", G_CALLBACK(wmdock_properties_chkaddonlywm), NULL);

	g_timeout_add (500, wmdock_properties_refresh_dapp_icon, NULL);

	if(g_list_length(wmdock->dapps) > 0)
		wmdock_properties_changed(prop.cbx, prop.txtCmd);

	gtk_widget_show_all (prop.dlg);
}
