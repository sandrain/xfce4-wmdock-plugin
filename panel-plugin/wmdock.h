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
