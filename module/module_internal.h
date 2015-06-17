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

#ifndef __GEOFENCE_MANAGER_PRIVATE_ITEM_DATA_H__
#define __GEOFENCE_MANAGER_PRIVATE_ITEM_DATA_H__

#include <glib.h>
#include <tizen_type.h>
#include <tizen_error.h>
#include "geofence-module.h"

typedef enum {
	GEOFENCE_MANAGER_ERROR_NONE = TIZEN_ERROR_NONE,						/**< Success */
	GEOFENCE_MANAGER_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY,			/**< Out of memory */
	GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER,		/**< Invalid parameter */
	GEOFENCE_MANAGER_ERROR_PERMISSION_DENIED = TIZEN_ERROR_PERMISSION_DENIED,		/**< Permission denied  */
	GEOFENCE_MANAGER_ERROR_NOT_SUPPORTED = TIZEN_ERROR_NOT_SUPPORTED,			/**< Not supported  */
	GEOFENCE_MANAGER_ERROR_NOT_INITIALIZED = TIZEN_ERROR_GEOFENCE_MANAGER		| 0x01,	/**< Geofence Manager is not initialized */
	GEOFENCE_MANAGER_ERROR_INVALID_ID = TIZEN_ERROR_GEOFENCE_MANAGER 		| 0x02,	/**< Geofence ID is not exist */
	GEOFENCE_MANAGER_ERROR_EXCEPTION = TIZEN_ERROR_GEOFENCE_MANAGER 		| 0x03,	/**< exception is occured */
	GEOFENCE_MANAGER_ERROR_ALREADY_STARTED = TIZEN_ERROR_GEOFENCE_MANAGER 		| 0x04,	/**< Geofence is already started */
	GEOFENCE_MANAGER_ERROR_TOO_MANY_GEOFENCE = TIZEN_ERROR_GEOFENCE_MANAGER 	| 0x05,	/**< Too many Geofence */
	GEOFENCE_MANAGER_ERROR_IPC = TIZEN_ERROR_GEOFENCE_MANAGER 			| 0x06,	/**< Error occured in GPS/WIFI/BT */
	GEOFENCE_MANAGER_ERROR_DATABASE = TIZEN_ERROR_GEOFENCE_MANAGER 			| 0x07,	/**< DB error occured in the server side */
	GEOFENCE_MANAGER_ERROR_PLACE_ACCESS_DENIED = TIZEN_ERROR_GEOFENCE_MANAGER 	| 0x08,	/**< Access to specified place is denied */
	GEOFENCE_MANAGER_ERROR_GEOFENCE_ACCESS_DENIED = TIZEN_ERROR_GEOFENCE_MANAGER 	| 0x09,	/**< Access to specified geofence is denied */
} geofence_manager_error_e;

typedef enum {
	GEOFENCE_MANAGER_TYPE_GEOPOINT = 1,
	GEOFENCE_MANAGER_TYPE_WIFI,
	GEOFENCE_MANAGER_TYPE_BT,
} geofence_manager_type_e;

typedef enum {
	GEOFENCE_MANAGER_ACCESS_TYPE_PRIVATE = 1,
	GEOFENCE_MANAGER_ACCESS_TYPE_PUBLIC,
	GEOFENCE_MANAGER_ACCESS_TYPE_UNKNOWN,
} geofence_manager_access_type_e;

typedef enum {
	GEOFENCE_STATE_UNCERTAIN = 0,
	GEOFENCE_STATE_IN,
	GEOFENCE_STATE_OUT,
} geofence_state_e;

#define _WLAN_BSSID_LEN		18
#define	_PLACE_NAME_LEN		64
#define _ADDRESS_LEN		64
#define _APP_ID_LEN		64

typedef struct {
	geofence_manager_type_e type;
	double latitude;
	double longitude;
	int radius;
	char address[_ADDRESS_LEN];
	char bssid[_WLAN_BSSID_LEN];
	char ssid[_WLAN_BSSID_LEN];
	int place_id;
} geofence_params_s;

typedef struct {
	char place_name[_PLACE_NAME_LEN];
} place_params_s;

typedef struct {
	geofence_state_e state;
	int seconds;
} geofence_status_s;

/* This can be substituted to GeofenceData*/
typedef struct _geofence_info_s {
	int fence_id;
	geofence_params_s param;
} geofence_info_s;

typedef struct _place_info_s {
	int place_id;
	place_params_s param;
} place_info_s;

int geofence_item_data_get_fence_id(gpointer data, int *fence_id);
int geofence_item_data_get_params(gpointer data, geofence_params_s *p);
int geofence_item_data_get_fence_status(gpointer data, geofence_status_s *s);
int add_fence_to_list(int fence_id, const char *fence_name, const double latitude, const double longitude, int radius, const char *bssid, geofence_type_e geofence_type, GList **geofence_list);
void remove_fence_from_list(int fence_id, GList **geofence_list);
void update_fence_state(int fence_id, geofence_state_e state, GList *geofence_list);

#endif /* __GEOFENCE_MANAGER_PRIVATE_ITEM_DATA_H__ */
