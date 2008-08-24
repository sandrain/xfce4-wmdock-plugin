/* $Id$
 *
 * wmdock xfce4 plugin by Andre Ellguth
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
#include <libxfce4panel/xfce-panel-convenience.h>




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
GtkWidget    *btnProperties = NULL;
DockappNode  *dappProperties = NULL;
GdkPixmap    *gdkPmTile = NULL;
GdkPixbuf    *gdkPbIcon = NULL;
WmdockPlugin *wmdock = NULL;
gchar        **rcCmds = NULL;
gint         rcCmdcnt = 0;


/* Properties dialog */
static struct {
 GtkWidget *dlg;
 GtkWidget *vbox, *vbox2, *vboxGeneral, *vboxDetect;
 GtkWidget *hbox;
 GtkWidget *frmGeneral, *frmDetect;
 GtkWidget *lblSel, *lblCmd;
 GtkWidget *chkDispTile, *chkPropButton, *chkAddOnlyWM;
 GtkWidget *imageContainer, *container;
 GtkWidget *imageTile, *image;
 GtkWidget *txtCmd;
 GtkWidget *cbx;
 GtkWidget *btnMoveUp, *btnMoveDown;
} prop;

/* Prototypes */
static void wmdock_properties_dialog_called_from_widget(GtkWidget *, XfcePanelPlugin *);
static void wmdock_properties_dialog(XfcePanelPlugin *);


/* #define DEBUG */

#ifdef DEBUG
/* fp needed for debug */
FILE           *fp = (FILE *) NULL;
#endif

static gboolean has_dockapp_hint(WnckWindow *w)
{
 Atom atype;
 int afmt;
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


static void set_xsmp_support(WnckWindow *w)
{
 /* Workaround to skip the XFCE4 session manager. If the window
  * has this X text property set, the XFCE4 session manager will not
  * automaticly start the dockapp after startup twice. */

 XTextProperty tp;
 static Atom _XA_SM_CLIENT_ID = None;

 _XA_SM_CLIENT_ID = XInternAtom (GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "SM_CLIENT_ID", False);
 
 
 tp.value = (unsigned char *) strdup("SM_CLIENT_ID");
 tp.encoding = XA_STRING;
 tp.format = 8;
 tp.nitems = 1;

 XSetTextProperty(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()),
		  wnck_window_get_xid(w),
		  &tp, _XA_SM_CLIENT_ID);

 XFree((unsigned char *)tp.value);
}


static GdkPixbuf *get_icon_from_xpm_scaled(const char **xpmData, gint width, gint height)
{
 GdkPixbuf *gdkPb = NULL;
 GdkPixbuf *gdkPbScaled = NULL;
	
 gdkPb = gdk_pixbuf_new_from_xpm_data (xpmData);
	
 gdkPbScaled = gdk_pixbuf_scale_simple(gdkPb, width, height, 
				       GDK_INTERP_BILINEAR);
	
 g_object_unref (G_OBJECT (gdkPb));
	
 return(gdkPbScaled);
}


static void wmdock_panel_draw_wmdock_icon (gboolean redraw)
{
 gdkPbIcon = get_icon_from_xpm_scaled((const char **) xfce4_wmdock_plugin_xpm, 
				      xfce_panel_plugin_get_size (wmdock->plugin) - 2,
				      xfce_panel_plugin_get_size (wmdock->plugin) - 2);
 if(redraw == TRUE && wmdockIcon) {
  gtk_image_set_from_pixbuf (GTK_IMAGE(wmdockIcon), gdkPbIcon);
 } else {
  if(wmdockIcon) gtk_widget_destroy(wmdockIcon);
  wmdockIcon = gtk_image_new_from_pixbuf (gdkPbIcon);

  gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(wmdockIcon), 
		     FALSE, FALSE, 0);
 }
 g_object_unref (G_OBJECT (gdkPbIcon));

 gtk_widget_show(GTK_WIDGET(wmdockIcon));
}


static void wmdock_panel_draw_properties_button ()
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


static void wmdock_fill_cmbx(DockappNode *dapp, GtkWidget *gtkComboBox)
{

 if(gtkComboBox) {
#ifdef DEBUG
  fprintf(fp, "wmdock: %s append to list\n", dapp->name);
  fflush(fp);
#endif
  gtk_combo_box_append_text (GTK_COMBO_BOX(gtkComboBox), dapp->name); 
 }
}


static void wmdock_refresh_properties_dialog()
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

  g_list_foreach(wmdock->dapps, (GFunc) wmdock_fill_cmbx, prop.cbx);  

  
 } else {
  gtk_combo_box_append_text (GTK_COMBO_BOX(prop.cbx), 
			     _("No dockapp is running!"));

  gtk_widget_set_state(prop.txtCmd, GTK_STATE_INSENSITIVE);

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


static void wmdock_destroy_dockapp(DockappNode *dapp)
{
 #ifdef DEBUG
 fprintf(fp, "Destroy dockapp %s\n", dapp->name);
 fflush(fp);
 #endif
 XDestroyWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), dapp->i);
}


static void wmdock_redraw_dockapp(DockappNode *dapp)
{
 gtk_widget_unmap (GTK_WIDGET(dapp->s));

 /* Tile in the background */
 if(wmdock->propDispTile == TRUE) {
  gtk_widget_map(dapp->bg);
  gtk_widget_set_app_paintable(GTK_WIDGET(dapp->s), TRUE);
  gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, gdkPmTile, FALSE);
  if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->s)) & GTK_MAPPED)
   gtk_widget_queue_draw(GTK_WIDGET(dapp->s));
 } else {
  gtk_widget_unmap(dapp->bg);
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


static DockappNode *wmdock_find_startup_dockapp(const gchar *compCmd)
{
 GList *dapps;
 DockappNode *dapp = NULL;

 dapps = wmdock->dapps;

 while(dapps) {
  dapp = (DockappNode *) dapps->data;
  
  if(dapp) {
   if(!dapp->name && dapp->cmd) {
    if(!g_ascii_strcasecmp(dapp->cmd, compCmd)) {
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


static void wmdock_dapp_closed(GtkSocket *socket, DockappNode *dapp)
{
#ifdef DEBUG 	
 fprintf(fp, "wmdock: closed window signal ! (window: %s)\n", dapp->name);
 fflush(fp);
#endif

 wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);

 gtk_widget_destroy(GTK_WIDGET(dapp->tile));
 g_free(dapp->name);
 g_free(dapp->cmd);
 g_free(dapp);

 if(g_list_length (wmdock->dapps) == 0) {
  wmdock_panel_draw_wmdock_icon(FALSE);
 }

 wmdock_refresh_properties_dialog();
}


static gchar *wmdock_get_dockapp_cmd(WnckWindow *w)
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


static GtkWidget *wmdock_create_tile_from_socket(DockappNode *dapp)
{
 GtkWidget *align = NULL;
 GtkWidget *tile = NULL;

 tile = gtk_fixed_new();

 dapp->bg = gtk_image_new_from_pixmap(gdkPmTile, NULL);
 gtk_fixed_put(GTK_FIXED(tile), dapp->bg, 0, 0);
 if(wmdock->propDispTile == TRUE)
  gtk_widget_show(dapp->bg);

 align = gtk_alignment_new(0.5, 0.5, 0, 0);
 gtk_widget_set_size_request(GTK_WIDGET(align), DEFAULT_DOCKAPP_WIDTH,
			     DEFAULT_DOCKAPP_HEIGHT);
 gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(dapp->s));
 gtk_fixed_put(GTK_FIXED(tile), align, 0, 0);

 gtk_widget_show(align);

 return (tile);
}


static void wmdock_window_open(WnckScreen *s, WnckWindow *w) 
{
 int wi, he;
 XWMHints *h;
 XWindowAttributes attr;
 DockappNode *dapp = NULL;
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


#ifdef DEBUG
  fprintf(fp, "wmdock: New dockapp %s with xid:%u pid:%u arrived sessid:%s\n",
	  wnck_window_get_name(w), wnck_window_get_xid(w), 
	  wnck_window_get_pid(w), wnck_window_get_session_id(w));
  fflush(fp);
#endif

  if(wmdock->propDispAddOnlyWM == TRUE && 
     g_str_has_prefix (wnck_window_get_name(w), "wm") == FALSE) {
   XFree(h);
   return;
  }

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
   XMapWindow(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), 
	      wnck_window_get_xid(w));
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

  /* Set this property to skip the XFCE4 session manager. */
  set_xsmp_support(w);

  dapp->name = g_strdup(wnck_window_get_name(w));
  dapp->cmd = cmd;

  if(wmdockIcon) {
   gtk_widget_destroy(wmdockIcon);
   wmdockIcon = NULL;
  }

  if(rcDapp == FALSE) {
   dapp->tile = wmdock_create_tile_from_socket(dapp);

   gtk_box_pack_start(GTK_BOX(wmdock->box), dapp->tile, FALSE, FALSE, 0);
  }

  gtk_socket_add_id(dapp->s, dapp->i);
		
  /* Tile in the background */
  if(wmdock->propDispTile == TRUE) {
   gtk_widget_set_app_paintable(GTK_WIDGET(dapp->s), TRUE);
   gdk_window_set_back_pixmap(GTK_WIDGET(dapp->s)->window, gdkPmTile, FALSE);
   if (GTK_WIDGET_FLAGS(GTK_WIDGET(dapp->s)) & GTK_MAPPED)
    gtk_widget_queue_draw(GTK_WIDGET(dapp->s));
  }

  gtk_widget_show(GTK_WIDGET(dapp->s));
  gtk_widget_show(GTK_WIDGET(dapp->tile));

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
 xfce_hvbox_set_orientation ((XfceHVBox *) wmdock->panelBox, orientation);
 xfce_hvbox_set_orientation ((XfceHVBox *) wmdock->box, orientation);
 gtk_widget_show(GTK_WIDGET(wmdock->panelBox));
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
  wmdock_panel_draw_wmdock_icon(TRUE);
 }

 return TRUE;
}


static gboolean wmdock_startup_dockapp(const gchar *cmd)
{
 gboolean ret;
 GError *err = NULL;

 ret = xfce_exec(cmd, FALSE, FALSE, &err);
 /* ret = g_spawn_command_line_async(cmd, &err); */

 /* Errors will be evaluate in a later version. */
 if(err) g_clear_error (&err);
	
 return(ret);
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
 wmdock->propDispPropButton = xfce_rc_read_bool_entry (rc, "disppropbtn", FALSE);
 wmdock->propDispAddOnlyWM = xfce_rc_read_bool_entry (rc, "dispaddonlywm", TRUE);


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
				    _("Failed to start %s!"),
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
    dapp->tile = wmdock_create_tile_from_socket(dapp);

    gtk_box_pack_start(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile),
		       FALSE, FALSE, 0);

    wmdock->dapps=g_list_append(wmdock->dapps, dapp);    
   }
			
   /* Sleep for n microseconds to startup dockapps in the right order. */
   /* g_usleep(250000); */
  }
 }
 
 xfce_rc_close (rc);
}


static void wmdock_free_data(XfcePanelPlugin *plugin)
{
#ifdef DEBUG
 fprintf(fp, "Called wmdock_free_data\n");
 fflush(fp);
#endif

 g_list_foreach(wmdock->dapps, (GFunc)wmdock_destroy_dockapp, NULL);

 gtk_widget_destroy(GTK_WIDGET(wmdockIcon));
 gtk_widget_destroy(GTK_WIDGET(wmdock->box));
 gtk_widget_destroy(GTK_WIDGET(wmdock->panelBox));
 gtk_widget_destroy(GTK_WIDGET(wmdock->align));
 gtk_widget_destroy(GTK_WIDGET(wmdock->eventBox));
 g_list_free(wmdock->dapps);
 g_free(wmdock);
	
#ifdef DEBUG
 fclose(fp);
#endif
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
  xfce_rc_write_bool_entry (rc, "disppropbtn", wmdock->propDispPropButton); 
  xfce_rc_write_bool_entry (rc, "dispaddonlywm", wmdock->propDispAddOnlyWM); 
 }

 xfce_rc_close(rc);

 /* Cleanup and close all dockapps! */
 /* wmdock_free_data(plugin); */
}


static void wmdock_properties_chkdisptile(GtkToggleButton *gtkChkDispTile, gpointer user_data)
{
 wmdock->propDispTile = gtk_toggle_button_get_active(gtkChkDispTile);

 g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
 gtk_widget_show_all(GTK_WIDGET(wmdock->box));
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
}


static gboolean wmdock_properties_refresh_dapp_icon()
{
 GdkPixmap *pm = NULL;
 DockappNode *dapp = NULL;
 gboolean ret;

 if(prop.dlg && prop.image && prop.cbx) {
  dapp = (DockappNode *) g_list_nth_data(wmdock->dapps,
					 gtk_combo_box_get_active(GTK_COMBO_BOX(prop.cbx)));
  if(dapp) {
   pm = gdk_pixmap_foreign_new (dapp->i);
   if(pm) {
    gtk_image_set_from_pixmap (GTK_IMAGE(prop.image), pm, NULL);
    gtk_widget_show(prop.image);
    g_object_unref (G_OBJECT(pm));
   }
   else {
    gtk_image_set_from_pixmap (GTK_IMAGE(prop.image), gdkPmTile, NULL);
    gtk_widget_show(prop.image);
   }
  }

  ret = TRUE;
 } else ret = FALSE;

#ifdef DEBUG
 if(ret == FALSE) {
  fprintf(fp, "wmdock: wmdock_properties_refresh_dapp_icon status changed to FALSE\n");
  fflush(fp);
 }
#endif

 return (ret);
}


static void wmdock_properties_changed (GtkWidget *gtkComboBox, GtkWidget *gtkTxtCmd)
{
 DockappNode *dapp = NULL;


 dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, gtk_combo_box_get_active(GTK_COMBO_BOX(gtkComboBox)));
 if(dapp) {
#ifdef DEBUG
  fprintf(fp, "wmdock: changed, %s selected:\n", dapp->name);
  fflush(fp);
#endif
	
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
  dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, pos);
		
  if(dapp) {
   wmdock->dapps = g_list_remove_all(wmdock->dapps, dapp);
   wmdock->dapps = g_list_insert(wmdock->dapps, dapp, pos - 1);
   gtk_combo_box_remove_text(GTK_COMBO_BOX(gtkComboBox), pos);
   gtk_combo_box_insert_text(GTK_COMBO_BOX(gtkComboBox), pos - 1, dapp->name);
   gtk_combo_box_set_active(GTK_COMBO_BOX(gtkComboBox), pos - 1);  
   gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile), pos - 1);

   g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
  }
 }
}


static void wmdock_properties_movedown (GtkWidget *gtkBtnMoveDown, GtkWidget *gtkComboBox)
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
   gtk_box_reorder_child(GTK_BOX(wmdock->box), GTK_WIDGET(dapp->tile), pos + 1);

   g_list_foreach(wmdock->dapps, (GFunc)wmdock_redraw_dockapp, NULL);
  }		
 }
}


static void wmdock_properties_savecmd (GtkWidget *gtkTxtCmd, GdkEventKey *event, GtkWidget *gtkComboBox)
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


static void wmdock_properties_dialog_response (GtkWidget  *gtkDlg, gint response)
{
 DockappNode *dapp = NULL;
 gint pos;

 if(!gtkDlg) return;

 switch(response) {
 case GTK_RESPONSE_NO: /* Remove dockapp */
  if(g_list_length(wmdock->dapps) > 0 && prop.cbx) {
   pos = gtk_combo_box_get_active(GTK_COMBO_BOX(prop.cbx));

   dapp = (DockappNode *) g_list_nth_data(wmdock->dapps, pos);
   if(dapp)
    wmdock_destroy_dockapp(dapp);
  }
  break;
  
 default:
  xfce_panel_plugin_unblock_menu (wmdock->plugin);
  gtk_widget_destroy (gtkDlg);

#ifdef DEBUG
  fprintf(fp, "wmdock: properties dlg closed\n");
  fflush(fp);
#endif

  prop.dlg = prop.cbx = prop.txtCmd = prop.image = NULL;
  break;
 }
}


static void wmdock_properties_dialog_called_from_widget(GtkWidget *widget, XfcePanelPlugin *plugin)
{
 wmdock_properties_dialog(plugin);
}


static void wmdock_properties_dialog(XfcePanelPlugin *plugin)
{
 if(prop.dlg) return; /* Return if properties dialog is already open. */

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
 prop.vboxGeneral = gtk_vbox_new(FALSE, 6);
 prop.vboxDetect = gtk_vbox_new(FALSE, 6);

 gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.frmGeneral,
		     FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prop.dlg)->vbox), prop.frmDetect,
		     FALSE, FALSE, 0);
 gtk_container_set_border_width (GTK_CONTAINER (prop.vboxGeneral), 4);	
 gtk_container_set_border_width (GTK_CONTAINER (prop.vboxDetect), 4);	

 prop.vbox = gtk_vbox_new(FALSE, 4);
 prop.vbox2 = gtk_vbox_new(FALSE, 4);
 gtk_container_set_border_width (GTK_CONTAINER (prop.vbox), 2);
 gtk_container_set_border_width (GTK_CONTAINER (prop.vbox2), 2);
 gtk_box_pack_start (GTK_BOX (prop.hbox), prop.vbox, FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX (prop.hbox), prop.vbox2, FALSE, FALSE, 0);


 prop.imageContainer = gtk_alignment_new(0.5, 0.5, 0, 0);
 gtk_widget_set_size_request(GTK_WIDGET(prop.imageContainer),
			     DEFAULT_DOCKAPP_WIDTH,
			     DEFAULT_DOCKAPP_HEIGHT);
 prop.container =  gtk_fixed_new();

 gdkPbIcon = gdk_pixbuf_new_from_xpm_data((const char**) 
					  xfce4_wmdock_plugin_xpm);

 prop.imageTile = gtk_image_new_from_pixmap(gdkPmTile, NULL);
 prop.image = gtk_image_new_from_pixbuf (gdkPbIcon);

 g_object_unref (G_OBJECT (gdkPbIcon));

 gtk_container_add(GTK_CONTAINER(prop.imageContainer), prop.image);
 gtk_container_add(GTK_CONTAINER(prop.container), prop.imageTile);
 gtk_container_add(GTK_CONTAINER(prop.container), prop.imageContainer);


 gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.container),
		     FALSE, FALSE, 0);

 prop.btnMoveUp = xfce_arrow_button_new (GTK_ARROW_UP);
 prop.btnMoveDown = xfce_arrow_button_new (GTK_ARROW_DOWN);
 gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.btnMoveUp), FALSE, 
		     FALSE, 0);
 gtk_box_pack_start (GTK_BOX(prop.vbox), GTK_WIDGET (prop.btnMoveDown), FALSE, 
		     FALSE, 0);

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
	
 prop.chkDispTile = gtk_check_button_new_with_label(_("Display tile in the background."));
 prop.chkPropButton = gtk_check_button_new_with_label(_("Display a separate WMdock properties\nbutton in the panel."));
 prop.chkAddOnlyWM = gtk_check_button_new_with_label(_("Add only dockapps which start with\nwm* in the name."));
 gtk_toggle_button_set_active((GtkToggleButton *) prop.chkDispTile, 
			      wmdock->propDispTile);
 gtk_toggle_button_set_active((GtkToggleButton *) prop.chkPropButton, 
			      wmdock->propDispPropButton);
 gtk_toggle_button_set_active((GtkToggleButton *) prop.chkAddOnlyWM, 
			      wmdock->propDispAddOnlyWM);
 gtk_misc_set_alignment (GTK_MISC (prop.chkDispTile), 0, 0);
 gtk_misc_set_alignment (GTK_MISC (prop.chkPropButton), 0, 0);
 gtk_misc_set_alignment (GTK_MISC (prop.chkAddOnlyWM), 0, 0);
 gtk_container_add(GTK_CONTAINER(prop.frmGeneral), prop.vboxGeneral);
 gtk_container_add(GTK_CONTAINER(prop.frmDetect), prop.vboxDetect);
 gtk_box_pack_start (GTK_BOX(prop.vboxGeneral), prop.chkDispTile, 
		     FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX(prop.vboxGeneral), prop.chkPropButton, 
		     FALSE, FALSE, 0);
 gtk_box_pack_start (GTK_BOX(prop.vboxDetect), prop.chkAddOnlyWM, 
		     FALSE, FALSE, 0);
 

 /* Fill the dockapp chooser with entries. */
 wmdock_refresh_properties_dialog();

 /* Connect some signals to the dialog widgets */
 g_signal_connect(G_OBJECT(prop.cbx), "changed", 
		  G_CALLBACK(wmdock_properties_changed), prop.txtCmd);
 g_signal_connect(G_OBJECT(prop.txtCmd), "key-release-event", 
		  G_CALLBACK(wmdock_properties_savecmd), prop.cbx);
 g_signal_connect(G_OBJECT(prop.btnMoveUp), "pressed", 
		  G_CALLBACK(wmdock_properties_moveup), prop.cbx);
 g_signal_connect(G_OBJECT(prop.btnMoveDown), "pressed", 
		  G_CALLBACK(wmdock_properties_movedown), prop.cbx);
 g_signal_connect(G_OBJECT(prop.chkDispTile), "toggled", 
		  G_CALLBACK(wmdock_properties_chkdisptile), NULL);
 g_signal_connect(G_OBJECT(prop.chkPropButton), "toggled", 
		  G_CALLBACK(wmdock_properties_chkpropbtn), NULL);
 g_signal_connect(G_OBJECT(prop.chkAddOnlyWM), "toggled", 
		  G_CALLBACK(wmdock_properties_chkaddonlywm), NULL);

 g_timeout_add (500, wmdock_properties_refresh_dapp_icon, NULL);

 if(g_list_length(wmdock->dapps) > 0) {
  wmdock_properties_changed(prop.cbx, prop.txtCmd);	
 }

 gtk_widget_show_all (prop.dlg);
}


static WmdockPlugin *wmdock_plugin_new (XfcePanelPlugin* plugin)
{
 wmdock = g_new0(WmdockPlugin, 1);
 wmdock->plugin = plugin;
 wmdock->dapps = NULL;
 wmdock->propDispTile = TRUE;
 wmdock->propDispPropButton = FALSE;
 wmdock->propDispAddOnlyWM = TRUE;

 memset(&prop, 0, sizeof(prop));

 wmdock->eventBox = gtk_event_box_new ();
 gtk_widget_show(GTK_WIDGET(wmdock->eventBox));

 wmdock->align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);

 gtk_widget_show(GTK_WIDGET(wmdock->align));

 gtk_container_add(GTK_CONTAINER(wmdock->eventBox), GTK_WIDGET(wmdock->align));

 wmdock->panelBox = xfce_hvbox_new(xfce_panel_plugin_get_orientation (plugin), FALSE, 0);
 gtk_widget_show(GTK_WIDGET(wmdock->panelBox));

 wmdock->box = xfce_hvbox_new(xfce_panel_plugin_get_orientation (plugin), FALSE, 0);

 gtk_box_pack_start(GTK_BOX(wmdock->panelBox), GTK_WIDGET(wmdock->box), 
		    FALSE, FALSE, 0);
 gtk_widget_show(GTK_WIDGET(wmdock->box));

 gtk_container_add (GTK_CONTAINER (wmdock->align), wmdock->panelBox);
	
 return wmdock;
}


static void wmdock_construct (XfcePanelPlugin *plugin)
{
 WnckScreen  *s;

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
	 
 g_signal_connect(s, "window_opened", G_CALLBACK(wmdock_window_open), NULL);
 g_signal_connect (plugin, "size-changed", G_CALLBACK (wmdock_size_changed), NULL);
 g_signal_connect (plugin, "orientation-changed", G_CALLBACK (wmdock_orientation_changed), NULL);
 g_signal_connect (plugin, "free-data", G_CALLBACK (wmdock_free_data), NULL);

 gtk_container_add (GTK_CONTAINER (plugin), wmdock->eventBox);
	
 xfce_panel_plugin_add_action_widget (plugin, wmdock->eventBox);

 /* Setup the tile image */    	                                  	
 gdkPmTile = gdk_pixmap_create_from_xpm_d (wmdock->eventBox->window, NULL,
					   NULL, tile_xpm);

 wmdock_panel_draw_wmdock_icon(FALSE);
	
 /* Configure plugin dialog */
 xfce_panel_plugin_menu_show_configure (plugin);
 g_signal_connect (plugin, "configure-plugin", G_CALLBACK (wmdock_properties_dialog), NULL);
	
 /* Read the config file and start the dockapps */
 wmdock_read_rc_file(plugin);

 wmdock_panel_draw_properties_button();
	
 g_signal_connect (plugin, "save", G_CALLBACK (wmdock_write_rc_file), NULL);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (wmdock_construct);
