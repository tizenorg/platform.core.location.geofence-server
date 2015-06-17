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

/**
 * @file	geofence_server_internal.h
 * @brief	Geofence server related internal APIs
 *
 */

#ifndef GEOFENCE_MANAGER_INTERNAL_H_
#define GEOFENCE_MANAGER_INTERNAL_H_

/*gboolean _get_cell_db (GeofenceItemData *item_data);
void _set_cell_db (GeofenceItemData *item_data, gboolean is_saved);
gboolean _check_cell_out (GeofenceServer *geofence_server, cell_status_e status);
gboolean _check_cell_db_existence (GeofenceServer *geofence_server, gboolean is_enabled);*/

/**
* @brief	Gets the min distance to next geofence
* @param[in] cur_lat	The current location latitude.
* @param[in] cur_lon	The current location longitude.
* @param[in] geofence_server	The geofence server
* @return		double
* @retval		The min distance to next geofence
*/
double _get_min_distance(double cur_lat, double cur_lon, GeofenceServer *geofence_server);

/**
* @brief	Gets the geofence using fence id
* @param[in] fence_id	The geofence id
* @param[in] geofence_server	The geofence server
* @return		GeofenceItemData
* @retval		GeofenceItemData
*/
GeofenceItemData *__get_item_by_fence_id(gint fence_id, GeofenceServer *geofence_server);

#endif /* GEOFENCE_MANAGER_INTERNAL_H_ */
