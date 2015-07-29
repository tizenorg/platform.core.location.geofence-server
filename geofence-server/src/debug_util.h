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

#ifndef _GEOFENCE_MANAGER_DEBUG_UTIL_H_
#define _GEOFENCE_MANAGER_DEBUG_UTIL_H_

#include <glib.h>
#include <libgen.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GEOFENCE_SERVER_DLOG
#define LBS_DLOG_DEBUG

#include <dlog.h>
#define TAG_GEOFENCE_SERVER      "GEOFENCE_SERVER"

/*#define __LOCAL_TEST__*/

#define LOGD_GEOFENCE(fmt, args...)  SLOG(LOG_DEBUG, TAG_GEOFENCE_SERVER, fmt, ##args)
#define LOGI_GEOFENCE(fmt, args...)  SLOG(LOG_INFO, TAG_GEOFENCE_SERVER, fmt, ##args)
#define LOGW_GEOFENCE(fmt, args...)  SLOG(LOG_WARN, TAG_GEOFENCE_SERVER, fmt, ##args)
#define LOGE_GEOFENCE(fmt, args...)  SLOG(LOG_ERROR, TAG_GEOFENCE_SERVER, fmt, ##args)
#define FUNC_ENTRANCE_SERVER         LOGD_GEOFENCE("ENTER >>>");

#ifdef __cplusplus
}
#endif
#endif				/* _GEOFENCE_MANAGER_DEBUG_UTIL_H_ */
