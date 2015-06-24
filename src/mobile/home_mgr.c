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

#include <aul.h>
#include <app.h>
#include <db-util.h>
#include <Elementary.h>
#include <errno.h>
#include <fcntl.h>
#include <pkgmgr-info.h>
#include <stdio.h>
#include <dd-deviced.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vconf.h>

#include "util.h"
#include "dbus_util.h"
#include "status.h"
#include "process_mgr.h"
#include "popup.h"

#define HOME_TERMINATED "home_terminated"
#define ISTRUE "TRUE"
#define SYSPOPUPID_VOLUME "volume"

#define DEAD_TIMER_SEC 2.0
#define DEAD_TIMER_COUNT_MAX 2

#define VCONFKEY_PRIVATE_STARTER_IS_FALLBACK "db/private/starter/is_fallback"
#define VCONFKEY_PRIVATE_STARTER_FALLBACK_PKG "db/private/starter/fallback_pkg"



int errno;
static struct {
	pid_t home_pid;
	pid_t volume_pid;
	int power_off;

	Ecore_Timer *dead_timer;
	int dead_count;

	Evas_Object *popup;
} s_home_mgr = {
	.home_pid = (pid_t)-1,
	.volume_pid = (pid_t)-1,
	.power_off = 0,

	.dead_timer = NULL,
	.dead_count = 0,

	.popup = NULL,
};



static void _after_launch_home(int pid)
{
	if (dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_HOMESCREEN) < 0){
		_E("failed to send oom dbus signal");
	}
	s_home_mgr.home_pid = pid;
}



static int _change_home_cb(const char *appid, const char *key, const char *value, void *cfn, void *afn)
{
	if (!strcmp(appid, MENU_SCREEN_PKG_NAME)) {
		_E("We cannot do anything anymore.");
	} else if (!strcmp(appid, status_active_get()->setappl_selected_package_name)) {
		if (vconf_set_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, MENU_SCREEN_PKG_NAME) != 0) {
			_E("cannot set the vconf key as %s", MENU_SCREEN_PKG_NAME);
		}
		/* change_home func will be called by changing the home */
		return 0;
	}
	_E("cannot change home");
	return -1;
}



#define SERVICE_OPERATION_MAIN_KEY "__APP_SVC_OP_TYPE__"
#define SERVICE_OPERATION_MAIN_VALUE "http://tizen.org/appcontrol/operation/main"
void home_mgr_open_home(const char *appid)
{
	char *home_appid = NULL;

	if (status_passive_get()->idle_screen_safemode) {
		home_appid = MENU_SCREEN_PKG_NAME;
	} else if (!appid) {
		home_appid = status_active_get()->setappl_selected_package_name;
	} else {
		home_appid = (char *) appid;
	}
	ret_if(!home_appid);

	process_mgr_must_launch(home_appid, SERVICE_OPERATION_MAIN_KEY, SERVICE_OPERATION_MAIN_VALUE, _change_home_cb, _after_launch_home);
}



static int _show_home_cb(status_active_key_e key, void *data)
{
	int seq = status_active_get()->starter_sequence;
	int is_fallback = 0;

	_D("[MENU_DAEMON] _show_home_cb is invoked(%d)", seq);

	switch (seq) {
	case 0:
		if (s_home_mgr.home_pid > 0) {
			_D("Home[%d] has to be terminated.", s_home_mgr.home_pid);
			if (aul_terminate_pid(s_home_mgr.home_pid) != AUL_R_OK) {
				_E("Failed to terminate %d", s_home_mgr.home_pid);
			}
			s_home_mgr.home_pid = -1; /* to freeze the dead_cb */
		}
		break;
	case 1:
		if (vconf_get_int(VCONFKEY_PRIVATE_STARTER_IS_FALLBACK, &is_fallback) < 0) {
			_E("Failed to get vconfkey : %s", VCONFKEY_PRIVATE_STARTER_IS_FALLBACK);
		}

		if (is_fallback) {
			if (vconf_set_int(VCONFKEY_PRIVATE_STARTER_IS_FALLBACK, 0)) {
				_E("Failed to set vconfkey : %s", VCONFKEY_PRIVATE_STARTER_IS_FALLBACK);
			}

			if (!strcmp(status_active_get()->setappl_selected_package_name, MENU_SCREEN_PKG_NAME)) {
				char *fallback_pkg;

				fallback_pkg = vconf_get_str(VCONFKEY_PRIVATE_STARTER_FALLBACK_PKG);
				_D("fallback pkg : %s", fallback_pkg);
				if (fallback_pkg) {
					int status;

					status = vconf_set_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, fallback_pkg);
					free(fallback_pkg);
					if (status == 0) {
						break;
					}
					_E("Failed to set vconfkey : %s (%d)", VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, status);
				} else {
					_E("Failed to get vconfkey : %s", VCONFKEY_PRIVATE_STARTER_FALLBACK_PKG);
				}
			}
		}

		home_mgr_open_home(NULL);
		break;
	default:
		_E("False sequence [%d]", seq);
		break;
	}

	return 1;
}



static int _change_selected_package_name(status_active_key_e key, void *data)
{
	char *appid = NULL;
	int seq = status_active_get()->starter_sequence;

	if (seq < 1) {
		_E("Sequence is not ready yet, do nothing");
		return 1;
	}

	_D("_change_selected_package_name is invoked");

	appid = status_active_get()->setappl_selected_package_name;
	if (!appid) {
		return 1;
	}
	_SECURE_D("pkg_name : %s", appid);

	if (s_home_mgr.home_pid > 0) {
		char old_appid[BUF_SIZE_512] = { 0 , };

		if (aul_app_get_pkgname_bypid(s_home_mgr.home_pid, old_appid, sizeof(old_appid)) == AUL_R_OK) {
			if (!strcmp(appid, old_appid)) {
				_D("Package is changed but same package is selected");
				return 1;
			}
		}

		if (AUL_R_OK != aul_terminate_pid(s_home_mgr.home_pid)) {
			_D("Failed to terminate pid %d", s_home_mgr.home_pid);
		}
		s_home_mgr.home_pid = -1;
		s_home_mgr.dead_count = 0;
		if (s_home_mgr.dead_timer) {
			ecore_timer_del(s_home_mgr.dead_timer);
			s_home_mgr.dead_timer = NULL;
		}
	}

	home_mgr_open_home(appid);

	return 1;
}



static void _after_launch_volume(int pid)
{
	if (dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_DEFAULT) < 0){
		_E("failed to send oom dbus signal");
	}
	s_home_mgr.volume_pid = pid;
}



static void _launch_after_home(int pid)
{
	if (pid > 0) {
		if(dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_HOMESCREEN) < 0){
			_E("failed to send oom dbus signal");
		}
	}
	s_home_mgr.home_pid = pid;
}



static void _launch_home(const char *appid)
{
	const char *home_appid = NULL;

	if (status_passive_get()->idle_screen_safemode) {
		home_appid = MENU_SCREEN_PKG_NAME;
	} else if (!appid) {
		home_appid = status_active_get()->setappl_selected_package_name;
	} else {
		home_appid = (char *) appid;
	}
	ret_if(!home_appid);

	process_mgr_must_launch(home_appid, HOME_TERMINATED, ISTRUE, _change_home_cb, _launch_after_home);
}



static void _popup_del_cb(void *data, Evas *e, Evas_Object *obj, void *event_info)
{
	_D("popup is deleted");

	s_home_mgr.popup = NULL;
}



static Eina_Bool _dead_timer_cb(void *data)
{
	Evas_Object *popup = NULL;
	char title[BUF_SIZE_128] = { 0, };
	char text[BUF_SIZE_1024] = { 0, };

	char *appid = (char *)data;
	retv_if(!appid, ECORE_CALLBACK_CANCEL);

	_D("dead count : %s(%d)", appid, s_home_mgr.dead_count);

	if (s_home_mgr.dead_count >= DEAD_TIMER_COUNT_MAX) {
		_D("Change homescreen package to default");

		if (vconf_set_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, MENU_SCREEN_PKG_NAME) != 0) {
			_E("cannot set the vconf key as %s", MENU_SCREEN_PKG_NAME);
			return ECORE_CALLBACK_RENEW;
		}

		strncpy(title, _("IDS_COM_POP_WARNING"), sizeof(title));
		title[sizeof(title) - 1] = '\0';

		snprintf(text, sizeof(text), _("IDS_IDLE_POP_UNABLE_TO_LAUNCH_PS"), appid);
		_D("title : %s / text : %s", title, text);

		if (!s_home_mgr.popup) {
			popup = popup_create(title, text);
			if (!popup) {
				_E("Failed to create popup");
			} else {
				s_home_mgr.popup = popup;
				evas_object_event_callback_add(popup, EVAS_CALLBACK_DEL, _popup_del_cb, NULL);
			}
		}

		/* set fallback status */
		if (vconf_set_int(VCONFKEY_PRIVATE_STARTER_IS_FALLBACK, 1) < 0) {
			_E("Failed to set vconfkey : %s", VCONFKEY_PRIVATE_STARTER_IS_FALLBACK);
		}

		if (vconf_set_str(VCONFKEY_PRIVATE_STARTER_FALLBACK_PKG, appid) < 0) {
			_E("Failed to set vconfkey : %s", VCONFKEY_PRIVATE_STARTER_FALLBACK_PKG);
		}
	}

	s_home_mgr.dead_timer = NULL;
	s_home_mgr.dead_count = 0;

	return ECORE_CALLBACK_CANCEL;
}



int home_mgr_check_dead_signal(int pid, void *data)
{
	char *appid = NULL;

	_D("Process %d is termianted", pid);

	if (pid < 0) {
		_E("pid : %d", pid);
		return 0;
	}

	if (s_home_mgr.power_off) {
		_E("power off");
		return 0;
	}

	appid = status_active_get()->setappl_selected_package_name;
	if (!appid) {
		_E("appid is NULL");
		return 0;
	}

	if (pid == s_home_mgr.home_pid) {
		s_home_mgr.dead_count++;
		_D("home dead count : %d", s_home_mgr.dead_count);

		if (!s_home_mgr.dead_timer) {
			_D("Add dead timer");
			s_home_mgr.dead_timer = ecore_timer_add(DEAD_TIMER_SEC, _dead_timer_cb, (void *)appid);
		}

		_launch_home(appid);
	} else if (pid == s_home_mgr.volume_pid) {
		process_mgr_must_syspopup_launch(SYSPOPUPID_VOLUME, NULL, NULL, NULL, _after_launch_volume);
	} else {
		_D("Unknown process, ignore it (dead pid %d, home pid %d)", pid, s_home_mgr.home_pid);
	}

	return 0;
}



static int _power_off_cb(status_active_key_e key, void *data)
{
	int val = status_active_get()->sysman_power_off_status;

	if (val == VCONFKEY_SYSMAN_POWER_OFF_DIRECT || val == VCONFKEY_SYSMAN_POWER_OFF_RESTART) {
		s_home_mgr.power_off = 1;
	} else {
		s_home_mgr.power_off = 0;
	}

	_D("power off status : %d", s_home_mgr.power_off);

	return 1;
}



static Eina_Bool _launch_volume_idler_cb(void *data)
{
	process_mgr_must_syspopup_launch(SYSPOPUPID_VOLUME, NULL, NULL, NULL, _after_launch_volume);
	return ECORE_CALLBACK_CANCEL;
}



void home_mgr_init(void *data)
{
	_D( "[MENU_DAEMON]home_mgr_init is invoked");

	status_active_register_cb(STATUS_ACTIVE_KEY_STARTER_SEQUENCE, _show_home_cb, NULL);
	status_active_register_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb, NULL);
	status_active_register_cb(STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME, _change_selected_package_name, NULL);
	_change_selected_package_name(STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME, NULL);

	ecore_idler_add(_launch_volume_idler_cb, NULL);

	aul_listen_app_dead_signal(home_mgr_check_dead_signal, NULL);
}



void home_mgr_fini(void)
{
	if (s_home_mgr.volume_pid > 0) {
		process_mgr_terminate_app(s_home_mgr.volume_pid, 1);
		s_home_mgr.volume_pid = -1;
	}

	status_active_unregister_cb(STATUS_ACTIVE_KEY_STARTER_SEQUENCE, _show_home_cb);
	status_active_unregister_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb);
	status_active_unregister_cb(STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME, _change_selected_package_name);
}



// End of a file
