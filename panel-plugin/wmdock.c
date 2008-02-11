/* $Id: wmdock.c,v 1.7 2008/02/10 20:49:39 ellguth Exp ellguth $
 *
 * wmdock xfce4 plugin by Andre Ellguth
 *
 * $Log: wmdock.c,v $
 * Revision 1.7  2008/02/10 20:49:39  ellguth
 * Fallback command line detection of the dockapp via pid integrated.
 * Dockapp preview in the configure dialog.
 * Code cleanup.
 *
 * Revision 1.6  2008/02/06 20:44:42  ellguth
 * Improvment of the dockapp detection.
 * The dockapps will be now started cleanly ordered.
 *
 * Revision 1.5  2007/12/19 19:31:08  ellguth
 * The "Display tile in the background" switchs works now correctly.
 * Added missing xfce header files in wmdock.c for hvbox and xfce-arrow.
 * The wmdock startup icon will now be scaled.
 * Code cleanup.
 *
 * Revision 1.4  2007/09/03 18:36:52  ellguth
 * Removed the use of function wnck_window_has_name().
 * Replace function XKillClient back to XDestroyWindow.
 *
 * Revision 1.3  2007/09/01 15:40:24  ellguth
 * Plugin don't crash anymore if you change the orientation of the panel.
 * Workaround added for wmnet.
 * Background tile will be shown.
 * Dockapps now centered in panel.
 *
 * Revision 1.2  2007/08/29 19:39:46  ellguth
 * No X applications with size greater than  64 pixels will be swallowed.
 *
 * Revision 1.1  2007/07/29 18:35:00  ellguth
 * Initial revision
 *
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
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-window.h>
#include <libxfce4panel/xfce-hvbox.h>
#include <libxfce4panel/xfce-arrow-button.h>


#include "wmdock.h"
#include "xfce4-wmdock-plugin.xpm"
#include "tile.xpm"

#define DEFAULT_DOCKAPP_WIDTH 64
#define DEFAULT_DOCKAPP_HEIGHT 64
#define FONT_WIDTH 4
#define MAX_WAITCNT 10000
#define WAITCNT_TIMEOUT 1000000
#define BUF_MAX 4096

Atom         XfceDockAppAtom;
GtkWidget    *wmdockIcon = NULL;
GtkWidget    *propDialog = NULL;
GtkWidget    *propDockappChooser = NULL;
GtkWidget    *propDockappCommand = NULL;
GtkWidget    *propDockappImage = NULL;
GdkPixmap    *gdkPmTile = NULL;
GdkPixbuf    *gdkPbIcon = NULL;
WmdockPlugin *wmdock = NULL;
gchar        **rcCmds = NULL;
gint         rcCmdcnt = 0;


/* #define DEBUG */

#ifdef DEBUG
/* fp needed for debug */
FILE           *fp = (FILE *) NULL;
#endif

gboolean has_dockapp_hint(WnckWindow * w)
{
 Atom atype;
 int afmt, i;
 unsigned long int nitems;
 unsigned long int naft;
 gboolean r = FALSE;
 unsigned char *dat = NULL;
	
	
 gdk_error_trap_push();
 if (XGetWindowProperty(
			GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
			wnck_window_get_xid(w), XfceDockAppAtom, 0, 1, False,
			XA_CARDINAL, &atype, &afmt, &nitems, &naft, &dat) == Success) {
  if (nitems==1 && ((long int *) dat)[0]==1) {
   r = TRUE;
  }
  XFree(dat);
 }
 XSync(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),False);
   	
 gdk_error_trap_pop();

 return (r);
}


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


void wmdock_fill_cmbx(DockappNode *dapp, GtkWidget *gtkComboBox)
{

 if(gtkComboBox) {
#ifdef DEBUG
  fprintf(fp, "wmdock: %s append to list\n", dapp->name);
  fflush(fp);
#endif
  gtk_combo_box_append_text (GTK_COMBO_BOX(gtkComboBox), dapp->name); 
 }
}


void wmdock_refresh_properties_dialog()
{
 gint pos = 0;
 
 if(!propDialog || !propDockappChooser || !propDockappCommand) return;

 /* Cleanup the old list */
 while((pos = gtk_combo_box_get_active (GTK_COMBO_BOX(propDockappChooser))) 
       != -1) {
  gtk_combo_box_remove_text(GTK_COMBO_BOX(propDockappChooser), pos);
  gtk_combo_box_set_active (GTK_COMBO_BOX(propDockappChooser), 0);
 }

 gtk_combo_box_popdown (GTK_COMBO_BOX(propDockappChooser));
 if(g_list_length(wmdock->dapps) > 0) {
  gtk_widget_set_sensitive (propDockappCommand, TRUE);

  g_list_foreach(wmdock->dapps, (GFunc) wmdock_fill_cmbx, propDockappChooser);  

  
 } else {
  gtk_combo_box_append_text (GTK_COMBO_BOX(propDockappChooser), 
			     "No dockapp are running!");

  gtk_widget_set_state(propDockappCommand, GTK_STATE_INSENSITIVE);

  gdkPbIcon = gdk_pixbuf_new_from_xpm_data((const char**) 
					   xfce4_wmdock_plugin_xpm);

  gtk_image_set_from_pixbuf (GTK_IMAGE(propDockappImage), gdkPbIcon);
  
  g_object_unref (G_OBJECT (gdkPbIcon));
 }

 gtk_combo_box_set_active(GTK_COMBO_BOX(propDockappChooser), 0);

 gtk_widget_show (propDockappImage);
 gtk_widget_show (propDockappChooser);
 gtk_widget_show (propDockappCommand);
}


void wmdock_destroy_dockapp(DockappNode *dapp)
{
 XDestroyWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), dapp->i);
}


void wmdock_redraw_dockapp(DockappNode *dapp)
{
 gtk_widget_unmap (GTK_WIDGET(dapp->s));

 /* Tile in the background */
 if(wmdock->propDispTile == TRUE) {
  gtk_widget_set_app_paintable(GTK_WIDGET(dapp->s), TRUE);
  gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, gdkPmTile, FALSE);
  if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->s)) & GTK_MAPPED)
   gtk_widget_queue_draw(GTK_WIDGET(dapp->s));
 } else {
  gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, NULL, TRUE);
 }
	
#ifdef DEBUG
 fprintf(fp, "wmdock: Dockapp %s redrawed with tile %d\n", dapp->name, 
	 wmdock->propDispTile);
 fflush(fp);
#endif

 gtk_widget_map (GTK_WIDGET(dapp->s));
	
 gtk_widget_show(GTK_WIDGET(dapp->s));
}


DockappNode *wmdock_find_startup_dockapp(const gchar *compCmd)
{
 GList *dapps;
 DockappNode *dapp = NULL;

 dapps = wmdock->dapps;

 while(dapps) {
  dapp = (DockappNode *) dapps->data;
  
  if(dapp) {
   if(!dapp->name && dapp->cmd) {
    if(!g_strcasecmp(dapp->cmd, compCmd)) {
#ifdef DEBUG
     fprintf(fp, "found startup dockapp with cmd %s\n", compCmd);
     fflush(fp);
#endif     
     return(dapp);
    }
   }
  }

  dapps = g_list_next(dapps);
 }

 return(NULL);
}


void wmdock_dapp_closed(GtkSocket *socket, DockappNode *dapp)
{
#ifdef DEBUG 	
 fprintf(fp, "wmdock: closed window signal ! (window: %s)\n", dapp->name);
 fflush(fp);
#endif

 wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);

 gtk_widget_destroy(GTK_WIDGET(dapp->s));
 g_free(dapp->name);
 g_free(dapp->cmd);
 g_free(dapp);

 if(g_list_length (wmdock->dapps) == 0) {
  gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm, 
				       xfce_panel_plugin_get_size (wmdock->plugin) - 2,
				       xfce_panel_plugin_get_size (wmdock->plugin) - 2);
  if(wmdockIcon) gtk_widget_destroy(wmdockIcon);
  wmdockIcon = gtk_image_new_from_pixbuf (gdkPbIcon);
  g_object_unref (G_OBJECT (gdkPbIcon));
		
  gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(wmdockIcon), FALSE, FALSE, 0);
  gtk_widget_show(GTK_WIDGET(wmdockIcon)); 	
 }

 wmdock_refresh_properties_dialog();
}


gchar *wmdock_get_dockapp_cmd(WnckWindow *w)
{
 gchar *cmd = NULL;
 int wpid = 0;
 int argc = 0;
 int fcnt, i;
 char **argv;
 FILE *procfp = NULL;
 char buf[BUF_MAX];

 XGetCommand(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
	     wnck_window_get_xid(w), &argv, &argc);
 if(argc > 0) {
  argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
  argv[argc] = NULL;
  cmd = g_strjoinv (" ", argv);
  XFreeStringList(argv);
 } else {
  /* Try to get the command line from the proc fs. */
  wpid = wnck_window_get_pid (w);

  if(wpid) {
   sprintf(buf, "/proc/%d/cmdline", wpid);

   procfp = fopen(buf, "r");
   
   if(procfp) {
    fcnt = read(fileno(procfp), buf, BUF_MAX);

    cmd = g_malloc(fcnt+2);
    if(!cmd) return (NULL);

    for(i = 0; i < fcnt; i++) {
     if(buf[i] == 0)
      *(cmd+i) = ' ';
     else
      *(cmd+i) = buf[i];
    }
    *(cmd+(i-1)) = 0;

    fclose(procfp);
   }
  }
 }
 
 if(!cmd) {
  /* If nothing helps fallback to the window name. */
  cmd = g_strdup(wnck_window_get_name(w));
 }

 return(cmd);
}


static void wmdock_window_open(WnckScreen *s, WnckWindow *w) 
{
 int wi, he;
 XWMHints *h;
 XWindowAttributes attr;
 DockappNode *dapp = NULL;
 gint rcstartupPos = 0;
 guint listLen = 0;
 gchar *cmd = NULL;
 gboolean rcDapp = FALSE;

 gdk_error_trap_push();
 gdk_flush();

 h = XGetWMHints(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
		 wnck_window_get_xid(w));

 if(!h) return;
	
 if(h->initial_state == WithdrawnState || 
    h->flags == (WindowGroupHint | StateHint | IconWindowHint)
    || has_dockapp_hint(w)) {

#ifdef DEBUG	
  fprintf(fp, "wmdock: new wmapp open\n");
  fflush(fp);
#endif

  /* wnck_window_get_geometry(w,&xp,&yp,&wi,&he); */
#ifdef DEBUG
  fprintf(fp, "wmdock: New dockapp %s with xid:%u pid:%u arrived sessid:%s\n",
	  wnck_window_get_name(w), wnck_window_get_xid(w), 
	  wnck_window_get_pid(w), wnck_window_get_session_id(w));
  fflush(fp);
#endif

  cmd = wmdock_get_dockapp_cmd(w);

  if(!cmd) {
   XFree(h);
   return;
  }

#ifdef DEBUG
  fprintf(fp, "wmdock: found cmd %s for window %s\n",
	  cmd, wnck_window_get_name(w));
  fflush(fp);
#endif

  if(rcCmds && (dapp = wmdock_find_startup_dockapp(cmd)))
   rcDapp = TRUE;

  if(rcDapp == FALSE) {
#ifdef DEBUG
   fprintf(fp, "wmdock: Create a new dapp window %s\n",
	   wnck_window_get_name(w));
   fflush(fp);
#endif   
   dapp = g_new0(DockappNode, 1);
   dapp->s = GTK_SOCKET(gtk_socket_new());
  }

  if(h->initial_state == WithdrawnState && h->icon_window) {
   XUnmapWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
		wnck_window_get_xid(w));
   dapp->i =h->icon_window;
   
  } else {
   dapp->i = wnck_window_get_xid(w);
  }

  if(!XGetWindowAttributes(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
			   dapp->i, &attr)) {
   wi = DEFAULT_DOCKAPP_WIDTH;
   he = DEFAULT_DOCKAPP_HEIGHT;
  } else {
   wi = attr.width;
   he = attr.height;
  }

  if(wi > DEFAULT_DOCKAPP_WIDTH || he > DEFAULT_DOCKAPP_HEIGHT) {
   /* It seems to be no dockapp, because the width or the height of the 
    * window a greater than 64 pixels. */
   gtk_widget_destroy(GTK_WIDGET(dapp->s));
   g_free(cmd);
   g_free(dapp);
   XFree(h);
   return;
  }

#ifdef DEBUG
  fprintf(fp, "wmdock: New dockapp %s width: %d height: %d\n",
	  wnck_window_get_name(w), wi, he);
  fflush(fp);
#endif
  
  gtk_widget_set_size_request(GTK_WIDGET(dapp->s), wi, he);

  wnck_window_set_skip_tasklist (w, TRUE);
  wnck_window_set_skip_pager (w, TRUE);

  dapp->name = g_strdup(wnck_window_get_name(w));
  dapp->cmd = cmd;

  if(wmdockIcon) {
   gtk_widget_destroy(wmdockIcon);
   wmdockIcon = NULL;
  }

  if(rcDapp == FALSE)
   gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), FALSE, FALSE, 
		      0);

  gtk_socket_add_id(dapp->s, dapp->i);
		
  /* Tile in the background */
  if(wmdock->propDispTile == TRUE) {
   gtk_widget_set_app_paintable(GTK_WIDGET(dapp->s), TRUE);
   gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, gdkPmTile, FALSE);
   if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->s)) & GTK_MAPPED)
    gtk_widget_queue_draw(GTK_WIDGET(dapp->s));
  }
        			
  gtk_widget_show(GTK_WIDGET(dapp->s));

  g_signal_connect(dapp->s, "plug-removed", G_CALLBACK(wmdock_dapp_closed), 
		   dapp);

  if(rcDapp == FALSE)
   wmdock->dapps=g_list_append(wmdock->dapps, dapp);

  wmdock_refresh_properties_dialog();
 }

 XFree(h);
}


static void wmdock_orientation_changed (XfcePanelPlugin *plugin, GtkOrientation orientation, gpointer user_data)
{
 /*
   if (xfce_panel_plugin_get_orientation (plugin) == GTK_ORIENTATION_VERTICAL)
   gtk_alignment_set (GTK_ALIGNMENT (wmdock->box), 0.5, 0.5, 0.0, 1.0);
   else
   gtk_alignment_set (GTK_ALIGNMENT (wmdock->box), 0.5, 0.5, 1.0, 0.0);
 */
 xfce_hvbox_set_orientation ((XfceHVBox *) wmdock->box, orientation);
 gtk_widget_show(GTK_WIDGET(wmdock->box));	
}


static gboolean wmdock_size_changed (XfcePanelPlugin *plugin, int size)
{
 if (xfce_panel_plugin_get_orientation (plugin) ==
     GTK_ORIENTATION_HORIZONTAL)  {
  gtk_widget_set_size_request (GTK_WIDGET (plugin), -1, size);
 } else {
  gtk_widget_set_size_request (GTK_WIDGET (plugin), size, -1);
 }
	
 if(wmdockIcon) { 
  gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm, 
				       xfce_panel_plugin_get_size (plugin) - 2,
				       xfce_panel_plugin_get_size (plugin) - 2);
  gtk_image_set_from_pixbuf (GTK_IMAGE(wmdockIcon), gdkPbIcon);
  g_object_unref (G_OBJECT (gdkPbIcon));
  gtk_widget_show(GTK_WIDGET(wmdockIcon));
 }

 return TRUE;
}


gboolean wmdock_startup_dockapp(const gchar *cmd)
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
 gchar     *file = NULL;
 XfceRc    *rc = NULL;
 gint      i;
 GtkWidget *gtkDlg;
 DockappNode *dapp = NULL;
	
 if (!(file = xfce_panel_plugin_lookup_rc_file (plugin))) return;

 rc = xfce_rc_simple_open (file, TRUE);
 g_free(file);
 
 if(!rc) return;

 rcCmds = xfce_rc_read_list_entry(rc, "cmds", ";");
 rcCmdcnt = xfce_rc_read_int_entry(rc, "cmdcnt", 0);
 wmdock->propDispTile = xfce_rc_read_bool_entry (rc, "disptile", TRUE);

 if(G_LIKELY(rcCmds != NULL)) {
  for (i = 0; i <= rcCmdcnt; i++) {
			
#ifdef DEBUG
   fprintf(fp, "wmdock: config will start: %s\n", rcCmds[i]);
   fflush(fp);
#endif

   if(!rcCmds[i]) continue;
   if(wmdock_startup_dockapp(rcCmds[i]) != TRUE) {
    gtkDlg = gtk_message_dialog_new(GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))), 
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_OK,
				    "Failed to start %s!",
				    rcCmds[i]);
    g_signal_connect (gtkDlg, "response", G_CALLBACK (wmdock_error_dialog_response), NULL);						 
    gtk_widget_show_all (gtkDlg);
   } else {
    /* Create some dummy widget entries to locate the right position on
       window swallow up. */

    dapp = g_new0(DockappNode, 1);
    dapp->name = NULL;
    dapp->cmd = rcCmds[i];

    dapp->s = GTK_SOCKET(gtk_socket_new());

    gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), FALSE, FALSE, 
		       0);

    wmdock->dapps=g_list_append(wmdock->dapps, dapp);    
   }
			
   /* Sleep for n microseconds to startup dockapps in the right order. */
   /* g_usleep(250000); */
  }
 }
 
 xfce_rc_close (rc);
}


static void wmdock_write_rc_file (XfcePanelPlugin *plugin)
{
 gchar       *file;
 XfceRc      *rc;
 gchar       **cmds;
 DockappNode *dapp = NULL;
 gint        i;
 
 if (!(file = xfce_panel_plugin_save_location (plugin, TRUE))) return;

 rc = xfce_rc_simple_open (file, FALSE);
 g_free (file);

 if (!rc) return;

 if(g_list_length (wmdock->dapps) > 0) {
  cmds = g_malloc(sizeof (gchar *) * (g_list_length (wmdock->dapps) + 1));

  for(i = 0; i < g_list_length(wmdock->dapps); i++) {
   dapp = g_list_nth_data(wmdock->dapps, i);
   if(dapp) {
    if(dapp->name && dapp->cmd)
     cmds[i] = g_strdup(dapp->cmd);
   }
  }
  /* Workaround for a xfce bug in xfce_rc_read_list_entry */
  cmds[i] = NULL;

  xfce_rc_write_list_entry(rc, "cmds", cmds, ";");

  g_strfreev(cmds);

  xfce_rc_write_int_entry (rc, "cmdcnt", g_list_length (wmdock->dapps));
  xfce_rc_write_bool_entry (rc, "disptile", wmdock->propDispTile); 
 }

 xfce_rc_close(rc);
}


static void wmdock_free_data(XfcePanelPlugin *plugin)
{
 g_list_foreach(wmdock->dapps, (GFunc)wmdock_destroy_dockapp, NULL);

 gtk_widget_destroy(GTK_WIDGET(wmdockIcon));
 gtk_widget_destroy(GTK_WIDGET(wmdock->box));
 gtk_widget_destroy(GTK_WIDGET(wmdock->align));
 gtk_widget_destroy(GTK_WIDGET(wmdock->ebox));
 g_list_free(wmdock->dapps);
 g_free(wmdock);
	
#ifdef DEBUG
 fclose(fp);
#endif
}


void wmdock_properties_chkdisptile(GtkToggleButton *gtkChkDispTile, gpointer user_data)
{
 wmdock->propDispTile = gtk_toggle_button_get_active(gtkChkDispTile);

 g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
 gtk_widget_show_all(GTK_WIDGET(wmdock->box));
}


void wmdock_properties_moveup (GtkWidget *gtkBtnMoveUp, GtkWidget *gtkComboBox)
{
 DockappNode *dapp = NULL;
 gint pos;
 
 pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));

 if(g_list_length(wmdock->dapps) > 1 && pos > 0) {
  dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, pos);
		
  if(dapp) {
   wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
   wmdock->dapps = g_list_insert(wmdock->dapps, dapp, pos - 1);
   gtk_combo_box_remove_text(GTK_COMBO_BOX(gtkComboBox), pos);
   gtk_combo_box_insert_text(GTK_COMBO_BOX(gtkComboBox), pos - 1, dapp->name);
   gtk_combo_box_set_active(GTK_COMBO_BOX(gtkComboBox), pos - 1);  
   gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), pos - 1);
			
   g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
  }
 }
}


void wmdock_properties_movedown (GtkWidget *gtkBtnMoveDown, GtkWidget *gtkComboBox)
{
 DockappNode *dapp = NULL;
 gint pos;
	
 pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));
	
 if(g_list_length(wmdock->dapps) > 1 && pos < g_list_length(wmdock->dapps) - 1) {
  dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, pos);
		
  if(dapp) {
   wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
   wmdock->dapps = g_list_insert(wmdock->dapps, dapp, pos + 1);
   gtk_combo_box_remove_text(GTK_COMBO_BOX(gtkComboBox), pos);
   gtk_combo_box_insert_text(GTK_COMBO_BOX(gtkComboBox), pos + 1, dapp->name);
   gtk_combo_box_set_active(GTK_COMBO_BOX(gtkComboBox), pos + 1);  
   gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->s), pos + 1);
			
   g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
  }		
 }
}


void wmdock_properties_savecmd (GtkWidget *gtkTxtCmd, GdkEventKey *event, GtkWidget *gtkComboBox)
{
 DockappNode *dapp = NULL;
 gint pos;

 pos = gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox));

 dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, pos);
 if(dapp) {
  g_free(dapp->cmd);
  dapp->cmd = g_strdup(gtk_entry_get_text(GTK_ENTRY(gtkTxtCmd)));
 }
}


void wmdock_properties_changed (GtkWidget *gtkComboBox, GtkWidget *gtkTxtCmd)
{
 DockappNode *dapp = NULL;
 GdkPixmap *pm = NULL;

 dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox)));
 if(dapp) {
#ifdef DEBUG
  fprintf(fp, "wmdock: changed, %s selected:\n", dapp->name);
  fflush(fp);
#endif
	
  gtk_entry_set_text(GTK_ENTRY(gtkTxtCmd), dapp->cmd);

  if(propDockappImage) {
  
   pm = gdk_pixmap_foreign_new (dapp->i);

   if(pm) {
    gtk_image_set_from_pixmap (GTK_IMAGE(propDockappImage), pm, NULL);

    gtk_widget_show(propDockappImage);

    g_object_unref (G_OBJECT(pm));
   }

   else {
    gtk_image_set_from_pixmap (GTK_IMAGE(propDockappImage), gdkPmTile, NULL);

    gtk_widget_show(propDockappImage);
   }
  }
 }
}


static void wmdock_properties_dialog_response (GtkWidget  *gtkDlg, gint response)
{
 if(!gtkDlg) return;

 xfce_panel_plugin_unblock_menu (wmdock->plugin);
 gtk_widget_destroy (gtkDlg);

#ifdef DEBUG
 fprintf(fp, "wmdock: properties dlg closed\n");
 fflush(fp);
#endif

 propDialog = propDockappChooser = propDockappCommand = propDockappImage = NULL;
}


static void wmdock_properties_dialog(XfcePanelPlugin *plugin)
{
 GtkWidget   *gtkDlg, *gtkVbox, *gtkVbox2, *gtkVboxGeneral, *gtkHbox, 
  *gtkLblSel, *gtkLblCmd, *gtkLblGeneral, *gtkChkDispTile, *gtkImageContainer,
  *gtkImageTile, *gtkTxtCmd, *gtkComboBox, *gtkImage, *gtkBtnMoveUp, 
  *gtkBtnMoveDown;

 xfce_panel_plugin_block_menu (plugin);

 /* Create the configure dialog. */
 propDialog = gtkDlg = xfce_titled_dialog_new_with_buttons (_("WMdock"),
							    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
							    GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
							    GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
							    NULL);

 gtk_window_set_position   (GTK_WINDOW (gtkDlg), GTK_WIN_POS_CENTER);
 gtk_window_set_icon_name  (GTK_WINDOW (gtkDlg), "xfce4-settings");

 g_signal_connect (gtkDlg, "response", 
		   G_CALLBACK (wmdock_properties_dialog_response),
		   NULL);

 /* Create the layout containers. */
 gtkHbox = gtk_hbox_new(FALSE, 6);
 gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gtkDlg)->vbox), gtkHbox, FALSE, FALSE, 0);
 gtk_container_set_border_width (GTK_CONTAINER (gtkHbox), 4);
	
 gtkVboxGeneral = gtk_vbox_new(FALSE, 6);
 gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gtkDlg)->vbox), gtkVboxGeneral, FALSE, FALSE, 0);
 gtk_container_set_border_width (GTK_CONTAINER (gtkVboxGeneral), 4);	

 gtkVbox = gtk_vbox_new(FALSE, 4);
 gtkVbox2 = gtk_vbox_new(FALSE, 4);
 gtk_container_set_border_width (GTK_CONTAINER (gtkVbox), 2);
 gtk_container_set_border_width (GTK_CONTAINER (gtkVbox2), 2);
 gtk_box_pack_start (GTK_BOX (gtkHbox), gtkVbox, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (gtkHbox), gtkVbox2, FALSE, FALSE, 0);

 gtkImageContainer = gtk_fixed_new();

 gdkPbIcon = gdk_pixbuf_new_from_xpm_data((const char**) 
					  xfce4_wmdock_plugin_xpm);

 gtkImageTile = gtk_image_new_from_pixmap(gdkPmTile, NULL);
 propDockappImage = gtkImage = gtk_image_new_from_pixbuf (gdkPbIcon);

 g_object_unref (G_OBJECT (gdkPbIcon));

 gtk_container_add(GTK_CONTAINER(gtkImageContainer), gtkImageTile);
 gtk_container_add(GTK_CONTAINER(gtkImageContainer), gtkImage);

 gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkImageContainer),
		     FALSE, FALSE, 0);

 gtkBtnMoveUp = xfce_arrow_button_new (GTK_ARROW_UP);
 gtkBtnMoveDown = xfce_arrow_button_new (GTK_ARROW_DOWN);
 gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkBtnMoveUp), FALSE, 
		     FALSE, 0);
 gtk_box_pack_start (GTK_BOX(gtkVbox), GTK_WIDGET (gtkBtnMoveDown), FALSE, 
		     FALSE, 0);

 gtkLblSel = gtk_label_new ("Select dockapp to configure:");
 gtk_misc_set_alignment (GTK_MISC (gtkLblSel), 0, 0);
 gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkLblSel, FALSE, FALSE, 0);

 /* Create the dockapp chooser combobox */
 propDockappChooser = gtkComboBox = gtk_combo_box_new_text();

 gtk_box_pack_start (GTK_BOX (gtkVbox2), gtkComboBox, FALSE, TRUE, 0);

 gtkLblCmd = gtk_label_new ("Shell command:");
 gtk_misc_set_alignment (GTK_MISC (gtkLblCmd), 0, 0);
 gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkLblCmd, FALSE, FALSE, 0);
 propDockappCommand = gtkTxtCmd = gtk_entry_new();
 if(g_list_length(wmdock->dapps) > 0) {
  gtk_editable_set_editable(GTK_EDITABLE(gtkTxtCmd), TRUE);
 } else {
  gtk_editable_set_editable(GTK_EDITABLE(gtkTxtCmd), FALSE);
 }
 gtk_box_pack_start (GTK_BOX(gtkVbox2), gtkTxtCmd, FALSE, FALSE, 0);
	
 gtkLblGeneral = gtk_label_new ("General settings:");
 gtk_misc_set_alignment (GTK_MISC (gtkLblGeneral), 0, 0);
 gtkChkDispTile = gtk_check_button_new_with_label("Display tile in the background");
 gtk_toggle_button_set_active((GtkToggleButton *) gtkChkDispTile, 
			      wmdock->propDispTile);
 gtk_misc_set_alignment (GTK_MISC (gtkChkDispTile), 0, 0);
 gtk_box_pack_start (GTK_BOX(gtkVboxGeneral), gtkLblGeneral, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX(gtkVboxGeneral), gtkChkDispTile, FALSE, FALSE, 0);

 /* Fill the dockapp chooser with entries. */
 wmdock_refresh_properties_dialog();

 /* Connect some signals to the dialog widgets */
 g_signal_connect(G_OBJECT(gtkComboBox), "changed", G_CALLBACK(wmdock_properties_changed), gtkTxtCmd);
 g_signal_connect(G_OBJECT(gtkTxtCmd), "key-release-event", G_CALLBACK(wmdock_properties_savecmd), gtkComboBox);
 g_signal_connect(G_OBJECT(gtkBtnMoveUp), "pressed", 
		  G_CALLBACK(wmdock_properties_moveup), gtkComboBox);
 g_signal_connect(G_OBJECT(gtkBtnMoveDown), "pressed", 
		  G_CALLBACK(wmdock_properties_movedown), gtkComboBox);
 g_signal_connect(G_OBJECT(gtkChkDispTile), "toggled", 
		  G_CALLBACK(wmdock_properties_chkdisptile), NULL);

 if(g_list_length(wmdock->dapps) > 0) {
  wmdock_properties_changed(gtkComboBox, gtkTxtCmd);	
 }

 gtk_widget_show_all (gtkDlg);
}


static WmdockPlugin *wmdock_plugin_new (XfcePanelPlugin* plugin)
{
 wmdock = g_new0(WmdockPlugin, 1);

 wmdock->plugin = plugin;
 wmdock->dapps = NULL;
 wmdock->propDispTile = TRUE;

 wmdock->ebox = gtk_event_box_new ();
 gtk_widget_show(GTK_WIDGET(wmdock->ebox));

 wmdock->align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

 gtk_widget_show(GTK_WIDGET(wmdock->align));

 gtk_container_add(GTK_CONTAINER(wmdock->ebox), GTK_WIDGET(wmdock->align));
	 
 /* 
    if(xfce_panel_plugin_get_orientation (plugin) == GTK_ORIENTATION_VERTICAL) {
    wmdock->box = gtk_vbox_new(FALSE, 0);
    } else {
    wmdock->box = gtk_hbox_new(FALSE, 0);
    }
 */
 wmdock->box = xfce_hvbox_new(xfce_panel_plugin_get_orientation (plugin), FALSE, 0);

 gtk_container_add (GTK_CONTAINER (wmdock->align), wmdock->box);
	
 return wmdock;
}


static void wmdock_construct (XfcePanelPlugin *plugin)
{
 WnckScreen *s;

#ifdef DEBUG
 char debugFile[BUF_MAX];
 sprintf(debugFile, "%s/wmdock-debug.%d", g_get_tmp_dir(), getpid());
 fp = fopen(debugFile, "w");
 if(!fp) fp = stderr;
#endif

 s = wnck_screen_get(0);

 xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

 XfceDockAppAtom=XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
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
	
 /* Setup the tile image */    	                                  	
 gdkPmTile = gdk_pixmap_create_from_xpm_d (wmdock->ebox->window, NULL,
					   NULL, tile_xpm);

 gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm, 
				      xfce_panel_plugin_get_size (plugin) - 2,
				      xfce_panel_plugin_get_size (plugin) - 2);
 wmdockIcon = gtk_image_new_from_pixbuf (gdkPbIcon);
 g_object_unref (G_OBJECT (gdkPbIcon));

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
