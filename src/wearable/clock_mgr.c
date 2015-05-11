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
#include <Elementary.h>
#include <vconf.h>
#include <dd-display.h>

#include "dbus_util.h"
#include "util.h"
#include "status.h"
#include "process_mgr.h"

#define PM_UNLOCK_TIMER_SEC 0.3



static struct {
	Eina_List *reserved_apps_list;
	char *reserved_popup_app_id;
} s_clock_mgr = {
	.reserved_apps_list = NULL,
	.reserved_popup_app_id = NULL,
};



static int _check_reserved_popup_status(void)
{
	int val = 0;
	int tmp = 0;

	val = status_passive_get()->starter_reserved_apps_status;
	tmp = val & 0x10;
	if(tmp == 0x10){
		if(aul_app_is_running(s_clock_mgr.reserved_popup_app_id) == 1){
			return TRUE;
		} else{
			_E("%s is not running now.", s_clock_mgr.reserved_popup_app_id);
			s_clock_mgr.reserved_popup_app_id = NULL;
			val = val ^ 0x10;
			vconf_set_int(VCONFKEY_STARTER_RESERVED_APPS_STATUS, val);
			_W("now reserved apps status %x", val);
			return FALSE;
		}
	}

	return FALSE;
}



static int _check_reserved_apps_status(void)
{
	int val = 0;

	val = status_passive_get()->starter_reserved_apps_status;
	_W("Current reserved apps status : %x", val);

	if(val > 0){
		return TRUE;
	}

	return FALSE;
}



static Eina_Bool _pm_unlock_timer_cb(void *data){
	/* PM_SLEEP_MARGIN : If the current status is lcd off, deviced reset timer to 1 second. */
	display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
	return ECORE_CALLBACK_CANCEL;
}



static void _on_lcd_changed_receive(void *data, DBusMessage *msg)
{
	int lcd_off = -1;
	int setup_wizard_state = -1;
	int count = 0;
	char *info = NULL;
	char *lcd_off_source = NULL;
	Eina_List *l = NULL;

	lcd_off = dbus_message_is_signal(msg, DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_OFF);

	if (lcd_off) {
		/**
		 * lcd off source
		 * string : 'powerkey' or 'timeout' or 'event' or 'unknown'
		*/
		lcd_off_source = dbus_util_msg_arg_get_str(msg);
		ret_if(!lcd_off_source);

		_D("lcd off source : %s", lcd_off_source);
		free(lcd_off_source);

		if (vconf_get_int(VCONFKEY_SETUP_WIZARD_STATE, &setup_wizard_state) < 0) {
			_E("Failed to get [%s]", VCONFKEY_SETUP_WIZARD_STATE);
		} else {
			if (setup_wizard_state == VCONFKEY_SETUP_WIZARD_LOCK) {
				_E("starter doesn't react for this case, setup wizard state is [%d]", setup_wizard_state);
				return;
			}
		}

		if(_check_reserved_popup_status() > 0){
			_W("reserved popup is on top. do nothing");
			return;
		}

		if (_check_reserved_apps_status() <= 0) {
			_W("reserved app is not running now.");
			return;
		}

		EINA_LIST_FOREACH(s_clock_mgr.reserved_apps_list, l, info){
			if(aul_app_is_running(info) == 1){
				// STAY_CUR_STATE : State is not changed directly and phone stay current state until timeout expired.
				display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
				process_mgr_must_open(info, NULL, NULL);
				ecore_timer_add(PM_UNLOCK_TIMER_SEC, _pm_unlock_timer_cb, NULL);
				break;
			} else{
				_W("%s is not running now", info);
				s_clock_mgr.reserved_apps_list = eina_list_remove_list(s_clock_mgr.reserved_apps_list, l);
				continue;
			}
		}

		count = eina_list_count(s_clock_mgr.reserved_apps_list);
		if(count == 0){
			_W("there is no reserved app.");
			vconf_set_int(VCONFKEY_STARTER_RESERVED_APPS_STATUS, 0);
		}
	}

}



void clock_mgr_init(void)
{
	_W("clock_mgr_init");

	dbus_util_receive_lcd_status(_on_lcd_changed_receive, NULL);
}



void clock_mgr_fini(void)
{
}



// End of a file
