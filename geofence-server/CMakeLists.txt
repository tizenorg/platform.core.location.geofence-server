CMAKE_MINIMUM_REQUIRED(VERSION 2.0)
PROJECT(geofence-server) 

SET(server_pkgs_LDFLAGS "${server_pkgs_LDFLAGS} -ldl")
SET(SERVER_EXTRA_CFLAGS "${SERVER_EXTRA_CFLAGS} -D_GNU_SOURCE")

AUX_SOURCE_DIRECTORY(src SERVER_SRCS)

INCLUDE_DIRECTORIES(src include)

#Process will be launched by systemd. TODO: checking dbus auto activation
CONFIGURE_FILE(org.tizen.lbs.Providers.GeofenceServer.service.in org.tizen.lbs.Providers.GeofenceServer.service @ONLY)
#INSTALL(FILES org.tizen.lbs.Providers.GeofenceServer.service DESTINATION /usr/share/dbus-1/system-services)
#INSTALL(FILES config/geofence-server.conf DESTINATION ${SYSCONF_DIR}/dbus-1/system.d)
INSTALL(FILES org.tizen.lbs.Providers.GeofenceServer.service DESTINATION /usr/share/dbus-1/services)
INSTALL(FILES config/geofence-server.conf DESTINATION ${SYSCONF_DIR}/dbus-1/session.d)

ADD_EXECUTABLE(${PROJECT_NAME} ${SERVER_SRCS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${server_pkgs_LDFLAGS} -lm)
SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS ${SERVER_EXTRA_CFLAGS})
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -pie")

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BIN_DIR})
