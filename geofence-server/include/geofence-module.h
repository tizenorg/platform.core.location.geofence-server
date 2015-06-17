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

#ifndef __GEOFENCE_MODULE_H__
#define __GEOFENCE_MODULE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file geofence-module.h
 * @brief This file contains the structure and enumeration for geofence plug-in development.
 */


/**
 * @brief This represents geofence parameter.
 */
typedef enum {
	GEOFENCE_TYPE_GEOPOINT = 1,				/**< Geofence is specified by geo position */
	GEOFENCE_TYPE_WIFI,						/**< Geofence is specified by WiFi hotspot */
	GEOFENCE_TYPE_BT,						/**< Geofence is specified BT */
} geofence_type_e;

typedef enum {
	ACCESS_TYPE_PRIVATE = 1,
	ACCESS_TYPE_PUBLIC,
	ACCESS_TYPE_UNKNOWN,
} access_type_e;

/**
 * @brief The geofence manager handle.
 */
typedef struct place_params_s *place_params_h;
/**
 * @brief The geofence params handle.
 */
typedef struct geofence_params_s *geofence_params_h;

/**
 * @brief This represents a geofence callback function for geofence plug-in.
 */
typedef void (*GeofenceModCB) (int fence_id, int state, gpointer userdata);

typedef void (*GeofenceModEventCB) (int place_id, int fence_id, int error, int state, gpointer userdata);

/**
 * @brief This represents APIs declared in a Geofence plug-in for Geofence modules.
 */
typedef struct {
	int (*create) (void *handle, GeofenceModCB geofence_cb, GeofenceModEventCB geofence_event_cb, void *userdata);
	int (*destroy) (void *handle);
	int (*enable_service) (void *handle, int fence_id, bool enable);
	int (*add_geopoint) (void *handle, int place_id, double latitude, double longitude, int radius, const char *address, int *fence_id);
	int (*add_bssid) (void *handle, int place_id, const char *bssid, const char *ssid, geofence_type_e type, int *fence_id);
	int (*add_place) (void *handle, const char *place_name, int *place_id);
	int (*update_place) (void *handle, int place_id, const char *place_name);
	int (*remove_geofence) (void *handle, int fence_id);
	int (*remove_place) (void *handle, int place_id);
	int (*start_geofence) (void *handle, int fence_id);
	int (*stop_geofence) (void *handle, int fence_id);
	int (*get_place_name) (void *handle, int place_id, char **place_name);
	int (*get_list) (void *handle, int place_id, int *fence_amount, int **fence_ids, struct geofence_params_s **params);
	int (*get_place_list) (void *handle, int *place_amount, int **place_ids, struct place_params_s **params);
} GeofenceModOps;

/**
 * @} @}
 */
#ifdef __cplusplus
}
#endif
#endif
