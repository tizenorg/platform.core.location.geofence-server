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

#ifndef _GEOFENCE_MANAGER_LOG_H_
#define _GEOFENCE_MANAGER_LOG_H_

#include <time.h>

void _init_log();
void _deinit_log();
void _print_log(const char *str);
struct tm *__get_current_time();

#define GEOFENCE_PRINT_LOG(state)	{ \
		char buf[256] = {0, }; \
		sprintf(buf, " [%s:%d] Status[%s]", __func__, __LINE__, #state); \
		_print_log(buf); \
	}
#define GEOFENCE_PRINT_LOG_WITH_ID(state, id)	{ \
		char buf[256] = {0, }; \
		sprintf(buf, " [%s:%d] Status[%s]. ID[%d]", __func__, __LINE__, #state, id); \
		_print_log(buf); \
	}

#endif /* _GEOFENCE_MANAGER_LOG_H_ */
