/* Copyright 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <signal.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>

#include "server.h"
#include "geofence_server_data_types.h"
#include "debug_util.h"

#ifdef _TIZEN_PUBLIC_
#include <msg.h>
#include <msg_transport.h>
#include <pmapi.h>
#endif

#include <vconf.h>
#include <vconf-keys.h>
#include <dlog.h>

#include <glib.h>
#include <glib-object.h>
#if !GLIB_CHECK_VERSION(2, 31, 0)
#include <glib/gthread.h>
#endif
/* for scan AP
#include <lbs_wps.h>*/
#include <network-cm-intf.h>
#include <network-pm-intf.h>
#include <network-wifi-intf.h>
#include <network-pm-config.h>
/* for telephony*/
#if USE_TAPI
#include <tapi_common.h>
#include <TapiUtility.h>
#endif
#include "geofence_server_private.h"
/* for bluetooth-geofence*/
#include <bluetooth.h>
#include <wifi.h>
#include <locations.h>

#define TIZEN_ENGINEER_MODE
#ifdef TIZEN_ENGINEER_MODE
#include "geofence_server_log.h"
#endif
#ifdef __LOCAL_TEST__
#include <vconf.h>
#include <vconf-keys.h>
#endif
geofence_callbacks g_fence_update_cb;
void *g_fence_user_data;

#if USE_TAPI
typedef struct {
	TapiHandle *tapi_handle;
} geofence_server_t;

geofence_server_t *g_geofence_server = NULL;

static geofence_server_t *__initialize_geofence_data(void)
{
	g_geofence_server = (geofence_server_t *)malloc(sizeof(geofence_server_t));
	if (g_geofence_server == NULL) {
		LOGI_GEOFENCE("Failed to alloc g_geofence_server");
		return NULL;
	}
	memset(g_geofence_server, 0x00, sizeof(geofence_server_t));

	return g_geofence_server;
}
static void __deinitialize_geofence_data(void)
{
	if (g_geofence_server != NULL) {
		free(g_geofence_server);
		g_geofence_server = NULL;
	}
}
#endif

static void __geofence_network_evt_cb(net_event_info_t *event_cb, void *user_data)
{
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	LOGD_GEOFENCE("==CM Event callback==, Event[%d]", event_cb->Event);

	if (g_fence_update_cb.network_evt_cb) {
		LOGD_GEOFENCE("geofence_network_evt_cb");
		g_fence_update_cb.network_evt_cb(event_cb, user_data);
	}
}

static void __geofence_bt_adapter_state_changed_cb(int result, bt_adapter_state_e adapter_state, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	LOGD_GEOFENCE("bt adapter changed callback, StateChanging[%d]", result);
	if (adapter_state == BT_ADAPTER_DISABLED) {
		LOGD_GEOFENCE("BT_ADAPTER_DISABLED");
		if (g_fence_update_cb.bt_apater_disable_cb) {
			LOGD_GEOFENCE("bt_apater_disable_cb");
			g_fence_update_cb.bt_apater_disable_cb(FALSE, user_data);
		}
	} else if (adapter_state == BT_ADAPTER_ENABLED) {
		LOGD_GEOFENCE("BT_ADAPTER_DISABLED");
		if (g_fence_update_cb.bt_apater_disable_cb) {
			LOGD_GEOFENCE("bt_apater_enable_cb");
			g_fence_update_cb.bt_apater_disable_cb(TRUE, user_data);
		}
	}
	LOGD_GEOFENCE("exit");
}

static void __geofence_bt_device_connection_state_changed_cb(bool connected, bt_device_connection_info_s *conn_info, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);
	g_return_if_fail(conn_info);

	if (g_fence_update_cb.bt_conn_state_changed_cb) {
		LOGD_GEOFENCE("bt_conn_state_changed_cb");
		g_fence_update_cb.bt_conn_state_changed_cb(connected, conn_info, user_data);
	}
	LOGD_GEOFENCE("exit");
}

static void __geofence_bt_adapter_device_discovery_state_changed_cb(int result,	bt_adapter_device_discovery_state_e discovery_state, bt_adapter_device_discovery_info_s *discovery_info, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	if (g_fence_update_cb.bt_discovery_cb) {
		LOGD_GEOFENCE("bt_conn_state_changed_cb");
		g_fence_update_cb.bt_discovery_cb(result, discovery_state, discovery_info, user_data);
	}
	LOGD_GEOFENCE("exit");
}

static void __geofence_wifi_device_connection_state_changed_cb(wifi_connection_state_e state, wifi_ap_h ap, void *user_data)
{
	LOGD_GEOFENCE("__geofence_wifi_device_connection_state_changed_cb()");

	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	if (g_fence_update_cb.wifi_conn_state_changed_cb) {
		LOGD_GEOFENCE("wifi_conn_state_changed_cb");
		g_fence_update_cb.wifi_conn_state_changed_cb(state, ap,	user_data);
	}
}

static void __geofence_wifi_device_state_changed_cb(wifi_device_state_e state, void *user_data)
{
	LOGD_GEOFENCE("__geofence_wifi_device_state_changed_cb()");

	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	if (g_fence_update_cb.wifi_device_state_changed_cb) {
		LOGD_GEOFENCE("wifi_conn_state_changed_cb");
		g_fence_update_cb.wifi_device_state_changed_cb(state, user_data);
	}
}

static void __geofence_wifi_rssi_level_changed_cb(wifi_rssi_level_e rssi_level, void *user_data)
{
	LOGD_GEOFENCE("__geofence_wifi_rssi_level_changed_cb()");
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	if (g_fence_update_cb.wifi_rssi_level_changed_cb) {
		LOGD_GEOFENCE("wifi_rssi_level_changed_cb");
		g_fence_update_cb.wifi_rssi_level_changed_cb(rssi_level, user_data);
	}
}

static void __geofence_gps_setting_changed_cb(location_method_e method,	bool enable, void *user_data)
{
	LOGD_GEOFENCE("__geofence_gps_setting_changed_cb()");
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	g_return_if_fail(geofence_server);

	if (g_fence_update_cb.gps_setting_changed_cb) {
		LOGD_GEOFENCE("GPS setting changed");
		g_fence_update_cb.gps_setting_changed_cb(method, enable, user_data);
	}
}

int _geofence_initialize_geofence_server(GeofenceServer *geofence_server)
{
	FUNC_ENTRANCE_SERVER;

	int ret = 0;

#if USE_TAPI
	geofence_server_t *server = NULL;

	server = __initialize_geofence_data();
	if (server == NULL)
		return -1;
#endif

	/*initialize to use bluetooth C-API*/
	ret = bt_initialize();
	if (BT_ERROR_NONE != ret) {
		LOGD_GEOFENCE("bt_initialize() failed(%d).", ret);
		return -1;
	}

	/* register the bluetooth adapter state changed callback*/
	ret = bt_adapter_set_state_changed_cb(__geofence_bt_adapter_state_changed_cb, geofence_server);
	if (BT_ERROR_NONE != ret) {
		LOGD_GEOFENCE("bt_adapter_set_state_changed_cb() failed(%d).", ret);
		bt_deinitialize();
		return -1;
	} else {
		LOGD_GEOFENCE("bt_adapter_set_state_changed_cb() success.", ret);
	}

	/* register the bluetooth device connection state changed callback*/
	ret = bt_device_set_connection_state_changed_cb(__geofence_bt_device_connection_state_changed_cb, geofence_server);
	if (BT_ERROR_NONE != ret) {
		LOGD_GEOFENCE("bt_device_set_connection_state_changed_cb() failed(%d).", ret);
		bt_adapter_unset_state_changed_cb();
		bt_deinitialize();
		return -1;
	} else {
		LOGD_GEOFENCE("bt_device_set_connection_state_changed_cb() success.", ret);
	}
	/*register for the discovery state change callback*/
	ret = bt_adapter_set_device_discovery_state_changed_cb(__geofence_bt_adapter_device_discovery_state_changed_cb,	(void *)geofence_server);
	if (BT_ERROR_NONE != ret)
		LOGE_GEOFENCE("Failed to set the callback for discovery");

	ret = wifi_initialize();
	if (WIFI_ERROR_NONE != ret) {
		LOGD_GEOFENCE("wifi_initialize() failed(%d).", ret);
		return -1;
	}

	if (net_register_client((net_event_cb_t) __geofence_network_evt_cb, geofence_server) != NET_ERR_NONE) {
		LOGD_GEOFENCE("net_register_client() failed");
		return -1;
	} else {
		LOGD_GEOFENCE("net_register_client() succeeded");
	}

	ret = wifi_set_connection_state_changed_cb(__geofence_wifi_device_connection_state_changed_cb, geofence_server);
	if (WIFI_ERROR_NONE != ret) {
		LOGD_GEOFENCE("wifi_set_connection_state_changed_cb() failed(%d).", ret);
		ret = wifi_deinitialize();
		if (ret != WIFI_ERROR_NONE)
			LOGD_GEOFENCE("wifi_deinitialize() failed(%d).", ret);
		return -1;
	} else {
		LOGD_GEOFENCE("wifi_set_connection_state_changed_cb() success.", ret);
	}

	ret = wifi_set_device_state_changed_cb(__geofence_wifi_device_state_changed_cb, geofence_server);
	if (WIFI_ERROR_NONE != ret) {
		LOGD_GEOFENCE("wifi_set_device_state_changed_cb() failed(%d).",	ret);
		ret = wifi_deinitialize();
		if (ret != WIFI_ERROR_NONE)
			LOGD_GEOFENCE("wifi_deinitialize() failed(%d).", ret);
		return -1;
	} else {
		LOGD_GEOFENCE("wifi_set_device_state_changed_cb() success.", ret);
	}

	ret = wifi_set_rssi_level_changed_cb(__geofence_wifi_rssi_level_changed_cb, geofence_server);
	if (WIFI_ERROR_NONE != ret) {
		LOGD_GEOFENCE("wifi_set_rssi_level_changed_cb() failed(%d).", ret);
		ret = wifi_deinitialize();
		if (ret != WIFI_ERROR_NONE)
			LOGD_GEOFENCE("wifi_deinitialize() failed(%d).", ret);
		return -1;
	} else {
		LOGD_GEOFENCE("wifi_set_rssi_level_changed_cb() success.", ret);
	}

	/*Set the callback for location*/
	ret = location_manager_set_setting_changed_cb(LOCATIONS_METHOD_GPS, __geofence_gps_setting_changed_cb, geofence_server);
	if (LOCATIONS_ERROR_NONE != ret) {
		LOGD_GEOFENCE("location_manager_set_setting_changed_cb() failed(%d)", ret);
		return -1;
	}
	ret = location_manager_set_setting_changed_cb(LOCATIONS_METHOD_WPS, __geofence_gps_setting_changed_cb, geofence_server);
	if (LOCATIONS_ERROR_NONE != ret) {
		LOGD_GEOFENCE("location_manager_set_setting_changed_cb() failed(%d)", ret);
		return -1;
	}

	return 0;
}

int _geofence_deinitialize_geofence_server()
{
	/* to denit geofence engine staff...*/

	/* unset bluetooth device connection state changed state event callback*/
	if (bt_device_unset_connection_state_changed_cb() != BT_ERROR_NONE) {
		LOGD_GEOFENCE("bt_device_unset_connection_state_changed_cb() failed.\n");
	} else {
		LOGD_GEOFENCE("bt_device_unset_connection_state_changed_cb() success.\n");
	}

	/* unset bluetooth adapter changed state event callback*/
	if (bt_adapter_unset_state_changed_cb() != BT_ERROR_NONE) {
		LOGD_GEOFENCE("bt_adapter_unset_state_changed_cb() failed.\n");
	} else {
		LOGD_GEOFENCE("bt_adapter_unset_state_changed_cb() success.\n");
	}

	/* deinit bluetooth api*/
	if (bt_deinitialize() != BT_ERROR_NONE) {
		LOGD_GEOFENCE("bt_deinitialize() failed.\n");
	} else {
		LOGD_GEOFENCE("bt_deinitialize() success.\n");
	}

	/*unset the callbacks related to wifi*/
	if (wifi_unset_connection_state_changed_cb() != WIFI_ERROR_NONE) {
		LOGD_GEOFENCE("wifi_unset_connection_state_changed_cb() failed.\n");
	} else {
		LOGD_GEOFENCE("wifi_unset_connection_state_changed_cb() success.\n");
	}

	if (wifi_deinitialize() != WIFI_ERROR_NONE) {
		LOGD_GEOFENCE("wifi_deinitialize() failed.\n");
	} else {
		LOGD_GEOFENCE("wifi_deinitialize() success.\n");
	}

	if (location_manager_unset_setting_changed_cb(LOCATIONS_METHOD_GPS) != LOCATIONS_ERROR_NONE) {
		LOGD_GEOFENCE("GPS unsetting failed\n");
	} else {
		LOGD_GEOFENCE("GPS unsetting success\n");
	}
#if USE_TAPI
	__deinitialize_geofence_data();
#endif

	return 0;
}

int _geofence_register_update_callbacks(geofence_callbacks *geofence_callback, void *user_data)
{
	g_fence_update_cb = *geofence_callback;
	g_fence_user_data = user_data;
	return 0;
}
