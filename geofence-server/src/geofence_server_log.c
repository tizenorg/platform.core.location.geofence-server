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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <vconf.h>
#include "debug_util.h"

#define __GEOFENCE_LOG_FILE__  "/opt/usr/media/geofence_log.txt"

int fd = -1;

void __get_current_time(struct tm *cur_time)
{
	time_t now;
	time(&now);
	localtime_r(&now, cur_time);
}

void _init_log()
{
	struct tm cur_time;
	char buf[256] = { 0, };

	__get_current_time(&cur_time);
	snprintf(buf, 256, "[%02d:%02d:%02d] -- START -- \n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
	LOGI_GEOFENCE("BUF[%s]", buf);
}

void _deinit_log()
{
	if (fd < 0)
		return;
	struct tm cur_time;
	char buf[256] = { 0, };

	__get_current_time(&cur_time);
	snprintf(buf, 256, "[%02d:%02d:%02d] -- END -- \n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec);
	LOGI_GEOFENCE("BUF[%s]", buf);

	close(fd);
	fd = -1;
}

void _print_log(const char *str)
{
	if (fd < 0)
		return;
	char buf[256] = { 0, };
	struct tm cur_time;
	__get_current_time(&cur_time);

	snprintf(buf, 256, "[%02d:%02d:%02d] %s\n", cur_time.tm_hour, cur_time.tm_min, cur_time.tm_sec, str);
	LOGI_GEOFENCE("BUF %s", buf);
}

