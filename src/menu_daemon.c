 /*
  * Copyright 2012  Samsung Electronics Co., Ltd
  *
  * Licensed under the Flora License, Version 1.1 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://floralicense.org/license/
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */



#include <ail.h>
#include <aul.h>
#include <db-util.h>
#include <Elementary.h>
#include <errno.h>
#include <fcntl.h>
#include <pkgmgr-info.h>
#include <stdio.h>
#include <sysman.h>
#include <syspopup_caller.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vconf.h>

#include "hw_key.h"
#include "pkg_event.h"
#include "util.h"
#include "xmonitor.h"


int errno;


#define QUERY_UPDATE_NAME "UPDATE app_info SET name='%s' where package='%s';"
#define SAT_DESKTOP_FILE "/opt/share/applications/org.tizen.sat-ui.desktop"
#define RELAUNCH_INTERVAL 100*1000
#define RETRY_MAXCOUNT 30

#define DEAD_TIMER_SEC 2.0
#define DEAD_TIMER_COUNT_MAX 2
#define MENU_SCREEN_PKG_NAME "org.tizen.menu-screen"

static struct info {
	pid_t home_pid;
	pid_t volume_pid;
	int power_off;

	int dead_count;
	Ecore_Timer *dead_timer;
} s_info = {
	.home_pid = -1,
	.volume_pid = -1,
	.power_off = 0,

	.dead_count = 0,
	.dead_timer = NULL,
};



#define RETRY_COUNT 5
int menu_daemon_open_app(const char *pkgname)
{
	register int i;
	int r = AUL_R_ETIMEOUT;
	for (i = 0; AUL_R_ETIMEOUT == r && i < RETRY_COUNT; i ++) {
		r = aul_open_app(pkgname);
		if (0 <= r) return r;
		else {
			_D("aul_open_app error(%d)", r);
			_F("cannot open an app(%s) by %d", pkgname, r);
		}
		usleep(500000);
	}

	return r;
}



int menu_daemon_launch_app(const char *pkgname, bundle *b)
{
	register int i;
	int r = AUL_R_ETIMEOUT;
	for (i = 0; AUL_R_ETIMEOUT == r && i < RETRY_COUNT; i ++) {
		r = aul_launch_app(pkgname, b);
		if (0 <= r) return r;
		else {
			_D("aul_launch_app error(%d)", r);
			_F("cannot launch an app(%s) by %d", pkgname, r);
		}
		usleep(500000);
	}

	return r;
}



bool menu_daemon_is_homescreen(pid_t pid)
{
	if (s_info.home_pid == pid) return true;
	return false;
}



inline char *menu_daemon_get_selected_pkgname(void)
{
	char *pkgname = NULL;

	pkgname = vconf_get_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME);
	retv_if(NULL == pkgname, NULL);

	return pkgname;
}



static bool _exist_package(char *pkgid)
{
	int ret = 0;
	pkgmgrinfo_pkginfo_h handle = NULL;

	ret = pkgmgrinfo_pkginfo_get_pkginfo(pkgid, &handle);
	if (PMINFO_R_OK != ret || NULL == handle) {
		_D("%s doesn't exist in this binary", pkgid);
		return false;
	}

	pkgmgrinfo_pkginfo_destroy_pkginfo(handle);

	return true;
}



inline void menu_daemon_open_homescreen(const char *pkgname)
{
	char *homescreen = NULL;
	char *tmp = NULL;

	system("echo -e '[${_G}menu-daemon launches home-screen${C_}]' > /dev/kmsg");

	if (NULL == pkgname) {
		tmp = menu_daemon_get_selected_pkgname();
		ret_if(NULL == tmp);
		homescreen = tmp;
	} else {
		homescreen = (char *) pkgname;
	}

	syspopup_destroy_all();

	int ret;
	ret = menu_daemon_open_app(homescreen);
	_D("can%s launch %s now. (%d)", ret < 0 ? "not" : "", homescreen, ret);
	if (ret < 0 && strcmp(homescreen, HOME_SCREEN_PKG_NAME) && _exist_package(HOME_SCREEN_PKG_NAME)) {
		_E("cannot launch package %s", homescreen);

		if (0 != vconf_set_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, HOME_SCREEN_PKG_NAME)) {
			_E("Cannot set value(%s) into key(%s)", VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, HOME_SCREEN_PKG_NAME);
		}

		while (AUL_R_ETIMEOUT == ret) {
			_E("Failed to open a default home, %s", HOME_SCREEN_PKG_NAME);
			ret = menu_daemon_open_app(HOME_SCREEN_PKG_NAME);
		}
	}

	if (ret < 0) {
		_E("Critical! Starter cannot launch anymore[%d]", ret);
		_F("Critical! Starter cannot launch anymore[%d]", ret);
	}

	s_info.home_pid = ret;
	if (ret > 0) {
		if (-1 == sysconf_set_mempolicy_bypid(ret, OOM_IGNORE)) {
			_E("Cannot set the memory policy for Home-screen(%d)", ret);
		} else {
			_D("Set the memory policy for Home-screen(%d)", ret);
		}
	}

	if (tmp) free(tmp);
}



static void _show_cb(keynode_t* node, void *data)
{
	int seq;

	_D("[MENU_DAEMON] _show_cb is invoked");

	if (node) {
		seq = vconf_keynode_get_int(node);
	} else {
		if (vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &seq) < 0) {
			_E("Failed to get sequence info");
			return;
		}
	}

	switch (seq) {
		case 0:
			if (s_info.home_pid > 0) {
				int pid;
				_D("pid[%d] is terminated.", s_info.home_pid);
				pid = s_info.home_pid;
				s_info.home_pid = -1;

				if (aul_terminate_pid(pid) != AUL_R_OK)
					_E("Failed to terminate %d", s_info.home_pid);
			}
			break;
		case 1:
			menu_daemon_open_homescreen(NULL);
			break;
		default:
			_E("False sequence [%d]", seq);
			break;
	}

	return;
}



static void _pkg_changed(keynode_t* node, void *data)
{
	char *pkgname;
	int seq;

	if (vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &seq) < 0) {
		_E("Do nothing, there is no sequence info yet");
		return;
	}

	if (seq < 1) {
		_E("Sequence is not ready yet, do nothing");
		return;
	}

	_D("_pkg_changed is invoked");

	pkgname = menu_daemon_get_selected_pkgname();
	if (!pkgname)
		return;

	_D("pkg_name : %s", pkgname);

	if (s_info.home_pid > 0) {
		char old_pkgname[256];

		if (aul_app_get_pkgname_bypid(s_info.home_pid, old_pkgname, sizeof(old_pkgname)) == AUL_R_OK) {
			if (!strcmp(pkgname, old_pkgname)) {
				_D("Package is changed but same package is selected");
				free(pkgname);
				return;
			}
		}

		if (AUL_R_OK != aul_terminate_pid(s_info.home_pid))
			_D("Failed to terminate pid %d", s_info.home_pid);

		s_info.dead_count = 0;
		if (s_info.dead_timer) {
			ecore_timer_del(s_info.dead_timer);
			s_info.dead_timer = NULL;
		}
	} else {
		/* If there is no running home */
		menu_daemon_open_homescreen(pkgname);
	}

	free(pkgname);
	return;
}



static void _launch_volume(void)
{
	int pid;
	int i;
	_D("_launch_volume");

	for (i=0; i<RETRY_MAXCOUNT; i++)
	{
		pid = syspopup_launch("volume", NULL);

		_D("syspopup_launch(volume), pid = %d", pid);

		if (pid <0) {
			_D("syspopup_launch(volume)is failed [%d]times", i);
			usleep(RELAUNCH_INTERVAL);
		} else {
			s_info.volume_pid = pid;
			return;
		}
	}
}

static Eina_Bool _dead_timer_cb(void *data)
{
	char *appid = (char *)data;
	retv_if(!appid, ECORE_CALLBACK_CANCEL);

	_D("dead count : %s(%d)", appid, s_info.dead_count);

	if (s_info.dead_count >= DEAD_TIMER_COUNT_MAX) {
		_D("Change homescreen package to default");
		if (vconf_set_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, MENU_SCREEN_PKG_NAME) != 0) {
			_D("cannot set the vconfkey as : %s", MENU_SCREEN_PKG_NAME);
			return ECORE_CALLBACK_CANCEL;
		}
	}

	s_info.dead_timer = NULL;
	s_info.dead_count = 0;

	return ECORE_CALLBACK_CANCEL;
}

int menu_daemon_check_dead_signal(int pid)
{
	if (s_info.power_off) {
		_D("Power off. ignore dead cb\n");
		return 0;
	}

	_D("Process %d is termianted", pid);

	if (pid < 0)
		return 0;

	if (pid == s_info.home_pid) {
		s_info.dead_count++;
		_D("home dead count : %d", s_info.dead_count);

		char *pkgname = NULL;
		pkgname = menu_daemon_get_selected_pkgname();
		retv_if(NULL == pkgname, 0);

		if (!s_info.dead_timer) {
			_D("Add dead timer");
			s_info.dead_timer = ecore_timer_add(DEAD_TIMER_SEC, _dead_timer_cb, (void *)pkgname);
		}

		_D("pkg_name : %s", pkgname);
		menu_daemon_open_homescreen(pkgname);
		free(pkgname);
	} else if (pid == s_info.volume_pid) {
		_launch_volume();
	} else {
		_D("Unknown process, ignore it (pid %d, home pid %d)",
				pid, s_info.home_pid);
	}


	return 0;
}



void menu_daemon_init(void *data)
{
	_D( "[MENU_DAEMON]menu_daemon_init is invoked");

	aul_launch_init(NULL,NULL);

	create_key_window();
	if (xmonitor_init() < 0) _E("cannot init xmonitor");

	pkg_event_init();
	_launch_volume();

	if (unlink(SAT_DESKTOP_FILE) != 0)
		_E("cannot remove sat-ui desktop.");

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, _pkg_changed, NULL) < 0)
		_E("Failed to add the callback for package change event");

	if (vconf_notify_key_changed(VCONFKEY_STARTER_SEQUENCE, _show_cb, NULL) < 0)
		_E("Failed to add the callback for show event");

	_pkg_changed(NULL, NULL);
	vconf_set_int(VCONFKEY_IDLE_SCREEN_LAUNCHED, VCONFKEY_IDLE_SCREEN_LAUNCHED_TRUE);
}



void menu_daemon_fini(void)
{
	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, _pkg_changed) < 0)
		_E("Failed to ignore the callback for package change event");

	if (vconf_ignore_key_changed(VCONFKEY_STARTER_SEQUENCE, _show_cb) < 0)
		_E("Failed to ignore the callback for show event");

	xmonitor_fini();
	pkg_event_fini();
	destroy_key_window();
}



// End of a file
