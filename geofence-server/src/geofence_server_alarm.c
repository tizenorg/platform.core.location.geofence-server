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

#include "geofence_server.h"
#include "geofence_server_private.h"
#include "geofence_server_log.h"
#include "debug_util.h"

int _geofence_add_alarm(int interval, alarm_cb_t alarm_cb, void *userdata)
{
	FUNC_ENTRANCE_SERVER;
	GeofenceServer *geofence_server = (GeofenceServer *) userdata;

	int ret = 0;
	alarm_id_t alarm_id = -1;

	ret = alarmmgr_add_alarm_withcb(ALARM_TYPE_DEFAULT, interval, 0, alarm_cb, geofence_server, &alarm_id);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		LOGE_GEOFENCE("Fail to alarmmgr_add_alarm_withcb : %d", ret);
	}
	LOGD_GEOFENCE("alarm_id : %d", alarm_id);

	return alarm_id;
}

int _geofence_remove_alarm(alarm_id_t alarm_id)
{
	FUNC_ENTRANCE_SERVER;
	int ret = 0;
	ret = alarmmgr_remove_alarm(alarm_id);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		LOGE_GEOFENCE("Fail to alarmmgr_remove_alarm : %d", ret);
	}

	return -1;
}
