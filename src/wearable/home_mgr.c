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
#include <dd-deviced.h>

#include "util.h"
#include "process_mgr.h"
#include "dbus_util.h"
#include "status.h"

#define W_HOME_PKGNAME "org.tizen.w-home"
#define W_CLOCK_VIEWER_PKGNAME "org.tizen.w-clock-viewer"


static struct {
	char *home_appid;
	int home_pid;
} s_home_mgr = {
	.home_appid = W_HOME_PKGNAME,
	.home_pid = -1,
};



static void _after_launch_home(int pid)
{
	if (-1 == deviced_conf_set_mempolicy_bypid(pid, OOM_IGNORE)) {
		_E("Cannot set the memory policy for Homescreen(%d)", pid);
	} else {
		_E("Set the memory policy for Homescreen(%d)", pid);
	}
	s_home_mgr.home_pid = pid;
}



void home_mgr_launch_home(void)
{
	process_mgr_must_launch(s_home_mgr.home_appid, NULL, NULL, NULL, _after_launch_home);
}



void home_mgr_launch_home_first(void)
{
	process_mgr_must_launch(s_home_mgr.home_appid, "home_op", "first_boot", NULL, _after_launch_home);
}



void home_mgr_launch_home_by_power(void)
{
	process_mgr_must_launch(s_home_mgr.home_appid, "home_op", "powerkey", NULL, _after_launch_home);
}



static int _dead_cb(int pid, void *data)
{
	_D("_dead_cb is called(pid : %d)", pid);

	if (pid == s_home_mgr.home_pid) {
		_E("Home(%d) is destroyed.", pid);
		home_mgr_launch_home();
	}

	return 0;
}



static void _on_lcd_changed_receive(void *data, DBusMessage *msg)
{
	int lcd_off = dbus_message_is_signal(msg, DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_OFF);

	if (lcd_off) {
		_D("LCD off");

		int ambient_mode = status_passive_get()->setappl_ambient_mode_bool;
		_D("ambient mode : %d", ambient_mode);
		if (ambient_mode) {
			process_mgr_must_launch(W_CLOCK_VIEWER_PKGNAME, NULL, NULL, NULL, NULL);
		}
	}
}



void home_mgr_init(void)
{
	aul_listen_app_dead_signal(_dead_cb, NULL);

	/* register lcd changed cb */
	dbus_util_receive_lcd_status(_on_lcd_changed_receive, NULL);
}



void home_mgr_fini(void)
{
}



