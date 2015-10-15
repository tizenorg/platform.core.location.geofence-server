CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(geofence-server)

SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(BIN_DIR "${PREFIX}/bin")

#Dependencies

SET(common_dp "glib-2.0 geofence-dbus dlog gio-2.0 capi-appfw-app-manager")
SET(server_dp "${common_dp} network vconf vconf-internal-keys capi-system-info gthread-2.0  gio-unix-2.0 sqlite3 db-util alarm-service deviced capi-location-manager capi-network-bluetooth capi-network-wifi libcore-context-manager libtzplatform-config")
SET(module_dp "${common_dp} gmodule-2.0 capi-geofence-manager")

# Set required packages
INCLUDE(FindPkgConfig)

## SERVER
pkg_check_modules(server_pkgs REQUIRED ${server_dp})
FOREACH(flag ${server_pkgs_CFLAGS})
	SET(SERVER_EXTRA_CFLAGS "${SERVER_EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

## MODULE
pkg_check_modules(module_pkgs REQUIRED ${module_dp})
FOREACH(flag ${module_pkgs_CFLAGS})
	SET(MODULE_EXTRA_CFLAGS "${MODULE_EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(EXTRA_CFLAGS "${EXTRA_CFLAGS}  -Wl,-zdefs ")

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS}")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -fvisibility=hidden -fPIC")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter -Wno-missing-field-initializers -Wno-missing-declarations -Wall -Wcast-align -Wno-sign-compare")

SET(SERVER_SRCS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/geofence-server")

ADD_DEFINITIONS("-DPREFIX=\"${CMAKE_INSTALL_PREFIX}\"")
ADD_DEFINITIONS(" -DEXPORT_API=\"__attribute__((visibility(\\\"default\\\")))\" ")

MESSAGE(${CMAKE_C_FLAGS})
MESSAGE(${CMAKE_EXE_LINKER_FLAGS})

ADD_SUBDIRECTORY(module)
ADD_SUBDIRECTORY(geofence-server)
