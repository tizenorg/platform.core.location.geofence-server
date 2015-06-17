libexec_PROGRAMS = geofence-server


geofence_server_SOURCES =	geofence_server.c \
			server.c \
			geofence_server_log.c \
			geofence_server_wifi.c \
			geofence_server_alarm.c \
			geofence_server_internal.c \
			geofence_server_bluetooth.c \
			geofence_server_appman.c 

geofence_server_CFLAGS =	-Wall -Wno-unused-variable -Wno-unused-function -fPIC -fPIE -fvisibility=hidden \
				-DEXPORT_API="__attribute__((visibility(\"default\")))" \
				-I$(srcdir)/include \
				$(PROVIDERS_CFLAGS)

geofence_server_LDFLAGS =	-Wl,--hash-style=both -Wl,--as-needed -Wl,-rpath=%{_libdir}, -pie

geofence_server_DEPENDENCIES =

geofence_server_LDADD =	$(geofence_server_DEPENDENCIES) \
				$(PROVIDERS_LIBS) \
				-ldl \
				-lm

providersdir = $(datadir)/lbs
providers_DATA = geofence-server.provider

servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.tizen.lbs.Providers.GeofenceServer.service.in
service_DATA = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@


$(conf_DATA): 	$(conf_in_files)
		cp $< $@

EXTRA_DIST = 	$(PCFILES).in \
		$(service_in_files) \
		$(providers_DATA) \
		$(conf_in_files)

DISTCLEANFILES =	$(service_DATA) \
			$(conf_DATA)

GEOFENCE_SERVER = geofence-server
bootdir = /etc/rc.d/init.d
boot_SCRIPTS = script/$(GEOFENCE_SERVER)

RC5_SCRIPT = $(DESTDIR)/etc/rc.d/rc5.d/S90geofence-server

install-exec-hook:
	mkdir -p $(DESTDIR)/etc/rc.d/rc5.d
	ln -sf ../init.d/$(GEOFENCE_SERVER) $(RC3_SCRIPT)
	ln -sf ../init.d/$(GEOFENCE_SERVER) $(RC5_SCRIPT)

uninstall-hook:
	rm -f $(RC3_SCRIPT) $(RC5_SCRIPT)


#INSTALL(FILES ../geofence-server.rule DESTINATION /etc/smack/accesses2.d)
