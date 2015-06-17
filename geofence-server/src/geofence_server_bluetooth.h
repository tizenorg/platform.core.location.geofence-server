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
 * @file	geofence_server_bluetooth.h
 * @brief	Geofence server bluetooth related APIs
 *
 */

#ifndef GEOFENCE_MANAGER_BLUETOOTH_H_
#define GEOFENCE_MANAGER_BLUETOOTH_H_

/**
* @brief	Bluetooth connection status change callback
* @Param[in]	connected	The bool value indicating the connection, 0 indicates not connected and 1 indicates connected.
* @Param[in]	conn_info	The connection information of the bluetooth
* @Param[in]	user_data	The user data to be returned
* @return		int
* @see	none
*/
void bt_conn_state_changed(gboolean connected, bt_device_connection_info_s *conn_info, void *user_data);

/**
 * @brief	Bluetooth adapter disabled callback
 * @Param[in]	connected	The bool value indicating the connection, 0 indicates not connected and 1 indicates connected.
 * @Param[in]	user_data	The user data to be returned
 * @see None.
 */
void bt_adp_disable(gboolean connected, void *user_data);

#endif /* GEOFENCE_MANAGER_BLUETOOTH_H_ */
