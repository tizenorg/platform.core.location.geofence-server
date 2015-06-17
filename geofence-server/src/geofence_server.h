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
 * @file	geofence_server.h
 * @brief	Geofence server related APIs
 *
 */

#ifndef _GEOFENCE_MANAGER_H_
#define _GEOFENCE_MANAGER_H_

#include <glib.h>
#include <bluetooth.h>
#include <wifi.h>
#include <locations.h>
#include <network-wifi-intf.h>
#include "geofence_server_data_types.h"

#define PACKAGE_NAME	"geofence-server"

/**
 * @brief	Bluetooth connection status change callback
 * @remarks	This callback will be called when the bluetooth connection status changes.
 * @Param[in]	connected	The bool value indicating the connection, 0 indicates not connected and 1 indicates connected.
 * @Param[in]	conn_info	The connection information of the bluetooth
 * @Param[in]	user_data	The user data to be returned
 * @see None.
 */
typedef void (*geofence_bt_conn_state_changed_cb) (gboolean connected, bt_device_connection_info_s *conn_info, void *user_data);

/**
 * @brief	Bluetooth adapter disabled callback
 * @remarks	This callback will be called when the bluetooth adapter is disabled.
 * @Param[in]	connected	The bool value indicating the connection, 0 indicates not connected and 1 indicates connected.
 * @Param[in]	user_data	The user data to be returned
 * @see None.
 */
typedef void (*geofence_bt_adapter_disable_cb) (gboolean connected, void *user_data);

/**
 * @brief	Wifi connection status change callback
 * @remarks	This callback will be called when the wifi connection status changes.
 * @Param[in]	state		The status of the wifi connection
 * @Param[in]	ap			The wifi ap
 * @Param[in]	user_data	The user data to be returned
 * @see None.
 */
typedef void (*geofence_wifi_conn_state_changed_cb) (wifi_connection_state_e state, wifi_ap_h ap, void *user_data);

/**
 * @brief	Wifi device status change callback
 * @remarks	This callback will be called when the wifi device status changes.
 * @Param[in]	state		The status of the wifi device
 * @Param[in]	user_data	The user data to be returned
 * @see None.
 */
typedef void (*geofence_wifi_device_state_changed_cb) (wifi_device_state_e state, void *user_data);

/**
 * @brief       Network scan status change callback
 * @remarks     This callback will be called when the wifi scanning happen in the background.
 * @Param[in]   event_cb        The callback of the event happened
 * @Param[in]   user_data       The user data to be returned
 * @see None.
 */
typedef void (*geofence_network_event_cb) (net_event_info_t *event_cb, void *user_data);

/**
 * @brief       BT Discovery status change callback
 * @remarks     This callback will be called when the BT scanning happen as soon as BT is switched on.
 * @Param[in]   event_cb        The callback of the event happened
 * @Param[in]   user_data       The user data to be returned
 * @see None.
 */
typedef void (*geofence_bt_adapter_device_discovery_state_changed_cb) (int result, bt_adapter_device_discovery_state_e discovery_state, bt_adapter_device_discovery_info_s *discovery_info, void *user_data);

/**
 * @brief Called when the state of location method is changed.
 * @since_tizen 2.3
 * @param[in] method    The method changed on setting
 * @param[in] enable    The setting value changed
 * @param[in] user_data  The user data passed from the callback registration function
 * @pre location_setting_changed_cb() will invoke this callback if you register this callback using location_manager_set_setting_changed_cb()
 * @see location_manager_set_setting_changed_cb()
 * @see location_manager_unset_setting_changed_cb()
 */
typedef void (*geofence_gps_setting_changed_cb) (location_method_e method, bool enable, void *user_data);

/**
 * Geofence callback structure.
 */
struct geofence_callbacks_s {
	geofence_bt_conn_state_changed_cb bt_conn_state_changed_cb;
	geofence_bt_adapter_disable_cb bt_apater_disable_cb;
	geofence_wifi_conn_state_changed_cb wifi_conn_state_changed_cb;
	geofence_wifi_device_state_changed_cb wifi_device_state_changed_cb;
	geofence_network_event_cb network_evt_cb;
	geofence_bt_adapter_device_discovery_state_changed_cb bt_discovery_cb;
	geofence_gps_setting_changed_cb gps_setting_changed_cb;
};

typedef struct geofence_callbacks_s geofence_callbacks;

/**
 * This enumeration describe the smart assistant status.
 */
typedef enum {
	GEOFENCE_SMART_ASSIST_NONE,
	GEOFENCE_SMART_ASSIST_ENABLED,
	GEOFENCE_SMART_ASSIST_DISABLED,
	GEOFENCE_SMART_ASSIST_COLLECTING,
} smart_assist_status_e;

/**
 * This enumeration describe the cell geofence in and out status.
 */
typedef enum {
	CELL_UNKNOWN = -1,
	CELL_OUT = 0,
	CELL_IN = 1
} cell_status_e;

/**
 * This enumeration describe the wifi geofence in and out status.
 */
typedef enum {
	WIFI_DIRECTION_UNKNOWN = -1,
	WIFI_DIRECTION_OUT = 0,
	WIFI_DIRECTION_IN = 1
} wifi_status_e;

/**
 * This enumeration describe the geofence client status.
 */
typedef enum {
	GEOFENCE_CLIENT_STATUS_NONE,
	GEOFENCE_CLIENT_STATUS_FIRST_LOCATION,
	GEOFENCE_CLIENT_STATUS_START,
	GEOFENCE_CLIENT_STATUS_RUNNING
} geofence_client_status_e;

#endif
