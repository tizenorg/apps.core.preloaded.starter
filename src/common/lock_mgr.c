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

#include "lock_mgr.h"
#include "package_mgr.h"
#include "process_mgr.h"
#include "hw_key.h"
#include "dbus_util.h"
#include "util.h"
#include "status.h"

#define PASSWORD_LOCK_PROGRESS "/tmp/.passwordlock"

static struct {
	int checkfd;
	alarm_id_t alarm_id;	/* -1 : None, others : set alarm */
	Eina_Bool is_alarm;	/* EINA_TRUE : can use alarm EINA_FALSE : cannot use */

	int old_lock_type;
	int lock_pid;
	int lcd_state;

#ifdef HAVE_X11
	lockw_data *lockw;
#endif
} s_lock_mgr = {
	.checkfd = 0,
	.alarm_id = -1,
	.is_alarm = EINA_FALSE,

	.old_lock_type = 0,
	.lock_pid = -1,
	.lcd_state = -1,

#ifdef HVAE_X11
	.lockw = NULL,
#endif
};



int lock_mgr_lcd_state_get(void)
{
	return s_lock_mgr.lcd_state;
}



int lock_mgr_get_lock_pid(void)
{
	return s_lock_mgr.lock_pid;
}



static int _alarm_del(alarm_id_t id, void * user_param)
{
	int ret = ALARMMGR_RESULT_SUCCESS;

	_D("delete alarm id : %d", id);

	ret = alarmmgr_remove_alarm(id);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		_E("Failed to remove alarm(%d)", ret );
	}

	return 0;
}



static void _alarm_unset(void)
{
	int ret = ALARMMGR_RESULT_SUCCESS;

	ret = alarmmgr_enum_alarm_ids(_alarm_del, NULL);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		_E("Failed to get list of alarm ids");
	}
}



static void _alarm_lockscreen_launch(alarm_id_t alarm_id, void *data)
{
	int ret = ALARMMGR_RESULT_SUCCESS;

	_D("alarm id : %d", alarm_id);

	/* launch lockscreen */
	if (!lock_mgr_lockscreen_launch()) {
		_E("Failed to launch lockscreen");
	}

	if (alarm_id != -1) {
		if (alarm_id != s_lock_mgr.alarm_id) {
			_E("alarm ids are different callback->id(%d), s_lock_mgr.alarm_id(%d)", alarm_id, s_lock_mgr.alarm_id);
			/* delete all registering alarm*/
			_alarm_unset();
			s_lock_mgr.alarm_id = -1;
		} else {
			ret = alarmmgr_remove_alarm(alarm_id);
			if (ret != ALARMMGR_RESULT_SUCCESS) {
				_E("Failed to remove alaram(%d)", ret);
				/* delete all registering alarm*/
				_alarm_unset();
			}
			s_lock_mgr.alarm_id = -1;
		}
	}
}



static Eina_Bool _alarm_init(void)
{
	int ret = 0;

	/* alarm id initialize */
	s_lock_mgr.alarm_id = -1;

	ret = alarmmgr_init(PACKAGE_NAME);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		_E("Failed to initialize alarmmgr(%d)", ret);
		return EINA_FALSE;
	}

	ret = alarmmgr_set_cb((alarm_cb_t)_alarm_lockscreen_launch, NULL);
	if (ret != ALARMMGR_RESULT_SUCCESS) {
		_E("Failed to set cb func(%d)", ret);
		return EINA_FALSE;
	}

	_D("alarm init success");

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
	if (!idle_lock_state) {
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

#ifdef HAVE_X11
	window_mgr_unregister_event(s_lock_mgr.lockw);
	window_mgr_fini(s_lock_mgr.lockw);
	s_lock_mgr.lockw = NULL;
#endif
}



#ifdef HAVE_X11
static Eina_Bool _lock_create_cb(void *data, int type, void *event)
{
	_D("lockw(%p), lock_pid(%d)", s_lock_mgr.lockw, s_lock_mgr.lock_pid);

	if (window_mgr_set_effect(s_lock_mgr.lockw, s_lock_mgr.lock_pid, event) == EINA_TRUE) {
		//FIXME sometimes show cb is not called.
		if (window_mgr_set_prop(s_lock_mgr.lockw, s_lock_mgr.lock_pid, event) == EINA_FALSE) {
			_E("window is not matched..!!");
		}
	}
	return ECORE_CALLBACK_PASS_ON;
}
#endif



#ifdef HAVE_X11
static Eina_Bool _lock_show_cb(void *data, int type, void *event)
{
	_D("lockw(%p), lock_pid(%d)", s_lock_mgr.lockw, s_lock_mgr.lock_pid);

	if (window_mgr_set_prop(s_lock_mgr.lockw, s_lock_mgr.lock_pid, event)) {
		int lock_type = status_active_get()->setappl_screen_lock_type_int;
		_D("lock type : %d", lock_type);

		window_mgr_set_scroll_prop(s_lock_mgr.lockw, lock_type);
	}

	return ECORE_CALLBACK_CANCEL;
}
#endif



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

#ifdef HAVE_X11
	window_mgr_unregister_event(s_lock_mgr.lockw);
#endif
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

		/* delete all alarm registering */
		_D("delete alarm : id(%d)", s_lock_mgr.alarm_id);
		_alarm_unset();
		s_lock_mgr.alarm_id = -1;
	} else if (lcd_off) {
		s_lock_mgr.lcd_state = LCD_STATE_OFF;

		int idle_lock_state = status_passive_get()->idle_lock_state;
		int lock_type = status_active_get()->setappl_screen_lock_type_int;
		_D("idle_lock_state(%d), lock type(%d)", idle_lock_state, lock_type);

		if (!lock_mgr_lockscreen_launch()) {
			_E("Failed to launch lockscreen");
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

#ifdef HAVE_X11
	/* reset window mgr before start win mgr  */
	window_mgr_unregister_event(s_lock_mgr.lockw);
	window_mgr_register_event(NULL, s_lock_mgr.lockw, _lock_create_cb, _lock_show_cb);
#endif

	lock_appid = status_passive_get()->setappl_3rd_lock_pkg_name_str;
	if (!lock_appid) {
		_E("set default lockscreen");
		lock_appid = STATUS_DEFAULT_LOCK_PKG_NAME;
	}

	_D("lockscreen appid : %s", lock_appid);

	switch (lock_type) {
	//case SETTING_SCREEN_LOCK_TYPE_NONE:
	case SETTING_SCREEN_LOCK_TYPE_OTHER:
		if (!strcmp(lock_appid, STATUS_DEFAULT_LOCK_PKG_NAME)) {
			_D("ignore launching lockscreen");
		} else {
			process_mgr_must_launch(lock_appid, NULL, NULL, _lock_changed_cb, _after_launch_lock);
			//@TODO: need to check(add error popup)
		}
		break;
	case SETTING_SCREEN_LOCK_TYPE_SWIPE:
		process_mgr_must_launch(lock_appid, NULL, NULL, _lock_changed_cb, _after_launch_lock);
		goto_if(s_lock_mgr.lock_pid < 0, ERROR);
		break;
	case SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD:
	case SETTING_SCREEN_LOCK_TYPE_PASSWORD:
		process_mgr_must_launch(lock_appid, NULL, NULL, _lock_changed_cb, _after_launch_lock);
		goto_if(s_lock_mgr.lock_pid < 0, ERROR);

		if (dbus_util_send_oomadj(s_lock_mgr.lock_pid, OOM_ADJ_VALUE_DEFAULT) < 0){
			_E("Failed to send oom dbus signal");
		}

		process_mgr_set_lock_priority(s_lock_mgr.lock_pid);
		display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
		break;
	default:
		_E("type error(%d)", lock_type);
		goto ERROR;
	}

	_W("lock_pid : %d", s_lock_mgr.lock_pid);

	return EINA_TRUE;

ERROR:
	_E("Failed to launch lockscreen");

	display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);

	return EINA_FALSE;
}



static void _lock_daemon_init(void)
{
	_SECURE_D("default lock screen pkg name is %s", status_passive_get()->setappl_3rd_lock_pkg_name_str);

	/* init alarm manager */
	s_lock_mgr.is_alarm = _alarm_init();

	/* register lcd changed cb */
	dbus_util_receive_lcd_status(_on_lcd_changed_receive, NULL);

#ifdef HAVE_X11
	/* Create internal 1x1 window */
	s_lock_mgr.lockw = window_mgr_init();
#endif
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

	ret = lock_mgr_lockscreen_launch();
	_D("ret : %d", ret);

	status_active_register_cb(STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT, _lock_type_changed_cb, NULL);

	if (feedback_initialize() != FEEDBACK_ERROR_NONE) {
		_E("Failed to initialize feedback");
	}

	return ret;
}



void lock_mgr_daemon_end(void)
{
#ifdef HAVE_X11
	if (s_lock_mgr.lockw) {
		free(s_lock_mgr.lockw);
	}
#endif
}
