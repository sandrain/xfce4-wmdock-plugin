SUBDIRS =								\
	panel-plugin							\
	icons								\
	po

AUTOMAKE_OPTIONS =							\
	1.8								\
	dist-bzip2

distclean-local:
	rm -rf *.cache *~ .project .settings .cdtproject

EXTRA_DIST = config.rpath  config.rpath m4/ChangeLog 								\
	intltool-extract.in						\
	intltool-merge.in						\
	intltool-update.in

DISTCLEANFILES =							\
	intltool-extract						\
	intltool-merge							\
	intltool-update

ACLOCAL_AMFLAGS = -I m4
