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

#ifndef _GEOFENCE_MANAGER_DATA_TYPES_H_
#define _GEOFENCE_MANAGER_DATA_TYPES_H_

#include <stdint.h>
#include <time.h>
#include <tizen_type.h>
#include <tizen_error.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tizen Geofence Server Error */
#if !defined(TIZEN_ERROR_GEOFENCE_SERVER)
#define TIZEN_ERROR_GEOFENCE_SERVER	TIZEN_ERROR_GEOFENCE_MANAGER
#endif

/**
 * This enumeration has geofence service status.
 */
typedef enum {
	GEOFENCE_STATUS_UNABAILABLE = 0,
	GEOFENCE_STATUS_ABAILABLE = 1,
}
geofence_status_t;

/**
 * This enumeration describe the geofence fence state.
 */
typedef enum {
    GEOFENCE_EMIT_STATE_UNCERTAIN = 0,
    GEOFENCE_EMIT_STATE_IN = 1,
    GEOFENCE_EMIT_STATE_OUT = 2,
} geofence_emit_state_e;

/**
 * This enumeration describe the geofence fence state.
 */
typedef enum {
    GEOFENCE_FENCE_STATE_UNCERTAIN = -1,
    GEOFENCE_FENCE_STATE_OUT = 0,
    GEOFENCE_FENCE_STATE_IN = 1,
} geofence_fence_state_e;

/**
 * This enumeration describe the geofence proximity state.
 */
typedef enum {
    GEOFENCE_PROXIMITY_UNCERTAIN = 0,
    GEOFENCE_PROXIMITY_FAR,
    GEOFENCE_PROXIMITY_NEAR,
    GEOFENCE_PROXIMITY_IMMEDIATE,
} geofence_proximity_state_e;

typedef enum {
    GEOFENCE_PROXIMITY_PROVIDER_LOCATION = 0,
    GEOFENCE_PROXIMITY_PROVIDER_WIFI,
    GEOFENCE_PROXIMITY_PROVIDER_BLUETOOTH,
    GEOFENCE_PROXIMITY_PROVIDER_BLE,
    GEOFENCE_PROXIMITY_PROVIDER_SENSOR,
} geofence_proximity_provider_e;

/**
 * This enumeration describe the geofence state.
 */
typedef enum {
    GEOFENCE_DIRECTION_BOTH = 0,
    GEOFENCE_DIRECTION_ENTER,
    GEOFENCE_DIRECTION_EXIT,
} geofence_direction_e;

/**
 * This enumeration has geofence service error type.
 */

typedef enum {
    FENCE_ERR_NONE = 0,	   /** No error */
    FENCE_ERR_SQLITE_FAIL = -100,
    FENCE_ERR_INVALID_PARAMETER = -101,
    FENCE_ERR_INTERNAL = -102,
    FENCE_ERR_FENCE_ID = -103,
    FENCE_ERR_PREPARE = -104,
    FENCE_ERR_FENCE_TYPE = -105,	/* geofence type ERROR */
    FENCE_ERR_STRING_TRUNCATED = -106,	/* String truncated */
    FENCE_ERR_COUNT = -107,	/* count <= 0 */
    FENCE_ERR_UNKNOWN = -108
} fence_err_e;

/**
 * @brief Enumerations of error code for Geofence manager.
 * @since_tizen 2.4
 */
typedef enum {
    GEOFENCE_SERVER_ERROR_NONE = TIZEN_ERROR_NONE,								       /**< Success */
    GEOFENCE_SERVER_ERROR_OUT_OF_MEMORY = TIZEN_ERROR_OUT_OF_MEMORY,		       /**< Out of memory */
    GEOFENCE_SERVER_ERROR_INVALID_PARAMETER = TIZEN_ERROR_INVALID_PARAMETER,	       /**< Invalid parameter */
    GEOFENCE_SERVER_ERROR_PERMISSION_DENIED = TIZEN_ERROR_PERMISSION_DENIED,	       /**< Permission denied  */
    GEOFENCE_SERVER_ERROR_NOT_SUPPORTED = TIZEN_ERROR_NOT_SUPPORTED,		       /**< Not supported  */
    GEOFENCE_SERVER_ERROR_NOT_INITIALIZED = TIZEN_ERROR_GEOFENCE_SERVER | 0x01,	      /**< Geofence Manager is not initialized */
    GEOFENCE_SERVER_ERROR_ID_NOT_EXIST = TIZEN_ERROR_GEOFENCE_SERVER | 0x02,	      /**< Geofence ID is not exist */
    GEOFENCE_SERVER_ERROR_EXCEPTION = TIZEN_ERROR_GEOFENCE_SERVER | 0x03,	      /**< exception is occured */
    GEOFENCE_SERVER_ERROR_ALREADY_STARTED = TIZEN_ERROR_GEOFENCE_SERVER | 0x04,	      /**< Geofence is already started */
    GEOFENCE_SERVER_ERROR_TOO_MANY_GEOFENCE = TIZEN_ERROR_GEOFENCE_SERVER | 0x05,     /**< Too many Geofence */
    GEOFENCE_SERVER_ERROR_IPC = TIZEN_ERROR_GEOFENCE_SERVER | 0x06,		     /**< Error occured in GPS/WIFI/BT */
    GEOFENCE_SERVER_ERROR_DATABASE = TIZEN_ERROR_GEOFENCE_SERVER | 0x07,	     /**< DB error occured in the server side */
    GEOFENCE_SERVER_ERROR_PLACE_ACCESS_DENIED = TIZEN_ERROR_GEOFENCE_SERVER | 0x08,	/**< Access to specified place is denied */
    GEOFENCE_SERVER_ERROR_GEOFENCE_ACCESS_DENIED = TIZEN_ERROR_GEOFENCE_SERVER | 0x09,	   /**< Access to specified geofence is denied */
} geofence_server_error_e;

/**
* This enumeration describes the geofence param state
*/
typedef enum {
    GEOFENCE_MANAGE_FENCE_ADDED = 0x00,
    GEOFENCE_MANAGE_FENCE_REMOVED,
    GEOFENCE_MANAGE_FENCE_STARTED,
    GEOFENCE_MANAGE_FENCE_STOPPED,

    GEOFENCE_MANAGE_PLACE_ADDED = 0x10,
    GEOFENCE_MANAGE_PLACE_REMOVED,
    GEOFENCE_MANAGE_PLACE_UPDATED,

    GEOFENCE_MANAGE_SETTING_ENABLED = 0x20,
    GEOFENCE_MANAGE_SETTING_DISABLED
} geofence_manage_e;

/**
 * This enumeration descript the Smart Assistant State
 */
typedef enum {
    GEOFENCE_SMART_ASSIST_STOP = 0,
    GEOFENCE_SMART_ASSIST_START
} geofence_smart_assist_state_e;

typedef enum {
    GEOFENCE_TYPE_GEOPOINT = 1,			/**< Geofence is specified by geospatial coordinate */
    GEOFENCE_TYPE_WIFI,					/**< Geofence is specified by Wi-Fi access point */
    GEOFENCE_TYPE_BT,					/**< Geofence is specified by Blutetooth device */
} geofence_type_e;

typedef enum {
    ACCESS_TYPE_PRIVATE = 1,
    ACCESS_TYPE_PUBLIC,
    ACCESS_TYPE_UNKNOWN,
} access_type_e;

#ifdef __cplusplus
}
#endif
#endif				/* _GEOFENCE_MANAGER_DATA_TYPES_H_ */
