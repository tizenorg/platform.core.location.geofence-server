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
 * @file	geofence_server_private.h
 * @brief	Geofence server related private structure and enumeration
 *
 */

#ifndef __GEOFENCE_MANAGER_PRIVATE_H__
#define __GEOFENCE_MANAGER_PRIVATE_H__

#include <geofence_dbus_server.h>
#include <locations.h>
#include <alarm.h>
#include <network-wifi-intf.h>
#include "geofence_server_data_types.h"
#include "geofence_server.h"
#include <stdio.h>
#include <activity_recognition.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Length of bssid */
#define WLAN_BSSID_LEN	18
#define APP_ID_LEN		64
#define ADDRESS_LEN		64
#define PLACE_NAME_LEN	64
#define BLE_INFO_LEN	256

/**
 * The geofence common information structure
 */
typedef struct {
	int fence_id;
	int enable;
	geofence_fence_state_e status;
	geofence_proximity_state_e proximity_status;
	geofence_type_e type;	/* Geocoordinate/WIFI/CurrentLocation/Bluetooth */
	access_type_e access_type;
	char appid[APP_ID_LEN];
	int running_status;		/*0-idle  1-running*/
	int place_id;
} fence_common_info_s;

/**
 *
 */
typedef struct {
	int place_id;
	access_type_e access_type;
	char place_name[PLACE_NAME_LEN];
	char appid[APP_ID_LEN];
} place_info_s;

/**
 *The geocoordinate structure
 */
typedef struct {
	double latitude;
	double longitude;
	double accuracy;
	int timestamp;
} location_fix_info_s;


/**
 *The geocoordinate structure
 */
typedef struct {
	double latitude;
	double longitude;
	double radius;
	char address[ADDRESS_LEN];
} geocoordinate_info_s;

typedef struct {
	int place_id;
	geofence_type_e type;
	double latitude;
	double longitude;
	int radius;
	char address[ADDRESS_LEN];
	char bssid[WLAN_BSSID_LEN];
	char ssid[WLAN_BSSID_LEN];
} geofence_s;

typedef struct _geofence_info_s {
	int fence_id;
	char app_id[APP_ID_LEN];
	access_type_e access_type;
	geofence_s param;
} geofence_info_s;

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
	char ssid[WLAN_BSSID_LEN];
	gboolean enabled;		/* bluetooth callback receive or not */
} bssid_info_s;

/**
 *The GeofenceItemData structure
 */
typedef struct {
	double distance;
	bool is_wifi_status_in;
	bool is_bt_status_in;
	geofence_client_status_e client_status;
	cell_status_e cell_status;
	int smart_assist_added;
	smart_assist_status_e smart_assistant_status;	/* enable or not after added */
	fence_common_info_s common_info;
	void *priv;			/* Save (latitude, longitude and radius) or AP list */
} GeofenceItemData;

/**
 *The GeofenceServer structure
 */
typedef struct {
	GMainLoop *loop;
	geofence_dbus_server_h geofence_dbus_server;
	GList *geofence_list;	/* list of geofence list for multi clients */
	GList *tracking_list;	/* list of geofence ids for tracking */
	location_fix_info_s *gps_fix_info;
	location_fix_info_s *wps_fix_info;
	int gps_trigger_interval;
	time_t last_loc_time;
	time_t last_result_time;
	int running_geopoint_cnt;
	int running_bt_cnt;
	int running_wifi_cnt;
#if USE_HW_GEOFENCE
	GeofenceModCB geofence_cb;
#endif
	gpointer userdata;
	/* for Geometry's GPS positioning*/
	location_manager_h loc_gps_manager;
	location_manager_h loc_wps_manager;
	FILE *log_file;
	int loc_gps_started_by_wps;
	int loc_gps_started;
	int loc_wps_started;
	char ble_info[BLE_INFO_LEN];
	int nearestTrackingFence;
	int connectedTrackingWifiFenceId;
	alarm_id_t timer_id;	/* ID for timer source*/
	alarm_id_t gps_alarm_id;	/* ID for WPS restart timer source*/
	alarm_id_t gps_timeout_alarm_id;
	alarm_id_t wps_timeout_alarm_id;
	alarm_id_t wps_alarm_id;
	alarm_id_t bt_alarm_id;

	activity_type_e activity_type;
	double activity_timestamp;

	activity_h activity_stationary_h;
	activity_h activity_walk_h;
	activity_h activity_run_h;
	activity_h activity_in_vehicle_h;
} GeofenceServer;

#ifdef __cplusplus
}
#endif
#endif				/* __GEOFENCE_MANAGER_PRIVATE_H__ */
