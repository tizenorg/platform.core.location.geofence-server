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

#ifndef _GEOFENCE_MANAGER_APPMAN_H_
#define _GEOFENCE_MANAGER_APPMAN_H_

#include <dlog.h>
#include <glib.h>

#include "geofence_server_data_types.h"
#include "geofence_server_private.h"

int geofence_manager_db_init(void);
int geofence_manager_db_reset(void);
int geofence_manager_close_db(void);
int geofence_manager_set_common_info(fence_common_info_s *fence_common_info, int *fence_id);
int geofence_manager_get_place_list_from_db(int *number_of_places, GList **places);
int geofence_manager_get_fence_list_from_db(int *number_of_fences, GList **fences, int place_id);
int geofence_manager_get_fenceid_list_from_db(int *number_of_fences, GList **fences, int place_id);
int geofence_manager_get_bssid_info(int fence_id, bssid_info_s **bt_info);
int geofence_manager_update_bssid_info(const int fence_id, bssid_info_s *bssid_info);
int geofence_manager_set_bssid_info(int fence_id, bssid_info_s *bt_info);
int geofence_manager_get_ap_info(int fence_id, GList **ap_list);
int geofence_manager_set_ap_info(int fence_id, GList *ap_list);
int geofence_manager_get_place_name(int place_id, char **name);
int geofence_manager_get_place_info(int place_id, place_info_s **place_info);
int geofence_manager_get_geocoordinate_info(int fence_id, geocoordinate_info_s **geocoordinate_info);
int geofence_manager_update_geocoordinate_info(int fence_id, geocoordinate_info_s *geocoordinate_info);
int geofence_manager_set_geocoordinate_info(int fence_id, geocoordinate_info_s *geocoordinate_info);
int geofence_manager_set_place_info(place_info_s *place_info, int *place_id);
int geofence_manager_update_place_info(int place_id, const char *place_info);
int geofence_manager_get_enable_status(const int fence_id, int *status);
int geofence_manager_set_enable_status(int fence_id, int status);
int geofence_manager_get_appid_from_places(int place_id, char **appid);
int geofence_manager_set_appid_to_places(int place_id, char *appid);
int geofence_manager_get_appid_from_geofence(int fence_id, char **appid);
int geofence_manager_set_appid_to_geofence(int fence_id, char *appid);
int geofence_manager_get_ble_info_from_geofence(int fence_id, char **ble_info);
int geofence_manager_set_ble_info_to_geofence(int fence_id, char *ble_info);
int geofence_manager_get_geofence_type(int fence_id, geofence_type_e *fence_type);
int geofence_manager_get_place_id(int fence_id, int *place_id);
int geofence_manager_set_geofence_type(int fence_id, geofence_type_e fence_type);
int geofence_manager_get_access_type(int fence_id, int place_id, access_type_e *fence_type);
int geofence_manager_get_placeid_from_geofence(int fence_id, int *place_id);
int geofence_manager_get_running_status(int fence_id, int *status);
int geofence_manager_set_running_status(int fence_id, int status);
int geofence_manager_get_direction(int fence_id, geofence_direction_e *direction);
int geofence_manager_set_direction(int fence_id, geofence_direction_e direction);
int geofence_manager_delete_fence_info(int fence_id);
int geofence_manager_delete_place_info(int place_id);
int geofence_manager_copy_wifi_info(wifi_info_s *src_wifi, wifi_info_s **dest_wifi);
int geofence_manager_create_wifi_info(int fence_id, char *bssid, wifi_info_s **new_wifi);
int geofence_manager_get_fences(const char *app_id, geofence_type_e fence_type, GList **fences);
int geofence_manager_get_count_of_fences(int *count);
int geofence_manager_get_place_count_by_placeid(int place_id, int *count);

#endif /* _GEOFENCE_MANAGER_APPMAN_H_ */
