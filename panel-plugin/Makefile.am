plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-wmdock-plugin

xfce4_wmdock_plugin_SOURCES =						\
	catchwindow.h							\
	catchwindow.c							\
	debug.h								\
	debug.c								\
	dnd.h								\
	dnd.c								\
	dockapp.h							\
	dockapp.c							\
	misc.h								\
	misc.c								\
	props.h								\
	props.c								\
	rcfile.h							\
	rcfile.c							\
	wmdock.h							\
	extern.h							\
	xfce4-wmdock-plugin.xpm						\
	tile.xpm							\
	wmdock.c

xfce4_wmdock_plugin_CFLAGS =						\
	-Wall								\
	-I$(top_srcdir)							\
	$(LIBXFCEGUI4_CFLAGS)						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(LIBWNCK_CFLAGS)						\
	-DGETTEXT_PACKAGE=\"$(GETTEXT_PACKAGE)\"			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

xfce4_wmdock_plugin_LDADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBWNCK_LIBS)							\
	$(LIBXFCEGUI4_LIBS)						\
	@X11_LIBS@

#
# Desktop file
#
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = wmdock.desktop.in.in

desktop_in_files =						\
	$(desktop_in_in_files:.desktop.in.in=.desktop.in)

%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@

desktop_DATA =							\
	$(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@


EXTRA_DIST =								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA)							\
	$(desktop_in_files)
