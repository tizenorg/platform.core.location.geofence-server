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

#include "geofence_server.h"
#include "geofence_server_private.h"
#include "geofence_server_log.h"
#include "debug_util.h"

static gint __find_custom_item_by_fence_id(gconstpointer data, gconstpointer compare_with)
{
	g_return_val_if_fail(data, 1);
	g_return_val_if_fail(compare_with, -1);
	int ret = -1;

	GeofenceItemData *item_data = (GeofenceItemData *)data;
	int *fence_id = (int *)compare_with;
	if (item_data->common_info.fence_id == *fence_id) {
		LOGD_GEOFENCE("Found[%d]", *fence_id);
		ret = 0;
	}

	return ret;
}

GeofenceItemData *__get_item_by_fence_id(gint fence_id, GeofenceServer *geofence_server)
{
	g_return_val_if_fail(geofence_server, NULL);

	geofence_server->geofence_list = g_list_first(geofence_server->geofence_list);
	GList *found_item = NULL;
	found_item = g_list_find_custom(geofence_server->geofence_list, &fence_id, __find_custom_item_by_fence_id);
	if (found_item == NULL || found_item->data == NULL) {
		LOGD_GEOFENCE("item_data is not found. found_item[%d]",	found_item);
		return NULL;
	}
	/*Get the item from the list and return it*/
	return (GeofenceItemData *)found_item->data;
}

double _get_min_distance(double cur_lat, double cur_lon, GeofenceServer *geofence_server)
{
	GList *fence_list = NULL;
	GList *item_list = NULL;
	int fence_id = 0;
	GeofenceItemData *item_data = NULL;
	geocoordinate_info_s *geocoordinate_info = NULL;
	int ret = 0;
	double min_dist = 100000.0, distance = 0.0;

	fence_list = geofence_server->tracking_list;

	item_list = g_list_first(fence_list);
	while (item_list) {
		fence_id = GPOINTER_TO_INT(item_list->data);
		LOGD_GEOFENCE("FENCE Id to find distance :: %d", fence_id);

		item_data = __get_item_by_fence_id(fence_id, geofence_server);
		if (item_data && item_data->common_info.type == GEOFENCE_TYPE_GEOPOINT) {
			geocoordinate_info = (geocoordinate_info_s *)item_data->priv;
			/* get_current_position/ check_fence_in/out  for geoPoint*/
			location_manager_get_distance(cur_lat, cur_lon,	geocoordinate_info->latitude, geocoordinate_info->longitude, &distance);
			if (distance < min_dist) 
				min_dist = distance;
		}
		item_list = g_list_next(item_list);
	}
	LOGD_GEOFENCE("Min : %f", min_dist);

	return min_dist;
}
