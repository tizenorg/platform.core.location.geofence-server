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

#include <stdlib.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <glib.h>

#include "geofence_server.h"
#include "server.h"
#include "debug_util.h"
#include "geofence_server_private.h"
#include "geofence_server_db.h"
#include "geofence_server_log.h"
#include "geofence_server_internal.h"

static void emit_wifi_geofence_inout_changed(GeofenceServer *geofence_server, int fence_id, int fence_status)
{
	LOGD_GEOFENCE("emit_wifi_geofence_inout_changed");
	char *app_id = NULL;
	int ret = FENCE_ERR_NONE;

	ret = geofence_manager_get_appid_from_geofence(fence_id, &app_id);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Error getting the app_id for fence id[%d]", fence_id);
		return;
	}
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data == NULL) {
		LOGD_GEOFENCE("getting item data failed. fence_id [%d]", fence_id);
		g_free(app_id);
		return;
	}
	if (fence_status == GEOFENCE_FENCE_STATE_IN) {
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_IN) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server, app_id,	fence_id, item_data->common_info.access_type, GEOFENCE_EMIT_STATE_IN);
			item_data->common_info.status = GEOFENCE_FENCE_STATE_IN;
		}
	} else if (fence_status == GEOFENCE_FENCE_STATE_OUT) {
		if (item_data->common_info.status != GEOFENCE_FENCE_STATE_OUT) {
			geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server, app_id,	fence_id, item_data->common_info.access_type, GEOFENCE_EMIT_STATE_OUT);
			item_data->common_info.status =	GEOFENCE_FENCE_STATE_OUT;
		}
	}
	if (app_id)
		free(app_id);
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
		LOGI_GEOFENCE("Stopping scan as there is no BLE address found");
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
		/* Add the string to the database. */
		geofence_manager_set_ble_info_to_geofence(geofence_server->connectedTrackingWifiFenceId, geofence_server->ble_info);
	}
}

static void emit_wifi_geofence_proximity_changed(GeofenceServer *geofence_server, int fence_id, int fence_proximity_status)
{
	FUNC_ENTRANCE_SERVER
	LOGD_GEOFENCE("emit_wifi_geofence_proximity_changed");
	char *app_id = NULL;
	int ret = FENCE_ERR_NONE;

	ret = geofence_manager_get_appid_from_geofence(fence_id, &app_id);
	if (ret != FENCE_ERR_NONE) {
		LOGE("Error getting the app_id for fence id[%d]", fence_id);
		return;
	}
	GeofenceItemData *item_data = __get_item_by_fence_id(fence_id, geofence_server);
	if (item_data == NULL) {
		LOGD_GEOFENCE("getting item data failed. fence_id [%d]", fence_id);
		g_free(app_id);
		return;
	}

	if (fence_proximity_status != item_data->common_info.proximity_status) {
		geofence_dbus_server_send_geofence_proximity_changed(geofence_server->geofence_dbus_server, app_id, fence_id, item_data->common_info.access_type, fence_proximity_status, GEOFENCE_PROXIMITY_PROVIDER_WIFI);
		if (geofence_server->connectedTrackingWifiFenceId == fence_id) {
			if (fence_proximity_status == GEOFENCE_PROXIMITY_IMMEDIATE) {
				LOGD_GEOFENCE("WIFI Fence. Scanning for BLE and storing in DB");
				g_stpcpy(geofence_server->ble_info, "");
				ret = bt_adapter_le_start_scan(bt_le_scan_result_cb, geofence_server);
				if (ret != BT_ERROR_NONE) {
					LOGE_GEOFENCE("Fail to start ble scan. %d", ret);
				}
			} else if (item_data->common_info.proximity_status == GEOFENCE_PROXIMITY_IMMEDIATE) { /* Stopping the scan if state changes from imm to somethingelse */
				ret = bt_adapter_le_stop_scan();
				if (ret != BT_ERROR_NONE)
					LOGE_GEOFENCE("Unable to stop the BLE scan/ Stopped already, error: %d", ret);
			}
		}
		item_data->common_info.proximity_status = fence_proximity_status;
	}
	if (app_id)
		free(app_id);
}

void wifi_rssi_level_changed(wifi_rssi_level_e rssi_level, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	wifi_ap_h ap_h;
	char *bssid = NULL;
	geofence_proximity_state_e state = GEOFENCE_PROXIMITY_UNCERTAIN;
	wifi_error_e rv = WIFI_ERROR_NONE;
	LOGI_GEOFENCE("running cnt: %d, connected id: %d", geofence_server->running_wifi_cnt, geofence_server->connectedTrackingWifiFenceId);
	if (geofence_server->running_wifi_cnt > 0 && geofence_server->connectedTrackingWifiFenceId != -1) {
		rv = wifi_get_connected_ap(&ap_h);
		if (rv != WIFI_ERROR_NONE) {
			LOGE_GEOFENCE("Fail to get the connected AP: Error - %d", rv);
			return;
		}
		rv = wifi_ap_get_bssid(ap_h, &bssid);
		if (rv != WIFI_ERROR_NONE) {
			LOGI_GEOFENCE("Fail to get the bssid: [%d]", rv);
		} else {
			/*Emit the proximity alert here using mConnectedFenceId*/
			if (rssi_level == WIFI_RSSI_LEVEL_4)
				state = GEOFENCE_PROXIMITY_IMMEDIATE;
			else if (rssi_level ==  WIFI_RSSI_LEVEL_3)
				state = GEOFENCE_PROXIMITY_NEAR;
			else
				state = GEOFENCE_PROXIMITY_FAR;
			emit_wifi_geofence_proximity_changed(geofence_server, geofence_server->connectedTrackingWifiFenceId, state);
		}
	}
}

void wifi_device_state_changed(wifi_device_state_e state, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);

	int fence_id = 0;
	geofence_type_e fence_type;
	GeofenceItemData *item_data = NULL;

	GList *fence_list = g_list_first(geofence_server->tracking_list);

	for (; fence_list != NULL; fence_list = g_list_next(fence_list)) {
		fence_id = GPOINTER_TO_INT(fence_list->data);
		item_data = NULL;
		item_data = __get_item_by_fence_id(fence_id, geofence_server);

		if (item_data == NULL)
			continue;

		fence_type = item_data->common_info.type;

		if (fence_type != GEOFENCE_TYPE_WIFI)
			continue;

		if (state == WIFI_DEVICE_STATE_DEACTIVATED) {
			LOGD_GEOFENCE("Emitted to fence_id [%d] GEOFENCE_FENCE_STATE_OUT", fence_id);
			emit_wifi_geofence_inout_changed(geofence_server, fence_id, GEOFENCE_FENCE_STATE_OUT);
			emit_wifi_geofence_proximity_changed(geofence_server, fence_id, GEOFENCE_PROXIMITY_UNCERTAIN);
		}
	}

	LOGD_GEOFENCE("exit");
}

void __geofence_check_wifi_matched_bssid(wifi_connection_state_e state,	char *bssid, void *user_data)
{
	LOGD_GEOFENCE("Comparing the matching bssids");
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	GList *tracking_fences = g_list_first(geofence_server->tracking_list);
	int tracking_fence_id = 0;
	bssid_info_s *bssid_info = NULL;
	geofence_type_e type = -1;

	/*Wifi tracking list has to be traversed here*/
	while (tracking_fences) {
		tracking_fence_id = GPOINTER_TO_INT(tracking_fences->data);
		tracking_fences = g_list_next(tracking_fences);
		if (FENCE_ERR_NONE != geofence_manager_get_geofence_type(tracking_fence_id, &type)) {
			LOGD_GEOFENCE("Error fetching the fence type/ fence does not exist");
			return;
		}
		if (type == GEOFENCE_TYPE_WIFI) {
			if (FENCE_ERR_NONE != geofence_manager_get_bssid_info(tracking_fence_id, &bssid_info)) {
				LOGD_GEOFENCE("Error fetching the fence bssid info/ fence does not exist");
				return;
			}
			if (!g_ascii_strcasecmp(bssid_info->bssid, bssid) || !g_ascii_strcasecmp(g_strdelimit(bssid_info->bssid, "-", ':'), bssid)) {
				LOGI_GEOFENCE("Matched wifi fence: fence_id = %d, bssid = %s", tracking_fence_id, bssid_info->bssid);
				if (state == WIFI_CONNECTION_STATE_CONNECTED) {
					emit_wifi_geofence_inout_changed(geofence_server, tracking_fence_id, GEOFENCE_FENCE_STATE_IN);
					geofence_server->connectedTrackingWifiFenceId = tracking_fence_id;
				} else if (state == WIFI_CONNECTION_STATE_DISCONNECTED) {
					emit_wifi_geofence_inout_changed(geofence_server, tracking_fence_id, GEOFENCE_FENCE_STATE_OUT);
					emit_wifi_geofence_proximity_changed(geofence_server, tracking_fence_id, GEOFENCE_PROXIMITY_UNCERTAIN);
					geofence_server->connectedTrackingWifiFenceId = -1;
				}
				break;	/*Because there cannot be two APs connected at the same time*/
			}
		}
	}

}

void wifi_conn_state_changed(wifi_connection_state_e state, wifi_ap_h ap, void *user_data)
{
	LOGD_GEOFENCE("wifi_conn_state_changed");
	GeofenceServer *geofence_server = (GeofenceServer *)user_data;
	char *ap_bssid = NULL;
	int rv = 0;
	g_return_if_fail(geofence_server);
	rv = wifi_ap_get_bssid(ap, &ap_bssid);

	if (rv != WIFI_ERROR_NONE) {
		LOGD_GEOFENCE("Failed to get the bssid");
		return;
	}

	if (state == WIFI_CONNECTION_STATE_CONNECTED) {
		LOGD_GEOFENCE("Wifi connected to [%s].", ap_bssid);
		__geofence_check_wifi_matched_bssid(state, ap_bssid, user_data);
	} else if (state == WIFI_CONNECTION_STATE_DISCONNECTED) {
		LOGD_GEOFENCE("Wifi disconnected with [%s].", ap_bssid);
		__geofence_check_wifi_matched_bssid(state, ap_bssid, user_data);
	}
}
