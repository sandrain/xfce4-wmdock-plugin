#ifndef __WMDOCK_H__
#define __WMDOCK_H__

#include <gtk/gtk.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include <libwnck/libwnck.h>

typedef struct _dockapp {
 WnckWindow *w;
 GtkSocket *s;
 GdkNativeWindow i;
 gchar *name;
 gchar *cmd;
} DockappNode;

typedef struct {
 XfcePanelPlugin *plugin;

 GtkWidget *ebox;
	
 /* Plugin specific definitions */
 GtkWidget *align;
 GtkWidget *box;
	
 gboolean propDispTile;

 GList *dapps;
} WmdockPlugin;

#endif /* __WMDOCK_H__ */
