/*
 * Copyright (c) 2000 - 2015 Samsung Electronics Co., Ltd. All rights reserved.
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

#include <Elementary.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <vconf.h>
#include <signal.h>
#include <app.h>
#include <alarm.h>
#include <feedback.h>

#include "util.h"



static struct {
	alarm_id_t alarm_id;	/* -1 : None, others : set alarm */
} s_hourly_alert = {
	.alarm_id = -1,
};



static int _alarm_delete_cb(alarm_id_t id, void *user_param)
{
	int ret = ALARMMGR_RESULT_SUCCESS;

	ret = alarmmgr_remove_alarm(id);
	if(ret != ALARMMGR_RESULT_SUCCESS) {
		_E("alarmmgr_enum_alarm_ids() failed");
	}

	return 0;
}



static void _alarm_unset(void *data)
{
	int ret = ALARMMGR_RESULT_SUCCESS;

	if (s_hourly_alert.alarm_id != -1) {
		_D("try to delete alarm_id(%d)", s_hourly_alert.alarm_id);
		ret = alarmmgr_remove_alarm(s_hourly_alert.alarm_id);
		if(ret != ALARMMGR_RESULT_SUCCESS) {
			ret = alarmmgr_enum_alarm_ids(_alarm_delete_cb, NULL);
			if(ret != ALARMMGR_RESULT_SUCCESS) {
				_E("alarmmgr_enum_alarm_ids() failed");
			}
		}
		s_hourly_alert.alarm_id = -1;
	}
}



static int _alarm_set(void *data)
{
	int ret = ALARMMGR_RESULT_SUCCESS;
	time_t current_time;
	struct tm current_tm;
	alarm_entry_t *alarm_info = NULL;
	alarm_id_t alarm_id;
	alarm_date_t alarm_time;

	/* delete before registering alarm ids */
	_alarm_unset(NULL);

	time(&current_time);

	/* alarm revision */
	current_time += 3600;	// +1 hour

	localtime_r(&current_time, &current_tm);

	alarm_info = alarmmgr_create_alarm();
	if(alarm_info == NULL) {
		_E("alarmmgr_create_alarm() is failed\n");
		return -1;
	}

	alarm_time.year = current_tm.tm_year;
	alarm_time.month = current_tm.tm_mon;
	alarm_time.day = current_tm.tm_mday;
	alarm_time.hour = current_tm.tm_hour;
	alarm_time.min = 0;
	alarm_time.sec = 0;

	//alarmmgr_set_repeat_mode(alarm_info, ALARM_REPEAT_MODE_ONCE, 0);
	ret = alarmmgr_set_repeat_mode(alarm_info, ALARM_REPEAT_MODE_REPEAT, 60*60);
	if(ret != ALARMMGR_RESULT_SUCCESS) {
		_E("alarmmgr_set_repeat_mode() failed");
		alarmmgr_free_alarm(alarm_info) ;
		return -1;
	}
	alarmmgr_set_time(alarm_info, alarm_time);
	alarmmgr_set_type(alarm_info, ALARM_TYPE_VOLATILE);

	ret = alarmmgr_add_alarm_with_localtime(alarm_info, NULL, &alarm_id);
	if(ret != ALARMMGR_RESULT_SUCCESS) {
		_E("alarmmgr_add_alarm_with_localtime() failed");
		alarmmgr_free_alarm(alarm_info) ;
		return -1;
	}

	s_hourly_alert.alarm_id = alarm_id;
	alarmmgr_free_alarm(alarm_info);

	return 0;
}



static int _alarm_cb(alarm_id_t alarm_id, void *data)
{
	_D("hourly_alert alarm callback called");

	feedback_initialize();
	feedback_play(FEEDBACK_PATTERN_NONE);
	feedback_deinitialize();

	return 0;
}



static int _alarm_init(void *data)
{
	int ret = 0;

	ret = alarmmgr_init("starter");
	retv_if(ret<0, -1);

	ret = alarmmgr_set_cb(_alarm_cb, NULL);
	retv_if(ret<0, -1);

	s_hourly_alert.alarm_id = -1;

	return 0;
}

static void _alarm_fini(void *data)
{
	_alarm_unset(NULL);
	alarmmgr_fini();
}



static Eina_Bool _register_hourly_alert_alarm(void)
{
	int ret = 0;

	ret = _alarm_init(NULL);
	retv_if(ret < 0, EINA_FALSE);

	_alarm_set(NULL);

	return EINA_TRUE;

}



static int _unregister_hourly_alert_alarm(void)
{
	_alarm_fini(NULL);
	return 0;
}



static void _hourly_alert_changed_cb(keynode_t* node, void *data)
{
	int hourly_alert = -1;

	_D("%s, %d", __func__, __LINE__);

	if (node) {
		hourly_alert = vconf_keynode_get_bool(node);
	} else {
		if (vconf_get_int(VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, &hourly_alert) < 0) {
			_E("Failed to get %s", VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL);
			return;
		}
	}

	if (hourly_alert == 1) {
		_E("hourly_alert is set");
		_register_hourly_alert_alarm();
	} else {
		_E("hourly_alert is unset");
		_unregister_hourly_alert_alarm();
	}

}



static void _hourly_system_time_changed_cb(keynode_t *node, void *data)
{
	_alarm_set(NULL);
}



void hourly_alert_init(void)
{
	int hourly_alert = -1;
	int ret = 0;

	ret = vconf_get_bool(VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, &hourly_alert);
	if (ret < 0){
		_E("can't get vconfkey value of [%s], ret=[%d]", VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, ret);
		hourly_alert = 0;
	} else if (hourly_alert == 1) {
		_D("[%s] value is [%d], hourly_alert is set..!!", VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, hourly_alert);
		if (_register_hourly_alert_alarm() == EINA_FALSE) {
			_E("_register_hourly_alert_alarm is failed..!!");
		}
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, _hourly_alert_changed_cb, NULL) < 0) {
		_E("Failed to add the callback for %s changed", VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL);
	}
	/* for time revision */
	if (vconf_notify_key_changed(VCONFKEY_SYSTEM_TIME_CHANGED, _hourly_system_time_changed_cb, NULL) < 0) {
		_E("Failed to add the callback for %s changed", VCONFKEY_SYSTEM_TIME_CHANGED);
	}
}



void hourly_alert_fini(void)
{
	_unregister_hourly_alert_alarm();

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL, _hourly_alert_changed_cb) < 0) {
		_E("Failed to ignore the callback for %s changed", VCONFKEY_SETAPPL_HOURLY_ALERT_BOOL);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SYSTEM_TIME_CHANGED, _hourly_system_time_changed_cb) < 0) {
		_E("Failed to ignore the callback for %s changed", VCONFKEY_SYSTEM_TIME_CHANGED);
	}
}



