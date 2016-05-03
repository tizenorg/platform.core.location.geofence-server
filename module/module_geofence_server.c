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
#include <geofence_module.h>
#include "log.h"

#define GEOFENCE_SERVER_SERVICE_NAME	"org.tizen.lbs.Providers.GeofenceServer"
#define GEOFENCE_SERVER_SERVICE_PATH	"/org/tizen/lbs/Providers/GeofenceServer"

#define MYPLACES_APP_ID  "org.tizen.myplace"

typedef enum {
	ACCESS_TYPE_PRIVATE = 1,
	ACCESS_TYPE_PUBLIC,
	ACCESS_TYPE_UNKNOWN,
} access_type_e;

typedef struct {
	geofence_client_dbus_h geofence_client;
	GeofenceModCB geofence_cb;
	GeofenceModProximityCB geofence_proximity_cb;
	GeofenceModEventCB geofence_event_cb;
	gchar *app_id;
	gpointer userdata;
} GeofenceManagerData;

EXPORT_API int add_geopoint(void *handle, int place_id, double latitude, double longitude, int radius, const char *address, int *fence_id)
{
	MOD_LOGD("add_geopoint");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	int new_fence_id = -1;
	int error_code = GEOFENCE_MANAGER_ERROR_NONE;

	new_fence_id = geo_client_add_geofence(geofence_manager->geofence_client, geofence_manager->app_id, place_id, GEOFENCE_TYPE_GEOPOINT, latitude, longitude, radius, address, "", "", &error_code);
	*fence_id = new_fence_id;

	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;
	else if (new_fence_id == -1)
		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int add_bssid(void *handle, int place_id, const char *bssid, const char *ssid, geofence_type_e type, int *fence_id)
{
	MOD_LOGD("add_bssid");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	int new_fence_id = -1;
	int error_code = GEOFENCE_MANAGER_ERROR_NONE;

	new_fence_id = geo_client_add_geofence(geofence_manager->geofence_client, geofence_manager->app_id, place_id, type, -1, -1, -1, "", bssid, ssid, &error_code);
	*fence_id = new_fence_id;

	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;
	else if (new_fence_id == -1)
		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int add_place(void *handle, const char *place_name, int *place_id)
{
	MOD_LOGD("add_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	int new_place_id = -1;
	int error_code = GEOFENCE_MANAGER_ERROR_NONE;

	new_place_id = geo_client_add_place(geofence_manager->geofence_client, geofence_manager->app_id, place_name, &error_code);
	*place_id = new_place_id;

	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;
	else if (new_place_id == -1)
		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int update_place(void *handle, int place_id, const char *place_name)
{
	MOD_LOGD("update_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = geo_client_update_place(geofence_manager->geofence_client, geofence_manager->app_id, place_id, place_name);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return ret;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int remove_geofence(void *handle, int fence_id)
{
	MOD_LOGD("remove_geofence");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = geo_client_delete_geofence(geofence_manager->geofence_client, geofence_manager->app_id, fence_id);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return ret;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int remove_place(void *handle, int place_id)
{
	MOD_LOGD("remove_place");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = geo_client_delete_place(geofence_manager->geofence_client, geofence_manager->app_id, place_id);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return ret;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int enable_service(void *handle, int fence_id, bool enable)
{
	MOD_LOGD("enable_service");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = geo_client_enable_geofence(geofence_manager->geofence_client, geofence_manager->app_id, fence_id, enable);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

static void geofence_callback(GVariant *param, void *user_data)
{
	g_return_if_fail(user_data);
	GeofenceManagerData *geofence_manager =	(GeofenceManagerData *)user_data;
	int fence_id, access_type, state;
	char *app_id = NULL;

	g_variant_get(param, "(siii)", &app_id, &fence_id, &access_type, &state);

	if (access_type == ACCESS_TYPE_PRIVATE) {
		if (!(g_strcmp0(geofence_manager->app_id, app_id))) {	/*Sending the alert only the app-id matches in case of private fence*/
			if (geofence_manager->geofence_cb)
				geofence_manager->geofence_cb(fence_id, state, geofence_manager->userdata);
		}
	} else {
		if (geofence_manager->geofence_cb)	/*Here filteration is done in the manager as public fences cannot be restricted/filtered.*/
			geofence_manager->geofence_cb(fence_id, state, geofence_manager->userdata);
	}
}

static void geofence_proximity_callback(GVariant *param, void *user_data)
{
	g_return_if_fail(user_data);
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)user_data;
	int fence_id, access_type, proximity_state, provider;
	char *app_id = NULL;

	g_variant_get(param, "(siiii)", &app_id, &fence_id, &access_type, &proximity_state, &provider);

	if (access_type == ACCESS_TYPE_PRIVATE) {
		if (!(g_strcmp0(geofence_manager->app_id, app_id))) {   /*Sending the alert only the app-id matches in case of private fence*/
			if (geofence_manager->geofence_proximity_cb)
				geofence_manager->geofence_proximity_cb(fence_id, proximity_state, provider, geofence_manager->userdata);
		}
	} else {
		if (geofence_manager->geofence_proximity_cb)      /*Here filteration is done in the manager as public fences cannot be restricted/filtered.*/
			geofence_manager->geofence_proximity_cb(fence_id, proximity_state, provider, geofence_manager->userdata);
	}
}

static void geofence_event_callback(GVariant *param, void *user_data)
{
	g_return_if_fail(user_data);
	GeofenceManagerData *geofence_manager =	(GeofenceManagerData *)user_data;
	int place_id, fence_id, access_type, error, state;
	char *app_id = NULL;

	g_variant_get(param, "(iiisii)", &place_id, &fence_id, &access_type, &app_id, &error, &state);


	MOD_LOGD("place_id: %d, fence_id: %d, Error: %d, State: %d(0x%x", place_id, fence_id, error, state, state);
	MOD_LOGD("app_id: %s", geofence_manager->app_id);

	if (access_type == ACCESS_TYPE_PUBLIC) {
		if (!g_strcmp0(app_id, MYPLACES_APP_ID) || !g_strcmp0(geofence_manager->app_id, app_id)) {
			if (geofence_manager->geofence_event_cb)
				geofence_manager->geofence_event_cb(place_id, fence_id, error, state, geofence_manager->userdata);
		}
	} else {
		if (!(g_strcmp0(geofence_manager->app_id, app_id))) {
			if (geofence_manager->geofence_event_cb)
				geofence_manager->geofence_event_cb(place_id, fence_id, error, state, geofence_manager->userdata);
		}
	}
}

EXPORT_API int get_place_name(void *handle, int place_id, char **place_name)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(place_name, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int error_code = GEOFENCE_MANAGER_ERROR_NONE;
	int ret = geo_client_get_place_name(geofence_manager->geofence_client, geofence_manager->app_id, place_id, place_name, &error_code);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE)
		return ret;
	return error_code;
}

EXPORT_API int get_geofences(void *handle, int place_id, int *fence_amount, int **fence_ids, geofence_s **params)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(fence_amount, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(fence_ids, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(params, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

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

	int ret = geo_client_get_geofences(geofence_manager->geofence_client, geofence_manager->app_id, place_id, &iter, &fence_cnt, &error_code);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE)
		return ret;
	else if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;

	*fence_amount = fence_cnt;
	MOD_LOGD("Total fence count : %d", *fence_amount);
	int *fence_id_array = (int *) g_slice_alloc0(sizeof(int) *fence_cnt);
	geofence_s *p = (geofence_s *) g_slice_alloc0(sizeof(geofence_s) *fence_cnt);

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
				g_strlcpy(p[index].address, g_variant_get_string(value, NULL), ADDRESS_LEN);
			} else if (!g_strcmp0(key, "bssid")) {
				g_strlcpy(p[index].bssid, g_variant_get_string(value, NULL), WLAN_BSSID_LEN);
			} else if (!g_strcmp0(key, "ssid")) {
				g_strlcpy(p[index].ssid, g_variant_get_string(value, NULL), WLAN_BSSID_LEN);
			}
		}
		MOD_LOGI("Fence_id: %d, Place_id: %d, Type: %d, lat: %f, lon: %f, rad: %d, address: %s, bssid: %s, ssid: %s", fence_id_array[index], p[index].place_id, p[index].type, p[index].latitude, p[index].longitude, p[index].radius, p[index].address, p[index].bssid, p[index].ssid);
		index++;
		g_variant_iter_free(iter_row);
	}
	if (iter != NULL)
		g_variant_iter_free(iter);
	*params = (geofence_s *) p;
	*fence_ids = fence_id_array;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int get_places(void *handle, int *place_amount, int **place_ids, place_s **params)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(place_amount, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(place_ids, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(params, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

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
	int error_code = GEOFENCE_MANAGER_ERROR_NONE;

	/*Call the geofence_client api here....*/
	int ret = geo_client_get_places(geofence_manager->geofence_client, geofence_manager->app_id, &iter, &place_cnt, &error_code);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE)
		return ret;
	if (error_code != GEOFENCE_MANAGER_ERROR_NONE)
		return error_code;

	*place_amount = place_cnt;
	MOD_LOGI("Total place count : %d", *place_amount);
	int *place_id_array = (int *)g_slice_alloc0(sizeof(int) *place_cnt);
	place_s *p = (place_s *)g_slice_alloc0(sizeof(place_s) *place_cnt);

	if (iter == NULL)
		MOD_LOGI("Iterator is null");

	while (g_variant_iter_next(iter, "a{sv}", &iter_row)) {
		while (g_variant_iter_loop(iter_row, "{sv}", &key, &value)) {
			if (!g_strcmp0(key, "place_id"))
				place_id_array[index] = g_variant_get_int32(value);
			else if (!g_strcmp0(key, "place_name"))
				g_strlcpy(p[index].place_name, g_variant_get_string(value, NULL), PLACE_NAME_LEN);
		}
		MOD_LOGI("place_id: %d, place_name: %s", place_id_array[index], p[index].place_name);
		index++;
		g_variant_iter_free(iter_row);
	}

	if (iter != NULL)
		g_variant_iter_free(iter);

	*params = (place_s *)p;
	*place_ids = place_id_array;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

static void on_signal_callback(const gchar *sig, GVariant *param, gpointer user_data)
{
	if (!g_strcmp0(sig, "GeofenceInout")) {
		MOD_LOGD("GeofenceInoutChanged");
		geofence_callback(param, user_data);
	} else if (!g_strcmp0(sig, "GeofenceProximity")) {
		MOD_LOGD("GeofenceProximityChanged");
		geofence_proximity_callback(param, user_data);
	} else if (!g_strcmp0(sig, "GeofenceEvent")) {
		MOD_LOGD("GeofenceEventInvoked");
		geofence_event_callback(param, user_data);
	} else {
		MOD_LOGD("Invalid signal[%s]", sig);
	}
}

EXPORT_API int start_geofence(void *handle, int fence_id)
{
	MOD_LOGD("start_geofence");
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_EXCEPTION);

	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	MOD_LOGD("geofence-server(%x)", geofence_manager);

	ret = geo_client_start_geofence(geofence_manager->geofence_client, geofence_manager->app_id, fence_id);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to start geofence_client_h. Error[%d]", ret);
		return ret;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int stop_geofence(void *handle, int fence_id)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	MOD_LOGD("geofence_manager->geofence_cb : %x", geofence_manager->geofence_cb);
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(geofence_manager->geofence_client,	GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = GEOFENCE_CLIENT_ERROR_NONE;

	ret = geo_client_stop_geofence(geofence_manager->geofence_client, geofence_manager->app_id, fence_id);
	if (ret != GEOFENCE_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to stop. Error[%d]", ret);
		return ret;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int create(void *handle, GeofenceModCB geofence_cb, GeofenceModProximityCB geofence_proximity_cb, GeofenceModEventCB geofence_event_cb, void *userdata)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *) handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(geofence_cb, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	/* create connnection */
	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	geofence_manager->geofence_cb = geofence_cb;
	geofence_manager->geofence_proximity_cb = geofence_proximity_cb;
	geofence_manager->geofence_event_cb = geofence_event_cb;
	geofence_manager->userdata = userdata;

	ret = geo_client_create(&(geofence_manager->geofence_client));
	if (ret != GEOFENCE_CLIENT_ERROR_NONE || !geofence_manager->geofence_client) {
		MOD_LOGE("Fail to create geofence_client_dbus_h. Error[%d]", ret);
		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;
	}

	MOD_LOGD("geofence_manager->geofence_client: %p", geofence_manager->geofence_client);
	ret = geo_client_start(geofence_manager->geofence_client, on_signal_callback, geofence_manager);

	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		if (ret == GEOFENCE_CLIENT_ACCESS_DENIED) {
			MOD_LOGE("Access denied[%d]", ret);
			return GEOFENCE_CLIENT_ACCESS_DENIED;
		}
		MOD_LOGE("Fail to start geofence_client_dbus_h. Error[%d]", ret);
		geo_client_destroy(geofence_manager->geofence_client);
		geofence_manager->geofence_client = NULL;

		return GEOFENCE_CLIENT_ERROR_DBUS_CALL;
	}

	return GEOFENCE_MANAGER_ERROR_NONE;
}

EXPORT_API int destroy(void *handle)
{
	MOD_LOGD("destroy");

	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_val_if_fail(geofence_manager, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);
	g_return_val_if_fail(geofence_manager->geofence_client, GEOFENCE_MANAGER_ERROR_INVALID_PARAMETER);

	int ret = GEOFENCE_MANAGER_ERROR_NONE;

	ret = geo_client_stop(geofence_manager->geofence_client);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		MOD_LOGE("Fail to stop. Error[%d]", ret);
		geo_client_destroy(geofence_manager->geofence_client);
		geofence_manager->geofence_client = NULL;
		return GEOFENCE_MANAGER_ERROR_IPC;
	}

	ret = geo_client_destroy(geofence_manager->geofence_client);
	if (ret != GEOFENCE_CLIENT_ERROR_NONE) {
		MOD_LOGE("Fail to destroy. Error[%d]", ret);
		return GEOFENCE_MANAGER_ERROR_IPC;
	}
	geofence_manager->geofence_client = NULL;
	geofence_manager->geofence_cb = NULL;
	geofence_manager->geofence_event_cb = NULL;
	geofence_manager->userdata = NULL;

	return GEOFENCE_MANAGER_ERROR_NONE;
}

static void __get_caller_app_id(void *handle)
{
	GeofenceManagerData *geofence_manager = (GeofenceManagerData *)handle;
	g_return_if_fail(geofence_manager);

	gchar *app_id = NULL;
	int ret = 0;

	pid_t pid = 0;
	pid = getpid();
	ret = app_manager_get_app_id(pid, &app_id);
	if (ret != APP_MANAGER_ERROR_NONE) {
		MOD_LOGE("Fail to get app_id from module_geofence_server. Err[%d]", ret);
	} else {
		MOD_LOGD("app_id: %s", app_id);
		geofence_manager->app_id = app_id;
	}
}

EXPORT_API gpointer init(GeofenceModOps *ops)
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
	ops->remove_geofence = remove_geofence;
	ops->get_geofences = get_geofences;

	ops->add_place = add_place;
	ops->update_place = update_place;
	ops->remove_place = remove_place;
	ops->get_place_name = get_place_name;
	ops->get_places = get_places;

	GeofenceManagerData *geofence_manager = g_new0(GeofenceManagerData, 1);
	g_return_val_if_fail(geofence_manager, NULL);

	geofence_manager->geofence_cb = NULL;
	geofence_manager->geofence_proximity_cb = NULL;
	geofence_manager->geofence_event_cb = NULL;
	geofence_manager->userdata = NULL;

	__get_caller_app_id(geofence_manager);

	return (gpointer) geofence_manager;
}

EXPORT_API void shutdown(gpointer handle)
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
	g_free(geofence_manager->app_id);
	g_free(geofence_manager);
	geofence_manager = NULL;
}
