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

#include <stdbool.h>
#include "module_internal.h"
#include "log.h"

/** Length of bssid */
#define WLAN_BSSID_LEN	18
#define APP_ID_LEN	64
#define FENCE_NAME_LEN	64

#define GEOFENCE_DEFAULT_RADIUS		200	/* Default 200 m */

/*
* TODO: after transiting geofence-server package on CMake build following structures must be elliminated,
* because they are copying existing structures in a sub-folder geofence-server
*/

/**
 * This enumeration describe the cell geofence in and out status.
 */
typedef enum {
	CELL_UNKNOWN = -1,
	CELL_OUT = 0,
	CELL_IN = 1
} cell_status_e;

/**
 * This enumeration descript the geofence fence state.
 */
typedef enum {
	GEOFENCE_FENCE_STATE_UNCERTAIN = -1,
	GEOFENCE_FENCE_STATE_OUT = 0,
	GEOFENCE_FENCE_STATE_IN = 1,
} geofence_fence_state_e;

/**
 * The geofence common information structure
 */
typedef struct {
	int fence_id;
	geofence_fence_state_e status;
	geofence_manager_type_e type;	/* Geocoordinate/WIFI/CurrentLocation/Bluetooth */
	char fence_name[FENCE_NAME_LEN];
	char appid[APP_ID_LEN];
	int smart_assist_id;
	gboolean cell_db;
	time_t last_update;
} fence_common_info_s;

/**
 *The geocoordinate structure
 */
typedef struct {
	double latitude;
	double longitude;
	double radius;
} geocoordinate_info_s;

/**
 *The wifi info structure
 */
typedef struct {
	char bssid[WLAN_BSSID_LEN];
} wifi_info_s;

/**
 *The bluetooth info structure
 */
typedef struct {
	char bssid[WLAN_BSSID_LEN];
	gboolean enabled;	/* bluetooth callback receive or not */
} bssid_info_s;

typedef enum {
	GEOFENCE_CLIENT_STATUS_NONE,
	GEOFENCE_CLIENT_STATUS_FIRST_LOCATION,
	GEOFENCE_CLIENT_STATUS_START,
	GEOFENCE_CLIENT_STATUS_RUNNING
} geofence_client_status_e;

/**
 *The GeofenceItemData structure
 */
typedef struct {
	double distance;
#ifdef __WIFI_DB_SUPPORTED__
	int wifi_db_set;
	wifi_status_e wifi_direction;
	wifi_status_e priv_wifi_direction;
#endif
	geofence_client_status_e client_status;
	cell_status_e cell_status;
	fence_common_info_s common_info;
	void *priv;		/* Save (latitude, longitude and radius) or AP list */

	bool is_wifi_status_in;
	bool is_bt_status_in;
} GeofenceItemData;

static gint __find_custom_item_by_fence_id(gconstpointer data, gconstpointer compare_with)
{
	g_return_val_if_fail(data, 1);
	g_return_val_if_fail(compare_with, -1);
	int ret = -1;

	GeofenceItemData *item_data = (GeofenceItemData *) data;
	int *fence_id = (int *) compare_with;
	if (item_data->common_info.fence_id == *fence_id) {
		ret = 0;
	}

	return ret;
}

GeofenceItemData *__get_item_by_fence_id(gint fence_id, GList *geofence_list)
{
	g_return_val_if_fail(geofence_list, NULL);

	geofence_list = g_list_first(geofence_list);
	GList *found_item = NULL;
	found_item = g_list_find_custom(geofence_list, &fence_id, __find_custom_item_by_fence_id);
	if (found_item == NULL || found_item->data == NULL) {
		MOD_LOGD("item_data is not found. found_item[%d]", found_item);
		return NULL;
	}

	/*Get the item from the list and return it*/
	return (GeofenceItemData *) found_item->data;
}
