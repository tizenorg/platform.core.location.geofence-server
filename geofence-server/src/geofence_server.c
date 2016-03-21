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
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <glib.h>
#include <glib-object.h>
#include <geofence_client.h>
#include <dd-display.h>
#include <vconf.h>
#include <vconf-internal-wifi-keys.h>
#include <vconf-internal-dnet-keys.h>
#include <vconf-internal-location-keys.h>
#include "geofence_server_data_types.h"
#include "geofence_server.h"
#include "server.h"
#include "debug_util.h"
#include "geofence_server_db.h"
#include "geofence_server_private.h"
#include "geofence_server_wifi.h"
#include "geofence_server_alarm.h"
#include "geofence_server_internal.h"
#include "geofence_server_bluetooth.h"
#include <wifi.h>
#include <network-wifi-intf.h>
#include <system_info.h>

#define TIZEN_ENGINEER_MODE
#ifdef TIZEN_ENGINEER_MODE
#include "geofence_server_log.h"
#endif

#define WPS_ACCURACY_TOLERANCE	100
#define GPS_TRIGGER_BOUNDARY 1000
#define SMART_ASSIST_HOME	1
#define SMART_ASSIST_TIMEOUT			60	/* Refer to LPP */
#define GEOFENCE_DEFAULT_RADIUS			200	/* Default 200 m */
#define GPS_TIMEOUT				60
#define WPS_TIMEOUT				60

#define MYPLACES_APPID	"org.tizen.myplace"
#define DEFAULT_PLACE_HOME 1
#define DEFAULT_PLACE_OFFICE 2
#define DEFAULT_PLACE_CAR 3

static int __emit_fence_proximity(GeofenceServer *geofence_server, int fence_id, geofence_proximity_state_e state);
static int __gps_alarm_cb(alarm_id_t alarm_id, void *user_data);
static int __wps_alarm_cb(alarm_id_t alarm_id, void *user_data);
static int __gps_timeout_cb(alarm_id_t alarm_id, void *user_data);
static int __wps_timeout_cb(alarm_id_t alarm_id, void *user_data);
static void __add_left_fences(gpointer user_data);
static void __start_activity_service(GeofenceServer *geofence_server);
static void __stop_activity_service(GeofenceServer *geofence_server);
static void __set_interval_for_gps(double min_distance, int min_fence_id, void *user_data);
static void __activity_cb(activity_type_e type, const activity_data_h data, double timestamp, activity_error_e error, void *user_data);
static bool __isWifiOn(void);
static bool __isDataConnected(void);

static bool __is_support_wps()
{
	const char *wps_feature = "http://tizen.org/feature/location.wps";
	bool is_wps_supported = false;
	system_info_get_platform_bool(wps_feature, &is_wps_supported);
	if (is_wps_supported == true) {
		location_manager_is_enabled_method(LOCATIONS_METHOD_WPS, &is_wps_supported);
	}

	return is_wps_supported;
}

static const char *__convert_wifi_error_to_string(wifi_error_e err_type)
{
	switch (err_type) {
		case WIFI_ERROR_NONE:
			return "NONE";
		case WIFI_ERROR_INVALID_PARAMETER:
			return "INVALID_PARAMETER";
		case WIFI_ERROR_OUT_OF_MEMORY:
			return "OUT_OF_MEMORY";
		case WIFI_ERROR_INVALID_OPERATION:
			return "INVALID_OPERATION";
		case WIFI_ERROR_ADDRESS_FAMILY_NOT_SUPPORTED:
			return "ADDRESS_FAMILY_NOT_SUPPORTED";
		case WIFI_ERROR_OPERATION_FAILED:
			return "OPERATION_FAILED";
		case WIFI_ERROR_NO_CONNECTION:
			return "NO_CONNECTION";
		case WIFI_ERROR_NOW_IN_PROGRESS:
			return "NOW_IN_PROGRESS";
		case WIFI_ERROR_ALREADY_EXISTS:
			return "ALREADY_EXISTS";
		case WIFI_ERROR_OPERATION_ABORTED:
			return "OPERATION_ABORTED";
		case WIFI_ERROR_DHCP_FAILED:
			return "DHCP_FAILED";
		case WIFI_ERROR_INVALID_KEY:
			return "INVALID_KEY";
		case WIFI_ERROR_NO_REPLY:
			return "NO_REPLY";
		case WIFI_ERROR_SECURITY_RESTRICTED:
			return "SECURITY_RESTRICTED";
		case WIFI_ERROR_PERMISSION_DENIED:
			return "PERMISSION_DENIED";
		default:
			return "NOT Defined";
	}
}

void emit_proximity_using_ble(GeofenceServer *geofence_server, int fence_id, geofence_proximity_state_e state)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(geofence_server);
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data) {
		if (item_data->common_info.proximity_status != state) {
			LOGI_GEOFENCE("Emitting proximity status(fence: %d): %d", fence_id, state);
			geofence_dbus_server_send_geofence_proximity_changed(geofence_server->geofence_dbus_server, item_data->common_info.appid, fence_id, item_data->common_info.access_type, state, GEOFENCE_PROXIMITY_PROVIDER_BLE);
			item_data->common_info.proximity_status = state;
		}
	} else {
		LOGD_GEOFENCE("Invalid item_data");
	}
}

static bool __check_for_match(char *str1, char *str2)
{
	if (g_strrstr(str1, str2) == NULL)
		return false;
	return true;
}

static void bt_le_scan_result_cb(int result, bt_adapter_le_device_scan_result_info_s *info, void *user_data)
{
	int ret = BT_ERROR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	LOGI_GEOFENCE("Current addresses: %s", geofence_server->ble_info);
	LOGI_GEOFENCE("Received address: %s", info->remote_address);
	if (info == NULL) {
		LOGI_GEOFENCE("Stopping scan as there is no BLE addresses found");
		ret = bt_adapter_le_stop_scan();
		if (ret != BT_ERROR_NONE)
			LOGE_GEOFENCE("Unable to stop the BLE scan, error: %d", ret);
		return;
	}
	/* Retrieve the information about the AP */
	if (!g_ascii_strcasecmp(geofence_server->ble_info, "")) {
		g_stpcpy(geofence_server->ble_info, info->remote_address);
	} else if (!__check_for_match(geofence_server->ble_info, info->remote_address)) { /* If duplicate does not exist */
		char *p = g_strjoin(";", geofence_server->ble_info, info->remote_address, NULL);
		g_stpcpy(geofence_server->ble_info, p);
		g_free(p);
	} else {
		LOGI_GEOFENCE("Stopping scan. Address: %s already exist in the string %s", info->remote_address, geofence_server->ble_info);
		ret = bt_adapter_le_stop_scan();
		if (ret != BT_ERROR_NONE)
			LOGE_GEOFENCE("Unable to stop the BLE scan, error: %d", ret);
		/* Add the string to the database. */
		LOGI_GEOFENCE("Writing address: %s to DB for fence: %d", geofence_server->ble_info, geofence_server->nearestTrackingFence);
		geofence_manager_set_ble_info_to_geofence(geofence_server->nearestTrackingFence, geofence_server->ble_info);
	}
}

void bt_le_scan_result_display_cb(int result, bt_adapter_le_device_scan_result_info_s *info, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	int ret = BT_ERROR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	int wps_state = 0;
	int gps_state = 0;
	int fence_id = -1;
	char *ble_info = NULL;
	ble_mode_e ble_proximity_mode = BLE_INFO_NONE;
	GList *tracking_list = g_list_first(geofence_server->tracking_list);
	if (info == NULL) {
		LOGI_GEOFENCE("Stopping scan as there is no BLE addresses found");
		ret = bt_adapter_le_stop_scan();
		if (ret != BT_ERROR_NONE)
			LOGE_GEOFENCE("Unable to stop the BLE scan, error: %d", ret);
		return;
	}
	if (!g_ascii_strcasecmp(geofence_server->ble_info, "")) {
		g_stpcpy(geofence_server->ble_info, info->remote_address);
	} else if (!__check_for_match(geofence_server->ble_info, info->remote_address)) { /* If duplicate does not exist */
		char *p = g_strjoin(";", geofence_server->ble_info, info->remote_address, NULL);
		g_stpcpy(geofence_server->ble_info, p);
		g_free(p);
	} else {
		LOGI_GEOFENCE("Stopping scan. Address: %s already exist in the string %s", info->remote_address, geofence_server->ble_info);
		ret = bt_adapter_le_stop_scan();
		if (ret != BT_ERROR_NONE)
			LOGE_GEOFENCE("Unable to stop the BLE scan, error: %d", ret);
		return;
	}
	if (tracking_list) {
		while (tracking_list) {
			ble_proximity_mode = BLE_INFO_NONE;
			fence_id = GPOINTER_TO_INT(tracking_list->data);
			GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
			if (item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
				vconf_get_int(VCONFKEY_LOCATION_NETWORK_ENABLED, &wps_state);
				vconf_get_int(VCONFKEY_LOCATION_ENABLED, &gps_state);
				if (wps_state == 0 && gps_state == 0)
					ble_proximity_mode = BLE_INFO_READ;
			} else if (item_data->common_info.type == GEOFENCE_TYPE_WIFI) {
				if (__isWifiOn() == false)
					ble_proximity_mode = BLE_INFO_READ;
			} else if (item_data->common_info.type == GEOFENCE_TYPE_BT) {
				bssid_info_s *bt_info = NULL;
				bt_info = (bssid_info_s *) item_data->priv;
				bt_device_info_s *bt_device_info = NULL;
				if (bt_info != NULL) {
					ret = bt_adapter_get_bonded_device_info(bt_info->bssid, &bt_device_info);
					if (ret == BT_ERROR_NONE) {
						if (bt_device_info->is_connected == false)
							ble_proximity_mode = BLE_INFO_READ;
					} else if (ret == BT_ERROR_REMOTE_DEVICE_NOT_BONDED) {
						ble_proximity_mode = BLE_INFO_READ; /*Its not bonded*/
					}
				}
			}
			if (ble_proximity_mode == BLE_INFO_READ) {
				geofence_manager_get_ble_info_from_geofence(fence_id, &ble_info);
				LOGI_GEOFENCE("Ble info read from DB: %s", ble_info);
				if (__check_for_match(ble_info, info->remote_address)) {
					LOGI_GEOFENCE("Matched for ble address: %s for the fence: %d", info->remote_address, fence_id);
					emit_proximity_using_ble(geofence_server, fence_id, GEOFENCE_PROXIMITY_IMMEDIATE);
				}
			}
			tracking_list = g_list_next(tracking_list);
		}
	}
}

void device_display_changed_cb(device_callback_e type, void *value, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	int ret = BT_ERROR_NONE;
	GList *tracking_list = g_list_first(geofence_server->tracking_list);
	if (tracking_list) {
		if (type == DEVICE_CALLBACK_DISPLAY_STATE) {
			display_state_e state = (display_state_e)value;
			if (state == DISPLAY_STATE_NORMAL) {
				LOGI_GEOFENCE("State: NORMAL");
				if (tracking_list) {
					LOGD_GEOFENCE("Scanning for BLE and read DB");
					g_stpcpy(geofence_server->ble_info, "");
					ret = bt_adapter_le_start_scan(bt_le_scan_result_display_cb, geofence_server);
					if (ret != BT_ERROR_NONE) {
						LOGE_GEOFENCE("Fail to start ble scan. %d", ret);
					}
				}
			} else if (state == DISPLAY_STATE_SCREEN_DIM)
				LOGI_GEOFENCE("State: DIM");
			else
				LOGI_GEOFENCE("State: OFF");
		}
	}
}

static int __emit_fence_event(GeofenceServer *geofence_server, int place_id, int fence_id, access_type_e access_type, const gchar *app_id, geofence_server_error_e error, geofence_manage_e state)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);

	LOGD_GEOFENCE("place_id: %d, fence_id: %d, access_type: %d, error: %d, state: %d", place_id, fence_id, access_type, error, state);

	geofence_dbus_server_send_geofence_event_changed(geofence_server->geofence_dbus_server, place_id, fence_id, access_type, app_id, error, state);
	return 0;
}

static int __emit_fence_inout(GeofenceServer *geofence_server, int fence_id, geofence_fence_state_e state)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);
	int ret = 0;

	LOGD_GEOFENCE("FenceId[%d], state[%d]", fence_id, state);
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data == NULL) {
		LOGD_GEOFENCE("Invalid item_data");
		return -1;
	}

	if (state == GEOFENCE_FENCE_STATE_IN) {
		/*LOGD_GEOFENCE("FENCE_IN to be set, current state: %d", item_data->common_info.status);*/
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_IN) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server,	item_data->common_info.appid, fence_id,	item_data->common_info.access_type, GEOFENCE_EMIT_STATE_IN);
			if (item_data->client_status == GEOFENCE_CLIENT_STATUS_START) {
				item_data->client_status = GEOFENCE_CLIENT_STATUS_RUNNING;
			}
			LOGD_GEOFENCE("%d : FENCE_IN", fence_id);
#ifdef TIZEN_ENGINEER_MODE
			GEOFENCE_PRINT_LOG("FENCE_IN");
#endif
		}

		item_data->common_info.status = GEOFENCE_FENCE_STATE_IN;

	} else if (state == GEOFENCE_FENCE_STATE_OUT) {
		/*LOGD_GEOFENCE("FENCE_OUT to be set, current state: %d", item_data->common_info.status);*/
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_OUT) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server,	item_data->common_info.appid, fence_id,	item_data->common_info.access_type, GEOFENCE_EMIT_STATE_OUT);
			__emit_fence_proximity(geofence_server, fence_id, GEOFENCE_PROXIMITY_UNCERTAIN);
			if (item_data->client_status ==	GEOFENCE_CLIENT_STATUS_START) {
				item_data->client_status = GEOFENCE_CLIENT_STATUS_RUNNING;
			}
			LOGD_GEOFENCE("%d : FENCE_OUT", fence_id);
#ifdef TIZEN_ENGINEER_MODE
			GEOFENCE_PRINT_LOG("FENCE_OUT");
#endif
		} else {
			LOGD_GEOFENCE("Fence status [%d]", item_data->common_info.status);
		}
		item_data->common_info.status = GEOFENCE_FENCE_STATE_OUT;
	} else {
		LOGD_GEOFENCE("Not emit, Prev[%d], Curr[%d]", item_data->common_info.status, state);
	}

	return ret;
}

static int __emit_fence_proximity(GeofenceServer *geofence_server, int fence_id, geofence_proximity_state_e state)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);
	geofence_proximity_provider_e provider = GEOFENCE_PROXIMITY_PROVIDER_LOCATION;
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data) {
		if (item_data->common_info.type == GEOFENCE_TYPE_WIFI) {
			provider = GEOFENCE_PROXIMITY_PROVIDER_WIFI;
		} else if (item_data->common_info.type == GEOFENCE_TYPE_BT) {
			provider = GEOFENCE_PROXIMITY_PROVIDER_BLUETOOTH;
		}
		geofence_dbus_server_send_geofence_proximity_changed(geofence_server->geofence_dbus_server, item_data->common_info.appid, fence_id, item_data->common_info.access_type, state, provider);
		item_data->common_info.proximity_status = state;
	} else {
		LOGD_GEOFENCE("Invalid item_data");
		return -1;
	}
	return 0;
}

static void __check_proximity_for_fence(double distance, int fence_id, int radius, geofence_proximity_state_e current_state, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	int ret = BT_ERROR_NONE;
	geofence_proximity_state_e state = GEOFENCE_PROXIMITY_UNCERTAIN;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	if (distance <= 50.0) {
		state = GEOFENCE_PROXIMITY_IMMEDIATE;
	} else if (distance > 50.0 && distance <= 100.0) {
		state = GEOFENCE_PROXIMITY_NEAR;
	} else if (distance > 100.0 && distance <= radius) {
		state = GEOFENCE_PROXIMITY_FAR;
	}
	if (current_state != state) {
		LOGD_GEOFENCE("PROXIMITY ALERTING for fence: %d, alert: %d, distance: %f", fence_id, state, distance);
		__emit_fence_proximity(geofence_server, fence_id, state);
		if (geofence_server->nearestTrackingFence == fence_id) {
			if (state == GEOFENCE_PROXIMITY_IMMEDIATE) {
				LOGD_GEOFENCE("Scanning for BLE and storing in DB");
				g_stpcpy(geofence_server->ble_info, "");
				ret = bt_adapter_le_start_scan(bt_le_scan_result_cb, geofence_server);
				if (ret != BT_ERROR_NONE) {
					LOGE_GEOFENCE("Fail to start ble scan. %d", ret);
				}
			} else if (current_state == GEOFENCE_PROXIMITY_IMMEDIATE) { /* Stopping the scan if state changes */
				ret = bt_adapter_le_stop_scan();
				if (ret != BT_ERROR_NONE)
					LOGE_GEOFENCE("Unable to stop the BLE scan/ Stopped already, error: %d", ret);
			}
		}
	}
}

static void __check_inout_by_gps(double latitude, double longitude, int fence_id, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	double distance = 0.0;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (!item_data || item_data->client_status == GEOFENCE_CLIENT_STATUS_NONE)
		return;

	geofence_fence_state_e status = GEOFENCE_FENCE_STATE_OUT;
	geocoordinate_info_s *geocoordinate_info = (geocoordinate_info_s *)item_data->priv;
	/* get_current_position/ check_fence_in/out  for geoPoint */
	location_manager_get_distance(latitude, longitude, geocoordinate_info->latitude, geocoordinate_info->longitude,	&distance);

	if (distance >= geocoordinate_info->radius) {
		LOGD_GEOFENCE("FENCE_OUT : based on distance. Distance[%f] for fence id: %d at (%f, %f)", distance, fence_id, latitude, longitude);
		status = GEOFENCE_FENCE_STATE_OUT;
	} else {
		LOGD_GEOFENCE("FENCE_IN : based on distance. Distance[%f] for fence id: %d at (%f, %f)", distance, fence_id, latitude, longitude);
		status = GEOFENCE_FENCE_STATE_IN;
	}

	/* Alert for the proximity */
	/*if (status == GEOFENCE_FENCE_STATE_IN) {*/
		__check_proximity_for_fence(distance, item_data->common_info.fence_id, geocoordinate_info->radius, item_data->common_info.proximity_status, geofence_server);
	/*}*/

	if (__emit_fence_inout(geofence_server, item_data->common_info.fence_id, status) == 0 && status == GEOFENCE_FENCE_STATE_IN) {
		LOGD_GEOFENCE("Disable timer");
	}
}

static void __stop_gps_alarms(void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	/*Stop the gps interval alarm if it is running...*/
	if (geofence_server->gps_alarm_id != -1) {
		/*LOGI_GEOFENCE("GPS interval timer removed. ID[%d]", geofence_server->gps_alarm_id);*/
		geofence_server->gps_alarm_id = _geofence_remove_alarm(geofence_server->gps_alarm_id);
	}
	/*Stop the timeout alarm if it is running...*/
	if (geofence_server->gps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("Timeout timer removed for gps. ID[%d]", geofence_server->gps_timeout_alarm_id);*/
		geofence_server->gps_timeout_alarm_id = _geofence_remove_alarm(geofence_server->gps_timeout_alarm_id);
	}
}

static void __stop_wps_alarms(void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	/*Stop the wps interval alarm if it is running...*/
	if (geofence_server->wps_alarm_id != -1) {
		/*LOGI_GEOFENCE("WPS interval timer removed. ID[%d]", geofence_server->wps_alarm_id);*/
		geofence_server->wps_alarm_id = _geofence_remove_alarm(geofence_server->wps_alarm_id);
	}
	/*Stop the timeout alarm if it is running...*/
	if (geofence_server->wps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("Timeout timer removed for wps. ID[%d]", geofence_server->wps_timeout_alarm_id);*/
		geofence_server->wps_timeout_alarm_id = _geofence_remove_alarm(geofence_server->wps_timeout_alarm_id);
	}
}

static void __check_current_location_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	int fence_id = 0;
	GList *tracking_list = NULL;
	GeofenceItemData *item_data = NULL;

	LOGD_GEOFENCE("Traversing the tracking list");
	tracking_list = g_list_first(geofence_server->tracking_list);
	LOGD_GEOFENCE("Got the first element in tracking list");

	while (tracking_list) {
		fence_id = GPOINTER_TO_INT(tracking_list->data);
		item_data = __get_item_by_fence_id(fence_id, geofence_server);

		if (item_data != NULL) {
			if (item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
				LOGD_GEOFENCE("TRACKING FENCE ID :: %d", fence_id);
				__check_inout_by_gps(latitude, longitude, fence_id, geofence_server);
			}
		}
		tracking_list = g_list_next(tracking_list);
	}
}

static int __start_wps_positioning(GeofenceServer *geofence_server, location_position_updated_cb callback)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);
	int ret = FENCE_ERR_NONE;

	if (geofence_server->loc_wps_manager == NULL) {
		ret = location_manager_create(LOCATIONS_METHOD_WPS, &geofence_server->loc_wps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to create location_manager_h for wps: %d", ret);
			return FENCE_ERR_UNKNOWN;
		}
	}
	if (geofence_server->loc_wps_started == FALSE) {
		ret = location_manager_set_position_updated_cb(geofence_server->loc_wps_manager, callback, 1, (void *) geofence_server);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to set callback for wps. %d", ret);
			return FENCE_ERR_UNKNOWN;
		}
		ret = location_manager_start(geofence_server->loc_wps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to start. %d", ret);
			location_manager_unset_position_updated_cb(geofence_server->loc_wps_manager);
			location_manager_destroy(geofence_server->loc_wps_manager);
			geofence_server->loc_wps_manager = NULL;
			return FENCE_ERR_UNKNOWN;
		}
		if (geofence_server->wps_timeout_alarm_id == -1)
			geofence_server->wps_timeout_alarm_id =	_geofence_add_alarm(WPS_TIMEOUT, __wps_timeout_cb, geofence_server);

		geofence_server->loc_wps_started = TRUE;
	} else {
		LOGD_GEOFENCE("loc_wps_started TRUE");
	}

	return ret;
}

static int __start_gps_positioning(GeofenceServer *geofence_server, location_position_updated_cb callback)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);
	int ret = FENCE_ERR_NONE;

	if (geofence_server->loc_gps_manager == NULL) {
		ret = location_manager_create(LOCATIONS_METHOD_GPS, &geofence_server->loc_gps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to create location_manager_h: %d", ret);
			return FENCE_ERR_UNKNOWN;
		}
	}

	if (geofence_server->loc_gps_started == FALSE) {
		ret = location_manager_set_position_updated_cb(geofence_server->loc_gps_manager, callback, geofence_server->gps_trigger_interval, (void *) geofence_server);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to set callback. %d", ret);
			return FENCE_ERR_UNKNOWN;
		}

		ret = location_manager_start(geofence_server->loc_gps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to start. %d", ret);
			location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
			location_manager_destroy(geofence_server->loc_gps_manager);
			geofence_server->loc_gps_manager = NULL;
			return FENCE_ERR_UNKNOWN;
		}
		if (geofence_server->gps_timeout_alarm_id == -1)
			geofence_server->gps_timeout_alarm_id =	_geofence_add_alarm(GPS_TIMEOUT, __gps_timeout_cb, geofence_server);

		geofence_server->loc_gps_started = TRUE;
	} else {
		LOGD_GEOFENCE("loc_gps_started TRUE");
	}

	return ret;
}

static void __stop_gps_positioning(gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	int ret = 0;
	if (geofence_server->loc_gps_started == TRUE) {
		ret = location_manager_stop(geofence_server->loc_gps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
		geofence_server->loc_gps_started = FALSE;
		ret = location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
	}

	if (geofence_server->loc_gps_manager != NULL) {
		ret = location_manager_destroy(geofence_server->loc_gps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
		geofence_server->loc_gps_manager = NULL;
	}
}

static void __stop_wps_positioning(gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	int ret = 0;
	if (geofence_server->loc_wps_started == TRUE) {
		ret = location_manager_stop(geofence_server->loc_wps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGI_GEOFENCE("Unable to stop the wps");
			return;
		}
		geofence_server->loc_wps_started = FALSE;
		ret = location_manager_unset_position_updated_cb(geofence_server->loc_wps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGI_GEOFENCE("Unable to unset the callback");
			return;
		}
	}

	if (geofence_server->loc_wps_manager != NULL) {
		ret = location_manager_destroy(geofence_server->loc_wps_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGI_GEOFENCE("Unable to destroy the wps manager");
			return;
		}
		geofence_server->loc_wps_manager = NULL;
	}
}

static int __get_time_diff(int timestamp)
{
	int current_time = 0;
	int timediff = 0;
	current_time = (g_get_real_time()/1000000);
	timediff = current_time - timestamp;
	return timediff;
}

static void __process_best_location(GeofenceServer *geofence_server)
{
	FUNC_ENTRANCE_SERVER;

	int gpsdiff = 0;
	int wpsdiff = 0;
	
	/* Check if any of the fix is null just return. It doesn't make sense to compare if only one fix is available*/
	if (geofence_server->gps_fix_info == NULL || geofence_server->wps_fix_info == NULL)
		return;

	/*Calculate the time difference*/
	gpsdiff = __get_time_diff(geofence_server->gps_fix_info->timestamp);
	wpsdiff = __get_time_diff(geofence_server->wps_fix_info->timestamp);

	if (gpsdiff < wpsdiff) {
		if ((geofence_server->gps_fix_info->timestamp - geofence_server->wps_fix_info->timestamp) <= 20) {
			if (geofence_server->gps_fix_info->accuracy <= geofence_server->wps_fix_info->accuracy) {
				LOGI_GEOFENCE("Using GPS fix");
				__check_current_location_cb(geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, 0.0, geofence_server->gps_fix_info->timestamp, geofence_server);
			} else {
				LOGI_GEOFENCE("Using WPS fix");
				__check_current_location_cb(geofence_server->wps_fix_info->latitude, geofence_server->wps_fix_info->longitude, 0.0, geofence_server->wps_fix_info->timestamp, geofence_server);
			}
		} else {
			LOGI_GEOFENCE("Time diff is more. So using latest GPS fix");
			__check_current_location_cb(geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, 0.0, geofence_server->gps_fix_info->timestamp, geofence_server);
		}
	} else {
		if ((geofence_server->wps_fix_info->timestamp - geofence_server->gps_fix_info->timestamp) <= 20) {
			if (geofence_server->wps_fix_info->accuracy <= geofence_server->gps_fix_info->accuracy) {
				LOGI_GEOFENCE("Using WPS fix");
				__check_current_location_cb(geofence_server->wps_fix_info->latitude, geofence_server->wps_fix_info->longitude, 0.0, geofence_server->wps_fix_info->timestamp, geofence_server);
			} else {
				LOGI_GEOFENCE("Using GPS fix");
				__check_current_location_cb(geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, 0.0, geofence_server->gps_fix_info->timestamp, geofence_server);
			}
		} else {
			LOGI_GEOFENCE("Time diff is more. So using latest WPS fix");
			__check_current_location_cb(geofence_server->wps_fix_info->latitude, geofence_server->wps_fix_info->longitude, 0.0, geofence_server->wps_fix_info->timestamp, geofence_server);
		}
	}
}

static void __geofence_standalone_gps_position_changed_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	double distance = 0;
	int interval = 0;
	int min_fence_id = 0;
	double hor_acc = 0.0;
	double ver_acc = 0.0;
	location_accuracy_level_e level;
	int ret = LOCATIONS_ERROR_NONE;

	/* Allocate memory for the location_info structure */
	if (geofence_server->gps_fix_info == NULL) {
		geofence_server->gps_fix_info = (location_fix_info_s *)g_malloc(sizeof(location_fix_info_s));
	}
	/* Store the location information in the structure for future use*/
	if (geofence_server->gps_fix_info != NULL) {
		geofence_server->gps_fix_info->latitude = latitude;
		geofence_server->gps_fix_info->longitude = longitude;
		ret = location_manager_get_accuracy(geofence_server->loc_gps_manager, &level, &hor_acc, &ver_acc);
		if (ret == LOCATIONS_ERROR_NONE) {
			LOGI_GEOFENCE("hor_acc:%f, ver_acc:%f", hor_acc, ver_acc);
			LOGD_GEOFENCE("*****%f, %f********", latitude, longitude);
			geofence_server->gps_fix_info->accuracy = hor_acc;
		}
	}
	/*Remove the timeout callback that might be running when requesting for fix.*/
	if (geofence_server->gps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("Removing the timeout alarm from restart gps");*/
		geofence_server->gps_timeout_alarm_id =	_geofence_remove_alarm(geofence_server->gps_timeout_alarm_id);
	}
	geofence_server->last_loc_time = timestamp;
	__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data);

	/* Distance based alarm */
	distance = _get_min_distance(latitude, longitude, &min_fence_id, geofence_server);
	geofence_server->nearestTrackingFence = min_fence_id;

	if (distance < 200) {
		LOGD_GEOFENCE("interval: 1 secs");
		interval = 1;
	} else if (distance < 500) {
		LOGD_GEOFENCE("interval: 3 secs");
		interval = 3;
	} else if (distance < 1000) {
		LOGD_GEOFENCE("interval: 6 secs");
		interval = 6;
	} else if (distance < 2000) {
		LOGD_GEOFENCE("interval: 20 secs");
		interval = 20;
	} else if (distance < 3000) {
		LOGD_GEOFENCE("interval : 1 min");
		interval = 1 * 60;
	} else if (distance < 5000) {
		LOGD_GEOFENCE("interval: 2 mins");
		interval = 2 * 60;
	} else if (distance < 10000) {
		LOGD_GEOFENCE("interval: 5 mins");
		interval = 5 * 60;
	} else if (distance < 20000) {
		LOGD_GEOFENCE("interval : 10 mins");
		interval = 10 * 60;
	} else if (distance < 100000) {
		LOGD_GEOFENCE("interval : 20 mins");
		interval = 20 * 60;
	} else {
		LOGD_GEOFENCE("interval : 60 mins");
		interval = 60 * 60;
	}

	/* remove the activity value when 10 hours later */
	if (geofence_server->last_loc_time - geofence_server->activity_timestamp > 10 * 60 * 60)
		geofence_server->activity_type = ACTIVITY_IN_VEHICLE;

	if (geofence_server->activity_type == ACTIVITY_STATIONARY)
		interval = interval * 10;
	else if (geofence_server->activity_type == ACTIVITY_WALK)
		interval = interval * 5;
	else if (geofence_server->activity_type == ACTIVITY_RUN)
		interval = interval * 3;
	LOGD_GEOFENCE("Unsetting the position_updated_cb");
	location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
	location_manager_stop(geofence_server->loc_gps_manager);
	geofence_server->loc_gps_started = FALSE;

	LOGI_GEOFENCE("Setting the gps interval of alrm %d s", interval);
	if (geofence_server->gps_alarm_id == -1) {
		LOGI_GEOFENCE("Setting the gps alarm from the callback");
		geofence_server->gps_alarm_id =	_geofence_add_alarm(interval, __gps_alarm_cb, geofence_server);
	}
}

static void __geofence_gps_position_changed_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	double hor_acc = 0.0;
	double ver_acc = 0.0;
	GeofenceItemData *item_data = NULL;
	int min_fence_id = -1;
	int min_distance = 0;
	location_accuracy_level_e level;
	int ret = LOCATIONS_ERROR_NONE;

	/*Remove the timeout callback that might be running when requesting for fix.*/
	if (geofence_server->gps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("Removing the timeout alarm from restart gps");*/
		geofence_server->gps_timeout_alarm_id =	_geofence_remove_alarm(geofence_server->gps_timeout_alarm_id);
	}

	/* Allocate memory for the location_info structure */
	if (geofence_server->gps_fix_info == NULL) {
		geofence_server->gps_fix_info = (location_fix_info_s *)g_malloc(sizeof(location_fix_info_s));
	}

	/* Store the location information in the structure for future use*/
	if (geofence_server->gps_fix_info != NULL) {
		geofence_server->gps_fix_info->latitude = latitude;
		geofence_server->gps_fix_info->longitude = longitude;
		geofence_server->gps_fix_info->timestamp = (g_get_real_time()/1000000); /* microsecs->millisecs->secs */
		ret = location_manager_get_accuracy(geofence_server->loc_gps_manager, &level, &hor_acc, &ver_acc);
		if (ret == LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("hor_acc:%f, ver_acc:%f", hor_acc, ver_acc);
			LOGD_GEOFENCE("*****%f, %f********", latitude, longitude);
			geofence_server->gps_fix_info->accuracy = hor_acc;
		}
	} else {
		LOGD_GEOFENCE("Invalid GPS fix data");
		return;
        }
	geofence_server->last_loc_time = timestamp;

	if (geofence_server->wps_fix_info && __get_time_diff(geofence_server->wps_fix_info->timestamp) <= 20 && geofence_server->gps_fix_info->accuracy <= 50.0) {
		LOGI_GEOFENCE("Going for fix comparison from gps fix");
		__process_best_location(geofence_server);
		/* Using GPS fix from this point. So stop WPS alarms which trigger next WPS request session */
		__stop_wps_alarms(geofence_server);
	} else if (geofence_server->gps_fix_info->accuracy <= 50.0) {
		LOGI_GEOFENCE("Emitting from GPS fix directly");
		__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data);
		/* Using GPS fix from point. So stop WPS alarms which trigger next WPS request session */
		__stop_wps_alarms(geofence_server);
	}

	location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
	location_manager_stop(geofence_server->loc_gps_manager);
	geofence_server->loc_gps_started = FALSE;

	/*Get minimum distance and fence_id of the nearest tracking fence*/
	if (geofence_server->gps_fix_info) {
		min_distance = _get_min_distance(geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, &min_fence_id, geofence_server);
		geofence_server->nearestTrackingFence = min_fence_id; /*This has to be updated frequently*/
		item_data = __get_item_by_fence_id(min_fence_id, geofence_server);
		if (item_data && geofence_server->loc_gps_started_by_wps == TRUE) {
			LOGI_GEOFENCE("******Setting the GPS interval******");
			__set_interval_for_gps(min_distance, min_fence_id, user_data);
		}
	}
}

static void __geofence_wps_position_changed_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	GeofenceItemData *item_data = NULL;
	double min_distance = 0.0;
	int min_fence_id = 0;
	double hor_acc = 0.0;
	double ver_acc = 0.0;
	int gps_state = 0;
	location_accuracy_level_e level;
	int ret = LOCATIONS_ERROR_NONE;
	int interval = 0;

	/* Allocate memory for the location_info structure */
	if (geofence_server->wps_fix_info == NULL) {
		geofence_server->wps_fix_info = (location_fix_info_s *)g_malloc(sizeof(location_fix_info_s));
	}
	 /*Remove the timeout callback that might be running when requesting for fix.*/
	if (geofence_server->wps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("Removing the timeout alarm from restart gps");*/
		geofence_server->wps_timeout_alarm_id = _geofence_remove_alarm(geofence_server->wps_timeout_alarm_id);
	}
	/* Store the location information in the structure for future use*/
	if (geofence_server->wps_fix_info != NULL) {
		geofence_server->wps_fix_info->latitude = latitude;
		geofence_server->wps_fix_info->longitude = longitude;
		geofence_server->wps_fix_info->timestamp = (g_get_real_time()/1000000); /* microsecs->millisecs->secs */
		ret = location_manager_get_accuracy(geofence_server->loc_wps_manager, &level, &hor_acc, &ver_acc);
		if (ret == LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("hor_acc:%f, ver_acc:%f", hor_acc, ver_acc);
			LOGD_GEOFENCE("*****%f, %f********", latitude, longitude);
			geofence_server->wps_fix_info->accuracy = hor_acc;
		}
	} else {
		LOGD_GEOFENCE("Invalid WPS fix data");
		return;
	}

	/*Get minimum distance and fence_id of the nearest tracking fence*/
	min_distance = _get_min_distance(latitude, longitude, &min_fence_id, geofence_server);
	LOGI_GEOFENCE("Nearest fence id: %d, distance: %f", min_fence_id, min_distance);
	geofence_server->nearestTrackingFence = min_fence_id;/* This has to be updated frequently*/

	item_data = __get_item_by_fence_id(min_fence_id, geofence_server);

	if (!item_data)
		return;/* There is no valid fence with this fence id. So return*/

	geocoordinate_info_s *geocoordinate_info = (geocoordinate_info_s *)item_data->priv;
		
	double interval_dist = (min_distance - geocoordinate_info->radius) - geofence_server->wps_fix_info->accuracy;
	LOGI_GEOFENCE("Distance for interval: %f", interval_dist);
	if (interval_dist < 15000) {
		interval = interval_dist/25; /*secs*/ /*Assuming 90 km/hr of speed - So 25 mtrs covered in 1 sec*/
	} else if (interval_dist >= 15000 && interval_dist < 18000) {
		interval = 10 * 60; /* 10 mins */
	} else if (interval_dist >= 18000 && interval_dist < 20000) {
		interval = 12 * 60; /* 12 mins */
	} else if (interval_dist >= 20000) {
		interval = 15 * 60; /*15 mins*/
	}
	if (interval < 15)
		interval = 15; /*15 sec */

	location_manager_unset_position_updated_cb(geofence_server->loc_wps_manager);
	location_manager_stop(geofence_server->loc_wps_manager);
	geofence_server->loc_wps_started = FALSE;

	LOGI_GEOFENCE("Setting the wps interval of %d secs", interval);
	if (geofence_server->wps_alarm_id == -1) {
		LOGI_GEOFENCE("Setting the wps alarm from the callback");
		geofence_server->wps_alarm_id = _geofence_add_alarm(interval, __wps_alarm_cb, geofence_server);
	}

	/* Get the GPS state here */
	vconf_get_int(VCONFKEY_LOCATION_ENABLED, &gps_state);
	if (gps_state == 1) {
		if (geofence_server->wps_fix_info->accuracy <= 100.0 && geofence_server->loc_gps_started_by_wps == false) {/*This works when GPS is not running or GPS timeout happens*/
			__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data);
		} else if (item_data->common_info.status == GEOFENCE_FENCE_STATE_UNCERTAIN) {
			__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data);
		}
		if (geofence_server->loc_gps_started_by_wps == FALSE && geofence_server->loc_gps_started == FALSE) {
			if (min_distance <= (geocoordinate_info->radius + GPS_TRIGGER_BOUNDARY)) {
				LOGD_GEOFENCE("Triggering GPS");
				/*LOGD_GEOFENCE("(GPS TRIGGER) GPS started at lat:%f, lon:%f for fence_id:%d at distance:%f", latitude, longitude, min_fence_id, min_distance);*/
				if (FENCE_ERR_NONE == __start_gps_positioning(geofence_server, __geofence_gps_position_changed_cb))
					geofence_server->loc_gps_started_by_wps = true;
				else
					LOGI_GEOFENCE("Error starting GPS/ GPS is off");
			}
		}
	} else
		__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data); /* Its WPS only mode so no need to worry abt accuracy */
}

static void __set_interval_for_gps(double min_distance, int min_fence_id, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	GeofenceItemData *item_data = NULL;
	bool isSwitched = false;
	item_data = __get_item_by_fence_id(min_fence_id, geofence_server);
	if (item_data && geofence_server->gps_fix_info) {
		geocoordinate_info_s *geocoordinate_info = (geocoordinate_info_s *)item_data->priv;
		if (geofence_server->gps_trigger_interval == 1 && (min_distance > (geocoordinate_info->radius + 100 + geofence_server->gps_fix_info->accuracy) && min_distance <= (geocoordinate_info->radius + 1000))) {
			isSwitched = true;
			LOGI_GEOFENCE("Setting the GPS interval as 5 secs");
			geofence_server->gps_trigger_interval = 5;
			/*LOGI_GEOFENCE("(GPS SWITCH) GPS changed from 1 to 5 sec at lat:%f, lon:%f for fence_id:%d at distance:%f", geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, min_fence_id, min_distance);*/
		} else if (geofence_server->gps_trigger_interval == 5 && min_distance <= (geocoordinate_info->radius + 100 + geofence_server->gps_fix_info->accuracy)) {
			isSwitched = true;
			LOGI_GEOFENCE("Setting the GPS interval as 1 secs");
			geofence_server->gps_trigger_interval = 1;
			/*LOGI_GEOFENCE("(GPS SWITCH) GPS changed from 5 to 1 sec at lat:%f, lon:%f for fence_id:%d at distance:%f", geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, min_fence_id, min_distance);*/
		} else if (min_distance > (geocoordinate_info->radius + 1000)) {
			/* Already stopped. Just that GPS trigger alarm wont be scheduled again */
			/*LOGI_GEOFENCE("(GPS TRIGGER) GPS stopped at lat:%f, lon:%f for fence:%d at distance:%f", geofence_server->gps_fix_info->latitude, geofence_server->gps_fix_info->longitude, min_fence_id, min_distance);*/
			geofence_server->loc_gps_started_by_wps = false;
			/*No need of GPS. So stop GPS and start the WPS from here*/
			location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
			location_manager_stop(geofence_server->loc_gps_manager);
			geofence_server->loc_gps_started = FALSE;
			__start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);/* Stopping the GPS here. So start using wps */
		}
		if ((geofence_server->loc_gps_started_by_wps == true && isSwitched == true) || geofence_server->gps_trigger_interval > 1) {
			LOGI_GEOFENCE("Setting the gps interval of %d secs during wps session", geofence_server->gps_trigger_interval);
			if (geofence_server->gps_alarm_id == -1) {
				/*Switching the interval for GPS. So stop and start using alarm*/
				location_manager_unset_position_updated_cb(geofence_server->loc_gps_manager);
				location_manager_stop(geofence_server->loc_gps_manager);
				geofence_server->loc_gps_started = FALSE;
				LOGI_GEOFENCE("Setting the gps alarm from the callback");
				geofence_server->gps_alarm_id = _geofence_add_alarm(geofence_server->gps_trigger_interval, __gps_alarm_cb, geofence_server);
			}
		}
	}
}

void bt_adapter_device_discovery_state_cb(int result, bt_adapter_device_discovery_state_e discovery_state, bt_adapter_device_discovery_info_s *discovery_info, void *user_data)
{
#if 0
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	GeofenceItemData *item_data = NULL;
	int i;
	int tracking_fence_id = 0;
	GList *tracking_fences = g_list_first(geofence_server->tracking_list);

	if (discovery_state != BT_ADAPTER_DEVICE_DISCOVERY_FOUND) {
		LOGI_GEOFENCE("BREDR discovery %s", discovery_state == BT_ADAPTER_DEVICE_DISCOVERY_STARTED ? "Started" : "Finished");
		/* Check only if some BT fence is running */
		if (discovery_state == BT_ADAPTER_DEVICE_DISCOVERY_FINISHED && geofence_server->running_bt_cnt > 0) {
			LOGI_GEOFENCE("Comparison for BT is done. Now emit the status...");
			while (tracking_fences) {
				tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
				tracking_fences = g_list_next(tracking_fences);
				item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
				if (item_data && item_data->common_info.type == GEOFENCE_TYPE_BT) {
					if (item_data->is_bt_status_in == true) {
						__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_IN);
					} else {
						__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_OUT);
					}
					item_data->is_bt_status_in = false;
				}
			}
		}
	} else {
		LOGI_GEOFENCE("%s, %s", discovery_info->remote_address, discovery_info->remote_name);
		LOGI_GEOFENCE("rssi: %d is_bonded: %d", discovery_info->rssi, discovery_info->is_bonded);

		if (geofence_server->running_bt_cnt > 0) {
			for (i = 0; i < discovery_info->service_count; i++) {
				LOGI_GEOFENCE("uuid: %s", discovery_info->service_uuid[i]);
			}
			LOGI_GEOFENCE("Tracking list is being checked for the BT geofence");
			__check_tracking_list(discovery_info->remote_address, geofence_server, GEOFENCE_TYPE_BT);
		}
	}
#endif
}

static void geofence_network_evt_cb(net_event_info_t *event_cb, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	int ret = -1;
	int wps_state = 0;
	int gps_state = 0;

	switch (event_cb->Event) {
	case NET_EVENT_WIFI_POWER_IND:
		LOGI_GEOFENCE("WIFI ON/OFF indication");
		vconf_get_int(VCONFKEY_LOCATION_NETWORK_ENABLED, &wps_state);
		vconf_get_int(VCONFKEY_LOCATION_ENABLED, &gps_state);
		if (__is_support_wps() == true && geofence_server->running_geopoint_cnt > 0) {
			if (__isWifiOn() == false) {
				LOGI_GEOFENCE("WIFI is OFF");
				/* In Tizen device(Kiran) WPS is not supported if WIFI is switched off */
				__stop_wps_positioning(geofence_server);
				__stop_wps_alarms(geofence_server);
				if (geofence_server->loc_gps_started_by_wps == true) {
					__stop_gps_positioning(geofence_server); /*Stop the gps if it was started by wps*/
					__stop_gps_alarms(geofence_server);
					geofence_server->loc_gps_started_by_wps = false;
				}
				if (gps_state == 1) {
					ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
					if (ret != FENCE_ERR_NONE) {
						LOGE_GEOFENCE("Fail to start standalone gps positioning. Error[%d]", ret);
					}
				}
			} else {
				if (__isDataConnected() == true) {/*&& wps_state == 1) {*/
					LOGI_GEOFENCE("DATA CONNECTION IS TRUE");
					if (wps_state == 1) {
						LOGI_GEOFENCE("WPS STATE IS 1");
						__stop_gps_positioning(geofence_server); /* Stop the gps which is running as wps can be used*/
						__stop_gps_alarms(geofence_server);
						/**** Start the WPS as mobile data is connected and wifi and wps are on *******/
						ret = __start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);
						if (ret != FENCE_ERR_NONE) {
							LOGE_GEOFENCE("Fail to start wps positioning. Error[%d]", ret);
						}
					}
				}
			}
		} else {
			LOGE_GEOFENCE("WPS is not supported");
		}
		break;
	case NET_EVENT_OPEN_IND:
		LOGI_GEOFENCE("Mobile internet connected");
		vconf_get_int(VCONFKEY_LOCATION_NETWORK_ENABLED, &wps_state);
		if (__is_support_wps() == true && geofence_server->running_geopoint_cnt > 0 && wps_state == 1 && __isWifiOn() == true && __isDataConnected() == true) {
			/**** Start the WPS as mobile data is connected and wifi is on *******/
			if (geofence_server->loc_gps_started_by_wps == false && geofence_server->loc_gps_started == true) {
				__stop_gps_positioning(geofence_server); /*GPS should be stopped only if it is running standalone*/
				__stop_gps_alarms(geofence_server);
			}	
			ret = __start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);
			if (ret != FENCE_ERR_NONE) {
				LOGE_GEOFENCE("Fail to start wps positioning. Error[%d]", ret);
			}
		}
		break;
	case NET_EVENT_CLOSE_IND:
		LOGI_GEOFENCE("Mobile internet disconnected");
		if (__is_support_wps() == true && geofence_server->running_geopoint_cnt > 0 && geofence_server->loc_wps_started == true) {
			/***** Start standalone gps as mobile data is disconnected *****/
			__stop_wps_positioning(geofence_server);
			__stop_wps_alarms(geofence_server);
			if (geofence_server->loc_gps_started_by_wps == true) {
				__stop_gps_positioning(geofence_server); /*Stop the gps if it was started by wps*/
				__stop_gps_alarms(geofence_server);
				geofence_server->loc_gps_started_by_wps = false;
			}
			ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
			if (ret != FENCE_ERR_NONE) {
				LOGE_GEOFENCE("Fail to start standalone gps positioning. Error[%d]", ret);
			}
		}
		break;
	default:
		break;
	}
}

static int __gps_timeout_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__gps_timeout_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("alarm_id : %d", alarm_id);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	geofence_server->gps_timeout_alarm_id = -1;	/*resetting the alarm id*/
	/*Stop the gps for sometime when there is no fix*/
	__stop_gps_positioning(geofence_server);
	if (geofence_server->loc_gps_started_by_wps == FALSE) {
		geofence_server->gps_alarm_id = _geofence_add_alarm(1 * 60, __gps_alarm_cb, geofence_server);
	} else {
		geofence_server->loc_gps_started_by_wps = FALSE;
	}
	return 0;
}

static int __gps_alarm_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__gps_alarm_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("gps alarm_id : %d", alarm_id);
	int ret = FENCE_ERR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	if (geofence_server->gps_alarm_id != -1) {
		/*LOGI_GEOFENCE("GPS interval timer removed. ID[%d]", geofence_server->gps_alarm_id);*/
		geofence_server->gps_alarm_id = _geofence_remove_alarm(geofence_server->gps_alarm_id);
		geofence_server->gps_alarm_id = -1;
	}
	if (geofence_server->loc_gps_started_by_wps == true) {
		ret = __start_gps_positioning(geofence_server, __geofence_gps_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
		}
	} else {
		ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start standalone gps positioning. Error[%d]", ret);
		}
	}
	return 0;
}

static int __wps_timeout_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__wps_timeout_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("alarm_id : %d", alarm_id);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	if (geofence_server->wps_timeout_alarm_id != -1) {
		/*LOGI_GEOFENCE("WPS timeout timer removed. ID[%d]", geofence_server->wps_timeout_alarm_id);*/
		geofence_server->wps_timeout_alarm_id = _geofence_remove_alarm(geofence_server->wps_timeout_alarm_id);
		geofence_server->wps_timeout_alarm_id = -1;     /*resetting the alarm id*/
	}
	/*Stop the wps for sometime when there is no fix*/
	__stop_wps_positioning(geofence_server);
	geofence_server->wps_alarm_id = _geofence_add_alarm(10, __wps_alarm_cb, geofence_server);
	/*display_unlock_state(LCD_OFF, PM_RESET_TIMER);*/
	return 0;
}

static int __wps_alarm_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__wps_alarm_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("wps alarm_id : %d", alarm_id);
	int ret = FENCE_ERR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	if (geofence_server->wps_alarm_id != -1) {
		/*LOGI_GEOFENCE("WPS interval timer removed. ID[%d]", geofence_server->wps_alarm_id);*/
		geofence_server->wps_alarm_id = _geofence_remove_alarm(geofence_server->wps_alarm_id);
		geofence_server->wps_alarm_id = -1;
	}
	if (__is_support_wps() == true && __isWifiOn() == true && __isDataConnected() == true) {
		ret = __start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start wps positioning. Error[%d]", ret);
		}
	} else {
		ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start standalone gps positioning. Error[%d]", ret);
		}
	}
	return 0;
}

static void gps_setting_changed_cb(location_method_e method, bool enable,
                                   void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	GList *tracking_fences = g_list_first(geofence_server->tracking_list);
	GeofenceItemData *item_data = NULL;
	int tracking_fence_id = 0;
	int ret = FENCE_ERR_NONE;
	int wps_state = 0;
	int gps_state = 0;
	/* Get the wps status */
	vconf_get_int(VCONFKEY_LOCATION_NETWORK_ENABLED, &wps_state);
	vconf_get_int(VCONFKEY_LOCATION_ENABLED, &gps_state);

	if (enable == false && geofence_server->running_geopoint_cnt > 0) {
		if (method == LOCATIONS_METHOD_GPS) {
			LOGI_GEOFENCE("Stopping the GPS from settings callback");
			__stop_gps_positioning(geofence_server);
			__stop_gps_alarms(geofence_server);

			if (wps_state == 0) { /* If data is connected then WPS will be running and alerts will be given through WPS*/
				while (tracking_fences) {
					tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
					tracking_fences = g_list_next(tracking_fences);
					item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
					if (item_data && item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
						__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_OUT);
						item_data->common_info.proximity_status = GEOFENCE_PROXIMITY_UNCERTAIN;
					}
				}
			}
		} else if (method == LOCATIONS_METHOD_WPS) {
			LOGI_GEOFENCE("Stopping the WPS from settings callback");
			__stop_wps_positioning(geofence_server);
			__stop_wps_alarms(geofence_server);

			if (gps_state == 0) { /* If data is connected then WPS will be running and alerts will be given through WPS*/
				while (tracking_fences) {
					tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
					tracking_fences = g_list_next(tracking_fences);
					item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
					if (item_data && item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
						__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_OUT);
						item_data->common_info.proximity_status = GEOFENCE_PROXIMITY_UNCERTAIN;
					}
				}
				return;
			}
			/* stop the gps if it was started by WPS */
			if (geofence_server->loc_gps_started_by_wps == true) {
				__stop_gps_positioning(geofence_server);
				__stop_gps_alarms(geofence_server);
				geofence_server->loc_gps_started_by_wps = false; /*So that WPS will use GPS if needed in its next fix(wps fix)*/
			}
			if (geofence_server->loc_gps_started == false && gps_state == 1) {/*As WPS is turned off standalone GPS should be used for tracking the fence*/
				ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
				if (ret != FENCE_ERR_NONE) {
					LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
					return;
				}
			}
		}
		if (geofence_server->loc_gps_started_by_wps == true) {
			geofence_server->loc_gps_started_by_wps = false; /*So that WPS will use GPS if needed in its next fix(wps fix)*/
		}
	} else if (enable == true && geofence_server->running_geopoint_cnt > 0) {
		if (method == LOCATIONS_METHOD_GPS) {
			geofence_server->loc_gps_started_by_wps = false; /* So that WPS will use GPS if needed in its next fix(wps fix) */
			if (wps_state == 0) { /*If wps is on then WPS would be already running. So no need to start GPS*/
				ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
				if (ret != FENCE_ERR_NONE) {
					LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
					return;
				}
			}
		} else if (method == LOCATIONS_METHOD_WPS) {
			if (__isWifiOn() == true && __isDataConnected() == true) {/* Start WPS positioning */
				ret = __start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);
				if (ret != FENCE_ERR_NONE) {
					LOGE_GEOFENCE("Fail to start wps positioning. Error[%d]", ret);
					return;
				}
			}
			if (geofence_server->loc_wps_started == true) {/* If WPS is successfully started, switch off gps*/
				__stop_gps_positioning(geofence_server);
				__stop_gps_alarms(geofence_server);
			}
		}
	}
}

/*********************************THIS HAS TO BE USED ONLY FOR TESTING*********************************************/
#ifdef __LOCAL_TEST__
static void __free_geofence_list(gpointer userdata)
{
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;

	GList *tmp_fence_list = g_list_first(geofence_server->geofence_list);
	while (tmp_fence_list) {
		GeofenceItemData *tmp_data = (GeofenceItemData *)tmp_fence_list->data;
		if (tmp_data) {
			g_free(tmp_data);
		}
		tmp_fence_list = g_list_next(tmp_fence_list);
	}
	geofence_server->geofence_list = NULL;
}
#endif

static int __check_fence_permission(int fence_id, const char *app_id)
{
	access_type_e access_type = ACCESS_TYPE_PUBLIC;
	char *appid;
	int ret = FENCE_ERR_NONE;
	ret = geofence_manager_get_access_type(fence_id, -1, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Error getting the access_type");
		return -1;
	}
	if (access_type == ACCESS_TYPE_PRIVATE) {
		ret = geofence_manager_get_appid_from_geofence(fence_id, &appid);
		if (ret != FENCE_ERR_NONE) {
			LOGE("Error getting the app_id for fence_id[%d]", fence_id);
			return -1;
		}
		if (g_strcmp0(appid, app_id)) {
			LOGE("Not authorized to access this private fence[%d]",	fence_id);
			return 0;
		}
	}
	return 1;
}

static int __check_place_permission(int place_id, const char *app_id)
{
	access_type_e access_type = ACCESS_TYPE_PUBLIC;
	char *appid;
	int ret = FENCE_ERR_NONE;
	ret = geofence_manager_get_access_type(-1, place_id, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Error getting the access_type");
		return -1;
	}
	if (access_type == ACCESS_TYPE_PRIVATE) {
		ret = geofence_manager_get_appid_from_places(place_id, &appid);
		if (ret != FENCE_ERR_NONE) {
			LOGE("Error getting the place_id for place_id[%d]", place_id);
			return -1;
		}
		if (g_strcmp0(appid, app_id)) {
			LOGE("Not authorized to access this private place[%d]",	place_id);
			return 0;
		}
	}
	return 1;
}

static void __stop_geofence_service(gint fence_id, const gchar *app_id, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);

	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	GeofenceItemData *item_data = NULL;
	int tracking_status = -1;
	int ret = FENCE_ERR_NONE;
	int place_id = -1;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;

	item_data = __get_item_by_fence_id(fence_id, geofence_server);	/*Fetch the fence details from add_list*/
	if (item_data == NULL) {
		LOGI_GEOFENCE("Invalid fence id - no fence exists with this fence id");
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_FENCE_STOPPED);
		return;		/*Invalid fence id - no fence exists with this fence id*/
	}
	ret = geofence_manager_get_place_id(fence_id, &place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the place_id from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STOPPED);
		return;
	}
	ret = geofence_manager_get_access_type(fence_id, -1, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the access type from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STOPPED);
		return;
	}
	ret = __check_fence_permission(fence_id, app_id);
	if (ret != 1) {
		LOGE("Permission denied or DB error occured while accessing the fence[%d]", fence_id);
		if (ret == 0) {
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_STOPPED);
		} else {
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STOPPED);
		}
		return;
	}
	ret = geofence_manager_get_running_status(fence_id, &tracking_status);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the running status from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STOPPED);
		return;
	}

	if (tracking_status == 0) {
		/*This fence is not in the tracking mode currently - nothing to do, just return saying the error*/
		LOGI_GEOFENCE("Fence ID: %d, is not in the tracking mode", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_STOPPED);
		return;
	}

	if (tracking_status > 0) {
		LOGI_GEOFENCE("Remove from tracklist: Fence id: %d", fence_id);
		item_data = __get_item_by_fence_id(fence_id, geofence_server);

		/*Item needs to be removed from the fence list*/
		if (item_data != NULL) {
			/*Main DB table should be updated here with the unsetting of running status flag*/
			tracking_status = tracking_status - 1;
			ret = geofence_manager_set_running_status(fence_id, tracking_status);
			if (ret != FENCE_ERR_NONE) {
				LOGI_GEOFENCE("Error resetting the running status in DB for fence: %d", fence_id);
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STOPPED);
				return;
			}
			/*Update the geofence count according to the type of geofence*/
			if (item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
				geofence_server->running_geopoint_cnt--;
				LOGI_GEOFENCE("Removed geopoint fence: %d from tracking list", fence_id);

				if (geofence_server->running_geopoint_cnt <= 0) {
					/*Stopping GPS...WPS*/
					__stop_gps_positioning(geofence_server);
					if (geofence_server->gps_fix_info != NULL) {
						g_free(geofence_server->gps_fix_info);
						geofence_server->gps_fix_info = NULL;
					}
					geofence_server->loc_gps_started_by_wps = false;
					__stop_wps_positioning(geofence_server);
					if (geofence_server->wps_fix_info != NULL) {
						g_free(geofence_server->wps_fix_info);
						geofence_server->wps_fix_info = NULL;
					}
					__stop_gps_alarms(geofence_server);
					__stop_wps_alarms(geofence_server);
					__stop_activity_service(geofence_server);
				}
			} else if (item_data->common_info.type == GEOFENCE_TYPE_BT) {
				geofence_server->running_bt_cnt--;
				LOGI_GEOFENCE("Removed bt fence: %d from tracking list", fence_id);

				if (geofence_server->running_bt_cnt <= 0) {
					/*May be unsetting the cb for bt discovery can be done here*/
				}
			} else if (item_data->common_info.type == GEOFENCE_TYPE_WIFI) {
				/*NOTHING NEED TO BE DONE HERE EXCEPT DECREMENTING THE COUNT*/
				geofence_server->running_wifi_cnt--;
				if (geofence_server->connectedTrackingWifiFenceId == fence_id) /*It means this fence is connected and it is stopped now*/
					geofence_server->connectedTrackingWifiFenceId = -1;
			}

			if (tracking_status == 0) {
				/*Remove the fence from the tracklist*/
				LOGD_GEOFENCE("Setting the fence status as uncertain here...");
				if (fence_id == geofence_server->nearestTrackingFence) {
					ret = bt_adapter_le_stop_scan();
					if (ret != BT_ERROR_NONE)
						LOGE_GEOFENCE("Unable to stop the BLE scan/ Stopped already, error: %d", ret);
				}
				item_data->common_info.status =	GEOFENCE_FENCE_STATE_UNCERTAIN;
				item_data->common_info.proximity_status = GEOFENCE_PROXIMITY_UNCERTAIN;
				geofence_server->tracking_list = g_list_remove(geofence_server->tracking_list, GINT_TO_POINTER(fence_id));
				if (g_list_length(geofence_server->tracking_list) == 0) {
					g_list_free(geofence_server->tracking_list);
					geofence_server->tracking_list = NULL;
				}
			}
		} else {
			LOGI_GEOFENCE("Geofence service is not running for this fence");
		}

	}
	/* Emit the error code */
	__emit_fence_event(geofence_server, place_id, fence_id, access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_STOPPED);
}


static bool __isWifiOn(void)
{
	int network_state = -1;
	vconf_get_int(VCONFKEY_WIFI_STATE, &network_state);
        if (network_state == 0)
                return false;
	return true;
}

static bool __isDataConnected(void)
{
	bool isDataConnected = false;
	int network_state = -1;
	int data_state = -1;

	int rv = vconf_get_int(VCONFKEY_NETWORK_WIFI_STATE, &network_state);
	if (rv == 0) {
		if (network_state == VCONFKEY_NETWORK_WIFI_CONNECTED) {
			LOGI_GEOFENCE("USING WIFI DATA");
			isDataConnected = true;
		}
	}
	if (isDataConnected == false) {
		rv = vconf_get_int(VCONFKEY_NETWORK_CELLULAR_STATE, &network_state);
		if (rv == 0) {
			if (network_state == VCONFKEY_NETWORK_CELLULAR_ON) {
				rv = vconf_get_int(VCONFKEY_DNET_STATE, &data_state);
				if (data_state == VCONFKEY_DNET_NORMAL_CONNECTED) {
			        	LOGI_GEOFENCE("USING MOBILE DATA");
			        	isDataConnected = true;
				}
			}
		}
	}
	return isDataConnected;
}

static int dbus_add_fence_cb(const gchar *app_id,
                             gint place_id,
                             gint geofence_type,
                             gdouble latitude,
                             gdouble longitude,
                             gint radius,
                             const gchar *address,
                             const gchar *bssid, const gchar *ssid, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;

	/* create fence id*/
	int fence_id = -1;
	int ret = FENCE_ERR_NONE;
	void *next_item_ptr = NULL;
	time_t cur_time;
	time(&cur_time);
	access_type_e access_type;

	ret = geofence_manager_get_access_type(-1, place_id, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the access type from the DB for place: %d or place-id does not exist.", place_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_FENCE_ADDED);
		return -1;
	}

	ret = __check_place_permission(place_id, app_id);
	if (ret != 1) {
		LOGE("Unable to add the fence. Permission denied or DB error occured while accessing the place[%d]", place_id);
		if (ret == 0) {
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_ADDED);
		} else {
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_ADDED);
		}
		return -1;
	}
	/* create GeofenceItemData item, and append it into geofence_list*/
	GeofenceItemData *item_data = (GeofenceItemData *)g_malloc0(sizeof(GeofenceItemData));
	if (item_data == NULL) {
		LOGI_GEOFENCE("Unable to add the fence because of malloc fail");
		__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_FENCE_ADDED);
		return -1;
	}

	item_data->distance = -1;
	item_data->client_status = GEOFENCE_CLIENT_STATUS_NONE;
	item_data->common_info.type = geofence_type;
	/*fences added by myplaces application are public fences by default*/
	if (!g_strcmp0(app_id, MYPLACES_APPID)) {
		item_data->common_info.access_type = ACCESS_TYPE_PUBLIC;
	} else {
		item_data->common_info.access_type = ACCESS_TYPE_PRIVATE;
	}
	item_data->common_info.enable = 1;
	item_data->common_info.status = GEOFENCE_FENCE_STATE_UNCERTAIN;
	item_data->common_info.proximity_status = GEOFENCE_PROXIMITY_UNCERTAIN;
	item_data->is_wifi_status_in = false;
	item_data->is_bt_status_in = false;
	g_strlcpy(item_data->common_info.appid, app_id, APP_ID_LEN);
	item_data->common_info.running_status = 0;
	item_data->common_info.place_id = place_id;

	/*DB is called and fence-id is retrieved from there(by auto increment mechanism)*/
	geofence_manager_set_common_info(&(item_data->common_info), &fence_id);
	item_data->common_info.fence_id = fence_id;
	LOGD_GEOFENCE("fence id : %d", item_data->common_info.fence_id);

	if (geofence_type == GEOFENCE_TYPE_GEOPOINT) {
		LOGD_GEOFENCE("Add geofence with GeoPoint");
		geocoordinate_info_s *geocoordinate_info = (geocoordinate_info_s *)g_malloc0(sizeof(geocoordinate_info_s));
		if (geocoordinate_info == NULL) {
			LOGI_GEOFENCE("Fail to set geocoordinate_info for GPS because of malloc fail");
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		geocoordinate_info->latitude = latitude;
		geocoordinate_info->longitude = longitude;
		if (radius < GEOFENCE_DEFAULT_RADIUS) {
			geocoordinate_info->radius = GEOFENCE_DEFAULT_RADIUS;
		} else {
			geocoordinate_info->radius = radius;
		}
		g_strlcpy(geocoordinate_info->address, address, ADDRESS_LEN);

		/*Geopoint information is saved in the DB*/
		ret = geofence_manager_set_geocoordinate_info(fence_id,	geocoordinate_info);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("Fail to set geocoordinate_info");
			ret = geofence_manager_delete_fence_info(fence_id);
			if (ret != FENCE_ERR_NONE)
				LOGI_GEOFENCE("Fail to delete fence_id[%d] from common table", fence_id);
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		item_data->priv = (void *) geocoordinate_info;

	} else if (geofence_type == GEOFENCE_TYPE_WIFI) {	/* Specific AP */
		LOGD_GEOFENCE("Add geofence with specific AP");

		bssid_info_s *wifi_info = NULL;
		wifi_info = (bssid_info_s *) g_malloc0(sizeof(bssid_info_s));
		if (wifi_info == NULL) {
			LOGI_GEOFENCE("Fail to set bssid_info for wifi because of malloc fail");
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		g_strlcpy(wifi_info->bssid, bssid, WLAN_BSSID_LEN);
		g_strlcpy(wifi_info->ssid, ssid, WLAN_BSSID_LEN);

		/*Wifi information is saved in the DB(both wifi and BT share the same bssid table here)*/
		ret = geofence_manager_set_bssid_info(fence_id, wifi_info);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("Fail to set bssid_info for wifi");
			ret = geofence_manager_delete_fence_info(fence_id);
			if (ret != FENCE_ERR_NONE)
				LOGI_GEOFENCE("Fail to delete fence_id[%d] from common table", fence_id);
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		item_data->priv = (void *) wifi_info;
	} else if (geofence_type == GEOFENCE_TYPE_BT) {
		LOGD_GEOFENCE("Add geofence with bluetooth bssid");

		bssid_info_s *bt_info = NULL;
		bt_info = (bssid_info_s *) g_malloc0(sizeof(bssid_info_s));
		if (bt_info == NULL) {
			LOGI_GEOFENCE("Fail to set bssid_info for BT because of malloc fail");
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		bt_info->enabled = TRUE;
		g_strlcpy(bt_info->bssid, bssid, WLAN_BSSID_LEN);
		g_strlcpy(bt_info->ssid, ssid, WLAN_BSSID_LEN);

		/*BT info is saved in the DB(both wifi and BT share the same bssid table here)*/
		ret = geofence_manager_set_bssid_info(fence_id, bt_info);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("Fail to set bssid_info for BT");
			ret = geofence_manager_delete_fence_info(fence_id);
			if (ret != FENCE_ERR_NONE)
				LOGI_GEOFENCE("Fail to delete fence_id[%d] from common table", fence_id);
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_ADDED);
			return -1;
		}
		item_data->priv = (void *) bt_info;
	}
	/*Adding the data to the geofence_list which contains the added geofences list information*/
	if (geofence_server->geofence_list == NULL) {
		geofence_server->geofence_list = g_list_append(geofence_server->geofence_list, item_data);
	} else {
		geofence_server->geofence_list = g_list_insert_before(geofence_server->geofence_list, next_item_ptr, item_data);
	}
	/*This code is just for testing purpose. It will be removed after the development phase - Karthik*/
	int temp_cnt = 0;
	GList *temp_list = g_list_first(geofence_server->geofence_list);
	temp_list = g_list_first(geofence_server->geofence_list);
	while (temp_list) {
		temp_cnt++;
		temp_list = g_list_next(temp_list);
	}
	LOGI_GEOFENCE("Fences in local list: %d", temp_cnt);
	geofence_manager_get_count_of_fences(&temp_cnt);
	LOGI_GEOFENCE("Fence count in DB: %d", temp_cnt);

	/*Emit the error code*/
	__emit_fence_event(geofence_server, place_id, fence_id,	item_data->common_info.access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_ADDED);
	return fence_id;
}

static int dbus_add_place_cb(const gchar *app_id,
                             const gchar *place_name, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	int place_id = -1;
	int ret = FENCE_ERR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	place_info_s *place_info = (place_info_s *)g_malloc0(sizeof(place_info_s));

	if (place_info == NULL) {
		LOGI_GEOFENCE("Unable to add the place due to malloc fail");
		__emit_fence_event(geofence_server, -1, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_PLACE_ADDED);
		return -1;
	}
	/*fences added by myplaces application are public fences by default*/
	if (!g_strcmp0(app_id, MYPLACES_APPID))
		place_info->access_type = ACCESS_TYPE_PUBLIC;
	else
		place_info->access_type = ACCESS_TYPE_PRIVATE;

	g_strlcpy(place_info->place_name, place_name, PLACE_NAME_LEN);
	g_strlcpy(place_info->appid, app_id, APP_ID_LEN);
	/*Add the place details to db*/
	ret = geofence_manager_set_place_info(place_info, &place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Unable to add the place due to DB error");
		__emit_fence_event(geofence_server, -1, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_ADDED);
		return -1;
	}
	__emit_fence_event(geofence_server, place_id, -1, place_info->access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_PLACE_ADDED);

	return place_id;
}

static void dbus_enable_geofence_cb(gint fence_id, const gchar *app_id, gboolean enable, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);

	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	int ret = FENCE_ERR_NONE;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;
	int place_id = -1;
	int enable_status = 0;
	geofence_manage_e manage_enum = GEOFENCE_MANAGE_SETTING_ENABLED;

	if (enable == 0)
		manage_enum = GEOFENCE_MANAGE_SETTING_DISABLED;

	ret = geofence_manager_get_access_type(fence_id, -1, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the access type from the DB for fence: %d or fence-id does not exist.", fence_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, manage_enum);
		return;
	}
	ret = geofence_manager_get_place_id(fence_id, &place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the place_id from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, manage_enum);
		return;
	}
	if (access_type == ACCESS_TYPE_PUBLIC) {
		if (g_strcmp0(app_id, MYPLACES_APPID) != 0) {
			LOGI_GEOFENCE("Received: %s", app_id);
			LOGI_GEOFENCE("Not authorized to enable/disable this fence[%d] service.", fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, manage_enum);
			return;
		}
		if (enable == true)
			enable_status = 1;
		ret = geofence_manager_set_enable_status(fence_id, enable_status);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("DB error in enabling/disabling the fence[%d].", fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, manage_enum);
			return;
		}
	} else {
		LOGI_GEOFENCE("Currently, only public fences can be enabled/disabled. It can be done by MyPlaces app only.");
	}
	/*Emit the error code*/
	__emit_fence_event(geofence_server, place_id, fence_id, access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, manage_enum);
}

static void dbus_update_place_cb(gint place_id, const gchar *app_id, const gchar *place_name, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	int ret = FENCE_ERR_NONE;
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;

	if (place_id == DEFAULT_PLACE_HOME || place_id == DEFAULT_PLACE_OFFICE || place_id == DEFAULT_PLACE_CAR) {
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED, GEOFENCE_MANAGE_PLACE_UPDATED);
		return;
	}

	place_info_s *place_info = (place_info_s *) g_malloc0(sizeof(place_info_s));
	if (place_info == NULL) {
		LOGI_GEOFENCE("malloc fail for place id[%d]", place_id);
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY, GEOFENCE_MANAGE_PLACE_UPDATED);
		return;
	}
	ret = geofence_manager_get_place_info(place_id, &place_info);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Place_id does not exist or DB error in getting the place info for place_id[%d].", place_id);
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_PLACE_UPDATED);
		g_free(place_info);
		return;
	}
	if (g_strcmp0(app_id, place_info->appid) != 0) {
		LOGI_GEOFENCE("Not authorized to update the place");
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED, GEOFENCE_MANAGE_PLACE_UPDATED);
		g_free(place_info);
		return;
	}

	/*Update the place details to db*/
	ret = geofence_manager_update_place_info(place_id, place_name);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Unable to update the place");
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_UPDATED);
		g_free(place_info);
		return;
	}
	__emit_fence_event(geofence_server, place_id, -1, place_info->access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_PLACE_UPDATED);
	g_free(place_info);
}

static void dbus_remove_fence_cb(gint fence_id, const gchar *app_id, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	g_return_if_fail(geofence_server);
	int ret = FENCE_ERR_NONE;
	int place_id = -1;
	char *app_id_db;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;

	/*//////////Required to be sent in the event callback////////////////--*/
	ret = geofence_manager_get_place_id(fence_id, &place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the place_id from the DB for fence: %d or fence-id does not exist", fence_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}
	/*//////////////////////////////////////////////////////////////////--*/
	ret = geofence_manager_get_appid_from_geofence(fence_id, &app_id_db);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Failed to get the appid, Error - %d", ret);
		__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}
	if (g_strcmp0(app_id_db, app_id) != 0) {
		LOGI_GEOFENCE("Not authorized to remove the fence");
		g_free(app_id_db);
		__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}
	g_free(app_id_db);
	/*/////////required to be sent in the event callback///////////////--*/
	ret = geofence_manager_get_access_type(fence_id, -1, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the access type from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}
	/*///////////////////////////////////////////////////////////////////////--*/
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data == NULL) {
		LOGI_GEOFENCE("Invalid fence_id[%d]", fence_id);
		return;
	}

	/*Stop the geofence service for the fence first if it is running*/
	int tracking_status = -1;
	ret = geofence_manager_get_running_status(fence_id, &tracking_status);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the running status from the DB for fence: %d or fence-id does not exist",	fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}
	if (tracking_status == 1) {
		__stop_geofence_service(fence_id, app_id, userdata);
	} else if (tracking_status > 1)	{/*its a public fence*/
		tracking_status = 1;	/*resetting the running status to 1 for forcefull stop from MYPlacesApp*/
		ret = geofence_manager_set_running_status(fence_id, tracking_status);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("Error resetting the running status in the DB for fence: %d or fence-id does not exist", fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_REMOVED);
			return;
		}
		__stop_geofence_service(fence_id, app_id, userdata);
	}
	/*Removing the fence id from the DB*/
	ret = geofence_manager_delete_fence_info(fence_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to delete fence_id[%d]", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_REMOVED);
		return;
	}

	/*Removing the fence id from the geofence_list which contains the added fence list details*/
	geofence_server->geofence_list = g_list_remove(geofence_server->geofence_list, item_data);
	LOGI_GEOFENCE("Removed fence_id[%d]", fence_id);
	g_free(item_data);	/*freeing the memory*/

	/*Check if the length of the geofence_list is 0 then free and make it null*/
	if (g_list_length(geofence_server->geofence_list) == 0) {
		g_list_free(geofence_server->geofence_list);
		geofence_server->geofence_list = NULL;
	}
	/*Emit the error code*/
	__emit_fence_event(geofence_server, place_id, fence_id, access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_REMOVED);
}

static void dbus_get_place_name_cb(gint place_id, const gchar *app_id, char **place_name, int *error_code, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;

	int ret = geofence_manager_get_access_type(-1, place_id, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the access type from the DB for place: %d or place-id does not exist.", place_id);
		*error_code = GEOFENCE_SERVER_ERROR_ID_NOT_EXIST;
		return;
	}

	ret = __check_place_permission(place_id, app_id);
	if (ret != 1) {
		LOGE("Unable to get the place name. Permission denied or DB error occured while accessing the place[%d]", place_id);
		if (ret == 0) {
			*error_code = GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED;
		} else {
			*error_code = GEOFENCE_SERVER_ERROR_DATABASE;
		}
		return;
	}
	ret = geofence_manager_get_place_name(place_id, place_name);
	if (ret != FENCE_ERR_NONE) {
		*error_code = GEOFENCE_SERVER_ERROR_DATABASE;
		return;
	}
	*error_code = GEOFENCE_SERVER_ERROR_NONE;
}

static void dbus_remove_place_cb(gint place_id, const gchar *app_id,
                                 gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	GList *fence_list = NULL, *list = NULL;
	int fence_cnt = 0;
	int tracking_status = 0;
	int ret = FENCE_ERR_NONE;
	GeofenceItemData *item_data = NULL;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;

	/* Default places */
	if (place_id == DEFAULT_PLACE_HOME || place_id == DEFAULT_PLACE_OFFICE || place_id == DEFAULT_PLACE_CAR) {
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED, GEOFENCE_MANAGE_PLACE_REMOVED);
		return;
	}
	ret = geofence_manager_get_access_type(-1, place_id, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Unable to fetch the access type for place_id[%d]", place_id);
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_PLACE_REMOVED);
		return;
	}

	place_info_s *place_info =
	    (place_info_s *) g_malloc0(sizeof(place_info_s));
	ret = geofence_manager_get_place_info(place_id, &place_info);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Place_id does not exist or DB error in getting the place info for place_id[%d].", place_id);
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_PLACE_REMOVED);
		g_free(place_info);
		return;
	}
	if (g_strcmp0(app_id, place_info->appid) != 0) {
		LOGI_GEOFENCE("Not authorized to remove the place");
		g_free(place_info);
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED, GEOFENCE_MANAGE_PLACE_REMOVED);
		return;
	}
	g_free(place_info);

	ret = geofence_manager_get_fenceid_list_from_db(&fence_cnt, &fence_list, place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Unable to fetch the fence list from the DB");
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_REMOVED);
		return;
	}
	int fence_id = 0;
	list = g_list_first(fence_list);
	while (list) {
		fence_id = GPOINTER_TO_INT(list->data);
		item_data = __get_item_by_fence_id(fence_id, geofence_server);
		ret = geofence_manager_get_running_status(fence_id, &tracking_status);
		if (ret != FENCE_ERR_NONE) {
			LOGE("Unable to fetch the running status before removing the fence while removing a place");
			__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_REMOVED);
			return;
		}
		if (tracking_status == 1) {
			__stop_geofence_service(fence_id, app_id, userdata);
		} else if (tracking_status > 1) {
			tracking_status = 1;	/*resetting the running status as it is a forcefull stop from MYPlacesApp*/
			ret = geofence_manager_set_running_status(fence_id, tracking_status);
			if (ret != FENCE_ERR_NONE) {
				LOGI_GEOFENCE("Error setting the running status from the DB for fence: %d or fence-id does not exist", fence_id);
				__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_REMOVED);
				return;
			}
			__stop_geofence_service(fence_id, app_id, userdata);
		}

		/*Removing the fence id from the geofence_list which contains the added fence list details*/
		geofence_server->geofence_list = g_list_remove(geofence_server->geofence_list, item_data);
		LOGI_GEOFENCE("Removed fence_id[%d]", fence_id);
		g_free(item_data);
		/*Check if the length of the geofence_list is 0 then free and make it null*/
		if (g_list_length(geofence_server->geofence_list) == 0) {
			g_list_free(geofence_server->geofence_list);
			geofence_server->geofence_list = NULL;
		}
		list = g_list_next(list);
	}
	ret = geofence_manager_delete_place_info(place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("DB error occured while removing the place from DB");
		__emit_fence_event(geofence_server, place_id, -1, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_PLACE_REMOVED);
		return;
	}
	__emit_fence_event(geofence_server, place_id, -1, access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_PLACE_REMOVED);
}

static void dbus_start_geofence_cb(gint fence_id, const gchar *app_id, gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);

	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	int tracking_fence_id = -1;
	void *next_item_ptr = NULL;
	GList *track_list = g_list_first(geofence_server->tracking_list);
	GeofenceItemData *item_data = NULL;

	int ret = FENCE_ERR_NONE;
	int tracking_status = -1;
	int place_id = -1;
	access_type_e access_type = ACCESS_TYPE_UNKNOWN;
	char *app_id_db = NULL;
	geofence_fence_state_e status_to_be_emitted = GEOFENCE_FENCE_STATE_UNCERTAIN;

	item_data = __get_item_by_fence_id(fence_id, geofence_server);	/*Fetch the fence details from add_list*/
	if (item_data == NULL) {
		LOGI_GEOFENCE("Invalid fence id - no fence exists with this fence id");
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_ID_NOT_EXIST, GEOFENCE_MANAGE_FENCE_STARTED);
		return;		/*Invalid fence id - no fence exists with this fence id*/
	}
	if (!g_strcmp0(app_id, MYPLACES_APPID)) {
		LOGI_GEOFENCE("My Places cannot start a fence");
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_STARTED);
		return;
	}
	ret = geofence_manager_get_place_id(fence_id, &place_id);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the place_id from the DB for fence: %d", fence_id);
		__emit_fence_event(geofence_server, -1, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
		return;
	}

	ret = geofence_manager_get_running_status(fence_id, &tracking_status);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error fetching the running status from the DB for fence: %d or fence-id does not exist.", fence_id);
		__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
		return;
	}

	ret = geofence_manager_get_access_type(fence_id, -1, &access_type);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Error getting the access_type");
		return;
	}
	if (access_type == ACCESS_TYPE_PRIVATE) {
		ret = geofence_manager_get_appid_from_geofence(fence_id, &app_id_db);
		if (ret != FENCE_ERR_NONE) {
			LOGE("Error getting the app_id for fence_id[%d]", fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
		}
		if (g_strcmp0(app_id_db, app_id)) {
			LOGE("Not authorized to access this private fence[%d]",	fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_STARTED);
			g_free(app_id_db);
			return;
		}
		g_free(app_id_db);
		if (tracking_status == 1) {
			LOGI_GEOFENCE("Private fence ID: %d, already exists in the tracking list", fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	access_type, app_id, GEOFENCE_SERVER_ERROR_ALREADY_STARTED, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
		} else {
			ret = geofence_manager_set_running_status(fence_id, 1);
			if (ret != FENCE_ERR_NONE) {
				LOGI_GEOFENCE("Error setting the fence status");
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
			tracking_status = 1;
		}
	} else if (access_type == ACCESS_TYPE_PUBLIC) {
		int enable = -1;
		ret = geofence_manager_get_enable_status(fence_id, &enable);
		if (ret != FENCE_ERR_NONE) {
			LOGI_GEOFENCE("Error fetching the enable status from the DB for fence: %d or fence-id does not exist.",	fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
		}
		if (enable == 0) {
			LOGI_GEOFENCE("Error - Fence[%d] is not enabled",
			              fence_id);
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
		}
		if (tracking_status > 0) {
			LOGI_GEOFENCE("Public fence ID: %d, already exists in the tracking list, incrementing the counter for fence", fence_id);
			ret = geofence_manager_set_running_status(fence_id, (tracking_status + 1));
			if (ret != FENCE_ERR_NONE) {
				LOGI_GEOFENCE("Error setting the fence status");
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
			__emit_fence_event(geofence_server, place_id, fence_id,	access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
		} else {
			ret = geofence_manager_set_running_status(fence_id, (tracking_status + 1));
			if (ret != FENCE_ERR_NONE) {
				LOGI_GEOFENCE("Error setting the fence status");
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_DATABASE, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
			tracking_status++;
		}
	}

	item_data->client_status = GEOFENCE_CLIENT_STATUS_START;

	if (item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {

		if (__is_support_wps() == true && __isDataConnected() == true && __isWifiOn() == true) {
			ret = __start_wps_positioning(geofence_server, __geofence_wps_position_changed_cb);
			if (ret != FENCE_ERR_NONE) {
				LOGE_GEOFENCE("Fail to start wps positioning. Error[%d]", ret);
				geofence_manager_set_running_status(fence_id, (tracking_status - 1));
				__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_IPC, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
		} else {
			ret = __start_gps_positioning(geofence_server, __geofence_standalone_gps_position_changed_cb);
			if (ret != FENCE_ERR_NONE) {
				LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
				geofence_manager_set_running_status(fence_id, (tracking_status - 1));
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_IPC, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
		}
		geofence_server->running_geopoint_cnt++;
		__start_activity_service(geofence_server);
	} else if (item_data->common_info.type == GEOFENCE_TYPE_BT) {
		LOGI_GEOFENCE("fence_type [GEOFENCE_TYPE_BT]");

		bssid_info_s *bt_info = NULL;
		if (item_data->priv != NULL) {
			bt_info = (bssid_info_s *) item_data->priv;
			if (!bt_info->enabled)
				bt_info->enabled = TRUE;
		}
		bt_adapter_state_e adapter_state = BT_ADAPTER_DISABLED;
		bt_error_e error = BT_ERROR_NONE;
		error = bt_adapter_get_state(&adapter_state);
		if (error == BT_ERROR_NONE) {
			geofence_server->running_bt_cnt++;
			if (adapter_state == BT_ADAPTER_DISABLED) {
				LOGE_GEOFENCE("BT Adapter is DISABLED");
				status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
			} else if (adapter_state == BT_ADAPTER_ENABLED) {
				bt_device_info_s *bt_device_info = NULL;
				if (bt_info != NULL) {
					ret = bt_adapter_get_bonded_device_info(bt_info->bssid,	&bt_device_info);
					if (ret != BT_ERROR_NONE) {
						LOGE_GEOFENCE("Fail to get the bonded device info/ Not bonded with any device. Error[%d]", ret);
						/*NEED TO BE DECIDED WHETHER TO REQUEST FOR A SCAN HERE OR JUST EMIT OUT AS STATUS*/
						if (ret == BT_ERROR_REMOTE_DEVICE_NOT_BONDED)
							status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
					} else {
						if (bt_device_info == NULL) {
							LOGI_GEOFENCE("bt_adapter_get_bonded_device_info [%s] failed.",	bt_info->bssid);
							status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
						} else {
							if (bt_device_info->is_bonded == TRUE && bt_device_info->is_connected == TRUE) {
								LOGI_GEOFENCE("[%s] bonded TRUE, connected TRUE", bt_info->bssid);
								status_to_be_emitted = GEOFENCE_FENCE_STATE_IN;
							} else {
								status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
							}

							ret = bt_adapter_free_device_info(bt_device_info);
							if (ret != BT_ERROR_NONE)
								LOGE_GEOFENCE("bt_adapter_free_device_info fail[%d]", ret);
						}
					}
				}
			}
		} else {
			if (error != BT_ERROR_NONE) {
				LOGI_GEOFENCE("Unable to get the BT adapter state. Not added to track list: %d", error);
				geofence_manager_set_running_status(fence_id, (tracking_status - 1));
				__emit_fence_event(geofence_server, place_id, fence_id, ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_IPC, GEOFENCE_MANAGE_FENCE_STARTED);
				return;
			}
		}
	} else if (item_data->common_info.type == GEOFENCE_TYPE_WIFI) {
		LOGI_GEOFENCE("fence_type [GEOFENCE_TYPE_WIFI]");
		wifi_error_e rv = WIFI_ERROR_NONE;
		int nWifiState;
		wifi_ap_h ap_h;
		char *ap_bssid = NULL;
		int bssidlen;
		bssid_info_s *wifi_info = NULL;
		vconf_get_int(VCONFKEY_WIFI_STATE, &nWifiState);

		if (nWifiState != 0) {
			LOGI_GEOFENCE("Wifi is on...");
			geofence_server->running_wifi_cnt++;	/*Incrementing the counter for wifi fence*/

			if (item_data->priv != NULL) {
				wifi_info = (bssid_info_s *) item_data->priv;
			}
			rv = wifi_get_connected_ap(&ap_h);
			if (rv != WIFI_ERROR_NONE) {
				LOGI_GEOFENCE("Fail/not connected to get the connected AP: [%s] , geofence will be added to the fence list", __convert_wifi_error_to_string(rv));
				if (rv == WIFI_ERROR_NO_CONNECTION) {
					LOGI_GEOFENCE("Not connected to any AP");
					status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
				}
			} else {
				rv = wifi_ap_get_bssid(ap_h, &ap_bssid);
				if (rv != WIFI_ERROR_NONE) {
					LOGI_GEOFENCE("Fail to get the bssid: [%d]\n", __convert_wifi_error_to_string(rv));
				} else {
					bssidlen = strlen(ap_bssid);
					LOGI_GEOFENCE("Connected AP: %s, %d\n",	ap_bssid, bssidlen);
					if (g_strcmp0(wifi_info->bssid,	ap_bssid) == 0) {
						status_to_be_emitted = GEOFENCE_FENCE_STATE_IN;
						geofence_server->connectedTrackingWifiFenceId = fence_id;
					} else {
						status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
					}

				}
			}
		} else {
			LOGI_GEOFENCE("Wifi is not switched on...");
			geofence_server->running_wifi_cnt++;	/*Incrementing the counter for wifi fence*/
			/*Emit the fence status as out as wifi is not switched on here*/
			status_to_be_emitted = GEOFENCE_FENCE_STATE_OUT;
		}
	} else {
		LOGI_GEOFENCE("Invalid fence_type[%d]",
		              item_data->common_info.type);
		return;
	}
	/*Adding the fence to the tracking list*/
	LOGI_GEOFENCE("Add to tracklist: Fence id: %d", fence_id);
	if (geofence_server->tracking_list == NULL) {
		geofence_server->tracking_list = g_list_append(geofence_server->tracking_list, GINT_TO_POINTER(fence_id));
	} else {
		geofence_server->tracking_list = g_list_insert_before(geofence_server->tracking_list, next_item_ptr, GINT_TO_POINTER(fence_id));
	}
	LOGI_GEOFENCE("Added fence id: Fence id: %d", fence_id);

	__emit_fence_event(geofence_server, place_id, fence_id, access_type, app_id, GEOFENCE_SERVER_ERROR_NONE, GEOFENCE_MANAGE_FENCE_STARTED);

	__emit_fence_inout(geofence_server, item_data->common_info.fence_id, status_to_be_emitted);

	track_list = g_list_first(geofence_server->tracking_list);
	while (track_list) {
		tracking_fence_id = GPOINTER_TO_INT(track_list->data);
		LOGI_GEOFENCE("%d", tracking_fence_id);
		track_list = g_list_next(track_list);
	}
}

static void dbus_stop_geofence_cb(gint fence_id, const gchar *app_id, gpointer userdata)
{
	__stop_geofence_service(fence_id, app_id, userdata);
}

static void __start_activity_service(GeofenceServer *geofence_server)
{
	FUNC_ENTRANCE_SERVER;
	bool activity_supported = TRUE;
	int ret = ACTIVITY_ERROR_NONE;

	if (geofence_server->activity_stationary_h == NULL) {
		activity_is_supported(ACTIVITY_STATIONARY, &activity_supported);
		if (activity_supported == TRUE) {
			ret = activity_create(&(geofence_server->activity_stationary_h));
			if (ret != ACTIVITY_ERROR_NONE) {
				LOGD_GEOFENCE("Fail to create stationary activity %d", ret);
			} else {
				ret = activity_start_recognition(geofence_server->activity_stationary_h, ACTIVITY_STATIONARY, __activity_cb, geofence_server);
				if (ret != ACTIVITY_ERROR_NONE)
					LOGD_GEOFENCE("Fail to start stationary activity %d", ret);
				else
					LOGD_GEOFENCE("Success to start stationary activity");
			}
		} else
			LOGD_GEOFENCE("Not support stationary activity");
	}

	if (geofence_server->activity_walk_h == NULL) {
		activity_is_supported(ACTIVITY_WALK, &activity_supported);
		if (activity_supported == TRUE) {
			ret = activity_create(&(geofence_server->activity_walk_h));
			if (ret != ACTIVITY_ERROR_NONE) {
				LOGD_GEOFENCE("Fail to create walk activity %d", ret);
			} else {
				ret = activity_start_recognition(geofence_server->activity_walk_h, ACTIVITY_WALK, __activity_cb, geofence_server);
				if (ret != ACTIVITY_ERROR_NONE)
					LOGD_GEOFENCE("Fail to start walk activity %d", ret);
				else
					LOGD_GEOFENCE("Success to start walk activity");
			}
		} else
			LOGD_GEOFENCE("Not support walk activity");
	}

	if (geofence_server->activity_run_h == NULL) {
		activity_is_supported(ACTIVITY_RUN, &activity_supported);
		if (activity_supported == TRUE) {
			ret = activity_create(&(geofence_server->activity_run_h));
			if (ret != ACTIVITY_ERROR_NONE) {
				LOGD_GEOFENCE("Fail to create run activity %d", ret);
			} else {
				ret = activity_start_recognition(geofence_server->activity_run_h, ACTIVITY_RUN, __activity_cb, geofence_server);
				if (ret != ACTIVITY_ERROR_NONE)
					LOGD_GEOFENCE("Fail to start run activity %d", ret);
				else
					LOGD_GEOFENCE("Success to start run activity");
			}
		} else
			LOGD_GEOFENCE("Not support run activity");
	}

	if (geofence_server->activity_in_vehicle_h == NULL) {
		activity_is_supported(ACTIVITY_IN_VEHICLE, &activity_supported);
		if (activity_supported == TRUE) {
			ret = activity_create(&(geofence_server->activity_in_vehicle_h));
			if (ret != ACTIVITY_ERROR_NONE) {
				LOGD_GEOFENCE("Fail to create in_vehicle activity %d", ret);
			} else {
				ret = activity_start_recognition(geofence_server->activity_in_vehicle_h, ACTIVITY_IN_VEHICLE, __activity_cb, geofence_server);
				if (ret != ACTIVITY_ERROR_NONE)
					LOGD_GEOFENCE("Fail to start in_vehicle activity %d", ret);
				else
					LOGD_GEOFENCE("Success to start in_vehicle activity");
			}
		} else
			LOGD_GEOFENCE("Not support in_vehicle activity");
	}
}

static void __stop_activity_service(GeofenceServer *geofence_server)
{
	FUNC_ENTRANCE_SERVER;
	int ret = ACTIVITY_ERROR_NONE;

	if (geofence_server->activity_stationary_h != NULL) {
		ret = activity_stop_recognition(geofence_server->activity_stationary_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to stop stationary activity %d", ret);
		else
			LOGD_GEOFENCE("Success to stop stationary activity");

		ret = activity_release(geofence_server->activity_stationary_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to release stationary activity %d", ret);
		else
			geofence_server->activity_stationary_h = NULL;
	}

	if (geofence_server->activity_walk_h != NULL) {
		ret = activity_stop_recognition(geofence_server->activity_walk_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to stop walk activity %d", ret);
		else
			LOGD_GEOFENCE("Success to stop walk activity");

		ret = activity_release(geofence_server->activity_walk_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to release walk activity %d", ret);
		else
			geofence_server->activity_walk_h = NULL;
	}

	if (geofence_server->activity_run_h != NULL) {
		ret = activity_stop_recognition(geofence_server->activity_run_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to stop run activity %d", ret);
		else
			LOGD_GEOFENCE("Success to stop run activity");

		ret = activity_release(geofence_server->activity_run_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to release run activity %d", ret);
		else
			geofence_server->activity_run_h = NULL;
	}

	if (geofence_server->activity_in_vehicle_h != NULL) {
		ret = activity_stop_recognition(geofence_server->activity_in_vehicle_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to stop in_vehicle activity %d", ret);
		else
			LOGD_GEOFENCE("Success to stop in_vehicle activity");

		ret = activity_release(geofence_server->activity_in_vehicle_h);
		if (ret != ACTIVITY_ERROR_NONE)
			LOGD_GEOFENCE("Fail to release in_vehicle activity %d", ret);
		else
			geofence_server->activity_in_vehicle_h = NULL;
	}
}

static void __activity_cb(activity_type_e type, const activity_data_h data, double timestamp, activity_error_e error, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	activity_accuracy_e accuracy;
	int result = ACTIVITY_ERROR_NONE;

	if (error != ACTIVITY_ERROR_NONE) {
		LOGD_GEOFENCE("Error in activity callback %d", error);
		return;
	}

	result = activity_get_accuracy(data, &accuracy);
	if (result != ACTIVITY_ERROR_NONE) {
		LOGD_GEOFENCE("Fail to get accuracy of activity %d", error);
		return;
	}

	if (accuracy >= ACTIVITY_ACCURACY_MID) {
		geofence_server->activity_type = type;
		geofence_server->activity_timestamp = timestamp;

		LOGD_GEOFENCE("Activity type = %d, timestamp = %lf", type, timestamp);
	}
}

static GVariant *dbus_get_geofences_cb(int place_id, const gchar *app_id, int *fenceCnt, int *errorCode, gpointer userdata)
{
	geofence_info_s *item;
	GVariantBuilder b;
	GList *fence_list = NULL, *list = NULL;
	place_info_s *place_info = NULL;
	int count = 0, fence_cnt = 0;
	int ret = 0;

	LOGI_GEOFENCE(">>> Enter");
	/*As same API is used to get the fence list, whenever complete fence list is requested, place_id is passed as -1 from the module.*/
	/*Whenever fence_list in a particular place is requested place_id will not be -1. This is jusr maintained internally*/
	if (place_id == -1) {
		ret = geofence_manager_get_fence_list_from_db(&count, &fence_list, -1);
	} else {
		ret = geofence_manager_get_place_info(place_id, &place_info);
		if (ret != FENCE_ERR_NONE) {
			LOGE("Error getting the place info for place_id[%d]", place_id);
			/* Send ZERO data gvariant*/
			*errorCode = GEOFENCE_SERVER_ERROR_DATABASE;
			*fenceCnt = fence_cnt;
			g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
			return g_variant_builder_end(&b);
		}
		if ((place_info != NULL) && (place_info->access_type == ACCESS_TYPE_PRIVATE)) {
			if (g_strcmp0(app_id, place_info->appid) != 0) {
				LOGI_GEOFENCE("Not authorized to access this private place[%d]", place_id);
				if (place_info)
					g_free(place_info);
				/* Send ZERO data gvariant*/
				*errorCode = GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED;
				*fenceCnt = fence_cnt;
				g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
				return g_variant_builder_end(&b);
			}
			if (place_info)
				g_free(place_info);
		}
		ret = geofence_manager_get_fence_list_from_db(&count, &fence_list, place_id);
	}
	LOGI_GEOFENCE("count = %d", count);

	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("get list failed");
		/* Send ZERO data gvariant*/
		*errorCode = GEOFENCE_SERVER_ERROR_DATABASE;
		*fenceCnt = fence_cnt;
		g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
		return g_variant_builder_end(&b);
	} else if (count == 0) {
		LOGI_GEOFENCE("List is empty");
		/* Send ZERO data gvariant*/
		*errorCode = GEOFENCE_SERVER_ERROR_NONE;
		*fenceCnt = fence_cnt;
		g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
		return g_variant_builder_end(&b);
	}

	/* Initialize for the container*/
	g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));

	list = g_list_first(fence_list);
	while (list) {
		item = (geofence_info_s *) list->data;

		if (item && ((item->access_type == ACCESS_TYPE_PUBLIC) || !(g_strcmp0(app_id, item->app_id)))) {
			/* Open container*/
			g_variant_builder_open(&b, G_VARIANT_TYPE("a{sv}"));

			/* Add parameters to dictionary*/
			g_variant_builder_add(&b, "{sv}", "fence_id", g_variant_new_int32(item->fence_id));

			LOGI_GEOFENCE("fence_id: %d, place_id: %d, latitude: %f, longitude: %f, radius: %d", item->fence_id, item->param.place_id, item->param.latitude, item->param.longitude,	item->param.radius);

			switch (item->param.type) {
				case GEOFENCE_TYPE_GEOPOINT: {
						g_variant_builder_add(&b, "{sv}", "place_id", g_variant_new_int32(item->param.place_id));
						g_variant_builder_add(&b, "{sv}", "geofence_type", g_variant_new_int32(item->param.type));
						g_variant_builder_add(&b, "{sv}", "latitude", g_variant_new_double(item->param.latitude));
						g_variant_builder_add(&b, "{sv}", "longitude", g_variant_new_double(item->param.longitude));
						g_variant_builder_add(&b, "{sv}", "radius", g_variant_new_int32(item->param.radius));
						g_variant_builder_add(&b, "{sv}", "address", g_variant_new_string(item->param.address));
						g_variant_builder_add(&b, "{sv}", "bssid", g_variant_new_string("NA"));
						g_variant_builder_add(&b, "{sv}", "ssid", g_variant_new_string("NA"));
					}
					break;
				case GEOFENCE_TYPE_WIFI:
				case GEOFENCE_TYPE_BT: {
						g_variant_builder_add(&b, "{sv}", "place_id", g_variant_new_int32(item->param.place_id));
						g_variant_builder_add(&b, "{sv}", "geofence_type", g_variant_new_int32(item->param.type));
						g_variant_builder_add(&b, "{sv}", "latitude", g_variant_new_double(0.0));
						g_variant_builder_add(&b, "{sv}", "longitude", g_variant_new_double(0.0));
						g_variant_builder_add(&b, "{sv}", "radius", g_variant_new_int32(0.0));
						g_variant_builder_add(&b, "{sv}", "address", g_variant_new_string("NA"));
						g_variant_builder_add(&b, "{sv}", "bssid", g_variant_new_string(item->param.bssid));
						g_variant_builder_add(&b, "{sv}", "ssid", g_variant_new_string(item->param.ssid));
					}
					break;
				default:
					LOGI_GEOFENCE("Unsupported type: [%d]",	item->param.type);
					break;
			}

			/* Close container*/
			g_variant_builder_close(&b);
			fence_cnt++;
		} else {
			if (item != NULL)
				LOGI_GEOFENCE("This fence id: %d is private. Not authorized to access by this app", item->fence_id);
		}

		/* Move to next node*/
		list = g_list_next(list);
	}
	*errorCode = GEOFENCE_SERVER_ERROR_NONE;
	*fenceCnt = fence_cnt;
	return g_variant_builder_end(&b);
}

static GVariant *dbus_get_places_cb(const gchar *app_id, int *placeCnt, int *errorCode, gpointer userdata)
{
	place_info_s *item;
	GVariantBuilder b;
	GList *place_list = NULL, *list = NULL;
	int count = 0, place_cnt = 0;
	int ret = 0;

	LOGI_GEOFENCE(">>> Enter");

	ret = geofence_manager_get_place_list_from_db(&count, &place_list);

	LOGI_GEOFENCE("count = %d", count);

	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("get list failed");
		/* Send ZERO data gvariant*/
		*errorCode = GEOFENCE_SERVER_ERROR_DATABASE;
		*placeCnt = place_cnt;
		g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
		return g_variant_builder_end(&b);
	} else if (count == 0) {
		LOGI_GEOFENCE("List is empty");
		/* Send ZERO data gvariant*/
		*errorCode = GEOFENCE_SERVER_ERROR_NONE;
		*placeCnt = place_cnt;
		g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));
		return g_variant_builder_end(&b);
	}

	/* Initialize for the container*/
	g_variant_builder_init(&b, G_VARIANT_TYPE("aa{sv}"));

	list = g_list_first(place_list);
	while (list) {
		item = (place_info_s *) list->data;

		if (item && ((item->access_type == ACCESS_TYPE_PUBLIC) || !(g_strcmp0(app_id, item->appid)))) {
			/* Open container*/
			g_variant_builder_open(&b, G_VARIANT_TYPE("a{sv}"));

			LOGI_GEOFENCE("place_id: %d, access_type: %d, place_name: %s, app_id: %s", item->place_id, item->access_type, item->place_name, item->appid);
			/* Add data to dictionary*/
			g_variant_builder_add(&b, "{sv}", "place_id", g_variant_new_int32(item->place_id));
			g_variant_builder_add(&b, "{sv}", "place_name", g_variant_new_string(item->place_name));
			/* Close container*/
			g_variant_builder_close(&b);
			place_cnt++;
		} else {
			if (item != NULL)
				LOGI_GEOFENCE("This place id: %d is private. Not authorized to access by this app", item->place_id);
		}
		list = g_list_next(list);
	}
	*errorCode = GEOFENCE_SERVER_ERROR_NONE;
	*placeCnt = place_cnt;
	return g_variant_builder_end(&b);
}

static void __add_default_place(char *place_name)
{
	int place_id;
	place_info_s *place_info = (place_info_s *) g_malloc0(sizeof(place_info_s));
	g_return_if_fail(place_info);

	place_info->access_type = ACCESS_TYPE_PUBLIC;
	g_strlcpy(place_info->place_name, place_name, PLACE_NAME_LEN);
	g_strlcpy(place_info->appid, "dummy_app_id", APP_ID_LEN);
	/*Add the place details to db*/
	int ret = geofence_manager_set_place_info(place_info, &place_id);
	if (ret != FENCE_ERR_NONE)
		LOGI_GEOFENCE("Unable to add the default places due to DB error");
}

static void __init_geofencemanager(GeofenceServer *geofence_server)
{
	FUNC_ENTRANCE_SERVER;

	geofence_server->loc_gps_started_by_wps = false;
	geofence_server->loc_gps_started = false;
	geofence_server->loc_wps_started = false;
	geofence_server->nearestTrackingFence = 0;
	geofence_server->connectedTrackingWifiFenceId = -1;
	geofence_server->gps_fix_info = NULL;
	geofence_server->wps_fix_info = NULL;
	g_stpcpy(geofence_server->ble_info, "");
	geofence_server->gps_trigger_interval = 1; /* 1 sec by default*/
	geofence_server->timer_id = -1;
	geofence_server->gps_alarm_id = -1;
	geofence_server->wps_alarm_id = -1;
	geofence_server->gps_timeout_alarm_id = -1;
	geofence_server->wps_timeout_alarm_id = -1;
	geofence_server->geofence_list = NULL;
	geofence_server->tracking_list = NULL;
	geofence_server->running_geopoint_cnt = 0;
	geofence_server->running_bt_cnt = 0;
	geofence_server->running_wifi_cnt = 0;
	geofence_server->activity_type = ACTIVITY_IN_VEHICLE;
	geofence_server->activity_timestamp = 0;

	geofence_server->activity_stationary_h = NULL;
	geofence_server->activity_walk_h = NULL;
	geofence_server->activity_run_h = NULL;
	geofence_server->activity_in_vehicle_h = NULL;

	/*Initializing the DB to store the fence informations*/
	if (geofence_manager_db_init() != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Error initalizing the DB");
	}
	/*Adding default places in the DB*/
	int place_id = DEFAULT_PLACE_HOME;
	int count = -1;

	while (place_id <= DEFAULT_PLACE_CAR) {
		if (geofence_manager_get_place_count_by_placeid(place_id, &count) == FENCE_ERR_NONE) {
			if (count == 0) {
				if (place_id == DEFAULT_PLACE_HOME) {
					__add_default_place("Home");
				} else if (place_id == DEFAULT_PLACE_OFFICE) {
					__add_default_place("Office");
				} else if (place_id == DEFAULT_PLACE_CAR) {
					__add_default_place("Car");
				}
			}
		} else {
			LOGI_GEOFENCE("Error adding the default place: %d", place_id);
		}
		place_id++;
		count = -1;
	}

	/*delete all fences at rebooting for a test. TODO: will be replaced by updating previous fences*/
	/*geofence_manager_db_reset();*/
}


#if USE_HW_GEOFENCE
static void __start_gps_geofence_client(void *handle, GeofenceModCB geofence_cb, void *userdata)
{
	FUNC_ENTRANCE_SERVER;
	/*Previously code to start the HW geofence client was there. It has been removed now*/
	return;
}

static void __stop_gps_geofence_client(void *handle)
{
	FUNC_ENTRANCE_SERVER;
	/*Stop tracking the geofences*/
	return;
}
#endif

int __copy_geofence_to_item_data(int fence_id, GeofenceItemData *item_data)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(item_data, FENCE_ERR_INVALID_PARAMETER);
	char *app_id = NULL;

	item_data->common_info.fence_id = fence_id;
	item_data->common_info.status = GEOFENCE_FENCE_STATE_UNCERTAIN;
	if (FENCE_ERR_NONE != geofence_manager_get_geofence_type(fence_id, &item_data->common_info.type))
		return FENCE_ERR_SQLITE_FAIL;

	if (FENCE_ERR_NONE != geofence_manager_get_access_type(fence_id, -1, &item_data->common_info.access_type))
		return FENCE_ERR_SQLITE_FAIL;

	if (FENCE_ERR_NONE != geofence_manager_get_enable_status(fence_id, &item_data->common_info.enable))
		return FENCE_ERR_SQLITE_FAIL;

	if (FENCE_ERR_NONE != geofence_manager_get_appid_from_geofence(fence_id, &app_id)) {
		g_free(app_id);
		return FENCE_ERR_SQLITE_FAIL;
	} else {
		g_strlcpy(item_data->common_info.appid, app_id, APP_ID_LEN);
		g_free(app_id);
	}
	if (FENCE_ERR_NONE != geofence_manager_get_placeid_from_geofence(fence_id, &item_data->common_info.place_id))
		return FENCE_ERR_SQLITE_FAIL;

	if (FENCE_ERR_NONE != geofence_manager_get_running_status(fence_id, &item_data->common_info.running_status))
		return FENCE_ERR_SQLITE_FAIL;

	if (item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
		geocoordinate_info_s *geocoordinate_info = NULL;
		int ret = FENCE_ERR_NONE;

		ret = geofence_manager_get_geocoordinate_info(fence_id,	&geocoordinate_info);
		if (ret != FENCE_ERR_NONE || geocoordinate_info == NULL) {
			LOGI_GEOFENCE("can not get geocoordinate_info");
			return FENCE_ERR_SQLITE_FAIL;
		}
		item_data->priv = (void *) geocoordinate_info;
	} else {
		bssid_info_s *bssid_info = NULL;
		int ret = FENCE_ERR_NONE;

		ret = geofence_manager_get_bssid_info(fence_id, &bssid_info);
		if (ret != FENCE_ERR_NONE || bssid_info == NULL) {
			LOGI_GEOFENCE("can not get bssid_info");
			return FENCE_ERR_SQLITE_FAIL;
		}
		item_data->priv = (void *) bssid_info;
	}
	return FENCE_ERR_NONE;
}

static void __add_left_fences(gpointer user_data)
{
	g_return_if_fail(user_data);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	if (geofence_server->geofence_list != NULL)
		return;
	GList *fence_list = NULL;
	int fence_id = 0;
	int count = 0;

	/*Get the number of fences count*/
	geofence_manager_get_count_of_fences(&count);
	if (count <= 0)
		return;
	/*Fetch the fences from the DB and populate it in the list*/
	geofence_manager_get_fences(NULL, 0, &fence_list);
	fence_list = g_list_first(fence_list);
	while (fence_list) {
		fence_id = GPOINTER_TO_INT(fence_list->data);

		/*if(geofence_server is not restarted by dbus auto activation method[It means phone is rebooted so app should start the fences again. If we start the service by ourself it is waste of power. So we should not do that])
		 * {
		 * geofence_manager_set_running_status(fence_id, 0); // resetting the running-status flag since it is a device reboot
		 * } */

		GeofenceItemData *item_data = (GeofenceItemData *)g_malloc0(sizeof(GeofenceItemData));
		if (FENCE_ERR_NONE != __copy_geofence_to_item_data(fence_id, item_data)) {
			g_free(item_data);
			return;
		}
		LOGI_GEOFENCE("adding fence_id = %d to fence_list", item_data->common_info.fence_id);

		/*Here fences from DB will be added to the list but tracking list is not populated here*/
		geofence_server->geofence_list = g_list_append(geofence_server->geofence_list, item_data);

		fence_list = g_list_next(fence_list);
	}
}

static void _glib_log(const gchar *log_domain, GLogLevelFlags log_level, const gchar *msg, gpointer user_data)
{
	LOGI_GEOFENCE("GLIB[%d] : %s", log_level, msg);
}

int main(int argc, char **argv)
{
	GeofenceServer *geofenceserver = NULL;
	LOGI_GEOFENCE("----------------Starting Server -----------------------------");
	/*Callback registrations*/
	geofence_callbacks cb;
	cb.bt_conn_state_changed_cb = bt_conn_state_changed;
	cb.bt_apater_disable_cb = bt_adp_disable;
	cb.device_display_changed_cb = device_display_changed_cb;
	cb.wifi_conn_state_changed_cb = wifi_conn_state_changed;
	cb.wifi_device_state_changed_cb = wifi_device_state_changed;
	cb.network_evt_cb = geofence_network_evt_cb;
	cb.bt_discovery_cb = bt_adapter_device_discovery_state_cb;
	cb.gps_setting_changed_cb = gps_setting_changed_cb;
	cb.wifi_rssi_level_changed_cb = wifi_rssi_level_changed;
#if !GLIB_CHECK_VERSION(2, 35, 0)
	g_type_init();
#endif
	geofenceserver = g_new0(GeofenceServer, 1);
	if (!geofenceserver) {
		LOGI_GEOFENCE("GeofenceServer create fail");
		return 1;
	}

	if (alarmmgr_init(PACKAGE_NAME) != ALARMMGR_RESULT_SUCCESS) {
		LOGI_GEOFENCE("alarmmgr_init fail");
		return 1;
	}

	g_log_set_default_handler(_glib_log, geofenceserver);

	/*Initialize the geofence manager where DB related activities exists*/
	__init_geofencemanager(geofenceserver);

	/*This will read the DB and populate the list*/
	__add_left_fences(geofenceserver);

	/*This call goes and registers the cb with Server.c where all the wifi,bt event callbacks are triggered*/
	_geofence_register_update_callbacks(&cb, geofenceserver);

	/*This call goes and make initializations of bt and wifi stacks and then register the callbacks with them*/
	_geofence_initialize_geofence_server(geofenceserver);

#ifdef TIZEN_ENGINEER_MODE
	_init_log();
#endif

	/* This call goes to Geofence_dbus_server.c and creates the actual server dbus connection who will interact with the client*/
	geofence_dbus_callback_s *dbus_callback;
	dbus_callback = g_new0(geofence_dbus_callback_s, 1);
	g_return_val_if_fail(dbus_callback, GEOFENCE_DBUS_SERVER_ERROR_MEMORY);

	dbus_callback->add_geofence_cb = dbus_add_fence_cb;
	dbus_callback->delete_geofence_cb = dbus_remove_fence_cb;
	dbus_callback->get_geofences_cb = dbus_get_geofences_cb;
	dbus_callback->enable_geofence_cb = dbus_enable_geofence_cb;
	dbus_callback->start_geofence_cb = dbus_start_geofence_cb;
	dbus_callback->stop_geofence_cb = dbus_stop_geofence_cb;
	dbus_callback->add_place_cb = dbus_add_place_cb;
	dbus_callback->update_place_cb = dbus_update_place_cb;
	dbus_callback->delete_place_cb = dbus_remove_place_cb;
	dbus_callback->get_place_name_cb = dbus_get_place_name_cb;
	dbus_callback->get_places_cb = dbus_get_places_cb;

	geofence_dbus_server_create(&(geofenceserver->geofence_dbus_server), dbus_callback, (void *) geofenceserver);

	LOGD_GEOFENCE("lbs_geofence_server_creation done");

	geofenceserver->loop = g_main_loop_new(NULL, TRUE);
	g_main_loop_run(geofenceserver->loop);

	LOGD_GEOFENCE("GEOFENCE_manager deamon Stop....");

	/*This call goes to server.c and deregisters all the callbacks w.r.t bt and wifi*/
	_geofence_deinitialize_geofence_server();

#ifdef TIZEN_ENGINEER_MODE
	_deinit_log();
#endif
	/*This call goes to Geofence_dbus_server.c and deletes the memory allocated to the server, hence destroys it*/
	geofence_dbus_server_destroy(geofenceserver->geofence_dbus_server);
	LOGD_GEOFENCE("lbs_server_destroy called");

	g_free(dbus_callback);
	g_main_loop_unref(geofenceserver->loop);
	g_free(geofenceserver);

	/*Closing the DB and the handle is aquired again when geofence server comes up.*/
	geofence_manager_close_db();

	return 0;
}
