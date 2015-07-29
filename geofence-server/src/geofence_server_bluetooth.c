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
#include <glib.h>

#include "geofence_server.h"
#include "server.h"
#include "debug_util.h"
#include "geofence_server_log.h"
#include "geofence_server_private.h"
#include "geofence_server_internal.h"
#include "geofence_server_db.h"

static gboolean __geofence_check_fence_status(int fence_status, GeofenceItemData *item_data)
{
	FUNC_ENTRANCE_SERVER
	gboolean ret = FALSE;

	if (fence_status != item_data->common_info.status) {
		LOGD_GEOFENCE("Fence status changed. %d -> %d", item_data->common_info.status, fence_status);
		ret = TRUE;
		item_data->common_info.status = fence_status;	/*update status*/
	}
	return ret;
}

static void emit_bt_geofence_inout_changed(GeofenceServer *geofence_server, GeofenceItemData *item_data, int fence_status)
{
	FUNC_ENTRANCE_SERVER
	char *app_id = (char *)g_malloc0(sizeof(char) * APP_ID_LEN);
	g_strlcpy(app_id, item_data->common_info.appid, APP_ID_LEN);
	if (app_id == NULL) {
		LOGD_GEOFENCE("get app_id failed. fence_id [%d]", item_data->common_info.fence_id);
		return;
	}

	if (fence_status == GEOFENCE_FENCE_STATE_IN) {
		geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server, app_id, item_data->common_info.fence_id, item_data->common_info.access_type, GEOFENCE_EMIT_STATE_IN);
	} else if (fence_status == GEOFENCE_FENCE_STATE_OUT) {
		geofence_dbus_server_send_geofence_inout_changed(geofence_server->geofence_dbus_server, app_id, item_data->common_info.fence_id, item_data->common_info.access_type, GEOFENCE_EMIT_STATE_OUT);
	}

	if (item_data->client_status == GEOFENCE_CLIENT_STATUS_START) {
		item_data->client_status = GEOFENCE_CLIENT_STATUS_RUNNING;
	}
	if (app_id)
		free(app_id);
}

static void __geofence_check_bt_fence_type(gboolean connected, const char *bssid, void *data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *)data;
	g_return_if_fail(geofence_server);
	g_return_if_fail(bssid);

	int fence_id = 0;
	geofence_type_e fence_type;
	GeofenceItemData *item_data = NULL;
	bssid_info_s *bt_info_from_db = NULL;
	bssid_info_s *bt_info_from_list = NULL;

	GList *fence_list = g_list_first(geofence_server->tracking_list);

	for (; fence_list != NULL; fence_list = g_list_next(fence_list)) {
		int ret = FENCE_ERR_NONE;
		item_data = NULL;
		fence_id = GPOINTER_TO_INT(fence_list->data);
		item_data = __get_item_by_fence_id(fence_id, geofence_server);

		if (item_data == NULL)
			continue;

		fence_type = item_data->common_info.type;

		if (fence_type != GEOFENCE_TYPE_BT)
			continue;

		bt_info_from_list = (bssid_info_s *) item_data->priv;

		if (bt_info_from_list == NULL || bt_info_from_list->enabled == FALSE)
			continue;

		ret = geofence_manager_get_bssid_info(fence_id, &bt_info_from_db);

		if (bt_info_from_db == NULL) {
			LOGD_GEOFENCE("Failed to get bt_info. Fence Id[%d], Error[%d]", fence_id, ret);
			continue;
		}
		LOGD_GEOFENCE("bt_info->bssid [%s]", bt_info_from_db->bssid);

		if (g_ascii_strcasecmp(bt_info_from_db->bssid, bssid) == 0) {
			if (connected) {	/* connected => FENCE_IN*/
				if (__geofence_check_fence_status(GEOFENCE_FENCE_STATE_IN, item_data) == TRUE) {
					LOGD_GEOFENCE("Emitted to fence_id [%d] GEOFENCE_FENCE_STATE_IN", fence_id);
					emit_bt_geofence_inout_changed(geofence_server, item_data, GEOFENCE_FENCE_STATE_IN);
				}
			} else {	/* disconnected => FENCE_OUT*/
				if (__geofence_check_fence_status(GEOFENCE_FENCE_STATE_OUT, item_data) == TRUE) {
					LOGD_GEOFENCE("Emitted to fence_id [%d] GEOFENCE_FENCE_STATE_OUT", fence_id);
					emit_bt_geofence_inout_changed(geofence_server, item_data, GEOFENCE_FENCE_STATE_OUT);
				}
			}
		}
		g_slice_free(bssid_info_s, bt_info_from_db);
		bt_info_from_db = NULL;
		bt_info_from_list = NULL;
	}

	LOGD_GEOFENCE("exit");
}

void bt_conn_state_changed(gboolean connected, bt_device_connection_info_s *conn_info, void *user_data)
{
	FUNC_ENTRANCE_SERVER
	GeofenceServer *geofence_server = (GeofenceServer *) user_data;
	g_return_if_fail(geofence_server);
	g_return_if_fail(conn_info);

	if (connected == true) {
		if (conn_info->remote_address == NULL) {
			LOGD_GEOFENCE("Bluetooth device connected, but remote_address not exist.");
		} else {
			LOGD_GEOFENCE("Bluetooth device connected [%s].", conn_info->remote_address);
			__geofence_check_bt_fence_type(connected, conn_info->remote_address, user_data);
		}
	} else {
		LOGD_GEOFENCE("Bluetooth device disconnected [%s]. reason [%d]", conn_info->remote_address, conn_info->disconn_reason);
		__geofence_check_bt_fence_type(connected, conn_info->remote_address, user_data);
	}
	LOGD_GEOFENCE("exit");
}

void bt_adp_disable(gboolean connected, void *user_data)
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

		if (fence_type != GEOFENCE_TYPE_BT)	/* check only bluetooth type*/
			continue;

		if (connected == FALSE) {
			if (__geofence_check_fence_status(GEOFENCE_FENCE_STATE_OUT, item_data) == TRUE) {
				LOGD_GEOFENCE("Emitted to fence_id [%d] GEOFENCE_FENCE_STATE_OUT", fence_id);
				emit_bt_geofence_inout_changed(geofence_server, item_data, GEOFENCE_FENCE_STATE_OUT);
			}
		}
	}

	LOGD_GEOFENCE("exit");
}
