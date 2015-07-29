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
 * @file	server.h
 * @brief	Server related APIs
 *
 */

#ifndef _GEOFENCE_SERVER_H_
#define _GEOFENCE_SERVER_H_

#include "geofence_server.h"
#include "geofence_server_private.h"
#include <bluetooth.h>
#include <wifi.h>

typedef enum {
    GEOFENCE_STATE_AVAILABLE,
    GEOFENCE_STATE_OUT_OF_SERVICE,
    GEOFENCE_STATE_TEMPORARILY_UNAVAILABLE,
} geofence_state_t;

/**
* @brief	Initilazes geofence server
* @return	int
* @retval	0 if success
* @see	none
*/
int _geofence_initialize_geofence_server(GeofenceServer *geofence_server);

/**
* @brief	Deinitilazes geofence server
* @return	int
* @retval	0 if success
* @see	none
*/
int _geofence_deinitialize_geofence_server();

/**
* @brief		Registers the update callbacks
* @param[in] geofence_callback	The callbacks
* @param[in] user_data			The user data
* @return		int
* @retval		0 if success
* @see	none
*/
int _geofence_register_update_callbacks(geofence_callbacks *geofence_callback, void *user_data);

#endif /* _GEOFENCE_SERVER_H_ */
