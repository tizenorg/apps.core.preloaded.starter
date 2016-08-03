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
#include <malloc.h>

#include <E_DBus.h>
#include <lazymount/lazy_mount.h>

#include <aul.h>
#include <vconf.h>
#include <signal.h>

#include "starter.h"
#include "lock_mgr.h"
#include "home_mgr.h"
#include "hw_key.h"
#include "process_mgr.h"
#include "util.h"
#include "status.h"
#include "hw_key.h"
#include "dbus_util.h"

#define APPID_SYS_LOCK "org.tizen.sys-lock"


#if 0
static void _hide_home(void)
{
	int seq = status_active_get()->starter_sequence;
	ret_if(seq == 1);

	vconf_set_int(VCONFKEY_STARTER_SEQUENCE, 0);
}
#endif



static void _show_home(void)
{
	vconf_set_int(VCONFKEY_STARTER_SEQUENCE, 1);
}



static void _signal_handler(int signum, siginfo_t *info, void *unused)
{
    _D("_signal_handler : Terminated...");
    elm_exit();
}



static int _power_off_cb(status_active_key_e key, void *data)
{
	int val = status_active_get()->sysman_power_off_status;

	if (val == VCONFKEY_SYSMAN_POWER_OFF_DIRECT
		|| val == VCONFKEY_SYSMAN_POWER_OFF_RESTART)
	{
	    _D("_power_off_cb : Terminated...");
	    elm_exit();
	}

	return 1;
}



static void _language_changed_cb(keynode_t *node, void *data)
{
	char *lang = NULL;

	ret_if(!node);

	lang = vconf_keynode_get_str(node);
	ret_if(!lang);

	_D("language is changed : %s", lang);

	elm_language_set(lang);
}



static int _set_i18n(const char *domain, const char *dir)
{
	char *r = NULL;

	if (domain == NULL) {
		errno = EINVAL;
		return -1;
	}

	char *lang = vconf_get_str(VCONFKEY_LANGSET);
	r = setlocale(LC_ALL, lang);
	if (!r) {
		_E("setlocale() error");
	}
	if (lang) {
		free(lang);
	}

	r = bindtextdomain(domain, dir);
	if (!r) {
		_E("bindtextdomain() error");
	}

	r = textdomain(domain);
	if (!r) {
		_E("textdomain() error");
	}

	if (vconf_notify_key_changed(VCONFKEY_LANGSET, _language_changed_cb, NULL) < 0) {
		_E("Failed to register changed cb : %s", VCONFKEY_LANGSET);
	}

	return 0;
}



static int _check_dead_signal(int pid, void *data)
{
#ifndef TIZEN_ARCH_ARM64
	int home_pid = 0;
	int volume_pid = 0;
	int indicator_pid = 0;
	int quickpanel_pid = 0;
	int lock_pid = 0;

	_D("Process %d is termianted", pid);

	if (pid < 0) {
		_E("pid : %d", pid);
		return 0;
	}

	/*
	 * If the architecture is not 64bit,
	 * starter try to re-launch these apps when the app is dead.
	 */
	home_pid = home_mgr_get_home_pid();
	volume_pid = home_mgr_get_volume_pid();
	indicator_pid = home_mgr_get_indicator_pid();
	quickpanel_pid = home_mgr_get_quickpanel_pid();
	lock_pid = lock_mgr_get_lock_pid();

	if (pid == home_pid) {
		_D("Homescreen is dead");
		home_mgr_relaunch_homescreen();
	} else if (pid == volume_pid) {
		_D("volume is dead");
		home_mgr_relaunch_volume();
	} else if (pid == indicator_pid) {
		_D("indicator is dead");
		home_mgr_relaunch_indicator();
	} else if (pid == quickpanel_pid) {
		_D("quickpanel is dead");
		home_mgr_relaunch_quickpanel();
	} else if (pid == lock_pid) {
		_D("lockscreen is dead");
		lock_mgr_unlock();
	} else {
		_D("Unknown process, ignore it");
	}
#else
	_D("Process %d is termianted", pid);

	if (pid < 0) {
		_E("pid : %d", pid);
		return 0;
	}
#endif

	return 0;
}



static void _sys_lock_status_changed_cb(void *data, DBusMessage *msg)
{
	int is_unlock = 0;

	_D("sys-lock signal is received");

	is_unlock = dbus_message_is_signal(msg, SYS_LOCK_INTERFACE_UNLOCK, SYS_LOCK_MEMBER_UNLOCK);
	if (is_unlock) {
		int ret = 0;

		/*
		 * Do mount user data partition
		 */
		ret = do_mount_user();
		if (ret != 0) {
			_E("Failed to do mount [/opt] area");
			return;
		}

		/*
		 * Wait user data partition mount
		 */
		ret = wait_mount_user();
		if (ret != 0) {
			_E("Failed to wait mount [/opt] area");
			return;
		}

		/*
		 * After user data partition mount,
		 * launch lockscreen, homescreen, etc.
		 */
		lock_mgr_init();
		home_mgr_init(NULL);

		_show_home();

		/*
		 * Send DBus signal to terminate sys lock
		 */
		dbus_util_send_sys_lock_teminate_signal();
	}
}



static void _init(struct appdata *ad)
{
	struct sigaction act;
	char err_buf[128] = {0,};

	memset(&act,0x00,sizeof(struct sigaction));
	act.sa_sigaction = _signal_handler;
	act.sa_flags = SA_SIGINFO;

	int ret = sigemptyset(&act.sa_mask);
	if (ret < 0) {
		if (strerror_r(errno, err_buf, sizeof(err_buf)) == 0) {
			_E("Failed to sigemptyset[%d / %s]", errno, err_buf);
		}
	}
	ret = sigaddset(&act.sa_mask, SIGTERM);
	if (ret < 0) {
		if (strerror_r(errno, err_buf, sizeof(err_buf)) == 0) {
			_E("Failed to sigaddset[%d / %s]", errno, err_buf);
		}
	}
	ret = sigaction(SIGTERM, &act, NULL);
	if (ret < 0) {
		if (strerror_r(errno, err_buf, sizeof(err_buf)) == 0) {
			_E("Failed to sigaction[%d / %s]", errno, err_buf);
		}
	}


	_set_i18n(PACKAGE, LOCALEDIR);

	status_register();
	status_active_register_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb, NULL);

	hw_key_create_window();

	e_dbus_init();

	///////////////////////////////////////////////
	int is_lazy_mount = 0;
	is_lazy_mount = get_need_ui_for_lazy_mount();
	if (is_lazy_mount) {
		/*
		 * Launch Sys-lock
		 */
		process_mgr_must_launch(APPID_SYS_LOCK, NULL, NULL, NULL, NULL);

		/*
		 * Register sys-lock status changed cb
		 */
		dbus_util_receive_lcd_status(_sys_lock_status_changed_cb, NULL);
	} else {
		lock_mgr_init();
		home_mgr_init(NULL);

		_show_home();
	}
	///////////////////////////////////////////////

	aul_listen_app_dead_signal(_check_dead_signal, NULL);
}



static void _fini(struct appdata *ad)
{
	hw_key_destroy_window();

	home_mgr_fini();
	lock_mgr_fini();

	status_active_unregister_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb);
	status_unregister();

	if (vconf_ignore_key_changed(VCONFKEY_LANGSET, _language_changed_cb) < 0) {
		_E("Failed to unregister changed cb : %s", VCONFKEY_LANGSET);
	}
}



int main(int argc, char *argv[])
{
	struct appdata ad;

	_D("starter is launched..!!");

	elm_init(argc, argv);
	_init(&ad);

	malloc_trim(0);
	elm_run();

	_fini(&ad);
	elm_shutdown();

	return 0;
}
