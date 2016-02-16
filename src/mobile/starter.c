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

#include <aul.h>
#include <vconf.h>
#include <signal.h>

#include "starter.h"
#include "lock_mgr.h"
#include "lock_pwd_util.h"
#include "lock_pwd_control_panel.h"
#include "home_mgr.h"
#include "hw_key.h"
#include "process_mgr.h"
#include "util.h"
#include "status.h"
#include "hw_key.h"

#define PWLOCK_LITE_PKG_NAME "org.tizen.pwlock-lite"

#define DATA_UNENCRYPTED "unencrypted"
#define DATA_MOUNTED "mounted"
#define SD_DATA_ENCRYPTED "encrypted"
#define SD_CRYPT_META_FILE ".MetaEcfsFile"
#define MMC_MOUNT_POINT	"/opt/storage/sdcard"



static void _hide_home(void)
{
	int seq = status_active_get()->starter_sequence;
	ret_if(seq == 1);

	vconf_set_int(VCONFKEY_STARTER_SEQUENCE, 0);
}



static void _show_home(void)
{
	int show_menu = 0;

	if (status_active_get()->starter_sequence || !show_menu) {
		vconf_set_int(VCONFKEY_STARTER_SEQUENCE, 1);
	}
}



#if 0
static Eina_Bool _finish_boot_animation(void *data)
{
	if (vconf_set_int(VCONFKEY_BOOT_ANIMATION_FINISHED, 1) != 0) {
		_E("Failed to set boot animation finished set");
	}
	_show_home();

	return ECORE_CALLBACK_CANCEL;
}



static int _fail_to_launch_pwlock(const char *appid, const char *key, const char *value, void *cfn, void *afn)
{
	_finish_boot_animation(NULL);
	return 0;
}



static void _after_launch_pwlock(int pid)
{
	process_mgr_set_pwlock_priority(pid);
	ecore_timer_add(0.5, _finish_boot_animation, NULL);
}
#endif



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



static int _boot_animation_finished_cb(status_active_key_e key, void *data)
{
	int val = status_active_get()->boot_animation_finished;
	_D("boot animation finished : %d", val);

	if (val == 1) {
		lock_mgr_daemon_start();
		_show_home();
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

	lock_pwd_util_view_init();
	lock_pwd_control_panel_emg_btn_text_update();
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
	int home_pid = 0;
	int volume_pid = 0;
	int lock_pid = 0;

	_D("Process %d is termianted", pid);

	if (pid < 0) {
		_E("pid : %d", pid);
		return 0;
	}

	home_pid = home_mgr_get_home_pid();
	volume_pid = home_mgr_get_volume_pid();
	lock_pid = lock_mgr_get_lock_pid();

	if (pid == home_pid) {
		_D("Homescreen is dead");
		home_mgr_relaunch_homescreen();
	} else if (pid == volume_pid) {
		_D("volume is dead");
		home_mgr_relaunch_volume();
	} else if (pid == lock_pid) {
		_D("lockscreen is dead");
		lock_mgr_unlock();
	} else {
		_D("Unknown process, ignore it");
	}

	return 0;
}



static void _init(struct appdata *ad)
{
	struct sigaction act;

	memset(&act,0x00,sizeof(struct sigaction));
	act.sa_sigaction = _signal_handler;
	act.sa_flags = SA_SIGINFO;

	int ret = sigemptyset(&act.sa_mask);
	if (ret < 0) {
		_E("Failed to sigemptyset[%s]", strerror(errno));
	}
	ret = sigaddset(&act.sa_mask, SIGTERM);
	if (ret < 0) {
		_E("Failed to sigaddset[%s]", strerror(errno));
	}
	ret = sigaction(SIGTERM, &act, NULL);
	if (ret < 0) {
		_E("Failed to sigaction[%s]", strerror(errno));
	}

	_set_i18n(PACKAGE, LOCALEDIR);

	status_register();
	status_active_register_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb, NULL);
	status_active_register_cb(STATUS_ACTIVE_KEY_BOOT_ANIMATION_FINISHED, _boot_animation_finished_cb, NULL);

	/* Ordering : _hide_home -> process_mgr_must_launch(pwlock) -> _show_home */
	_hide_home();
#if 0
	process_mgr_must_launch(PWLOCK_LITE_PKG_NAME, NULL, NULL, _fail_to_launch_pwlock, _after_launch_pwlock);
#endif

	hw_key_create_window();
	home_mgr_init(NULL);

	aul_listen_app_dead_signal(_check_dead_signal, NULL);
}



static void _fini(struct appdata *ad)
{
	home_mgr_fini();
	hw_key_destroy_window();
	lock_mgr_daemon_end();

	status_active_unregister_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb);
	status_active_unregister_cb(STATUS_ACTIVE_KEY_BOOT_ANIMATION_FINISHED, _boot_animation_finished_cb);
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

	elm_run();

	_fini(&ad);
	elm_shutdown();

	return 0;
}
