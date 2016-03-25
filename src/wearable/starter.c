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
#include <errno.h>
#include <vconf.h>
#include <signal.h>
#include <dd-deviced.h>
#include <Ecore_Wayland.h>

#include "hw_key.h"
#include "util.h"
#include "hourly_alert.h"
#include "dbus_util.h"
#include "clock_mgr.h"
#include "status.h"
#include "home_mgr.h"
#include "process_mgr.h"

int errno;



static struct {
	int lcd_status;
} s_starter = {
	.lcd_status = -1,
};



static void _signal_handler(int signum, siginfo_t *info, void *unused)
{
    _D("_signal_handler : Terminated...");
    elm_exit();
}



static int _power_off_cb(status_active_key_e key, void *data)
{
	int val = status_active_get()->sysman_power_off_status;

	if (val > VCONFKEY_SYSMAN_POWER_OFF_POPUP) {
		_E("power off status : %d", val);
	    elm_exit();
	}

	return 1;
}



static int _change_language_cb(status_active_key_e key, void *data)
{
	_D("%s, %d", __func__, __LINE__);

	if (status_active_get()->langset) {
		elm_language_set(status_active_get()->langset);
	}

	return 1;
}



static int _change_sequence_cb(status_active_key_e key, void *data)
{
	int seq = status_active_get()->starter_sequence;

	if (seq == 1) {
		home_mgr_launch_home_first();
	}

	return 1;
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
		s_starter.lcd_status = 1;
	} else if(lcd_off) {
		_W("LCD off");
		s_starter.lcd_status = 0;
	} else {
		_E("%s dbus_message_is_signal error", DEVICED_INTERFACE_DISPLAY);
	}
}



static void _init(void)
{
	struct sigaction act;
	char err_buf[128] = {0,};

	memset(&act,0x00,sizeof(struct sigaction));
	act.sa_sigaction = _signal_handler;
	act.sa_flags = SA_SIGINFO;

	int ret = sigemptyset(&act.sa_mask);
	if (ret < 0) {
		strerror_r(errno, err_buf, sizeof(err_buf));
		_E("Failed to sigemptyset[%d / %s]", errno, err_buf);
	}
	ret = sigaddset(&act.sa_mask, SIGTERM);
	if (ret < 0) {
		strerror_r(errno, err_buf, sizeof(err_buf));
		_E("Failed to sigaddset[%d / %s]", errno, err_buf);
	}
	ret = sigaction(SIGTERM, &act, NULL);
	if (ret < 0) {
		strerror_r(errno, err_buf, sizeof(err_buf));
		_E("Failed to sigaction[%d / %s]", errno, err_buf);
	}

	status_register();
	status_active_register_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb, NULL);
	status_active_register_cb(STATUS_ACTIVE_KEY_LANGSET, _change_language_cb, NULL);

	home_mgr_launch_home();

	dbus_util_receive_lcd_status(_on_lcd_changed_receive, NULL);

	home_mgr_init();
	clock_mgr_init();
	hourly_alert_init();
	hw_key_create_window();
}



static void _fini(void)
{
	hw_key_destroy_window();
	hourly_alert_fini();
	clock_mgr_fini();
	home_mgr_fini();

	status_active_unregister_cb(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, _power_off_cb);
	status_active_unregister_cb(STATUS_ACTIVE_KEY_LANGSET, _change_language_cb);
	status_active_unregister_cb(STATUS_ACTIVE_KEY_STARTER_SEQUENCE, _change_sequence_cb);
	status_unregister();
}



int main(int argc, char *argv[])
{
	int ret = 0;

	_D("starter is launched..!!");

	ret = elm_init(argc, argv);
	if (ret != 1) {
		_E("elm_init() failed : %d", ret);
		return -1;
	}

	ret = ecore_wl_init(NULL);
	if (ret == 0) {
		_E("ecore_wl_init() failed : %d", ret);
		elm_shutdown();
		return -1;
	}

	_init();

	elm_run();

	_fini();
	elm_shutdown();

	return 0;
}
