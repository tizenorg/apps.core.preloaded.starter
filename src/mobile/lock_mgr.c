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

#include <vconf.h>
#include <vconf-keys.h>

#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <feedback.h>
#include <time.h>
#include <dd-deviced.h>
#include <dd-display.h>
#include <aul.h>
#include <system_settings.h>
#include <system_info.h>
#include <telephony.h>

#include "lock_mgr.h"
#include "package_mgr.h"
#include "process_mgr.h"
#include "hw_key.h"
#include "dbus_util.h"
#include "util.h"
#include "status.h"

#define SYSTEM_INFO_KEY_NETWORK_TELEPHONY "http://tizen.org/feature/network.telephony"



static struct {
	int checkfd;
	int old_lock_type;
	int lock_pid;
	int lcd_state;
	Eina_Bool is_support_telephony;
} s_lock_mgr = {
	.checkfd = 0,
	.old_lock_type = 0,
	.lock_pid = -1,
	.lcd_state = -1,
	.is_support_telephony = EINA_FALSE,
};

static telephony_handle_list_s tel_handle_list;



int lock_mgr_lcd_state_get(void)
{
	return s_lock_mgr.lcd_state;
}



int lock_mgr_get_lock_pid(void)
{
	return s_lock_mgr.lock_pid;
}



static bool _check_support_telephony(void)
{
	int ret = 0;
	bool is_support= EINA_FALSE;

	ret = system_info_get_platform_bool(SYSTEM_INFO_KEY_NETWORK_TELEPHONY, &is_support);
	if (ret != SYSTEM_INFO_ERROR_NONE) {
		_E("Failed to get system information : %s", SYSTEM_INFO_KEY_NETWORK_TELEPHONY);
		return false;
	}

	return is_support;
}



static void _init_telephony(void)
{
	int ret = 0;

	ret = telephony_init(&tel_handle_list);
	if (ret != TELEPHONY_ERROR_NONE) {
		_E("Failed to initialize telephony");
	}
}



static void _deinit_telephony(void)
{
	int ret = 0;

	ret = telephony_deinit(&tel_handle_list);
	if (ret != TELEPHONY_ERROR_NONE) {
		_E("Failed to deinitialize telephony");
	}
}



static Eina_Bool _check_call_idle_status(void)
{
	telephony_call_h *call_list = NULL;
	unsigned int call_count = 0;
	int ret = 0;
	int i = 0, j = 0;

	for (i = 0; i < tel_handle_list.count; i++) {
		ret = telephony_call_get_call_list(tel_handle_list.handle[i], &call_count, &call_list);
		if (ret != TELEPHONY_ERROR_NONE) {
			_E("Failed to get telephony call list");
			continue;
		}

		if (call_count == 0) {
			_E("[%d] No calls available", i);
		} else {
			for (j = 0; j < call_count; j++) {
				telephony_call_status_e status;

				ret = telephony_call_get_status(call_list[i], &status);
				if (ret != TELEPHONY_ERROR_NONE) {
					_E("Failed to get call status(%d)", ret);
					continue;
				}

				if (status != TELEPHONY_CALL_STATUS_IDLE) {
					_D("[%d] call status : %d", i, status);
					telephony_call_release_call_list(call_count, &call_list);
					return EINA_FALSE;
				}
			}
		}

		telephony_call_release_call_list(call_count, &call_list);
	}

	return EINA_TRUE;
}



void lock_mgr_sound_play(lock_sound_type_e type)
{
	int val = status_passive_get()->setappl_sound_lock_bool;
	ret_if(!val);

	switch(type) {
	case LOCK_SOUND_LOCK:
		feedback_play_type(FEEDBACK_TYPE_SOUND, FEEDBACK_PATTERN_LOCK);
		break;
	case LOCK_SOUND_UNLOCK:
		feedback_play_type(FEEDBACK_TYPE_SOUND, FEEDBACK_PATTERN_UNLOCK);
		break;
	case LOCK_SOUND_BTN_KEY:
		feedback_play_type(FEEDBACK_TYPE_SOUND, FEEDBACK_PATTERN_SIP);
		break;
	case LOCK_SOUND_TAP:
		feedback_play_type(FEEDBACK_TYPE_SOUND, FEEDBACK_PATTERN_TAP);
		break;
	default:
		break;
	}
}



void lock_mgr_idle_lock_state_set(int lock_state)
{
	_D("lock state : %d", lock_state);

	int ret = 0;

	ret = vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, lock_state);
	if (ret < 0) {
		_E("Failed to set vconfkey : VCONFKEY_IDLE_LOCK_STATE");
	}

	ret = vconf_set_int(VCONFKEY_IDLE_LOCK_STATE_READ_ONLY, lock_state);
	if (ret < 0) {
		_E("Failed to set vconfkey : VCONFKEY_IDLE_LOCK_STATE_READ_ONLY");
	}
}



static void _after_launch_lock(int pid)
{
	int idle_lock_state = 0;

	if (dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_DEFAULT) < 0) {
		_E("cannot send oomadj for pid[%d]", pid);
	}
	process_mgr_set_lock_priority(pid);
	display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
	s_lock_mgr.lock_pid = pid;

	idle_lock_state = status_passive_get()->idle_lock_state;
	if (idle_lock_state == VCONFKEY_IDLE_UNLOCK) {
		lock_mgr_idle_lock_state_set(VCONFKEY_IDLE_LOCK);
		lock_mgr_sound_play(LOCK_SOUND_LOCK);
	}
}



static int _lock_changed_cb(const char *appid, const char *key, const char *value, void *cfn, void *afn)
{
	_D("%s", __func__);

	return 0;
}



static void _other_lockscreen_unlock(void)
{
	_D("unlock other lock screen");
}



void lock_mgr_unlock(void)
{
	int lock_type = status_active_get()->setappl_screen_lock_type_int;
	_D("lock type(%d)", lock_type);

	lock_mgr_idle_lock_state_set(VCONFKEY_IDLE_UNLOCK);
	lock_mgr_sound_play(LOCK_SOUND_UNLOCK);
	s_lock_mgr.lock_pid = 0;

	if (lock_type == SETTING_SCREEN_LOCK_TYPE_OTHER) {
		_other_lockscreen_unlock();
	}
}



static void _on_lcd_changed_receive(void *data, DBusMessage *msg)
{
	int lcd_on = 0;
	int lcd_off = 0;

	_D("LCD signal is received");

	lcd_on = dbus_message_is_signal(msg, DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_ON);
	lcd_off = dbus_message_is_signal(msg, DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_OFF);

	if (lcd_on) {
		_W("LCD on");
		s_lock_mgr.lcd_state = LCD_STATE_ON;
	} else if (lcd_off) {
		int idle_lock_state = 0;
		Eina_Bool is_call_idle = EINA_TRUE;
		Eina_Bool ret = EINA_FALSE;
		s_lock_mgr.lcd_state = LCD_STATE_OFF;
		idle_lock_state = status_passive_get()->idle_lock_state;
		_D("idle_lock_state(%d)", idle_lock_state);

		is_call_idle = _check_call_idle_status();
		if (is_call_idle == EINA_FALSE) {
			char *lcd_off_source = NULL;

			/*
			 * lcd off source
			 * string : 'powerkey' or 'timeout' or 'event' or 'unknown'
			 */
			lcd_off_source = dbus_util_msg_arg_get_str(msg);
			_I("lcd off source : %s", lcd_off_source);

			if (lcd_off_source) {
				/* If LCD turn off by means other than 'powerkey' when call is running,
				 * do not launch lockscreen.
				 */
				if (strncmp(lcd_off_source, "powerkey", strlen(lcd_off_source))) {
					_E("Do not launch lockscreen");
					free(lcd_off_source);
					return;
				}

				free(lcd_off_source);
			}
		}

		if (idle_lock_state == VCONFKEY_IDLE_UNLOCK) {
			ret = lock_mgr_lockscreen_launch();
			if (ret != EINA_TRUE) {
				_E("Failed to launch lockscreen");
			}
		}
	} else {
		_E("%s dbus_message_is_signal error", DEVICED_INTERFACE_DISPLAY);
	}
}



Eina_Bool lock_mgr_lockscreen_launch(void)
{
	const char *lock_appid = NULL;
	int lock_type = 0;

	lock_type = status_active_get()->setappl_screen_lock_type_int;
	_D("lock type : %d", lock_type);

	//PM LOCK - don't go to sleep
	display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);

	lock_appid = status_passive_get()->setappl_3rd_lock_pkg_name_str;
	if (!lock_appid) {
		_E("set default lockscreen");
		lock_appid = STATUS_DEFAULT_LOCK_PKG_NAME;
	}

	_I("lockscreen appid : %s", lock_appid);

	switch (lock_type) {
	case SETTING_SCREEN_LOCK_TYPE_NONE:
		_D("Lockscreen type is NONE");
		return EINA_TRUE;
	case SETTING_SCREEN_LOCK_TYPE_SWIPE:
	case SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD:
	case SETTING_SCREEN_LOCK_TYPE_PASSWORD:
	case SETTING_SCREEN_LOCK_TYPE_OTHER:
		process_mgr_must_launch(lock_appid, NULL, NULL, _lock_changed_cb, _after_launch_lock);
		goto_if(s_lock_mgr.lock_pid < 0, ERROR);
		break;
	default:
		_E("type error(%d)", lock_type);
		goto ERROR;
	}

	_I("lock pid : %d", s_lock_mgr.lock_pid);

	return EINA_TRUE;

ERROR:
	_E("Failed to launch lockscreen");
	display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
	return EINA_FALSE;
}



static void _lock_daemon_init(void)
{
	_SECURE_I("lockscreen : %s", status_passive_get()->setappl_3rd_lock_pkg_name_str);

	/* register lcd changed cb */
	dbus_util_receive_lcd_status(_on_lcd_changed_receive, NULL);
}



static int _lock_type_changed_cb(status_active_key_e key, void *data)
{
	int lock_type = status_active_get()->setappl_screen_lock_type_int;
	retv_if(lock_type == s_lock_mgr.old_lock_type, 1);

	_D("lock type is changed : %d -> %d", s_lock_mgr.old_lock_type, lock_type);

	s_lock_mgr.old_lock_type = lock_type;

	return 1;
}



int lock_mgr_daemon_start(void)
{
	int lock_type = 0;
	int ret = 0;

	_lock_daemon_init();

	lock_type = status_active_get()->setappl_screen_lock_type_int;
	_D("lock type : %d", lock_type);

	status_active_register_cb(STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT, _lock_type_changed_cb, NULL);

	s_lock_mgr.is_support_telephony = _check_support_telephony();
	if (s_lock_mgr.is_support_telephony == true) {
		_init_telephony();
	}

	ret = lock_mgr_lockscreen_launch();
	if (ret != EINA_TRUE) {
		_E("Failed to launch lockscreen");
	}

	if (feedback_initialize() != FEEDBACK_ERROR_NONE) {
		_E("Failed to initialize feedback");
	}

	return ret;
}



void lock_mgr_daemon_end(void)
{
	status_active_unregister_cb(STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT, _lock_type_changed_cb);

	if (s_lock_mgr.is_support_telephony == EINA_TRUE) {
		_deinit_telephony();
	}

	feedback_deinitialize();
}
