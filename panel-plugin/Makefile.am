plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-wmdock-plugin

xfce4_wmdock_plugin_SOURCES =						\
	wmdock.h							\
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
	$(LIBXFCEGUI4_LIBS)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = wmdock.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libdir\@,$(libdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST =								\
	$(desktop_in_in_files)

CLEANFILES =								\
	$(desktop_DATA)
