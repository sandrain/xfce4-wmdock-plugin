plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-wmdock-plugin

xfce4_wmdock_plugin_SOURCES =						\
	wmdock.h							\
	xfce4-wmdock-plugin.xpm						\
	wmdock.c

xfce4_wmdock_plugin_CFLAGS =						\
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
desktop_in_files = wmdock.desktop.in
%.desktop: %.desktop.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST =								\
	$(desktop_in_files)

CLEANFILES =								\
	$(desktop_DATA)

