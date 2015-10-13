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

#include <sqlite3.h>
#include <sys/time.h>
#include <db-util.h>
#include <gio/gio.h>
#include <sys/stat.h>
#include <string.h>
#include <bluetooth.h>
#include <wifi.h>
#include <tzplatform_config.h>
#include <sys/types.h>
#include <fcntl.h>

#include "debug_util.h"
#include "geofence_server.h"
#include "geofence_server_db.h"
#include "geofence_server_private.h"

/* dbspace path for Tizen 3.0 was changed.
#define GEOFENCE_SERVER_DB_FILE                ".geofence-server.db"
#define GEOFENCE_SERVER_DB_PATH                "/opt/dbspace/"GEOFENCE_SERVER_DB_FILE
*/

#define GEOFENCE_DB_NAME	".geofence-server.db"
#define GEOFENCE_DB_FILE	tzplatform_mkpath(TZ_USER_DB, GEOFENCE_DB_NAME)

#define MAX_DATA_NAME		20
#define DATA_LEN			20

#define GEOFENCE_INVALID	0

char *menu_table[4] = { "GeoFence", "FenceGeocoordinate", "FenceGeopointWifi", "FenceBssid" };

const char *group_id = NULL;

#ifdef SUPPORT_ENCRYPTION
static char *password = "k1s2c3w4k5a6";
const char *col_latitude = "la";
const char *col_longitude = "lo";
const char *col_radius = "r";
#endif

typedef enum {
    FENCE_MAIN_TABLE = 0,	/*GeoFence */
    FENCE_GEOCOORDINATE_TAB,	/*FenceGeocoordinate */
    FENCE_GEOPOINT_WIFI_TABLE,	/*FenceCurrentLocation */
    FENCE_BSSID_TABLE	/*FenceBluetoothBssid */
} fence_table_type_e;

static struct {
	sqlite3 *handle;
} db_info_s = {
	.handle = NULL,
};

#define SQLITE3_RETURN(ret, msg, state) \
	if (ret != SQLITE_OK) { \
		LOGI_GEOFENCE("sqlite3 Error[%d] : %s", ret, msg); \
		sqlite3_reset(state); \
		sqlite3_clear_bindings(state); \
		sqlite3_finalize(state); \
		return FENCE_ERR_SQLITE_FAIL; \
	}

/*
 * \note
 * DB Table schema
 *
 * GeoFence
 * +----------+-------+-------+------------+-------+-------+-----------+---------+
 * | fence_id | name     | app_id  | geofence_type |direction |enable    |smart_assist_id|time_stamp
 * +-------+-------+-------+-------+
 * |   -   |   -   |   -   |   -   |
 * +-------+-------+-------+-------+
 * CREATE TABLE GeoFence ( fence_id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, app_id TEXT NOT NULL, geofence_type INTEGER," \
		"direction INTEGER,  enable INTEGER,  smart_assist_id INTEGER, time_stamp INTEGER)";
 *
 *
 * FenceGeocoordinate
 * +----------+---------+--------+------+
 * | fence_id | latitude     | longitude | radius
 * +-------+---------+-----+---------+
 * |   -   |    -    |  -  |    -    |     -    |    -    |
 * +-------+---------+-----+---------+
 *  CREATE TABLE FenceGeocoordinate ( fence_id INTEGER , latitude DOUBLE, longitude DOUBLE, radius DOUBLE, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE)";
 *
 *
 * FenceCurrentLocation
 * +-----+-------+------
 * |bssid 1|fence_id1 |...
 * +-----+-------+------
 * |bssid 2|fence_id1|...
 * +-----+-------+------
 * |bssid 3|fence_id1|...
 * +-----+-------+------
 * |bssid 1|fence_id2|...
 * +-----+-------+------
 * |bssid 2|fence_id2|...
 * +-------+---------+-----+---------+
 * |   -   |    -    |  -  |    -    |     -    |    -    |
 * +-------+---------+-----+---------+
*CREATE TABLE FenceCurrentLocation ( fence_id INTEGER,  bssid TEXT,  FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE)";
*/

static inline int begin_transaction(void)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *stmt;
	int ret;

	ret = sqlite3_prepare_v2(db_info_s.handle, "BEGIN TRANSACTION", -1, &stmt, NULL);

	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		LOGI_GEOFENCE("Failed to do update (%s)", sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(stmt);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(stmt);
	return FENCE_ERR_NONE;
}

static inline int rollback_transaction(void)
{
	FUNC_ENTRANCE_SERVER;
	int ret;
	sqlite3_stmt *stmt;

	ret = sqlite3_prepare_v2(db_info_s.handle, "ROLLBACK TRANSACTION", -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		LOGI_GEOFENCE("Failed to do update (%s)", sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(stmt);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(stmt);
	return FENCE_ERR_NONE;
}

static inline int commit_transaction(void)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *stmt;
	int ret;

	ret = sqlite3_prepare_v2(db_info_s.handle, "COMMIT TRANSACTION", -1, &stmt, NULL);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		LOGI_GEOFENCE("Failed to do update (%s)", sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(stmt);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(stmt);
	return FENCE_ERR_NONE;
}

static inline int __geofence_manager_db_create_places_table(void)
{
	FUNC_ENTRANCE_SERVER;
	char *err = NULL;
	char *ddl;

	ddl = sqlite3_mprintf("CREATE TABLE Places ( place_id INTEGER PRIMARY KEY AUTOINCREMENT, access_type INTEGER, place_name TEXT NOT NULL, app_id TEXT NOT NULL)");
	if (sqlite3_exec(db_info_s.handle, ddl, NULL, NULL, &err) != SQLITE_OK) {
		LOGI_GEOFENCE("Failed to execute the DDL (%s)", err);
		sqlite3_free(ddl);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_changes(db_info_s.handle) == 0) {
		LOGI_GEOFENCE("No changes  to DB");
	}
	sqlite3_free(ddl);
	return FENCE_ERR_NONE;
}

static inline int __geofence_manager_db_create_geofence_table(void)
{
	FUNC_ENTRANCE_SERVER;
	char *err = NULL;
	char *ddl;

	ddl = sqlite3_mprintf("CREATE TABLE GeoFence ( fence_id INTEGER PRIMARY KEY AUTOINCREMENT, place_id INTEGER, enable INTEGER, app_id TEXT NOT NULL, geofence_type INTEGER, access_type INTEGER, running_status INTEGER, FOREIGN KEY(place_id) REFERENCES Places(place_id) ON DELETE CASCADE)");

	if (sqlite3_exec(db_info_s.handle, ddl, NULL, NULL, &err) != SQLITE_OK) {
		LOGI_GEOFENCE("Failed to execute the DDL (%s)", err);
		sqlite3_free(ddl);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_changes(db_info_s.handle) == 0) {
		LOGI_GEOFENCE("No changes  to DB");
	}
	sqlite3_free(ddl);
	return FENCE_ERR_NONE;
}

static inline int __geofence_manager_db_create_geocoordinate_table(void)
{
	FUNC_ENTRANCE_SERVER;
	char *err = NULL;
	char *ddl;

	ddl = sqlite3_mprintf("CREATE TABLE FenceGeocoordinate ( fence_id INTEGER PRIMARY KEY, latitude TEXT NOT NULL, longitude TEXT NOT NULL, radius TEXT NOT NULL, address TEXT, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE ON UPDATE CASCADE)");

	if (sqlite3_exec(db_info_s.handle, ddl, NULL, NULL, &err) != SQLITE_OK) {
		LOGI_GEOFENCE("Failed to execute the DDL (%s)", err);
		sqlite3_free(ddl);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_changes(db_info_s.handle) == 0) {
		LOGI_GEOFENCE("No changes to DB");
	}
	sqlite3_free(ddl);
	return FENCE_ERR_NONE;
}

static inline int __geofence_manager_db_create_wifi_data_table(void)
{
	FUNC_ENTRANCE_SERVER;
	char *err = NULL;
	char *ddl;

	ddl = sqlite3_mprintf("CREATE TABLE FenceGeopointWifi (fence_id INTEGER, bssid TEXT, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE ON UPDATE CASCADE)");

	if (sqlite3_exec(db_info_s.handle, ddl, NULL, NULL, &err) != SQLITE_OK) {
		LOGI_GEOFENCE("Failed to execute the DDL (%s)", err);
		sqlite3_free(ddl);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_changes(db_info_s.handle) == 0) {
		LOGI_GEOFENCE("No changes to DB");
	}
	sqlite3_free(ddl);
	return FENCE_ERR_NONE;
}

/* DB table for save the pair of fence id and bluetooth bssid */
static inline int __geofence_manager_db_create_bssid_table(void)
{
	FUNC_ENTRANCE_SERVER
	char *err = NULL;
	char *ddl;

	ddl = sqlite3_mprintf("CREATE TABLE FenceBssid (fence_id INTEGER PRIMARY KEY, bssid TEXT, ssid TEXT, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE ON UPDATE CASCADE)");

	if (sqlite3_exec(db_info_s.handle, ddl, NULL, NULL, &err) != SQLITE_OK) {
		LOGI_GEOFENCE("Failed to execute the DDL (%s)", err);
		sqlite3_free(ddl);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (sqlite3_changes(db_info_s.handle) == 0)
		LOGI_GEOFENCE("No changes to DB");
	sqlite3_free(ddl);
	return FENCE_ERR_NONE;
}

static int __geofence_manager_open_db_handle(const int open_flag)
{
	LOGI_GEOFENCE("enter");
	int ret = SQLITE_OK;

	ret = db_util_open_with_options(GEOFENCE_DB_FILE, &db_info_s.handle, open_flag, NULL);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("sqlite3_open_v2 Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		return FENCE_ERR_SQLITE_FAIL;
	}

	return FENCE_ERR_NONE;
}

static int __geofence_manager_db_get_count_by_fence_id_and_bssid(int fence_id, char *bssid, fence_table_type_e table_type, int *count)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(bssid, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM %Q where fence_id = %d AND bssid = %Q;", menu_table[table_type], fence_id, bssid);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	*count = sqlite3_column_int(state, 0);
	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

static int __geofence_manager_db_insert_bssid_info(const int fence_id, const char *bssid_info, const char *ssid)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(bssid_info, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	int index = 0;
	int count = -1;
	const char *tail;
	char *bssid = NULL;

	char *query = sqlite3_mprintf("INSERT INTO %Q(fence_id, bssid, ssid) VALUES (?, ?, ?)", menu_table[FENCE_BSSID_TABLE]);
	bssid = (char *)g_malloc0(sizeof(char) * WLAN_BSSID_LEN);
	g_strlcpy(bssid, bssid_info, WLAN_BSSID_LEN);
	LOGI_GEOFENCE("fence_id[%d], bssid[%s], ssid[%s]", fence_id, bssid, ssid);

	ret = __geofence_manager_db_get_count_by_fence_id_and_bssid(fence_id, bssid, FENCE_BSSID_TABLE, &count);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("__geofence_manager_db_get_count_by_fence_id_and_bssid() failed. ERROR(%d)", ret);
		sqlite3_free(query);
		return ret;
	}
	if (count > 0) {
		LOGI_GEOFENCE("count = %d", count);
		return FENCE_ERR_NONE;
	}

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_bind_int(state, ++index, fence_id);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, bssid, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, ssid, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		g_free(bssid);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_reset(state);
	sqlite3_clear_bindings(state);
	sqlite3_finalize(state);
	g_free(bssid);
	sqlite3_free(query);
	LOGI_GEOFENCE("fence_id[%d], bssid[%s], ssid[%s] inserted db table [%s] successfully.",	fence_id, bssid_info, ssid, menu_table[FENCE_BSSID_TABLE]);

	return FENCE_ERR_NONE;
}

static int __geofence_manager_db_insert_wifi_data_info(gpointer data, gpointer user_data)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(data, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(user_data, FENCE_ERR_INVALID_PARAMETER);
	int *fence_id = (int *) user_data;
	sqlite3_stmt *state = NULL;
	wifi_info_s *wifi_info = NULL;
	int ret = SQLITE_OK;
	int index = 0;
	int count = -1;
	const char *tail;
	char *bssid = NULL;
	wifi_info = (wifi_info_s *) data;
	bssid = (char *)g_malloc0(sizeof(char) * WLAN_BSSID_LEN);
	g_strlcpy(bssid, wifi_info->bssid, WLAN_BSSID_LEN);
	LOGI_GEOFENCE("fence_id[%d] bssid[%s]", *fence_id, wifi_info->bssid);

	char *query = sqlite3_mprintf("INSERT INTO FenceGeopointWifi(fence_id, bssid) VALUES (?, ?)");

	ret = __geofence_manager_db_get_count_by_fence_id_and_bssid(*fence_id, bssid, FENCE_GEOPOINT_WIFI_TABLE, &count);
	if (count > 0) {
		LOGI_GEOFENCE("count = %d", count);
		sqlite3_free(query);
		return ret;
	}

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_bind_int(state, ++index, *fence_id);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, bssid, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		g_free(bssid);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_reset(state);
	sqlite3_clear_bindings(state);
	sqlite3_finalize(state);
	g_free(bssid);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

static int __geofence_manager_delete_table(int fence_id, fence_table_type_e table_type)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;

	char *query = sqlite3_mprintf("DELETE from %Q where fence_id = %d;", menu_table[table_type], fence_id);
	LOGI_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, NULL);
	if (SQLITE_OK != ret) {
		LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	ret = sqlite3_step(state);
	if (SQLITE_DONE != ret) {
		LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	sqlite3_finalize(state);
	LOGI_GEOFENCE("fence_id[%d], deleted from db table [%s] successfully.", fence_id, menu_table[table_type]);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

static int __geofence_manager_delete_place_table(int place_id)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;

	char *query = sqlite3_mprintf("DELETE from Places where place_id = %d;", place_id);

	LOGI_GEOFENCE("current place id is [%d]", place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, NULL);
	if (SQLITE_OK != ret) {
		LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	ret = sqlite3_step(state);
	if (SQLITE_DONE != ret) {
		LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	sqlite3_finalize(state);
	LOGI_GEOFENCE("place_id[%d], deleted place from db table Places successfully.", place_id);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

static inline void __geofence_manager_db_create_table(void)
{
	FUNC_ENTRANCE_SERVER;
	int ret;
	begin_transaction();

	ret = __geofence_manager_db_create_places_table();
	if (ret < 0) {
		rollback_transaction();
		return;
	}

	ret = __geofence_manager_db_create_geofence_table();
	if (ret < 0) {
		rollback_transaction();
		return;
	}

	ret = __geofence_manager_db_create_geocoordinate_table();
	if (ret < 0) {
		rollback_transaction();
		return;
	}

	ret = __geofence_manager_db_create_wifi_data_table();
	if (ret < 0) {
		rollback_transaction();
		return;
	}

	ret = __geofence_manager_db_create_bssid_table();
	if (ret < 0) {
		rollback_transaction();
		return;
	}

	commit_transaction();
}

/* Get fence id count in certain table, such as GeoFence/FenceGeocoordinate/FenceCurrentLocation */
static int __geofence_manager_db_get_count_of_fence_id(int fence_id, fence_table_type_e table_type, int *count)
{
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM %Q where fence_id = %d;", menu_table[table_type], fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	*count = sqlite3_column_int(state, 0);
	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

static int __geofence_manager_db_enable_foreign_keys(void)
{
	sqlite3_stmt *state = NULL;
	int ret = FENCE_ERR_NONE;
	char *query = sqlite3_mprintf("PRAGMA foreign_keys = ON;");

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, NULL);
	if (SQLITE_OK != ret) {
		LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	ret = sqlite3_step(state);
	if (SQLITE_DONE != ret) {
		LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

#ifdef SUPPORT_ENCRYPTION
void replaceChar(char *src, char oldChar, char newChar)
{
	while (*src) {
		if (*src == oldChar)
			*src = newChar;
		src++;
	}
}

void __geofence_manager_generate_password(char *password)
{
	char *bt_address = NULL;
	char *wifi_address = NULL;
	char *token = NULL, *save_token = NULL;
	int bt_temp[6] = {0}, wifi_temp[6] = {0};
	int i = 0, fkey[6], lkey[6];
	char s1[100], s2[100], result[200];
	char keyword[6] = { 'b', 'w', 'd', 's', 'j', 'f' };
	int ret = 0;

	ret = bt_adapter_get_address(&bt_address);
	if (ret != BT_ERROR_NONE) {
		LOGD_GEOFENCE("bt address get fail %d", ret);
	}

	ret = wifi_get_mac_address(&wifi_address);
	if (ret != WIFI_ERROR_NONE) {
		LOGD_GEOFENCE("wifi address get fail %d", ret);
	}

	if (bt_address) {
		token = strtok_r(bt_address, ":", &save_token);
		i = 0;
		while (token) {
			bt_temp[i++] = atoi(token);
			token = strtok_r(NULL, ":", &save_token);
			if (i >= 6)
				break;
		}
	}

	if (wifi_address) {
		token = strtok_r(wifi_address, ":", &save_token);
		i = 0;
		while (token) {
			wifi_temp[i++] = atoi(token);
			token = strtok_r(NULL, ":", &save_token);
			if (i >= 6)
				break;
		}
	}

	memset((void *) s1, 0, sizeof(s1));
	memset((void *) s2, 0, sizeof(s2));
	memset((void *) result, 0, sizeof(result));

	for (i = 0; i < 6; i++) {
		fkey[i] = bt_temp[i] * wifi_temp[i];
		lkey[i] = bt_temp[i] + wifi_temp[i];
	}

	for (i = 0; i < 6; i++) {
		sprintf(s1, "%s%x", s1, fkey[i]);
		sprintf(s2, "%s%x", s2, lkey[i]);
		replaceChar(s1, 0x30 + ((i * 2) % 10), keyword[i]);
		replaceChar(s2, 0x30 + ((i * 2 + 1) % 10), keyword[i]);
		LOGD_GEOFENCE("s1 %s", s1);
		LOGD_GEOFENCE("s2 %s", s2);
	}

	sprintf(result, "%s%s", s1, s2);
	LOGD_GEOFENCE("result : %s", result);

	password = result;

	if (bt_address != NULL)
		free(bt_address);
	if (wifi_address != NULL)
		free(wifi_address);
}
#endif

static int __check_db_file()
{
	int fd = -1;

    fd = open(GEOFENCE_DB_FILE, O_RDONLY);
    if (fd < 0) {
             LOGW_GEOFENCE("DB file(%s) is not exist.", GEOFENCE_DB_FILE);
             return -1;
     }
     close(fd);
     return 0;
}

/**
 * This function in DB and create  GeoFence/FenceGeocoordinate /FenceCurrentLocation four table on DB if necessary.
 *
 * @param[in]    struct of  fence_point_info_s
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_db_init(void)
{
	FUNC_ENTRANCE_SERVER;
	struct stat stat;
	int open_flag = 0;

	/*
	geofence_db_file = g_strdup_printf("%s/%s", GEOFENCE_DB_PATH, GEOFENCE_DB_FILE);
	*/

	if (__check_db_file()) {
		LOGW_GEOFENCE("db(%s) file doesn't exist.", GEOFENCE_DB_FILE);
		open_flag = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE| SQLITE_OPEN_FULLMUTEX;
	} else {
		if (lstat(GEOFENCE_DB_FILE, &stat) < 0) {
			LOGE_GEOFENCE("Can't get db(%s) information.", GEOFENCE_DB_FILE);
			return FENCE_ERR_SQLITE_FAIL;
		}
		open_flag = SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX;
	}

	if (__geofence_manager_open_db_handle(open_flag) != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to create db file(%s).", GEOFENCE_DB_FILE);
		return FENCE_ERR_SQLITE_FAIL;
	}

	if (open_flag & SQLITE_OPEN_CREATE)
		__geofence_manager_db_create_table();

	return FENCE_ERR_NONE;
}

int geofence_manager_db_reset(void)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int idx = 0;
	int ret = SQLITE_OK;
	char *query = NULL;

	for (idx = 0; idx < 4; idx++) {
		query = sqlite3_mprintf("DELETE from %Q;", menu_table[idx]);

		ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, NULL);
		if (SQLITE_OK != ret) {
			LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
			sqlite3_free(query);
			return FENCE_ERR_SQLITE_FAIL;
		}

		ret = sqlite3_step(state);
		if (SQLITE_DONE != ret) {
			LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
			sqlite3_finalize(state);
			sqlite3_free(query);
			return FENCE_ERR_SQLITE_FAIL;
		}
		sqlite3_finalize(state);
		sqlite3_free(query);
	}
	return FENCE_ERR_NONE;
}

int geofence_manager_set_place_info(place_info_s *place_info, int *place_id)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(place_info, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(place_id, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	int index = 0;
	const char *tail;
	char *appid = NULL;
	char *place_name = NULL;
	char *query = sqlite3_mprintf("INSERT INTO Places (access_type, place_name, app_id) VALUES (?, ?, ?)");

	place_name = (char *)g_malloc0(sizeof(char) * PLACE_NAME_LEN);
	g_strlcpy(place_name, place_info->place_name, PLACE_NAME_LEN);
	appid = (char *)g_malloc0(sizeof(char) * APP_ID_LEN);
	g_strlcpy(appid, place_info->appid, APP_ID_LEN);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}
	LOGD_GEOFENCE("appid[%s] access_type[%d] place_name[%s]", appid, place_info->access_type, place_info->place_name);

	ret = sqlite3_bind_int(state, ++index, place_info->access_type);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, place_name, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, appid, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		g_free(place_name);
		g_free(appid);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	*place_id = sqlite3_last_insert_rowid(db_info_s.handle);
	LOGI_GEOFENCE(" auto-genarated place_id[%d]", *place_id);
	sqlite3_reset(state);
	sqlite3_clear_bindings(state);
	sqlite3_finalize(state);
	g_free(place_name);
	g_free(appid);
	sqlite3_free(query);

	if (*place_id < 1) {
		LOGI_GEOFENCE("TMP Invalid fence_id");
		*place_id = 0;
	}

	return FENCE_ERR_NONE;
}

int geofence_manager_set_common_info(fence_common_info_s *fence_info, int *fence_id)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_info, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	int index = 0;
	const char *tail;
	char *appid = NULL;
	char *query = sqlite3_mprintf("INSERT INTO GeoFence (place_id, enable, app_id, geofence_type, access_type, running_status) VALUES (?, ?, ?, ?, ?, ?)");
	appid = (char *)g_malloc0(sizeof(char) * APP_ID_LEN);
	g_strlcpy(appid, fence_info->appid, APP_ID_LEN);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	LOGD_GEOFENCE("place_id[%d], enable[%d], appid[%s] geofence_type[%d] access_type[%d] running_status[%d]", fence_info->place_id, fence_info->enable, appid, fence_info->running_status, fence_info->type, fence_info->access_type, fence_info->place_id);

	ret = sqlite3_bind_int(state, ++index, fence_info->place_id);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_int(state, ++index, fence_info->enable);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, appid, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_int(state, ++index, fence_info->type);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_int(state, ++index, fence_info->access_type);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_int(state, ++index, fence_info->running_status);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		g_free(appid);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	*fence_id = sqlite3_last_insert_rowid(db_info_s.handle);
	LOGI_GEOFENCE(" auto-genarated fence_id[%d]", *fence_id);
	sqlite3_reset(state);
	sqlite3_clear_bindings(state);
	sqlite3_finalize(state);
	g_free(appid);
	sqlite3_free(query);

	if (*fence_id < 1) {
		LOGI_GEOFENCE("TMP Invalid fence_id");
		*fence_id = 0;
	}

	return FENCE_ERR_NONE;
}

int geofence_manager_get_place_list_from_db(int *number_of_places, GList **places)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;
	int count = 0;

	query = sqlite3_mprintf("SELECT place_id, place_name, access_type, app_id FROM Places");

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}
	GList *place_list = NULL;
	int column_index = 0;
	do {
		ret = sqlite3_step(state);

		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("DONE...!!! : %d", ret);
			break;
		}
		column_index = 0;
		place_info_s *place = g_slice_new0(place_info_s);

		if (place == NULL)
			continue;

		place->place_id = sqlite3_column_int(state, column_index++);
		g_strlcpy(place->place_name, (char *) sqlite3_column_text(state, column_index++), PLACE_NAME_LEN);
		place->access_type = sqlite3_column_int(state, column_index++);
		g_strlcpy(place->appid, (char *) sqlite3_column_text(state, column_index++), APP_ID_LEN);
		place_list = g_list_append(place_list, place);
		count++;
	} while (ret != SQLITE_DONE);

	*places = place_list;
	*number_of_places = count;

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

int geofence_manager_get_fence_list_from_db(int *number_of_fences, GList **fences, int place_id)
{
	FUNC_ENTRANCE_SERVER;

	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;
	int count = 0;

	if (place_id == -1)
		query = sqlite3_mprintf("SELECT DISTINCT A.fence_id, A.app_id, A.geofence_type, A.access_type, A.place_id, B.latitude, B.longitude, B.radius, B.address, C.bssid, C.ssid FROM GeoFence A LEFT JOIN FenceGeocoordinate B ON A.fence_id = B.fence_id LEFT JOIN FenceBssid C ON A.fence_id = C.fence_id GROUP BY A.fence_id");
	else
		query = sqlite3_mprintf("SELECT DISTINCT A.fence_id, A.app_id, A.geofence_type, A.access_type, A.place_id, B.latitude, B.longitude, B.radius, B.address, C.bssid, C.ssid FROM GeoFence A LEFT JOIN FenceGeocoordinate B ON A.fence_id = B.fence_id LEFT JOIN FenceBssid C ON A.fence_id = C.fence_id WHERE A.place_id = %d GROUP BY A.fence_id", place_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}
	GList *fence_list = NULL;
	do {
		ret = sqlite3_step(state);

		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("DONE...!!! : %d", ret);
			break;
		}
		int column_index = 0;

		geofence_info_s *fence = g_slice_new0(geofence_info_s);

		if (fence == NULL)
			continue;

		fence->fence_id = sqlite3_column_int(state, column_index++);
		g_strlcpy(fence->app_id, (char *) sqlite3_column_text(state, column_index++), APP_ID_LEN);
		fence->param.type = sqlite3_column_int(state, column_index++);
		fence->access_type = sqlite3_column_int(state, column_index++);
		fence->param.place_id = sqlite3_column_int(state, column_index++);
		char *data_name = NULL;

		data_name = (char *) sqlite3_column_text(state, column_index++);
		if (!data_name || !strlen(data_name))
			LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
		else
			fence->param.latitude = atof(data_name);

		data_name = (char *) sqlite3_column_text(state, column_index++);
		if (!data_name || !strlen(data_name))
			LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
		else
			fence->param.longitude = atof(data_name);

		data_name = (char *) sqlite3_column_text(state, column_index++);
		if (!data_name || !strlen(data_name))
			LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
		else
			fence->param.radius = atof(data_name);

		g_strlcpy(fence->param.address, (char *) sqlite3_column_text(state, column_index++), ADDRESS_LEN);
		g_strlcpy(fence->param.bssid, (char *) sqlite3_column_text(state, column_index++), WLAN_BSSID_LEN);
		g_strlcpy(fence->param.ssid, (char *) sqlite3_column_text(state, column_index++), WLAN_BSSID_LEN);
		LOGI_GEOFENCE("radius = %d, bssid = %s", fence->param.radius, fence->param.bssid);
		fence_list = g_list_append(fence_list, fence);
		count++;
	} while (ret != SQLITE_DONE);

	*fences = fence_list;
	*number_of_fences = count;

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

int geofence_manager_get_fenceid_list_from_db(int *number_of_fences, GList **fences, int place_id)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;
	int count = 0;
	query = sqlite3_mprintf("SELECT fence_id FROM GeoFence WHERE place_id = %d", place_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}
	GList *fence_list = NULL;
	int column_index = 0;
	int fence_id = 0;
	do {
		ret = sqlite3_step(state);
		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("DONE...!!! : %d", ret);
			break;
		}
		fence_id = 0;
		fence_id = sqlite3_column_int(state, column_index);
		fence_list = g_list_append(fence_list, GINT_TO_POINTER(fence_id));
		count++;
	} while (ret != SQLITE_DONE);
	*fences = fence_list;
	*number_of_fences = count;

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

int geofence_manager_update_geocoordinate_info(int fence_id, geocoordinate_info_s *geocoordinate_info)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(geocoordinate_info, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	const char *tail;
	int ret = SQLITE_OK;

	char *query = sqlite3_mprintf("UPDATE FenceGeocoordinate SET latitude = %lf, longitude = %lf, radius = %lf where fence_id = %d;", geocoordinate_info->latitude, geocoordinate_info->longitude, geocoordinate_info->radius, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	LOGI_GEOFENCE("fence_id: %d has been successfully updated.", fence_id);
	return FENCE_ERR_NONE;
}

int geofence_manager_update_place_info(int place_id, const char *place_info_name)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(place_id, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	const char *tail;
	int ret = SQLITE_OK;
	char *place_name = NULL;

	place_name = (char *)g_malloc0(sizeof(char) * PLACE_NAME_LEN);
	g_strlcpy(place_name, place_info_name, PLACE_NAME_LEN);

	char *query = sqlite3_mprintf("UPDATE Places SET place_name = %Q where place_id = %d", place_name, place_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		g_free(place_name);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		g_free(place_name);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	g_free(place_name);
	sqlite3_free(query);
	LOGI_GEOFENCE("place_id: %d has been successfully updated.", place_id);
	return FENCE_ERR_NONE;
}

/**
 * This function set geocoordinate info  in DB.
 *
 * @param[in]		fence_id
 * @param[out]		struct of geocoordinate_info_s
 * @return		FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_geocoordinate_info(int fence_id, geocoordinate_info_s *geocoordinate_info)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(geocoordinate_info, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	int index = 0;
	const char *tail;
	int count = -1;
	char data_name_lat[MAX_DATA_NAME] = { 0 };
	char data_name_lon[MAX_DATA_NAME] = { 0 };
	char data_name_rad[MAX_DATA_NAME] = { 0 };
	char *query = sqlite3_mprintf("INSERT INTO FenceGeocoordinate(fence_id, latitude, longitude, radius, address) VALUES (?, ?, ?, ?, ?)");

	ret = __geofence_manager_db_get_count_of_fence_id(fence_id, FENCE_GEOCOORDINATE_TAB, &count);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to get geofence_manager_db_get_count_of_fence_id [%d]", ret);
		sqlite3_free(query);
		return ret;
	} else if (count) {	/* fence id has been in FenceGeocoordinate table */
		sqlite3_free(query);
		return FENCE_ERR_FENCE_ID;
	}

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_bind_int(state, ++index, fence_id);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

#ifdef SUPPORT_ENCRYPTION
	if (password == NULL)
		__geofence_manager_generate_password(password);
#endif

	ret = snprintf(data_name_lat, MAX_DATA_NAME, "%lf", geocoordinate_info->latitude);

	ret = sqlite3_bind_text(state, ++index, data_name_lat, -1, SQLITE_STATIC);

	/*ret = sqlite3_bind_double (state, ++index, geocoordinate_info->latitude);*/
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = snprintf(data_name_lon, MAX_DATA_NAME, "%lf", geocoordinate_info->longitude);
	if (ret < 0) {
		LOGD_GEOFENCE("ERROR: String will be truncated");
		return FENCE_ERR_STRING_TRUNCATED;
	}

	ret = sqlite3_bind_text(state, ++index, data_name_lon, -1, SQLITE_STATIC);
	/*ret = sqlite3_bind_double (state, ++index, geocoordinate_info->longitude);*/
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = snprintf(data_name_rad, MAX_DATA_NAME, "%lf", geocoordinate_info->radius);
	if (ret < 0) {
		LOGD_GEOFENCE("ERROR: String will be truncated");
		return FENCE_ERR_STRING_TRUNCATED;
	}

	ret = sqlite3_bind_text(state, ++index, data_name_rad, -1, SQLITE_STATIC);
	/*ret = sqlite3_bind_double (state, ++index, geocoordinate_info->radius);*/
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_bind_text(state, ++index, geocoordinate_info->address, -1, SQLITE_STATIC);
	SQLITE3_RETURN(ret, sqlite3_errmsg(db_info_s.handle), state);

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_reset(state);
	sqlite3_clear_bindings(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function get geocoordinate info from DB.
 *
 * @param[in]	fence_id
 * @param[out]	struct of geocoordinate_info_s
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_geocoordinate_info(int fence_id, geocoordinate_info_s **geocoordinate_info)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	int index = 0;
	char *data_name = NULL;
	char *query = sqlite3_mprintf("SELECT * FROM FenceGeocoordinate where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*geocoordinate_info = (geocoordinate_info_s *)g_malloc0(sizeof(geocoordinate_info_s));
	g_return_val_if_fail(*geocoordinate_info, FENCE_ERR_INVALID_PARAMETER);

#ifdef SUPPORT_ENCRYPTION
	if (password == NULL)
		__geofence_manager_generate_password(password);
#endif

	data_name = (char *) sqlite3_column_text(state, ++index);

	if (!data_name || !strlen(data_name)) {
		LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
	} else {
		(*geocoordinate_info)->latitude = atof(data_name);
	}

	data_name = (char *) sqlite3_column_text(state, ++index);
	if (!data_name || !strlen(data_name)) {
		LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
	} else {
		(*geocoordinate_info)->longitude = atof(data_name);
	}

	data_name = (char *) sqlite3_column_text(state, ++index);
	if (!data_name || !strlen(data_name)) {
		LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
	} else {
		(*geocoordinate_info)->radius = atof(data_name);
	}

	g_strlcpy((*geocoordinate_info)->address, (char *) sqlite3_column_text(state, ++index), ADDRESS_LEN);

	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function get ap list  from DB.
 *
 * @param[in]	fence_id
 * @param[out]	ap_list
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_ap_info(const int fence_id, GList **ap_list)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	int count = -1;
	int i = 0;
	wifi_info_s *wifi_info = NULL;
	const char *bssid = NULL;

	char *query1 = sqlite3_mprintf("SELECT COUNT(bssid) FROM FenceGeopointWifi where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query1, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query1);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGD_GEOFENCE("Fail to get count sqlite3_step");
		sqlite3_finalize(state);
		sqlite3_free(query1);
		return FENCE_ERR_SQLITE_FAIL;
	}

	count = sqlite3_column_int(state, 0);
	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query1);
	if (count <= 0) {
		LOGI_GEOFENCE("ERROR: count = %d", count);
		return FENCE_ERR_COUNT;
	} else {
		LOGD_GEOFENCE("count[%d]", count);
	}

	char *query2 = sqlite3_mprintf("SELECT * FROM FenceGeopointWifi where fence_id = %d;", fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query2, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query2);
		return FENCE_ERR_PREPARE;
	}

	for (i = 0; i < count; i++) {
		ret = sqlite3_step(state);
		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
			break;
		}
		wifi_info = g_slice_new0(wifi_info_s);
		g_return_val_if_fail(wifi_info, -1);
		if (wifi_info) {
			bssid = (const char *) sqlite3_column_text(state, 1);
			g_strlcpy(wifi_info->bssid, bssid, WLAN_BSSID_LEN);
			*ap_list = g_list_append(*ap_list, (gpointer) wifi_info);
		}
	}

	sqlite3_finalize(state);
	sqlite3_free(query2);
	return FENCE_ERR_NONE;
}

/*This function get place info from DB.
 *
 * @param[in]	place_id
 * @param[out]	struct of place_info_s
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_place_info(int place_id, place_info_s **place_info)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(place_id, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	int index = 0;
	char *data_name = NULL;
	char *query = sqlite3_mprintf("SELECT * FROM Places where place_id = %d;", place_id);

	LOGD_GEOFENCE("current place id is [%d]", place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}
	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}
	*place_info = (place_info_s *)g_malloc0(sizeof(place_info_s));
	g_return_val_if_fail(*place_info, FENCE_ERR_INVALID_PARAMETER);

	data_name = (char *)sqlite3_column_text(state, ++index);
	if (!data_name || !strlen(data_name)) {
		LOGI_GEOFENCE("ERROR: data_name is NULL!!!");
	} else {
		(*place_info)->access_type = atof(data_name);
	}

	g_strlcpy((*place_info)->place_name, (char *)sqlite3_column_text(state,	++index), PLACE_NAME_LEN);
	g_strlcpy((*place_info)->appid, (char *)sqlite3_column_text(state, ++index), APP_ID_LEN);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function insert ap list  in DB.
 *
 * @param[in]	fence_id
 * @param[out]	ap_list
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_ap_info(int fence_id, GList *ap_list)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(ap_list, FENCE_ERR_INVALID_PARAMETER);
	int ret = FENCE_ERR_NONE;
	int count = -1;

	ret = __geofence_manager_db_get_count_of_fence_id(fence_id, FENCE_GEOPOINT_WIFI_TABLE, &count);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to get geofence_manager_db_get_count_of_fence_id [%d]", ret);
		return ret;
	} else {
		if (count) {	/* fence id has been in FenceCurrentLocation table */
			LOGI_GEOFENCE("count is [%d]", count);
			return FENCE_ERR_FENCE_ID;
		}
	}

	g_list_foreach(ap_list, (GFunc) __geofence_manager_db_insert_wifi_data_info, &fence_id);

	return FENCE_ERR_NONE;
}

/**
 * This function get bluetooth info from DB.
 *
 * @param[in]	fence_id
 * @param[out]	bt_info which contained bssid of bluetooth and correspond of fence_id.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_bssid_info(const int fence_id, bssid_info_s **bssid_info)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	int count = -1;
	int i = 0;
	bssid_info_s *bssid_info_from_db = NULL;
	const char *bssid = NULL;
	const char *ssid = NULL;

	char *query1 = sqlite3_mprintf("SELECT COUNT(bssid) FROM %s where fence_id = %d;", menu_table[FENCE_BSSID_TABLE], fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query1, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query1);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGD_GEOFENCE("Fail to get count sqlite3_step");
		sqlite3_finalize(state);
		sqlite3_free(query1);
		return FENCE_ERR_SQLITE_FAIL;
	}

	count = sqlite3_column_int(state, 0);
	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query1);
	if (count <= 0) {
		LOGI_GEOFENCE("ERROR: count = %d", count);
		return FENCE_ERR_COUNT;
	} else {
		LOGD_GEOFENCE("count[%d]", count);
	}

	char *query2 = sqlite3_mprintf("SELECT * FROM %s where fence_id = %d;", menu_table[FENCE_BSSID_TABLE], fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query2, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query2);
		return FENCE_ERR_PREPARE;
	}

	/*'count' should be 1. because bluetooth bssid and fence_id matched one by one.*/
	for (i = 0; i < count; i++) {
		ret = sqlite3_step(state);
		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
			break;
		}
		bssid_info_from_db = g_slice_new0(bssid_info_s);
		g_return_val_if_fail(bssid_info_from_db, -1);
		if (bssid_info_from_db) {
			bssid = (const char *)sqlite3_column_text(state, 1);
			ssid = (const char *)sqlite3_column_text(state, 2);
			g_strlcpy(bssid_info_from_db->bssid, bssid, WLAN_BSSID_LEN);
			g_strlcpy(bssid_info_from_db->ssid, ssid, WLAN_BSSID_LEN);
			*bssid_info = bssid_info_from_db;
		}
	}

	sqlite3_finalize(state);
	sqlite3_free(query2);
	return FENCE_ERR_NONE;
}

int geofence_manager_update_bssid_info(const int fence_id, bssid_info_s *bssid_info)
{
	FUNC_ENTRANCE_SERVER
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(bssid_info, FENCE_ERR_INVALID_PARAMETER);
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail;
	char *query = sqlite3_mprintf("UPDATE %Q SET bssid = %Q where fence_id = %d;", menu_table[FENCE_BSSID_TABLE], bssid_info->bssid, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	LOGI_GEOFENCE("Fence_id: %d has been successfully updated.", fence_id);
	return FENCE_ERR_NONE;
}

/**
 * This function insert bssid information in DB.
 *
 * @param[in]	fence_id
 * @param[in]	bssid_info which contained bssid of wifi or bluetooth for geofence.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_bssid_info(int fence_id, bssid_info_s *bssid_info)
{
	FUNC_ENTRANCE_SERVER
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(bssid_info, FENCE_ERR_INVALID_PARAMETER);
	int ret = FENCE_ERR_NONE;
	int count = -1;

	ret = __geofence_manager_db_get_count_of_fence_id(fence_id, FENCE_BSSID_TABLE, &count);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to get geofence_manager_db_get_count_of_fence_id [%d]", ret);
		return ret;
	} else {
		if (count) {	/* fence id has been in FenceBssid table */
			LOGI_GEOFENCE("count is [%d]", count);
			return FENCE_ERR_FENCE_ID;
		}
	}

	ret = __geofence_manager_db_insert_bssid_info(fence_id, bssid_info->bssid, bssid_info->ssid);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("Fail to insert the bssid info");
		return ret;
	}
	return FENCE_ERR_NONE;
}

/**
 * This function get enable status  from DB.
 *
 * @param[in]	fence_id
 * @param[in]	status: 1 enbale, 0 disable.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_enable_status(const int fence_id, int *status)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT enable FROM GeoFence where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*status = sqlite3_column_int(state, 0);

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function set  enable  on DB.
 *
 * @param[in]	fence_id
 * @param[in]	status: 1 enbale, 0 disable.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_enable_status(int fence_id, int status)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE GeoFence SET enable = %d where fence_id = %d;", status, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get name from DB.
 *
 * @param[in]	fence_id
 * @param[out]	name
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_place_name(int place_id, char **name)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *tmp = NULL;

	char *query = sqlite3_mprintf("SELECT place_name FROM Places where place_id = %d;", place_id);

	LOGD_GEOFENCE("current place id is [%d]", place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	tmp = (char *) sqlite3_column_text(state, 0);
	if (!tmp || !strlen(tmp)) {
		LOGI_GEOFENCE("ERROR: name is NULL!!!");
	} else {
		*name = g_strdup(tmp);
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function set name on DB.
 *
 * @param[in]	fence_id
 * @param[in]	name
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_place_name(int place_id, const char *name)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE Places SET place_name = %Q where place_id = %d;", name, place_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get appid from DB.
 *
 * @param[in]  place_id
 * @param[in]  appid
 * @return     FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_appid_from_places(int place_id, char **appid)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *id = NULL;

	char *query = sqlite3_mprintf("SELECT app_id FROM Places where place_id = %d;", place_id);

	LOGD_GEOFENCE("current place id is [%d]", place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	id = (char *) sqlite3_column_text(state, 0);
	if (!id || !strlen(id)) {
		LOGI_GEOFENCE("ERROR: appid is NULL!!!");
	} else {
		*appid = g_strdup(id);
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function set appid on DB.
 *
 * @param[in]   place_id
 * @param[in]   appid.
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_appid_to_places(int place_id, char *appid)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE Places SET app_id = %Q where place_id = %d;", appid, place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get appid from DB.
 *
 * @param[in]	fence_id
 * @param[in]	appid
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_appid_from_geofence(int fence_id, char **appid)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *id = NULL;

	char *query = sqlite3_mprintf("SELECT app_id FROM GeoFence where fence_id = %d;", fence_id);
	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	id = (char *) sqlite3_column_text(state, 0);
	if (!id || !strlen(id)) {
		LOGI_GEOFENCE("ERROR: appid is NULL!!!");
	} else {
		*appid = g_strdup(id);
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function set appid on DB.
 *
 * @param[in]	fence_id
 * @param[in]	appid.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_appid_to_geofence(int fence_id, char *appid)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE GeoFence SET app_id = %Q where fence_id = %d;", appid, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get geofence type from DB.
 *
 * @param[in]	fence_id
 * @param[in]	geofence_type_e.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_geofence_type(int fence_id, geofence_type_e *fence_type)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT geofence_type FROM GeoFence where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*fence_type = sqlite3_column_int(state, 0);

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

int geofence_manager_get_place_id(int fence_id, int *place_id)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT place_id FROM GeoFence where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*place_id = sqlite3_column_int(state, 0);

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function get geofence/place access type from DB.
 *
 * @param[in]	fence_id/place_id
 * @param[in]	access_type_e.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_access_type(int fence_id, int place_id, access_type_e *fence_type)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;

	if (place_id == -1)
		query = sqlite3_mprintf("SELECT access_type FROM GeoFence WHERE fence_id = %d;", fence_id);
	else if (fence_id == -1)
		query = sqlite3_mprintf("SELECT access_type FROM Places WHERE place_id = %d", place_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	LOGD_GEOFENCE("current place id is [%d]", place_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*fence_type = sqlite3_column_int(state, 0);

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function set geofence type on DB.
 *
 * @param[in]	fence_id
 * @param[in]	fence_type.
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_geofence_type(int fence_id, geofence_type_e fence_type)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE GeoFence SET geofence_type = %d where fence_id = %d;", fence_type, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get geofence place_id from DB.
 *
 * @param[in]	fence_id
 * @param[in]	place_id
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_placeid_from_geofence(int fence_id, int *place_id)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT place_id FROM GeoFence where fence_id = %d;", fence_id);
	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*place_id = sqlite3_column_int(state, 0);

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function get running status from DB.
 *
 * @param[in]	fence_id
 * @param[in]	int
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_running_status(int fence_id, int *running_status)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT running_status FROM GeoFence where fence_id = %d;", fence_id);

	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*running_status = sqlite3_column_int(state, 0);

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function set running state on DB.
 *
 * @param[in]	fence_id
 * @param[in]	state
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_running_status(int fence_id, int running_status)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE GeoFence SET running_status = %d where fence_id = %d;", running_status, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/**
 * This function get direction type from DB.
 *
 * @param[in]	fence_id
 * @param[in]	direction
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_get_direction(int fence_id, geofence_direction_e *direction)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;

	char *query = sqlite3_mprintf("SELECT direction FROM GeoFence where fence_id = %d;", fence_id);
	LOGD_GEOFENCE("current fence id is [%d]", fence_id);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*direction = sqlite3_column_int(state, 0);

	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function set direction type on DB.
 *
 * @param[in]	fence_id
 * @param[in]	direction
 * @return	FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_set_direction(int fence_id, geofence_direction_e direction)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state;
	int ret = SQLITE_OK;
	const char *tail;

	char *query = sqlite3_mprintf("UPDATE GeoFence SET direction = %d where fence_id = %d;", direction, fence_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_DONE) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_finalize(state);
	sqlite3_free(query);

	return FENCE_ERR_NONE;
}

/**
 * This function remove fence from DB.
 *
 * @param[in]    fence_id
 * @return       FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_delete_fence_info(int fence_id)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id, FENCE_ERR_INVALID_PARAMETER);
	int ret = FENCE_ERR_NONE;
	geofence_type_e fence_type = GEOFENCE_INVALID;

	ret = geofence_manager_get_geofence_type(fence_id, &fence_type);
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_delete_fence_point_info");
		return ret;
	}

	ret = __geofence_manager_db_enable_foreign_keys();
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_db_enable_foreign_keys");
		return ret;
	}

	ret = __geofence_manager_delete_table(fence_id, FENCE_MAIN_TABLE);
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_delete_fence_point_info");
		return ret;
	}

	return ret;
}

/**
 * This function remove place from DB.
 *
 * @param[in]      place_id
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_delete_place_info(int place_id)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(place_id, FENCE_ERR_INVALID_PARAMETER);
	int ret = FENCE_ERR_NONE;

	ret = __geofence_manager_db_enable_foreign_keys();
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_db_enable_foreign_keys");
		return ret;
	}

	ret = __geofence_manager_delete_place_table(place_id);
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_delete_place_info");
		return ret;
	}

	return ret;
}

/**
 * This function close  DB handle.
 *
 * @param[in]    fence_id
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_close_db(void)
{
	FUNC_ENTRANCE_SERVER;
	int ret = SQLITE_OK;

	if (db_info_s.handle == NULL) {
		return FENCE_ERR_NONE;
	}

	ret = db_util_close(db_info_s.handle);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Close DB ERROR!!!");
		return FENCE_ERR_SQLITE_FAIL;
	}

	return FENCE_ERR_NONE;
}

/**
 * This function deletes all data on db.
 *
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_reset(void)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;

	ret = __geofence_manager_db_enable_foreign_keys();
	if (FENCE_ERR_NONE != ret) {
		LOGI_GEOFENCE("Fail to geofence_manager_db_enable_foreign_keys");
		return ret;
	}

	char *query_two = sqlite3_mprintf("DELETE from %Q;", menu_table[FENCE_MAIN_TABLE]);

	ret = sqlite3_prepare_v2(db_info_s.handle, query_two, -1, &state, NULL);
	if (SQLITE_OK != ret) {
		LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query_two);
		return FENCE_ERR_SQLITE_FAIL;
	}

	ret = sqlite3_step(state);
	if (SQLITE_DONE != ret) {
		LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
		sqlite3_finalize(state);
		sqlite3_free(query_two);
		return FENCE_ERR_SQLITE_FAIL;
	}
	sqlite3_reset(state);
	sqlite3_free(query_two);

	char *query_three = sqlite3_mprintf("UPDATE sqlite_sequence SET seq = 0 where name = %Q;", menu_table[FENCE_MAIN_TABLE]);

	ret = sqlite3_prepare_v2(db_info_s.handle, query_three, -1, &state, NULL);
	if (SQLITE_OK != ret) {
		LOGI_GEOFENCE("Fail to connect to table. Error[%s]", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query_three);
		return FENCE_ERR_SQLITE_FAIL;
	}

	ret = sqlite3_step(state);
	if (SQLITE_DONE != ret) {
		LOGI_GEOFENCE("Fail to step. Error[%d]", ret);
		sqlite3_finalize(state);
		sqlite3_free(query_three);
		return FENCE_ERR_SQLITE_FAIL;
	}

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query_three);
	return FENCE_ERR_NONE;
}

/**
 * This function copy source wifi info to dest wifi info.
 *
 * @param[in]    src_wifi
 * @param[out]  dest_wifi
 * @return         FENCE_ERR_NONE on success, negative values for errors
 */
int geofence_manager_copy_wifi_info(wifi_info_s *src_wifi, wifi_info_s **dest_wifi)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(src_wifi, FENCE_ERR_INVALID_PARAMETER);

	*dest_wifi = (wifi_info_s *)g_malloc0(sizeof(wifi_info_s));
	g_return_val_if_fail(*dest_wifi, -1);

	g_strlcpy((*dest_wifi)->bssid, src_wifi->bssid, WLAN_BSSID_LEN);

	return FENCE_ERR_NONE;
}

/**
* This function create a wifi infor .
*
* @param[in]	fence_id
* @param[in]	bssid
* @param[out]	wifi info
* @return	FENCE_ERR_NONE on success, negative values for errors
*/
int geofence_manager_create_wifi_info(int fence_id, char *bssid, wifi_info_s **new_wifi)
{
	FUNC_ENTRANCE_SERVER;
	g_return_val_if_fail(fence_id >= 0, FENCE_ERR_INVALID_PARAMETER);
	g_return_val_if_fail(bssid, FENCE_ERR_INVALID_PARAMETER);

	*new_wifi = (wifi_info_s *)g_malloc0(sizeof(wifi_info_s));
	g_strlcpy((*new_wifi)->bssid, bssid, WLAN_BSSID_LEN);

	return FENCE_ERR_NONE;
}

/**
* This function get fence id count  by params such as app id and fence type and enable status .
*
* @param[in]    app_id : if app_id == NULL: ALL
* @param[in]    fence_type:if GEOFENCE_TYPE_INVALID == NULL: ALL fence type
* @param[in]    enable_status
* @param[out]  fence id count
* @return	    FENCE_ERR_NONE on success, negative values for errors
*/
int geofence_manager_get_count_by_params(const char *app_id, geofence_type_e fence_type, int *count)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;

	if (NULL == app_id) {
		if (GEOFENCE_INVALID != fence_type) {	/* app_id == NULL : All  and  GEOFENCE_TYPE_INVALID != fence_type */
			query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM GeoFence where geofence_type = %d ;", fence_type);
		} else {
			query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM GeoFence ;");
		}
	} else {			/*app_id not NULL */
		if (GEOFENCE_INVALID != fence_type) {	/* app_id not NULL   and  GEOFENCE_TYPE_INVALID != fence_type */
			query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM GeoFence where app_id = %Q AND geofence_type = %d ;", app_id, fence_type);
		} else {
			query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM GeoFence where app_id = %Q ;", app_id);
		}
	}

	LOGI_GEOFENCE("app_id[%s] fence_type[%d] ", app_id, fence_type);
	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*count = sqlite3_column_int(state, 0);

	if (*count <= 0) {
		LOGI_GEOFENCE("ERROR: count = %d", *count);
		return FENCE_ERR_COUNT;
	} else {
		LOGI_GEOFENCE("count[%d]", *count);
	}

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

/*
	app_id == NULL : All, geofence_type_e : INVALID - all, IN enable_status : enable, disable or both. Output : a list of geofence_id
*/
int geofence_manager_get_fences(const char *app_id, geofence_type_e fence_type, GList **fences)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = NULL;
	int i = 0;
	int fence_id = 0;
	int count = -1;

	ret = geofence_manager_get_count_by_params(app_id, fence_type, &count);
	if (ret != FENCE_ERR_NONE) {
		LOGI_GEOFENCE("ERROR: geofence_manager_get_count_of_fences_by_app.");
		return ret;
	}

	if (NULL == app_id) {
		if (GEOFENCE_INVALID != fence_type) {	/* app_id == NULL : All  and  GEOFENCE_TYPE_INVALID != fence_type */
			query = sqlite3_mprintf("SELECT fence_id FROM GeoFence where geofence_type = %d;", fence_type);
		} else {
			query = sqlite3_mprintf("SELECT fence_id FROM GeoFence;");
		}
	} else {			/*app_id not NULL */
		if (GEOFENCE_INVALID != fence_type) {	/* app_id not NULL   and  GEOFENCE_TYPE_INVALID != fence_type */
			query = sqlite3_mprintf("SELECT fence_id FROM GeoFence where app_id = %Q AND geofence_type = %d ;", app_id, fence_type);
		} else {
			query = sqlite3_mprintf("SELECT fence_id FROM GeoFence where app_id = %Q;", app_id);
		}
	}

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	for (i = 0; i < count; i++) {
		ret = sqlite3_step(state);
		if (ret != SQLITE_ROW) {
			LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
			break;
		}
		fence_id = sqlite3_column_int(state, 0);
		LOGI_GEOFENCE("fence id is [%d]", fence_id);
		*fences = g_list_append(*fences, (gpointer) GINT_TO_POINTER(fence_id));
	}

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

int geofence_manager_get_count_of_fences(int *count)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = sqlite3_mprintf("SELECT COUNT(fence_id) FROM GeoFence;");

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*count = sqlite3_column_int(state, 0);

	if (*count < 0) {
		LOGI_GEOFENCE("ERROR: count = %d", *count);
		return FENCE_ERR_COUNT;
	} else {
		LOGI_GEOFENCE("count[%d]", *count);
	}

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}

int geofence_manager_get_place_count_by_placeid(int place_id, int *count)
{
	FUNC_ENTRANCE_SERVER;
	sqlite3_stmt *state = NULL;
	int ret = SQLITE_OK;
	const char *tail = NULL;
	char *query = sqlite3_mprintf("SELECT COUNT(place_id) FROM Places WHERE place_id=%d;", place_id);

	ret = sqlite3_prepare_v2(db_info_s.handle, query, -1, &state, &tail);
	if (ret != SQLITE_OK) {
		LOGI_GEOFENCE("Error: %s", sqlite3_errmsg(db_info_s.handle));
		sqlite3_free(query);
		return FENCE_ERR_PREPARE;
	}

	ret = sqlite3_step(state);
	if (ret != SQLITE_ROW) {
		LOGI_GEOFENCE("sqlite3_step Error[%d] : %s", ret, sqlite3_errmsg(db_info_s.handle));
		sqlite3_finalize(state);
		sqlite3_free(query);
		return FENCE_ERR_SQLITE_FAIL;
	}

	*count = sqlite3_column_int(state, 0);

	if (*count < 0) {
		LOGI_GEOFENCE("ERROR: place count = %d", *count);
		return FENCE_ERR_COUNT;
	} else {
		LOGI_GEOFENCE("place count[%d]", *count);
	}

	sqlite3_reset(state);
	sqlite3_finalize(state);
	sqlite3_free(query);
	return FENCE_ERR_NONE;
}
