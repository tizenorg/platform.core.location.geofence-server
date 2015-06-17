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

#ifndef __MOD_LOG_H__
#define __MOD_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#define GEOFENCE_SERVER_DLOG

#include <dlog.h>
#define TAG_GEOFENCE_MOD      "GEOFENCE_MOD"

#define LOGD_GEOFENCE(fmt, args...)  LOG(LOG_DEBUG, TAG_GEOFENCE_MOD, fmt, ##args)
#define LOGI_GEOFENCE(fmt, args...)  LOG(LOG_INFO, TAG_GEOFENCE_MOD, fmt, ##args)
#define LOGW_GEOFENCE(fmt, args...)  LOG(LOG_WARN, TAG_GEOFENCE_MOD, fmt, ##args)
#define LOGE_GEOFENCE(fmt, args...)  LOG(LOG_ERROR, TAG_GEOFENCE_SERVER_MOD, fmt, ##args)

#define MOD_LOGD(fmt, args...)  LOG(LOG_DEBUG, TAG_GEOFENCE_MOD, fmt, ##args)
#define MOD_LOGW(fmt, args...)  LOG(LOG_WARN,  TAG_GEOFENCE_MOD, fmt, ##args)
#define MOD_LOGI(fmt, args...)  LOG(LOG_INFO,  TAG_GEOFENCE_MOD, fmt, ##args)
#define MOD_LOGE(fmt, args...)  LOG(LOG_ERROR, TAG_GEOFENCE_MOD, fmt, ##args)
#define MOD_SECLOG(fmt, args...)  SECURE_LOG(LOG_DEBUG, TAG_GEOFENCE_MOD, fmt, ##args)

#define FUNC_ENTRANCE_SERVER         LOGD_GEOFENCE(">>> Entered!!");

#ifdef __cplusplus
}
#endif
#endif
