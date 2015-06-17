pkgdir = $(libdir)/geofence/module
pkg_LTLIBRARIES = libgeofence.la

libgeofence_la_SOURCES = module_geofence_server.c \
					module_internal.c

libgeofence_la_CFLAGS = \
		   		   -D_GNU_SOURCE \
				   -fPIC \
				   -I$(srcdir) \
				   -I$(srcdir)/../geofence-server \
				   -I$(srcdir)/../geofence-server/include \
				   $(MODULE_CFLAGS)

libgeofence_la_LIBADD = \
				   -lm\
				   -ldl\
				   $(MODULE_LIBS)
