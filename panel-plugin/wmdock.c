/* $Id: wmdock.c,v 1.1 2007/07/29 18:35:00 ellguth Exp ellguth $
 *
 * wmdock xfce4 plugin by Andre Ellguth
 *
 * $Log: wmdock.c,v $
 * Revision 1.1  2007/07/29 18:35:00  ellguth
 * Initial revision
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xatom.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include "wmdock.h"
#include "xfce4-wmdock-plugin.xpm"

#define DEFAULT_DOCKAPP_WIDTH 64
#define DEFAULT_DOCKAPP_HEIGHT 64
#define FONT_WIDTH 4

Atom           DockAppAtom;
GtkWidget      *wmdockIcon = NULL;
GdkBitmap  	   *gdkBitmap;
GtkStyle   	   *gtkStyle;
GdkPixmap  	   *gdkPixmap;
WmdockPlugin   *wmdock;
/* fp needed for debug */
FILE           *fp;


gboolean has_dockapp_hint(WnckWindow * w)
{
	Atom atype;
	int afmt;
	unsigned long int nitems;
	unsigned long int naft;
	gboolean ret=FALSE;
	unsigned char * dat;
	gdk_error_trap_push();
	if (XGetWindowProperty(
			GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
			wnck_window_get_xid(w),DockAppAtom,0,1,False,
			XA_CARDINAL,&atype,&afmt,&nitems,&naft,&dat)==Success) {
		if (nitems==1 && ((long int *)dat)[0]==1) {
			ret=TRUE;
		}
		XFree(dat);
	}
	XSync(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),False);
	gdk_error_trap_pop();
	return ret;
}


void print_xids(DockappNode *dapp)
{
	/* Debug 	
	fprintf(fp, "Dockapp active with xid: %u\n", dapp->i);
	*/
}


void wmdock_destroy_dockapp(DockappNode *dapp)
{
	XDestroyWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), dapp->i);
}


void wmdock_dapp_closed(GtkSocket *socket, DockappNode *dapp)
{
	/* Debug 	
 	fprintf(fp, "- wmdock: closed window signal ! (xid: %u)\n", dapp->i);
	*/

	wmdock->dapps = g_slist_remove_all(wmdock->dapps, dapp); 
	gtk_widget_destroy(GTK_WIDGET(dapp->s));

	g_free(dapp);

	if(g_slist_length (wmdock->dapps) == 0) {
		wmdockIcon = gtk_image_new_from_pixmap(gdkPixmap, gdkBitmap);
		
 		gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(wmdockIcon), FALSE, FALSE, 0);
 		gtk_widget_show(GTK_WIDGET(wmdockIcon)); 	
 	}
}


static void wmdock_window_open(WnckScreen * s, WnckWindow * w) 
{
	int xp, yp, wi, he;
	XWMHints *h;
	DockappNode *dapp = NULL;
	int argc = 0;
	char **argv;

	gdk_error_trap_push();
	h = XGetWMHints(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
					wnck_window_get_xid(w));

	if((h && h->initial_state==WithdrawnState) || has_dockapp_hint(w)) {
		/* Debug	
		fprintf(fp, "- wmdock: new wmapp open\n");
		*/	

		wnck_window_get_geometry(w,&xp,&yp,&wi,&he);
  		/* Debug
  		fprintf(fp, "New dockapp %s with xid: %u pid: %u arrived\n", wnck_window_get_name(w), wnck_window_get_xid(w), wnck_window_get_pid(w));
  		*/	
 
		dapp = g_new0(DockappNode, 1);

		/* gtksocket = GTK_SOCKET(gtk_socket_new()); */
		dapp->s = GTK_SOCKET(gtk_socket_new());
		/* i = wnck_window_get_xid(w); */
		dapp->i = wnck_window_get_xid(w);
		gtk_widget_set_size_request(GTK_WIDGET(dapp->s), wi, he);

		dapp->name = g_strdup(wnck_window_get_name(w));

		XGetCommand(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), dapp->i, &argv, &argc);
		if(argc > 0) {
			argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
			argv[argc] = NULL;
			dapp->cmd = g_strjoinv (" ", argv);
		} else {
			dapp->cmd = g_strdup(dapp->name);
		}

		if(wmdockIcon) {
			gtk_widget_destroy(wmdockIcon);
			wmdockIcon = NULL;
		}
		gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), FALSE, FALSE, 0);

		gtk_socket_add_id(dapp->s, dapp->i);

		gtk_widget_show(GTK_WIDGET(dapp->s));

		g_signal_connect(dapp->s, "plug-removed", G_CALLBACK(wmdock_dapp_closed), dapp);  
		wmdock->dapps=g_slist_append(wmdock->dapps, dapp);

		/* Test */ 
		g_slist_foreach(wmdock->dapps, (GFunc)print_xids, NULL);
	}

	XFree(h);
}


static void wmdock_orientation_changed (XfcePanelPlugin *plugin,
					GtkOrientation   orientation,
					WmdockPlugin *wmdock)
{
	if (orientation == GTK_ORIENTATION_VERTICAL)
		gtk_alignment_set (GTK_ALIGNMENT (wmdock->align), 0.5, 0.5, 0.0, 1.0);
	else
		gtk_alignment_set (GTK_ALIGNMENT (wmdock->align), 0.5, 0.5, 1.0, 0.0);
}


static gboolean wmdock_size_changed (XfcePanelPlugin *plugin, int size)
{
	if (xfce_panel_plugin_get_orientation (plugin) ==
		GTK_ORIENTATION_HORIZONTAL)  {
		gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
	} else {
		gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);
	}

	return TRUE;
}


gboolean wmdock_startup_dockapp(gchar *cmd)
{
	gboolean r;
	GError **err = NULL;
	
	r = g_spawn_command_line_async(cmd, err);

	/* Errors will be evaluate in a later version. */
	if(err) g_clear_error (err);
	
	return(r);
}


static void wmdock_error_dialog_response (GtkWidget  *gtkDlg, gint response)
{
	gtk_widget_destroy (gtkDlg);
}


static void wmdock_read_rc_file (XfcePanelPlugin *plugin)
{
	gchar  	 *file;
	XfceRc 	 *rc;
	gint   	 i;
	gchar  	 **cmds = NULL;
	gint        cmdcnt;
	GtkWidget   *gtkDlg;

	if (!(file = xfce_panel_plugin_lookup_rc_file (plugin))) return;

	rc = xfce_rc_simple_open (file, TRUE);
	g_free(file);
 
	if(!rc) return;

	cmds = xfce_rc_read_list_entry(rc, "cmds", ";");
	cmdcnt = xfce_rc_read_int_entry(rc, "cmdcnt", 0);

	if(G_LIKELY(cmds != NULL)) {
		for (i = cmdcnt; i >= 0; i--) {
			/* Debug
 			fprintf(fp, "config will start: %s\n", cmds[i]);
 			*/	
			if(!cmds[i]) continue;
			if(wmdock_startup_dockapp(cmds[i]) != TRUE) {
				gtkDlg = gtk_message_dialog_new(GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))), 
 			 								 	GTK_DIALOG_DESTROY_WITH_PARENT,
 			 								 	GTK_MESSAGE_ERROR,
 			 								 	GTK_BUTTONS_OK,
 			 								 	"Failed to start %s!",
 			 								 	cmds[i]);
 				g_signal_connect (gtkDlg, "response", G_CALLBACK (wmdock_error_dialog_response), NULL);						 
 				gtk_widget_show_all (gtkDlg);
			}
			/* Sleep for n microseconds to startup dockapps in the right order. */ 
			g_usleep(150000);

			g_free(cmds[i]);
		}

		g_free(cmds);
	}
 
	xfce_rc_close (rc);
}


static void wmdock_write_rc_file (XfcePanelPlugin *plugin)
{
	gchar       *file;
	XfceRc      *rc;
	gchar       **cmds;
	DockappNode *dapp = NULL;
	gint   	    i;
 
	if (!(file = xfce_panel_plugin_save_location (plugin, TRUE))) return;

	rc = xfce_rc_simple_open (file, FALSE);
	g_free (file);

	if (!rc) return;

	if(g_slist_length (wmdock->dapps) > 0) {
		cmds = g_malloc(sizeof (gchar *) * (g_slist_length (wmdock->dapps) + 1));

		for(i = 0; i < g_slist_length(wmdock->dapps); i++) {
			dapp = g_slist_nth_data(wmdock->dapps, i);
			if(dapp) {
				cmds[i] = g_strdup(dapp->cmd);
			}
		}
		/* Workaround for a xfce bug in xfce_rc_read_list_entry */
		cmds[i] = NULL;

		xfce_rc_write_list_entry(rc, "cmds", cmds, ";");

		g_strfreev(cmds);

		xfce_rc_write_int_entry (rc, "cmdcnt", g_slist_length (wmdock->dapps));
	}

	xfce_rc_close(rc);
}


static void wmdock_free_data(XfcePanelPlugin *plugin)
{
	g_slist_foreach(wmdock->dapps, (GFunc)wmdock_destroy_dockapp, NULL);
	gtk_widget_destroy(GTK_WIDGET(wmdock->box));	
	g_slist_free(wmdock->dapps);
	g_free(wmdock);
	/* Debug
 	fclose(fp);
 	*/	
}


void wmdock_fill_cmbx(DockappNode *dapp, GtkWidget *gtkComboBox)
{

	if(gtkComboBox) {
 		/* Debug
		fprintf(fp, "%s append to list\n", dapp->name);
		*/
		gtk_combo_box_append_text ((GtkComboBox *) gtkComboBox, dapp->name); 
 	}
}


void wmdock_rebuild_fromlist ()
{
	if(g_slist_length(wmdock->dapps) > 0) {

	}
}


void wmdock_properties_moveup (GtkWidget *gtkBtnMoveUp, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;
 
	pos = gtk_combo_box_get_active((GtkComboBox *) gtkComboBox);

	if(g_slist_length(wmdock->dapps) > 1 && pos > 0) {
		dapp = (DockappNode *) g_slist_nth_data(wmdock->dapps, pos);
		
		if(dapp) {
			wmdock->dapps = g_slist_remove_all(wmdock->dapps, dapp);
			wmdock->dapps = g_slist_insert(wmdock->dapps, dapp, pos - 1);
			gtk_combo_box_remove_text((GtkComboBox *) gtkComboBox, pos);
			gtk_combo_box_insert_text((GtkComboBox *) gtkComboBox, pos - 1, dapp->name);
			gtk_combo_box_set_active((GtkComboBox *) gtkComboBox, pos - 1);  
			gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), pos - 1);
		}
	}
}


void wmdock_properties_movedown (GtkWidget *gtkBtnMoveDown, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;
	
	pos = gtk_combo_box_get_active((GtkComboBox *) gtkComboBox);
	
	if(g_slist_length(wmdock->dapps) > 1 && pos < g_slist_length(wmdock->dapps) - 1) {
		dapp = (DockappNode *) g_slist_nth_data(wmdock->dapps, pos);
		
		if(dapp) {
			wmdock->dapps = g_slist_remove_all(wmdock->dapps, dapp);
			wmdock->dapps = g_slist_insert(wmdock->dapps, dapp, pos + 1);
			gtk_combo_box_remove_text((GtkComboBox *) gtkComboBox, pos);
			gtk_combo_box_insert_text((GtkComboBox *) gtkComboBox, pos + 1, dapp->name);
			gtk_combo_box_set_active((GtkComboBox *) gtkComboBox, pos + 1);  
			gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), pos + 1);
		}		
	}
}


void wmdock_properties_savecmd (GtkWidget *gtkTxtCmd, GdkEventKey *event, GtkWidget *gtkComboBox)
{
	DockappNode *dapp = NULL;
	gint pos;

	pos = gtk_combo_box_get_active((GtkComboBox *) gtkComboBox);

	dapp = (DockappNode *) g_slist_nth_data(wmdock->dapps, pos);
	if(dapp) {
		g_free(dapp->cmd);
		dapp->cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkTxtCmd)));
	}
}


void wmdock_properties_changed (GtkWidget *gtkComboBox, GtkWidget *gtkTxtCmd)
{
	DockappNode *dapp = NULL;

	dapp = (DockappNode *) g_slist_nth_data(wmdock->dapps, gtk_combo_box_get_active((GtkComboBox *) gtkComboBox));
	if(dapp) {
 		/*
 		fprintf(fp, "changed, %s selected:\n", dapp->name);
 		*/	
	
		gtk_entry_set_text(GTK_ENTRY(gtkTxtCmd), dapp->cmd);
	} 
}


static void wmdock_properties_dialog_response (GtkWidget  *gtkDlg, gint response)
{
	xfce_panel_plugin_unblock_menu (wmdock->plugin);
	
	gtk_widget_destroy (gtkDlg);
}


static void wmdock_properties_dialog(XfcePanelPlugin *plugin)
{
	GtkWidget   *gtkDlg, *gtkVbox, *gtkVbox2, *gtkHbox, *gtkLblSel, *gtkLblCmd, 
 				*gtkTxtCmd, *gtkComboBox, *gtkImage, *gtkBtnMoveUp, *gtkBtnMoveDown;

	xfce_panel_plugin_block_menu (plugin);

	gtkDlg = xfce_titled_dialog_new_with_buttons (_("WMdock"),
				GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
				GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
				GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
				NULL);

	gtk_window_set_position   (GTK_WINDOW (gtkDlg), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name  (GTK_WINDOW (gtkDlg), "xfce4-settings");

	g_signal_connect (gtkDlg, "response", 
					G_CALLBACK (wmdock_properties_dialog_response),
					NULL);

	gtkHbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gtkDlg)->vbox), gtkHbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (gtkHbox), 4);

	gtkVbox = gtk_vbox_new(FALSE, 4);
	gtkVbox2 = gtk_vbox_new(FALSE, 4);
	gtk_container_set_border_width (GTK_CONTAINER (gtkVbox), 2);
	gtk_container_set_border_width (GTK_CONTAINER (gtkVbox2), 2);
	gtk_box_pack_start (GTK_BOX (gtkHbox), gtkVbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (gtkHbox), gtkVbox2, FALSE, FALSE, 0);

	gtkImage = gtk_image_new_from_pixmap(gdkPixmap, gdkBitmap);
	gtk_widget_set_name(gtkImage, "image");
	gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkImage), FALSE, FALSE, 0);

	gtkBtnMoveUp = gtk_button_new_with_label ("Move up"); 
	gtkBtnMoveDown = gtk_button_new_with_label ("Move down");
	gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkBtnMoveUp), FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkBtnMoveDown), FALSE, FALSE, 0);

	gtkLblSel = gtk_label_new ("Select dockapp to configure:");
	gtk_misc_set_alignment (GTK_MISC (gtkLblSel), 0, 0);
	gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkLblSel, FALSE, FALSE, 0);

	gtkComboBox = gtk_combo_box_new_text();

	if(g_slist_length(wmdock->dapps) > 0) {
		g_slist_foreach(wmdock->dapps, (GFunc) wmdock_fill_cmbx, gtkComboBox);
	} else {
 		/*
 		fprintf(fp, "draw icon in properties\n");
		*/	
		gtk_combo_box_append_text ((GtkComboBox *) gtkComboBox, "No dockapp are running!");
	}
	gtk_combo_box_set_active((GtkComboBox *) gtkComboBox, 0);

	gtk_box_pack_start (GTK_BOX (gtkVbox2), gtkComboBox, FALSE, TRUE, 0);

	gtkLblCmd = gtk_label_new ("Shell command:");
	gtk_misc_set_alignment (GTK_MISC (gtkLblCmd), 0, 0);
	gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkLblCmd, FALSE, FALSE, 0);
	gtkTxtCmd = gtk_entry_new();
	gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkTxtCmd, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(gtkComboBox), "changed", G_CALLBACK(wmdock_properties_changed), gtkTxtCmd);
	g_signal_connect(G_OBJECT(gtkTxtCmd), "key-release-event", G_CALLBACK(wmdock_properties_savecmd), gtkComboBox);
	g_signal_connect(G_OBJECT(gtkBtnMoveUp), "pressed", G_CALLBACK(wmdock_properties_moveup), gtkComboBox);
	g_signal_connect(G_OBJECT(gtkBtnMoveDown), "pressed", G_CALLBACK(wmdock_properties_movedown), gtkComboBox);

	if(g_slist_length(wmdock->dapps) > 0) {
		wmdock_properties_changed(gtkComboBox, gtkTxtCmd);	
	}

	gtk_widget_show_all (gtkDlg);
}


static WmdockPlugin *wmdock_plugin_new (XfcePanelPlugin* plugin)
{
	wmdock = g_new0(WmdockPlugin, 1);

	wmdock->plugin = plugin;
	wmdock->dapps = NULL;

	wmdock->ebox = gtk_event_box_new ();
	gtk_widget_show(GTK_WIDGET(wmdock->ebox));

	wmdock->align = gtk_alignment_new (0.5, 0.5, 1.0, 1.0);

	gtk_widget_show(GTK_WIDGET(wmdock->align));

	gtk_container_add(GTK_CONTAINER(wmdock->ebox), GTK_WIDGET(wmdock->align));
	 
	if(xfce_panel_plugin_get_orientation (plugin) == GTK_ORIENTATION_VERTICAL) {
		wmdock->box = gtk_vbox_new(FALSE, 0);
	} else {
		wmdock->box = gtk_hbox_new(FALSE, 0);
	}

	gtk_container_add (GTK_CONTAINER (wmdock->align), wmdock->box);
 
	return wmdock;
}


static void wmdock_construct (XfcePanelPlugin *plugin)
{
	WnckScreen *s;

	/* Debug
 	fp = fopen("/tmp/xfce4-wmdock.log", "w");
	*/	
	s = wnck_screen_get(0);

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	DockAppAtom=XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
							"_XFCE4_DOCKAPP",False);

	wmdock = wmdock_plugin_new (plugin);
	 
	gtk_widget_show(GTK_WIDGET(wmdock->box));
	 
	g_signal_connect(s, "window_opened", G_CALLBACK(wmdock_window_open), NULL);
	g_signal_connect (plugin, "size-changed", G_CALLBACK (wmdock_size_changed), NULL);
	g_signal_connect (plugin, "orientation-changed", G_CALLBACK (wmdock_orientation_changed), NULL);
	g_signal_connect (plugin, "free-data", G_CALLBACK (wmdock_free_data), NULL);

	gtk_container_add (GTK_CONTAINER (plugin), wmdock->ebox);
	
	xfce_panel_plugin_add_action_widget (plugin, wmdock->ebox);
	xfce_panel_plugin_set_expand(plugin, TRUE);
	
	/* Setup the icon */
	gtkStyle = gtk_widget_get_default_style();
	gdkPixmap = gdk_pixmap_create_from_xpm_d (wmdock->ebox->window, &gdkBitmap,
    	                                       &gtkStyle->bg[GTK_STATE_NORMAL],
        	                                   xfce4_wmdock_plugin_xpm);

	wmdockIcon = gtk_image_new_from_pixmap(gdkPixmap, gdkBitmap);

	gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(wmdockIcon), FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(wmdockIcon));

	/* Configure plugin dialog */
	xfce_panel_plugin_menu_show_configure (plugin);
	g_signal_connect (plugin, "configure-plugin", G_CALLBACK (wmdock_properties_dialog), NULL);

	/* Read the config file and start the dockapps */
	wmdock_read_rc_file(plugin); 

	g_signal_connect (plugin, "save", G_CALLBACK (wmdock_write_rc_file), NULL);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (wmdock_construct);
