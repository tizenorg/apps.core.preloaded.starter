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

#include <bundle.h>
#include <Elementary.h>
//#include <Ecore_X.h>
#include <Ecore_Input.h>
//#include <utilX.h>

#include <dd-display.h>
#include <feedback.h>
#include <vconf.h>

#include "hw_key.h"
#include "util.h"
#include "status.h"
#include "dbus_util.h"
#include "home_mgr.h"
#include "process_mgr.h"

#define GRAB_TWO_FINGERS 2
#define POWERKEY_TIMER_SEC 0.25
#define POWERKEY_LCDOFF_TIMER_SEC 0.4
#define LONG_PRESS_TIMER_SEC 0.7

#define APP_CONTROL_OPERATION_MAIN_KEY "__APP_SVC_OP_TYPE__"
#define APP_CONTROL_OPERATION_MAIN_VALUE "http://tizen.org/appcontrol/operation/main"

#define USE_DBUS_POWEROFF 1
#define W_TASKMGR_PKGNAME "org.tizen.w-taskmanager"



static struct {
	//Ecore_X_Window win;
	Ecore_Event_Handler *key_up;
	Ecore_Event_Handler *key_down;
	Ecore_Event_Handler *two_fingers_hold_hd;
	Ecore_Timer *power_long_press_timer;
	Ecore_Timer *power_release_timer;
	Eina_Bool is_lcd_on;
	Eina_Bool is_long_press;
	int powerkey_count;
	Eina_Bool is_cancel;
} key_info = {
	//.win = 0x0,
	.key_up = NULL,
	.key_down = NULL,
	.two_fingers_hold_hd = NULL,
	.power_long_press_timer = NULL,
	.power_release_timer = NULL,
	.is_lcd_on = EINA_FALSE,
	.is_long_press = EINA_FALSE,
	.powerkey_count = 0,
	.is_cancel = EINA_FALSE,
};



static Eina_Bool _powerkey_timer_cb(void *data)
{
	_W("%s, powerkey count[%d]", __func__, key_info.powerkey_count);

	key_info.power_release_timer = NULL;

	if (VCONFKEY_SETUP_WIZARD_LOCK == status_passive_get()->setup_wizard_state) {
		_E("setdup_wizard is running");
		key_info.powerkey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	}

	if (VCONFKEY_PM_KEY_LOCK == status_passive_get()->pm_key_ignore) {
		_E("Critical Low Batt Clock Mode");
		key_info.powerkey_count = 0;
		if(key_info.is_lcd_on) {
			_W("just turn off LCD");
			display_change_state(LCD_OFF);
		} else {
			_W("just turn on LCD by powerkey.. starter ignore powerkey operation");
		}
		return ECORE_CALLBACK_CANCEL;
	}

	if (key_info.powerkey_count % 2 == 0) {
		/* double press */
		_W("powerkey double press");
		key_info.powerkey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	}
	key_info.powerkey_count = 0;

	if (key_info.is_lcd_on) {
		if(VCONFKEY_PM_STATE_LCDOFF <= status_active_get()->pm_state) {
			_E("Already lcd state was changed while powerkey op. starter ignore powerkey operation");
			return ECORE_CALLBACK_CANCEL;
		}
	} else {
		_W("just turn on LCD by powerkey.. starter ignore powerkey operation");
		return ECORE_CALLBACK_CANCEL;
	}

	if (VCONFKEY_CALL_VOICE_ACTIVE == status_passive_get()->call_state) {
		_W("call state is [%d] -> just turn off LCD");
		display_change_state(LCD_OFF);
		return ECORE_CALLBACK_CANCEL;
	}

	if (VCONFKEY_IDLE_LOCK == status_passive_get()->idle_lock_state) {
		_W("lock state is [%d] -> just turn off LCD");
		display_change_state(LCD_OFF);
		return ECORE_CALLBACK_CANCEL;
	}

	if (0 < status_passive_get()->remote_lock_islocked) {
		_W("remote lock is on top (%d), -> just turn off LCD", status_passive_get()->remote_lock_islocked);
		display_change_state(LCD_OFF);
		return ECORE_CALLBACK_CANCEL;
	}

	home_mgr_launch_home_by_power();

	return ECORE_CALLBACK_CANCEL;
}


static Eina_Bool _long_press_timer_cb(void* data)
{
	key_info.power_long_press_timer = NULL;
	key_info.is_long_press = EINA_TRUE;
	key_info.powerkey_count = 0;

	if (0 < status_passive_get()->remote_lock_islocked){
		_W("remote lock is on top (%d), -> just turn off LCD", status_passive_get()->remote_lock_islocked);
		return ECORE_CALLBACK_CANCEL;
	}

	if (key_info.power_release_timer) {
		ecore_timer_del(key_info.power_release_timer);
		key_info.power_release_timer = NULL;
		_D("delete power_release_timer");
	}

#if USE_DBUS_POWEROFF
	dbus_util_send_poweroff_signal();
#else
	_D("launch power off syspopup");
	process_mgr_syspopup_launch("poweroff-syspopup", NULL, NULL, NULL, NULL);
#endif

	feedback_initialize();
	feedback_play_type(FEEDBACK_TYPE_VIBRATION, FEEDBACK_PATTERN_HOLD);
	feedback_deinitialize();

	return ECORE_CALLBACK_CANCEL;
}


#if 0
static Eina_Bool _key_release_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Up *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_release_cb : %s Released", ev->keyname);

	if (!strcmp(ev->keyname, KEY_POWER)) {
		_W("POWER Key is released");

		if(key_info.power_long_press_timer) {
			ecore_timer_del(key_info.power_long_press_timer);
			key_info.power_long_press_timer = NULL;
			_D("delete long press timer");
		}

		// Check powerkey timer
		if(key_info.power_release_timer) {
			ecore_timer_del(key_info.power_release_timer);
			key_info.power_release_timer = NULL;
			_D("delete powerkey timer");
		}

		// Cancel key operation
		if (EINA_TRUE == key_info.is_cancel) {
			_D("Cancel key is activated");
			key_info.is_cancel = EINA_FALSE;
			key_info.powerkey_count = 0; //initialize powerkey count
			return ECORE_CALLBACK_RENEW;
		}

		// Check long press operation
		if(key_info.is_long_press) {
			_D("ignore power key release by long poress");
			key_info.is_long_press = EINA_FALSE;
			return ECORE_CALLBACK_RENEW;
		}

		if(key_info.is_lcd_on) {
			key_info.power_release_timer = ecore_timer_add(POWERKEY_TIMER_SEC, _powerkey_timer_cb, NULL);
		} else {
			_D("lcd off --> [%f]sec timer", POWERKEY_LCDOFF_TIMER_SEC);
			key_info.power_release_timer = ecore_timer_add(POWERKEY_LCDOFF_TIMER_SEC, _powerkey_timer_cb, NULL);
		}
		if (!key_info.power_release_timer) {
			_E("Critical, cannot add a timer for powerkey");
		}
	} else if (!strcmp(ev->keyname, KEY_CANCEL)) {
		_D("CANCEL Key is released");
		key_info.is_cancel = EINA_FALSE;
	}

	return ECORE_CALLBACK_RENEW;
}
#endif



#if 0
static Eina_Bool _key_press_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Down *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_press_cb : %s Pressed", ev->keyname);

	if (!strcmp(ev->keyname, KEY_POWER)) {
		_W("POWER Key is pressed");

		/**
		 * lcd status
		 * 1 : lcd normal
		 * 2 : lcd dim
		 * 3 : lcd off
		 * 4 : suspend
		 */
		if (VCONFKEY_PM_STATE_LCDDIM >= status_active_get()->pm_state) {
			key_info.is_lcd_on = EINA_TRUE;
		} else if (VCONFKEY_PM_STATE_LCDOFF <= status_active_get()->pm_state) {
			key_info.is_lcd_on = EINA_FALSE;
		}

		key_info.powerkey_count++;
		_W("powerkey count : %d", key_info.powerkey_count);

		if(key_info.power_release_timer) {
			ecore_timer_del(key_info.power_release_timer);
			key_info.power_release_timer = NULL;
		}

		if (key_info.power_long_press_timer) {
			ecore_timer_del(key_info.power_long_press_timer);
			key_info.power_long_press_timer = NULL;
		}

		key_info.is_long_press = EINA_FALSE;
		key_info.power_long_press_timer = ecore_timer_add(LONG_PRESS_TIMER_SEC, _long_press_timer_cb, NULL);
		if(!key_info.power_long_press_timer) {
			_E("Failed to add power_long_press_timer");
		}
	} else if (!strcmp(ev->keyname, KEY_CANCEL)) {
		_D("CANCEL key is pressed");
		key_info.is_cancel = EINA_TRUE;
	}

	return ECORE_CALLBACK_RENEW;
}
#endif



#if 0
static Eina_Bool _w_gesture_hold_cb(void *data, int ev_type, void *ev)
{
	Ecore_X_Event_Gesture_Notify_Hold *e = ev;

	if (VCONFKEY_PM_KEY_LOCK == status_passive_get()->pm_key_ignore) {
		_E("Critical Low Batt Clock Mode, ignore gesture");
		return ECORE_CALLBACK_RENEW;
	}

	if (SETTING_PSMODE_WEARABLE_ENHANCED == status_passive_get()->setappl_psmode) {
		_E("UPS Mode, ignore gesture");
		return ECORE_CALLBACK_RENEW;
	}

	if(e->num_fingers == GRAB_TWO_FINGERS) {
		_D("subtype[%d]: hold[%d]\n", e->subtype, e->hold_time);
		if (e->subtype == ECORE_X_GESTURE_BEGIN) {
			_D("Begin : launch task mgr..!!");
			dbus_util_send_cpu_booster_signal();
			process_mgr_must_launch(W_TASKMGR_PKGNAME, APP_CONTROL_OPERATION_MAIN_KEY, APP_CONTROL_OPERATION_MAIN_VALUE, NULL, NULL);
		}
	}

	return ECORE_CALLBACK_RENEW;
}
#endif



void hw_key_create_window(void)
{
#if 0 //build error
	int status = -1;
	int ret = -1;

	_W("hw_key_create_window");

	key_info.win = ecore_x_window_input_new(0, 0, 0, 1, 1);
	if (!key_info.win) {
		_E("Failed to create hidden window");
		return;
	}
	ecore_x_event_mask_unset(key_info.win, ECORE_X_EVENT_MASK_NONE);
	ecore_x_icccm_title_set(key_info.win, "w_starter,key,receiver");
	ecore_x_netwm_name_set(key_info.win, "w_starter,key,receiver");
	ecore_x_netwm_pid_set(key_info.win, getpid());

	ret = utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_POWER, SHARED_GRAB);
	if (ret != 0) {
		_E("utilx_grab_key KEY_POWER GrabSHARED_GRAB failed, ret[%d]", ret);
	}

	key_info.key_up = ecore_event_handler_add(ECORE_EVENT_KEY_UP, _key_release_cb, NULL);
	if (!key_info.key_up) {
		_E("Failed to register a key up event handler");
	}

	key_info.key_down = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, _key_press_cb, NULL);
	if (!key_info.key_down) {
		_E("Failed to register a key down event handler");
	}

	status = ecore_x_gesture_event_grab(key_info.win, ECORE_X_GESTURE_EVENT_HOLD, GRAB_TWO_FINGERS);
	_E("ECORE_X_GESTURE_EVENT_HOLD Grab(%d fingers) status[%d]\n", GRAB_TWO_FINGERS, status);

	key_info.two_fingers_hold_hd = ecore_event_handler_add(ECORE_X_EVENT_GESTURE_NOTIFY_HOLD, _w_gesture_hold_cb, NULL);
	if (!key_info.two_fingers_hold_hd) {
		_E("Failed to register handler : ECORE_X_EVENT_GESTURE_NOTIFY_TAPNHOLD\n");
	}
#endif
}



void hw_key_destroy_window(void)
{
#if 0 //build error
	int status;

	if (key_info.two_fingers_hold_hd) {
		ecore_event_handler_del(key_info.two_fingers_hold_hd);
		key_info.two_fingers_hold_hd = NULL;
	}

	status = ecore_x_gesture_event_ungrab(key_info.win, ECORE_X_GESTURE_EVENT_HOLD, GRAB_TWO_FINGERS);
	if (!status) {
		_E("ECORE_X_GESTURE_EVENT_HOLD UnGrab(%d fingers) failed, status[%d]\n", GRAB_TWO_FINGERS, status);
	}

	if (key_info.key_up) {
		ecore_event_handler_del(key_info.key_up);
		key_info.key_up = NULL;
	}

	if (key_info.key_down) {
		ecore_event_handler_del(key_info.key_down);
		key_info.key_down = NULL;
	}

	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_POWER);

	ecore_x_window_delete_request_send(key_info.win);
	key_info.win = 0x0;
#endif
}



// End of a file
