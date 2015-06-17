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

#define TIZEN_ENGINEER_MODE
#ifdef TIZEN_ENGINEER_MODE
#include "geofence_server_log.h"
#endif

#define GEOFENCE_SERVER_SERVICE_NAME	"org.tizen.lbs.Providers.GeofenceServer"
#define GEOFENCE_SERVER_SERVICE_PATH	"/org/tizen/lbs/Providers/GeofenceServer"
#define TIME_INTERVAL	5

#define SMART_ASSIST_HOME	1

#define SMART_ASSIST_TIMEOUT			60	/* Refer to LPP */
#define GEOFENCE_DEFAULT_RADIUS			200	/* Default 200 m */

#define NPS_TIMEOUT				180

#define MYPLACES_APPID	"org.tizen.myplace"
#define DEFAULT_PLACE_HOME 1
#define DEFAULT_PLACE_OFFICE 2
#define DEFAULT_PLACE_CAR 3

static int __nps_alarm_cb(alarm_id_t alarm_id, void *user_data);
static int __nps_timeout_cb(alarm_id_t alarm_id, void *user_data);
static void __add_left_fences(gpointer user_data);
static void __stop_geofence_service(gint fence_id, const gchar *app_id, gpointer userdata);
static void __start_activity_service(GeofenceServer *geofence_server);
static void __stop_activity_service(GeofenceServer *geofence_server);
static void __activity_cb(activity_type_e type, const activity_data_h data, double timestamp, activity_error_e error, void *user_data);

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

#if 0 /* Not used */
static const char *__bt_get_error_message(bt_error_e err)
{
	switch (err) {
	case BT_ERROR_NONE:
		return "BT_ERROR_NONE";
	case BT_ERROR_CANCELLED:
		return "BT_ERROR_CANCELLED";
	case BT_ERROR_INVALID_PARAMETER:
		return "BT_ERROR_INVALID_PARAMETER";
	case BT_ERROR_OUT_OF_MEMORY:
		return "BT_ERROR_OUT_OF_MEMORY";
	case BT_ERROR_RESOURCE_BUSY:
		return "BT_ERROR_RESOURCE_BUSY";
	case BT_ERROR_TIMED_OUT:
		return "BT_ERROR_TIMED_OUT";
	case BT_ERROR_NOW_IN_PROGRESS:
		return "BT_ERROR_NOW_IN_PROGRESS";
	case BT_ERROR_NOT_INITIALIZED:
		return "BT_ERROR_NOT_INITIALIZED";
	case BT_ERROR_NOT_ENABLED:
		return "BT_ERROR_NOT_ENABLED";
	case BT_ERROR_ALREADY_DONE:
		return "BT_ERROR_ALREADY_DONE";
	case BT_ERROR_OPERATION_FAILED:
		return "BT_ERROR_OPERATION_FAILED";
	case BT_ERROR_NOT_IN_PROGRESS:
		return "BT_ERROR_NOT_IN_PROGRESS";
	case BT_ERROR_REMOTE_DEVICE_NOT_BONDED:
		return "BT_ERROR_REMOTE_DEVICE_NOT_BONDED";
	case BT_ERROR_AUTH_REJECTED:
		return "BT_ERROR_AUTH_REJECTED";
	case BT_ERROR_AUTH_FAILED:
		return "BT_ERROR_AUTH_FAILED";
	case BT_ERROR_REMOTE_DEVICE_NOT_FOUND:
		return "BT_ERROR_REMOTE_DEVICE_NOT_FOUND";
	case BT_ERROR_SERVICE_SEARCH_FAILED:
		return "BT_ERROR_SERVICE_SEARCH_FAILED";
	case BT_ERROR_REMOTE_DEVICE_NOT_CONNECTED:
		return "BT_ERROR_REMOTE_DEVICE_NOT_CONNECTED";
#ifndef TIZEN_TV
	case BT_ERROR_PERMISSION_DENIED:
		return "BT_ERROR_PERMISSION_DENIED";
	case BT_ERROR_SERVICE_NOT_FOUND:
		return "BT_ERROR_SERVICE_NOT_FOUND";
#endif
	default:
		return "NOT Defined";
	}
}
#endif

static int __emit_fence_event(GeofenceServer *geofence_server, int place_id, int fence_id, access_type_e access_type, const gchar *app_id, geofence_server_error_e error, geofence_manage_e state)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);

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
		LOGD_GEOFENCE("FENCE_IN to be set, current state: %d",
			item_data->common_info.status);
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_IN) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server,	item_data->common_info.appid, fence_id,	item_data->common_info.access_type, GEOFENCE_EMIT_STATE_IN);
			if (item_data->client_status == GEOFENCE_CLIENT_STATUS_START) {
				item_data->client_status = GEOFENCE_CLIENT_STATUS_RUNNING;
			}
#ifdef TIZEN_ENGINEER_MODE
			GEOFENCE_PRINT_LOG("FENCE_IN")
#endif
		}

		item_data->common_info.status = GEOFENCE_FENCE_STATE_IN;

	} else if (state == GEOFENCE_FENCE_STATE_OUT) {
		LOGD_GEOFENCE("FENCE_OUT to be set, current state: %d",
			item_data->common_info.status);
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_OUT) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server,	item_data->common_info.appid, fence_id,	item_data->common_info.access_type, GEOFENCE_EMIT_STATE_OUT);
			if (item_data->client_status ==	GEOFENCE_CLIENT_STATUS_START) {
				item_data->client_status = GEOFENCE_CLIENT_STATUS_RUNNING;
			}
#ifdef TIZEN_ENGINEER_MODE
			GEOFENCE_PRINT_LOG("FENCE_OUT")
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

static void __check_inout_by_gps(double latitude, double longitude, int fence_id, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	double distance = 0.0;
	LOGD_GEOFENCE("fence_id [%d]", fence_id);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (!item_data || item_data->client_status == GEOFENCE_CLIENT_STATUS_NONE)
		return;

	location_accuracy_level_e level = 0;
	double horizontal = 0.0;
	double vertical = 0.0;
	geofence_fence_state_e status = GEOFENCE_FENCE_STATE_OUT;

	geocoordinate_info_s *geocoordinate_info = (geocoordinate_info_s *)item_data->priv;

	/* get_current_position/ check_fence_in/out  for geoPoint */
	location_manager_get_accuracy(geofence_server->loc_manager, &level, &horizontal, &vertical);
	location_manager_get_distance(latitude, longitude, geocoordinate_info->latitude, geocoordinate_info->longitude,	&distance);

	if (distance >= geocoordinate_info->radius) {
		LOGD_GEOFENCE("FENCE_OUT : based on distance. Distance[%f]", distance);
		status = GEOFENCE_FENCE_STATE_OUT;
	} else {
		LOGD_GEOFENCE("FENCE_IN : based on distance. Distance[%f]", distance);
		status = GEOFENCE_FENCE_STATE_IN;
	}

	if (__emit_fence_inout(geofence_server, item_data->common_info.fence_id, status) == 0 && status == GEOFENCE_FENCE_STATE_IN) {
		LOGD_GEOFENCE("Disable timer");
	}
}

static void __check_current_location_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	location_accuracy_level_e level;
	double hor_acc = 0.0; 
	double ver_acc = 0.0;
	int ret = 0;
	int fence_id = 0;
	GList *tracking_list = NULL;
	GeofenceItemData *item_data = NULL;

	ret = location_manager_get_accuracy(geofence_server->loc_manager, &level, &hor_acc, &ver_acc);
	if (ret == LOCATIONS_ERROR_NONE) {
		LOGD_GEOFENCE("hor_acc:%f, ver_acc:%f", hor_acc, ver_acc);
	}
	if (hor_acc > 500) {
		return;
	}

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
	LOGD_GEOFENCE("Unsetting the position_updated_cb");
	location_manager_unset_position_updated_cb(geofence_server->loc_manager);
	location_manager_stop(geofence_server->loc_manager);
	geofence_server->loc_started = FALSE;
}

static void __geofence_position_changed_cb(double latitude, double longitude, double altitude, time_t timestamp, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	double distance = 0;
	int interval = 0;
	/*Remove the timeout callback that might be running when requesting for fix.*/
	if (geofence_server->nps_timeout_alarm_id != -1) {
		LOGI_GEOFENCE("Removing the timeout alarm from restart gps");
		geofence_server->nps_timeout_alarm_id =	_geofence_remove_alarm(geofence_server->nps_timeout_alarm_id);
	}
	geofence_server->last_loc_time = timestamp;
	__check_current_location_cb(latitude, longitude, altitude, timestamp, user_data);

	/* Distance based alarm */
	distance = _get_min_distance(latitude, longitude, geofence_server);

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

	LOGI_GEOFENCE("Setting the interval of alrm %d s", interval);

	if (geofence_server->nps_alarm_id == -1) {
		LOGI_GEOFENCE("Setting the nps alarm from the callback");
		geofence_server->nps_alarm_id =	_geofence_add_alarm(interval, __nps_alarm_cb, geofence_server);
	}
	return;
}

static void __check_tracking_list(const char *bssid, void *user_data,
	geofence_type_e type)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	int tracking_fence_id = 0;
	GeofenceItemData *item_data = NULL;
	GList *tracking_fences = g_list_first(geofence_server->tracking_list);

	while (tracking_fences) {
		tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
		tracking_fences = g_list_next(tracking_fences);
		item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
		if (item_data != NULL) {
			if (item_data->common_info.type == type) {
				if (type == GEOFENCE_TYPE_WIFI) {
					bssid_info_s *wifi_info = (bssid_info_s *)item_data->priv;

					if ((!g_ascii_strcasecmp(wifi_info->bssid, bssid) || !g_ascii_strcasecmp(g_strdelimit(wifi_info->bssid, "-", ':'), bssid)) && item_data->is_wifi_status_in == false) {
						LOGI_GEOFENCE("Matched wifi fence: fence_id = %d, bssid = %s", item_data->common_info.fence_id,	wifi_info->bssid);
						item_data->is_wifi_status_in =	true;
					}
				} else if (type == GEOFENCE_TYPE_BT) {
					bssid_info_s *bt_info =	(bssid_info_s *)item_data->priv;

					if ((!g_ascii_strcasecmp(bt_info->bssid, bssid) || !g_ascii_strcasecmp(g_strdelimit(bt_info->bssid, "-", ':'), bssid)) && item_data->is_bt_status_in == false) {
						LOGI_GEOFENCE("Matched bt fence: fence_id = %d, bssid received = %s, bssid = %s", item_data->common_info.fence_id, bt_info->bssid, bssid);
						item_data->is_bt_status_in = true;
					}
				}
			}
		} else {
			LOGI_GEOFENCE("No data present for the fence: %d", tracking_fence_id);
		}
	}
}

void bt_adapter_device_discovery_state_cb(int result, bt_adapter_device_discovery_state_e discovery_state, bt_adapter_device_discovery_info_s *discovery_info, void *user_data)
{
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
		LOGI_GEOFENCE("%s %s", discovery_info->remote_address, discovery_info->remote_name);
		LOGI_GEOFENCE("rssi: %d is_bonded: %d", discovery_info->rssi, discovery_info->is_bonded);

		if (geofence_server->running_bt_cnt > 0) {
			for (i = 0; i < discovery_info->service_count; i++) {
				LOGI_GEOFENCE("uuid: %s", discovery_info->service_uuid[i]);
			}
			LOGI_GEOFENCE("Tracking list is being checked for the BT geofence");
			__check_tracking_list(discovery_info->remote_address, geofence_server, GEOFENCE_TYPE_BT);
		}
	}
}

static void geofence_network_evt_cb(net_event_info_t *event_cb, void *user_data)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	GList *tracking_fences = g_list_first(geofence_server->tracking_list);
	GeofenceItemData *item_data = NULL;
	int tracking_fence_id = 0;

	switch (event_cb->Event) {
	case NET_EVENT_WIFI_SCAN_IND:

		LOGD_GEOFENCE("Got WIFI scan Ind : %d\n", event_cb->Error);

		net_profile_info_t *profiles = NULL;
		int num_of_profile = 0;

		if (geofence_server->running_wifi_cnt > 0) {	/*Check only if some wifi fence is running*/
			if (NET_ERR_NONE != net_get_profile_list(NET_DEVICE_WIFI, &profiles, &num_of_profile)) {
				LOGD_GEOFENCE("Failed to get the scanned list");
			} else {
				LOGD_GEOFENCE("Scan results retrieved successfully. No.of profiles: %d", num_of_profile);
				if (num_of_profile > 0 && profiles != NULL) {
					int cnt;
					for (cnt = 0; cnt < num_of_profile; cnt++) {
						net_wifi_profile_info_t *ap_info = &profiles[cnt].ProfileInfo.Wlan;
						LOGD_GEOFENCE("BSSID %s", ap_info->bssid);
						__check_tracking_list(ap_info->bssid, geofence_server, GEOFENCE_TYPE_WIFI);
					}
					LOGD_GEOFENCE("Comparing fences with scan results is done.Now emit the status to the application");
					while (tracking_fences) {
						tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
						tracking_fences = g_list_next(tracking_fences);
						item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
						if (item_data && item_data->common_info.type ==	GEOFENCE_TYPE_WIFI) {
							if (item_data->is_wifi_status_in == true) {
								__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_IN);
							} else {
								__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_OUT);
							}
							item_data->is_wifi_status_in = false;
						}
					}
				}
			}
		}

		break;
	default:
		break;
	}
}

static int __start_gps_positioning(GeofenceServer *geofence_server, location_position_updated_cb callback)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(geofence_server, -1);
	int ret = FENCE_ERR_NONE;

	if (geofence_server->loc_manager == NULL) {
		ret = location_manager_create(LOCATIONS_METHOD_GPS, &geofence_server->loc_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to create location_manager_h: %d", ret);
			return FENCE_ERR_UNKNOWN;
		}
	}

	if (geofence_server->loc_started == FALSE) {
		ret = location_manager_set_position_updated_cb(geofence_server->loc_manager, callback, 1, (void *) geofence_server);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to set callback. %d", ret);
			return FENCE_ERR_UNKNOWN;
		}

		ret = location_manager_start(geofence_server->loc_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			LOGD_GEOFENCE("Fail to start. %d", ret);
			return FENCE_ERR_UNKNOWN;
		}
		if (geofence_server->nps_timeout_alarm_id == -1)
			geofence_server->nps_timeout_alarm_id =	_geofence_add_alarm(NPS_TIMEOUT, __nps_timeout_cb, geofence_server);

		geofence_server->loc_started = TRUE;
	} else {
		LOGD_GEOFENCE("loc_started TRUE");
	}

	return ret;
}

static void __stop_gps_positioning(gpointer userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;
	int ret = 0;
	if (geofence_server->loc_started == TRUE) {
		ret = location_manager_stop(geofence_server->loc_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
		geofence_server->loc_started = FALSE;
		ret = location_manager_unset_position_updated_cb
			(geofence_server->loc_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
	}

	if (geofence_server->loc_manager != NULL) {
		ret = location_manager_destroy(geofence_server->loc_manager);
		if (ret != LOCATIONS_ERROR_NONE) {
			return;
		}
		geofence_server->loc_manager = NULL;
	}
}

static int __nps_timeout_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__nps_timeout_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("alarm_id : %d", alarm_id);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	geofence_server->nps_timeout_alarm_id = -1;	/*resetting the alarm id*/
	/*Stop the gps for sometime when there is no fix*/
	__stop_gps_positioning(geofence_server);
	geofence_server->nps_alarm_id = _geofence_add_alarm(1 * 60, __nps_alarm_cb, geofence_server);
	display_unlock_state(LCD_OFF, PM_RESET_TIMER);
	return 0;
}

static int __nps_alarm_cb(alarm_id_t alarm_id, void *user_data)
{
	LOGI_GEOFENCE("__nps_alarm_cb");
	g_return_val_if_fail(user_data, -1);
	LOGD_GEOFENCE("alarm_id : %d", alarm_id);
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	__start_gps_positioning(geofence_server, __geofence_position_changed_cb);
	geofence_server->nps_alarm_id = -1;
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
	if (enable == false && geofence_server->running_geopoint_cnt > 0) {
		LOGI_GEOFENCE("Stopping the GPS from settings callback");
		__stop_gps_positioning(geofence_server);

		/*Stop the interval alarm if it is running...*/
		if (geofence_server->nps_alarm_id != -1) {
			LOGI_GEOFENCE("Interval timer removed. ID[%d]", geofence_server->nps_alarm_id);
			geofence_server->nps_alarm_id = _geofence_remove_alarm(geofence_server->nps_alarm_id);
		}
		/*stop the timeout alarm if it is running...*/
		if (geofence_server->nps_timeout_alarm_id != -1) {
			LOGI_GEOFENCE("Timeout timer removed. ID[%d]",
				geofence_server->nps_timeout_alarm_id);
			geofence_server->nps_timeout_alarm_id =	_geofence_remove_alarm(geofence_server->nps_timeout_alarm_id);
		}
		while (tracking_fences) {
			tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
			tracking_fences = g_list_next(tracking_fences);
			item_data = __get_item_by_fence_id(tracking_fence_id, geofence_server);
			if (item_data && item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
				__emit_fence_inout(geofence_server, item_data->common_info.fence_id, GEOFENCE_FENCE_STATE_OUT);
			}
		}
	} else if (enable == true && geofence_server->running_geopoint_cnt > 0) {
		ret = __start_gps_positioning(geofence_server, __geofence_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
			return;
		}
	}
}

#if 0 /* Not used */
static int __check_fence_interval(alarm_id_t alarm_id, void *data)
{
	return TRUE;
}

static void __pause_geofence_service(void *userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
}

static void __resume_geofence_service(void *userdata)
{
	FUNC_ENTRANCE_SERVER;
	g_return_if_fail(userdata);
}
#endif

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

static int __add_fence(const gchar *app_id,
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

static int __add_place(const gchar *app_id,
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

static void __enable_service(gint fence_id, const gchar *app_id, gboolean enable, gpointer userdata)
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

static void __update_place(gint place_id, const gchar *app_id, const gchar *place_name, gpointer userdata)
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

static void __remove_fence(gint fence_id, const gchar *app_id, gpointer userdata)
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

static void __get_place_name(gint place_id, const gchar *app_id, char **place_name, int *error_code, gpointer userdata)
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

static void __remove_place(gint place_id, const gchar *app_id,
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

static void __start_geofence_service(gint fence_id, const gchar *app_id, gpointer userdata)
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
		ret = __start_gps_positioning(geofence_server, __geofence_position_changed_cb);
		if (ret != FENCE_ERR_NONE) {
			LOGE_GEOFENCE("Fail to start gps positioning. Error[%d]", ret);
			geofence_manager_set_running_status(fence_id, (tracking_status - 1));
			__emit_fence_event(geofence_server, place_id, fence_id,	ACCESS_TYPE_UNKNOWN, app_id, GEOFENCE_SERVER_ERROR_IPC, GEOFENCE_MANAGE_FENCE_STARTED);
			return;
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
					/*Stopping GPS...*/
					__stop_gps_positioning(geofence_server);

					/*Stop the interval alarm if it is running...*/
					if (geofence_server->nps_alarm_id != -1) {
						LOGI_GEOFENCE("Interval timer removed. ID[%d]",	geofence_server->nps_alarm_id);
						geofence_server->nps_alarm_id =	_geofence_remove_alarm(geofence_server->nps_alarm_id);
					}
					/*Stop the timeout alarm if it is running...*/
					if (geofence_server->nps_timeout_alarm_id != -1) {
						LOGI_GEOFENCE("Timeout timer removed. ID[%d]", geofence_server->nps_timeout_alarm_id);
						geofence_server->nps_timeout_alarm_id =	_geofence_remove_alarm(geofence_server->nps_timeout_alarm_id);
					}
					
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
			}

			if (tracking_status == 0) {
				/*Remove the fence from the tracklist*/
				LOGD_GEOFENCE("Setting the fence status as uncertain here...");
				item_data->common_info.status =	GEOFENCE_FENCE_STATE_UNCERTAIN;
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

static GVariant *__get_list(int place_id, const gchar *app_id, int *fenceCnt, int *errorCode, gpointer userdata)
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
			case GEOFENCE_TYPE_GEOPOINT:{
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
			case GEOFENCE_TYPE_BT:{
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

static GVariant *__get_place_list(const gchar *app_id, int *placeCnt, int *errorCode, gpointer userdata)
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

	geofence_server->loc_started = FALSE;

	geofence_server->timer_id = -1;
	geofence_server->nps_alarm_id = -1;
	geofence_server->nps_timeout_alarm_id = -1;
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

	/*Callback registrations*/
	geofence_callbacks cb;
	cb.bt_conn_state_changed_cb = bt_conn_state_changed;
	cb.bt_apater_disable_cb = bt_adp_disable;
	cb.wifi_conn_state_changed_cb = wifi_conn_state_changed;
	cb.wifi_device_state_changed_cb = wifi_device_state_changed;
	cb.network_evt_cb = geofence_network_evt_cb;
	cb.bt_discovery_cb = bt_adapter_device_discovery_state_cb;
	cb.gps_setting_changed_cb = gps_setting_changed_cb;

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
	geofence_dbus_server_create(GEOFENCE_SERVER_SERVICE_NAME, GEOFENCE_SERVER_SERVICE_PATH, "geofence_manager", "geofence manager provider", &(geofenceserver->geofence_dbus_server), __add_fence, __add_place, __enable_service, __update_place, __remove_fence, __remove_place, __get_place_name, __get_list, __get_place_list, __start_geofence_service, __stop_geofence_service, (void *) geofenceserver);

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

	g_main_loop_unref(geofenceserver->loop);

	/*Closing the DB and the handle is aquired again when geofence server comes up.*/
	geofence_manager_close_db();

	g_free(geofenceserver);

	return 0;
}
