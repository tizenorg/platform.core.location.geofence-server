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
 * @file	geofence_server_wifi.h
 * @brief	Geofence server wifi related APIs
 *
 */

#ifndef GEOFENCE_MANAGER_WIFI_H_
#define GEOFENCE_MANAGER_WIFI_H_

/**
* @brief	Handles the wifi scan results
* @param[in] ap_list	The ap list.
* @param[in] user_data	The user data.
* @see None.
*/
void handle_scan_result(GList *ap_list, void *user_data);

/**
* @brief	Indicated the wifi scan results
* @param[in] ap_list	The ap list.
* @param[in] user_data	The user data.
* @see None.
*/
void wifi_indication(GList *ap_list, void *user_data);

/**
 * @brief	Gets Wifi enabled status
 * @Param[in]	data		The status of the wifi connection
 * @return		gboolean
 * @retval	false if disabled and true if enabled
 * @see None.
 */
gboolean geofence_get_enable_wifi_state(gpointer data);

/**
 * @brief	Handles Wifi connection status change
 * @Param[in]	state		The status of the wifi connection
 * @Param[in]	ap			The wifi ap
 * @Param[in]	user_data	The user data
 * @see None.
 */
void wifi_conn_state_changed(wifi_connection_state_e state, wifi_ap_h ap, void *user_data);

/**
 * @brief	Wifi device status change
 * @Param[in]	state		The status of the wifi device
 * @Param[in]	user_data	The user data
 * @see None.
 */
void wifi_device_state_changed(wifi_device_state_e state, void *user_data);
#endif /* GEOFENCE_MANAGER_WIFI_H_ */
