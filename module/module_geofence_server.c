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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gmodule.h>
#include <stdio.h>
#include <stdlib.h>
#include <app_manager.h>
#include <dlfcn.h>
#include <geofence_client.h>
#include "module_internal.h"
#include "log.h"

#define GEOFENCE_MODULE_API __attribute__((visibility("default"))) G_MODULE_EXPORT
#define MYPLACES_APP_ID  "org.tizen.myplace"

/**
 * This enumeration descript the geofence type.
 * Should be SYNC to geofence_mananger_data_types.h
 */
typedef enum {
	GEOFENCE_SERVER_TYPE_INVALID = 0,
	GEOFENCE_SERVER_TYPE_GEOPOINT = 1,
	GEOFENCE_SERVER_TYPE_WIFI,
	GEOFENCE_SERVER_TYPE_BT
} geofence_server_type_e;

/**
 * Enumerations of the geofence bssid type.
 */
typedef enum {
	GEOFENCE_BSSID_TYPE_WIFI = 0,
	GEOFENCE_BSSID_TYPE_BT
} geofence_bssid_type_e;

#define GEOFENCE_STATE_UNCERTAIN	0
#define GEOFENCE_STATE_IN		1
#define GEOFENCE_STATE_OUT		2
#define _WLAN_BSSID_LEN			18/* bssid 17 + "null"*/

typedef struct {
	int enabled;
	int geofence_type;
	gint geofence_id;
	gdouble latitude;
	gdouble longitude;
	gint radius;
	char bssid[_WLAN_BSSID_LEN];
} GeofenceData;

typedef struct {
	geofence_client_dbus_h geofence_client;
	GList *geofence_list;
	GeofenceModCB geofence_cb;
	GeofenceModEventCB geofence_event_cb;
	gpointer userdata;
} GeofenceManagerData;

#define GEOFENCE_SERVER_SERVICE_NAME	"org.tizen.lbs.Providers.GeofenceServer"
#define GEOFENCE_SERVER_SERVICE_PATH	"/org/tizen/lbs/Providers/GeofenceServer"

GEOFENCE_MODULE_API int add_geopoint(void *handle, int place_id, double latitude, double longitude, int radius, const char *address, int *fence_id)
{
	MOD_LOGD("add_geopoint");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	int geofence_id = -1;

	geofence_id = geo_client_add_geofence(geofence_manager->geofence_client, place_id, GEOFENCE_TYPE_GEOPOINT, latitude, longitude, radius, address, "", "");
	if (geofence_id == -1)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	*fence_id = geofence_id;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int add_bssid(void *handle, int place_id, const char *bssid, const char *ssid, geofence_type_e type, int *fence_id)
{
	MOD_LOGD("add_bssid");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	int geofence_id = -1;

	geofence_id = geo_client_add_geofence(geofence_manager->geofence_client, place_id, type, -1, -1, -1, "", bssid, ssid);
	if (geofence_id == -1)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	*fence_id = geofence_id;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int add_place(void *handle, const char *place_name,	int *place_id)
{
	MOD_LOGD("add_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	int placeid = -1;

	placeid = geo_client_add_place(geofence_manager->geofence_client, place_name);
	if (placeid == -1)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	*place_id = placeid;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int update_place(void *handle, int place_id, const char *place_name)
{
	MOD_LOGD("update_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = geo_client_update_place(geofence_manager->geofence_client, place_id, place_name);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int remove_geofence(void *handle, int fence_id)
{
	MOD_LOGD("remove_geofence");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = geo_client_delete_geofence(geofence_manager->geofence_client, fence_id);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int remove_place(void *handle, int place_id)
{
	MOD_LOGD("remove_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = geo_client_delete_place(geofence_manager->geofence_client, place_id);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int enable_service(void *handle, int fence_id, bool enable)
{
	MOD_LOGD("enable_service");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = geo_client_enable_service(geofence_manager->geofence_client, fence_id, enable);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

static void geofence_callback(GVariant *param, void *user_data)
{
	g_return_if_fail(user_data);
	GeofenceManagerData *geofence_manager =	(GeofenceManagerData *)user_data;
	int fence_id, access_type, state;
	char *app_id = NULL;
	pid_t pid = 0;
	char *appid_from_app = NULL;
	g_variant_get(param, "(siii)", &app_id, &fence_id, &access_type, &state);

	MOD_LOGI("Getting the app id");
	pid = getpid();
	int ret = app_manager_get_app_id(pid, &appid_from_app);
	if (ret != APP_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to get app_id from module_geofence_server. Err[%d]", ret);
		return;
	}
	MOD_LOGI("APP ID from server : %s", app_id);
	MOD_LOGI("APP ID from app manager : %s", appid_from_app);

	if (access_type == ACCESS_TYPE_PRIVATE) {
		if (!(g_strcmp0(appid_from_app, app_id))) {	/*Sending the alert only the app-id matches in case of private fence*/
			if (geofence_manager->geofence_cb)
				geofence_manager->geofence_cb(fence_id, state, geofence_manager->userdata);
		}
	} else {
		if (geofence_manager->geofence_cb)	/*Here filteration is done in the manager as public fences cannot be restricted/filtered.*/
			geofence_manager->geofence_cb(fence_id, state, geofence_manager->userdata);
	}
	if (appid_from_app)
		g_free(appid_from_app);
	if (app_id)
		g_free(app_id);
}

static void geofence_event_callback(GVariant *param, void *user_data)
{
	g_return_if_fail(user_data);
	GeofenceManagerData *geofence_manager =	(GeofenceManagerData *)user_data;
	int place_id, fence_id, access_type, error, state;
	char *app_id = NULL;
	pid_t pid = 0;
	char *appid = NULL;
	g_variant_get(param, "(iiisii)", &place_id, &fence_id, &access_type, &app_id, &error, &state);

	MOD_LOGI("Getting the app id");
	pid = getpid();
	int ret = app_manager_get_app_id(pid, &appid);
	if (ret != APP_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to get app_id from module_geofence_server. Err[%d]", ret);
		return;
	}
	MOD_LOGI("APP ID from server : %s", app_id);
	MOD_LOGI("APP ID from app manager : %s", appid);
	MOD_LOGI("Fence_ID: %d, Error: %d, State: %d", fence_id, error, state);

	if (access_type == ACCESS_TYPE_PRIVATE) {
		if (!(g_strcmp0(appid, app_id))) {
			if (geofence_manager->geofence_event_cb)
				geofence_manager->geofence_event_cb(place_id, fence_id, error, state, geofence_manager->userdata);
		}
	} else if (access_type == ACCESS_TYPE_PUBLIC) {
		if (!g_strcmp0(app_id, MYPLACES_APP_ID) || !g_strcmp0(appid, app_id)) {
			if (geofence_manager->geofence_event_cb)
				geofence_manager->geofence_event_cb(place_id, fence_id, error, state, geofence_manager->userdata);
		}
	} else {
		if (!(g_strcmp0(appid, app_id))) {
			if (geofence_manager->geofence_event_cb)
				geofence_manager->geofence_event_cb(place_id, fence_id, error, state, geofence_manager->userdata);
		}
	}
	if (appid)
		g_free(appid);
	if (app_id)
		g_free(app_id);
}

GEOFENCE_MODULE_API int get_place_name(void *handle, int place_id, char **place_name)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(place_name, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int error_code = GEOFENCE_MANAGER_ERROR_NONE;
	int ret = geo_client_get_place_name(geofence_manager->geofence_client, place_id, place_name, &error_code);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	return error_code;
}

GEOFENCE_MODULE_API int get_list(void *handle, int place_id, int *fence_amount, int **fence_ids, struct geofence_params_s **params)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(fence_amount, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(fence_ids, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(params, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	if (!geofence_manager) {
		*fence_amount = 0;
		return GEOFENCE_MANAGER_ERROR_NONE;
	}
	int index = 0;
	GVariantIter *iter = NULL;
	GVariantIter *iter_row = NULL;
	gchar *key;
	GVariant *value;
	int fence_cnt = 0;
	int error_code = GEOFENCE_MANAGER_ERROR_NONE;

	/*Call the geofence_client api here....*/
	geo_client_get_list(geofence_manager->geofence_client, place_id, &iter, &fence_cnt, &error_code);
	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;

	*fence_amount = fence_cnt;
	MOD_LOGI("Total fence count : %d", *fence_amount);
	int *fence_id_array = (int *) g_slice_alloc0(sizeof(int)*fence_cnt);
	geofence_params_s *p = (geofence_params_s *)g_slice_alloc0(sizeof(geofence_params_s)*fence_cnt);

	if (iter == NULL) {
		MOD_LOGI("Iterator is null");
	}
	while (g_variant_iter_next(iter, "a{sv}", &iter_row)) {
		while (g_variant_iter_loop(iter_row, "{sv}", &key, &value)) {
			if (!g_strcmp0(key, "fence_id")) {
				fence_id_array[index] =
					g_variant_get_int32(value);
			} else if (!g_strcmp0(key, "place_id")) {
				p[index].place_id = g_variant_get_int32(value);
			} else if (!g_strcmp0(key, "geofence_type")) {
				p[index].type = g_variant_get_int32(value);
			} else if (!g_strcmp0(key, "latitude")) {
				p[index].latitude = g_variant_get_double(value);
			} else if (!g_strcmp0(key, "longitude")) {
				p[index].longitude = g_variant_get_double(value);
			} else if (!g_strcmp0(key, "radius")) {
				p[index].radius = g_variant_get_int32(value);
			} else if (!g_strcmp0(key, "address")) {
				g_strlcpy(p[index].address, g_variant_get_string(value, NULL), _ADDRESS_LEN);
			} else if (!g_strcmp0(key, "bssid")) {
				g_strlcpy(p[index].bssid, g_variant_get_string(value, NULL), _WLAN_BSSID_LEN);
			} else if (!g_strcmp0(key, "ssid")) {
				g_strlcpy(p[index].ssid, g_variant_get_string(value, NULL), _WLAN_BSSID_LEN);
			}
		}
		MOD_LOGI("Fence_id: %d, Place_id: %d, Type: %d, lat: %f, lon: %f, rad: %d, address: %s, bssid: %s, ssid: %s", fence_id_array[index], p[index].place_id, p[index].type, p[index].latitude, p[index].longitude, p[index].radius, p[index].address, p[index].bssid, p[index].ssid);
		index++;
		g_variant_iter_free(iter_row);
	}
	g_variant_iter_free(iter);
	*params = (struct geofence_params_s *) p;
	*fence_ids = fence_id_array;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int get_place_list(void *handle, int *place_amount, int **place_ids, struct place_params_s **params)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(place_amount, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(place_ids, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(params, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	if (!geofence_manager) {
		*place_amount = 0;
		return GEOFENCE_MANAGER_ERROR_NONE;
	}
	int index = 0;
	GVariantIter *iter = NULL;
	GVariantIter *iter_row = NULL;
	gchar *key;
	GVariant *value;
	int place_cnt = 0;
	int error_code = -1;

	/*Call the geofence_client api here....*/
	geo_client_get_place_list(geofence_manager->geofence_client, &iter, &place_cnt, &error_code);
	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;

	*place_amount = place_cnt;
	MOD_LOGI("Total place count : %d", *place_amount);
	int *place_id_array = (int *)g_slice_alloc0(sizeof(int)*place_cnt);
	place_params_s *p = (place_params_s *)g_slice_alloc0(sizeof(place_params_s)*place_cnt);

	if (iter == NULL)
		MOD_LOGI("Iterator is null");

	while (g_variant_iter_next(iter, "a{sv}", &iter_row)) {
		while (g_variant_iter_loop(iter_row, "{sv}", &key, &value)) {
			if (!g_strcmp0(key, "place_id")) {
				place_id_array[index] = g_variant_get_int32(value);
			} else if (!g_strcmp0(key, "place_name")) {
				g_strlcpy(p[index].place_name, g_variant_get_string(value, NULL), _PLACE_NAME_LEN);
			}
		}
		MOD_LOGI("place_id: %d, place_name: %s", place_id_array[index], p[index].place_name);
		index++;
		g_variant_iter_free(iter_row);
	}
	g_variant_iter_free(iter);
	*params = (struct place_params_s *) p;
	*place_ids = place_id_array;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

static void on_signal_callback(const gchar *sig, GVariant *param, gpointer user_data)
{
	if (!g_strcmp0(sig, "GeofenceInout")) {
		MOD_LOGD("GeofenceInoutChanged");
		geofence_callback(param, user_data);
	} else if (!g_strcmp0(sig, "GeofenceEvent")) {
		MOD_LOGD("GeofenceEventInvoked");
		geofence_event_callback(param, user_data);
	} else {
		MOD_LOGD("Invalid signal[%s]", sig);
	}
}

GEOFENCE_MODULE_API int start_geofence(void *handle, int fence_id)
{
	MOD_LOGD("start_geofence");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	MOD_LOGD("geofence-server(%x)", geofence_manager);

	ret = geo_client_start_geofence(geofence_manager->geofence_client, fence_id);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to start geofence_client_h. Error[%d]", ret);
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int stop_geofence(void *handle, int fence_id)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	MOD_LOGD("geofence_manager->geofence_cb : %x", geofence_manager->geofence_cb);
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(geofence_manager->geofence_client,	GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = GEOFENCE_CLIENT_ERROR_NONE;

	ret = geo_client_stop_geofence(geofence_manager->geofence_client, fence_id);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to stop. Error[%d]", ret);
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int create(void *handle, GeofenceModCB geofence_cb,
	GeofenceModEventCB geofence_event_cb, void *userdata)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(geofence_cb, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	/* create connnection */
	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	geofence_manager->geofence_cb = geofence_cb;
	geofence_manager->geofence_event_cb = geofence_event_cb;
	geofence_manager->userdata = userdata;

	ret = geo_client_create(&(geofence_manager->geofence_client));
	if (ret != GEOFENCE_CLIENT_ERROR_NONE || !geofence_manager->geofence_client) {
		MOD_LOGE("Fail to create geofence_client_dbus_h. Error[%d]", ret);
		return GEOFENCE_MANAGER_ERROR_EXCEPTION;
	}

	ret = geo_client_start(GEOFENCE_SERVER_SERVICE_NAME, GEOFENCE_SERVER_SERVICE_PATH, geofence_manager->geofence_client, on_signal_callback, on_signal_callback, geofence_manager);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		if (ret == GEOFENCE_CLIENT_ACCESS_DENIED) {
			MOD_LOGE("Access denied[%d]", ret);
			return GEOFENCE_CLIENT_ACCESS_DENIED;
		}
		MOD_LOGE("Fail to start geofence_client_dbus_h. Error[%d]", ret);
		geo_client_destroy(geofence_manager->geofence_client);
		geofence_manager->geofence_client = NULL;

		return GEOFENCE_CLIENT_ERROR_UNKNOWN;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API int destroy(void *handle)
{
	MOD_LOGD("destroy");

	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);
	g_return_val_if_fail(geofence_manager->geofence_client, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	ret = geo_client_stop(geofence_manager->geofence_client);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		MOD_LOGE("Fail to stop. Error[%d]", ret);
		geo_client_destroy(geofence_manager->geofence_client);
		geofence_manager->geofence_client = NULL;
		return GEOFENCE_CLIENT_ERROR_UNKNOWN;
	}

	ret = geo_client_destroy(geofence_manager->geofence_client);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		MOD_LOGE("Fail to destroy. Error[%d]", ret);
		return GEOFENCE_CLIENT_ERROR_UNKNOWN;
	}
	geofence_manager->geofence_client = NULL;
	geofence_manager->geofence_cb = NULL;
	geofence_manager->geofence_event_cb = NULL;
	geofence_manager->userdata = NULL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

GEOFENCE_MODULE_API gpointer init(GeofenceModOps *ops)
{
	MOD_LOGD("init");

	g_return_val_if_fail(ops, NULL);
	ops->create = create;
	ops->destroy = destroy;
	ops->enable_service = enable_service;
	ops->start_geofence = start_geofence;
	ops->stop_geofence = stop_geofence;
	ops->add_geopoint = add_geopoint;
	ops->add_bssid = add_bssid;
	ops->add_place = add_place;
	ops->update_place = update_place;
	ops->remove_geofence = remove_geofence;
	ops->remove_place = remove_place;
	ops->get_place_name = get_place_name;
	ops->get_list = get_list;
	ops->get_place_list = get_place_list;

	GeofenceManagerData *geofence_manager = g_new0(GeofenceManagerData, 1);
	g_return_val_if_fail(geofence_manager, NULL);

	geofence_manager->geofence_cb = NULL;
	geofence_manager->geofence_event_cb = NULL;
	geofence_manager->userdata = NULL;

	return (gpointer) geofence_manager;
}

GEOFENCE_MODULE_API void shutdown(gpointer handle)
{
	MOD_LOGD("shutdown");
	g_return_if_fail(handle);
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;

	if (geofence_manager->geofence_client) {
		geo_client_stop(geofence_manager->geofence_client);
		geo_client_destroy(geofence_manager->geofence_client);
		geofence_manager->geofence_client = NULL;
	}

	geofence_manager->geofence_cb = NULL;

	g_free(geofence_manager);
	geofence_manager = NULL;
}
