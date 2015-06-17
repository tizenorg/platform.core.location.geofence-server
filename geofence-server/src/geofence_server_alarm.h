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
 * @file	geofence_server_alarm.h
 * @brief	Geofence server alarm related APIs
 *
 */

#ifndef GEOFENCE_MANAGER_ALARM_H_
#define GEOFENCE_MANAGER_ALARM_H_

/**
* @brief	Adds the geofence alarm
* @param[in] interval	Interval value in int.
* @param[in] alarm_cb	The alarm callback function
* @return		int
* @retval		alarmid if success
			-1 if addition of alarm fails
* @see	_geofence_remove_alarm
*/
int _geofence_add_alarm(int interval, alarm_cb_t alarm_cb, void *userdata);

/**
* @brief	Removes the geofence alarm
* @param[in] alarm_id	The alarm id.
* @return		int
* @retval		-1
* @see	_geofence_add_alarm
*/
int _geofence_remove_alarm(alarm_id_t alarm_id);

#endif /* GEOFENCE_MANAGER_ALARM_H_ */
