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

#ifdef HAVE_X11

#include <app.h>
#include <bundle.h>
#include <Elementary.h>
#include <Ecore_X.h>
#include <Ecore_Input.h>
#include <dd-deviced.h>
#include <syspopup_caller.h>
#include <utilX.h>
#include <vconf.h>
#include <system/media_key.h>
#include <aul.h>
#include <feedback.h>
#include <system_settings.h>

#include "hw_key.h"
#include "home_mgr.h"
#include "util.h"
#include "dbus_util.h"
#include "lock_mgr.h"
#include "status.h"
#include "process_mgr.h"
#include "lock_pwd_util.h"

#define APPID_CAMERA "org.tizen.camera-app"
#define APPID_CALLLOG "org.tizen.calllog"
#define APPID_MUSIC_PLAYER "org.tizen.music-player"
#define APPID_TASKMGR "org.tizen.task-mgr"
#define APPID_BROWSER "org.tizen.browser"
#define APPID_EMAIL "org.tizen.email"
#define APPID_DIALER "org.tizen.phone"

#define STR_ATOM_XKEY_COMPOSITION "_XKEY_COMPOSITION"
#define STR_ATOM_KEYROUTER_NOTIWINDOW "_KEYROUTER_NOTIWINDOW"

#define LONG_PRESS_TIMER_SEC 0.4
#define HOMEKEY_TIMER_SEC 0.2
#define CANCEL_KEY_TIMER_SEC 0.3

static struct {
	Ecore_X_Window win;
	Ecore_Event_Handler *key_up;
	Ecore_Event_Handler *key_down;
	Ecore_Timer *home_long_press_timer;
	Ecore_Timer *home_multi_press_timer;
	Eina_Bool cancel;
	Ecore_X_Window keyrouter_notiwindow;
	int homekey_count;
} key_info = {
	.win = 0x0,
	.key_up = NULL,
	.key_down = NULL,
	.home_long_press_timer = NULL,
	.home_multi_press_timer = NULL,
	.cancel = EINA_FALSE,
	.keyrouter_notiwindow = 0x0,
	.homekey_count = 0,
};



static void _after_launch_taskmgr(int pid)
{
	if(0 < pid) {
		if(dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_DEFAULT) < 0){
			_E("failed to send oom dbus signal");
		}
	}
}



static Eina_Bool _launch_taskmgr_cb(void* data)
{
	int val = -1;

	_D("Launch TASKMGR");

	key_info.home_long_press_timer = NULL;

	if (vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &val) < 0) {
		_E("Cannot get VCONFKEY for lock state");
	} else if (VCONFKEY_IDLE_LOCK == val) {
		_E("lock state, ignore home key long press..!!");
		return ECORE_CALLBACK_CANCEL;
	}

	process_mgr_must_launch(APPID_TASKMGR, NULL, NULL, NULL, _after_launch_taskmgr);

	return ECORE_CALLBACK_CANCEL;
}



static void _release_multimedia_key(const char *value)
{
	ret_if(NULL == value);
	_D("Multimedia key is released with %s", value);
	process_mgr_must_launch(APPID_MUSIC_PLAYER, "multimedia_key", value, NULL, NULL);
}



static Eina_Bool _launch_by_home_key(void *data)
{
	int ret = 0;

	if (status_passive_get()->idle_lock_state > VCONFKEY_IDLE_UNLOCK) {
		return ECORE_CALLBACK_CANCEL;
	}

	ret = home_mgr_open_home(NULL);
	if(ret > 0) {
		dbus_util_send_home_raise_signal();
	}

	return ECORE_CALLBACK_CANCEL;
}



static Eina_Bool _home_multi_press_timer_cb(void *data)
{
	_W("homekey count[%d]", key_info.homekey_count);

	key_info.home_multi_press_timer = NULL;

	if(0 == key_info.homekey_count % 2) {
		key_info.homekey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	} else if(key_info.homekey_count >= 3) {
		key_info.homekey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	}

	/* Single homekey operation */
	key_info.homekey_count = 0;
	_launch_by_home_key(data);

	return ECORE_CALLBACK_CANCEL;

}



#define SERVICE_OPERATION_POPUP_SEARCH "http://samsung.com/appcontrol/operation/search"
#define SEARCH_PKG_NAME "org.tizen.sfinder"
static int _launch_search(void)
{
	app_control_h app_control;
	int ret = APP_CONTROL_ERROR_NONE;

	app_control_create(&app_control);
	app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);
	app_control_set_app_id(app_control, SEARCH_PKG_NAME);

	ret = app_control_send_launch_request(app_control, NULL, NULL);

	if(ret != APP_CONTROL_ERROR_NONE) {
		_E("Cannot launch search!! err[%d]", ret);
	}

	app_control_destroy(app_control);
	return ret;
}



static void _cancel_key_events(void)
{
	key_info.homekey_count = 0;

	if (key_info.home_long_press_timer) {
		ecore_timer_del(key_info.home_long_press_timer);
		key_info.home_long_press_timer = NULL;
	}

	if(key_info.home_multi_press_timer) {
		ecore_timer_del(key_info.home_multi_press_timer);
		key_info.home_multi_press_timer = NULL;
	}
}



static Eina_Bool _key_release_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Up *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_release_cb : %s Released", ev->keyname);

	/* Priority 1 : Cancel event */
	if (!strcmp(ev->keyname, KEY_CANCEL)) {
		_D("CANCEL Key is released");
		key_info.cancel = EINA_FALSE;
		return ECORE_CALLBACK_RENEW;
	}

	if (EINA_TRUE == key_info.cancel) {
		_D("CANCEL is on");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 2 : Execute before checking the lock status */
	if (!strcmp(ev->keyname, KEY_MEDIA)) {
		_release_multimedia_key("KEY_PLAYCD");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 3 : Check the lock status */
	if ((status_passive_get()->idle_lock_state  == VCONFKEY_IDLE_LOCK)
		&& (status_active_get()->setappl_screen_lock_type_int > SETTING_SCREEN_LOCK_TYPE_NONE)) {
		if (!strcmp(ev->keyname, KEY_BACK)) {
			_D("Back key is released");
			lock_pwd_util_back_key_relased();
		} else {
			_D("phone lock state, ignore home key.");
		}
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 4 : These keys are only activated after checking the lock state */
	if (!strcmp(ev->keyname, KEY_END)) {
	} else if (!strcmp(ev->keyname, KEY_CONFIG)) {
	} else if (!strcmp(ev->keyname, KEY_SEND)) {
	} else if (!strcmp(ev->keyname, KEY_HOME)) {
		_W("Home Key is released");

		syspopup_destroy_all();

		if(key_info.home_multi_press_timer) {
			_D("delete homekey timer");
			ecore_timer_del(key_info.home_multi_press_timer);
			key_info.home_multi_press_timer = NULL;
		}

		if (key_info.home_long_press_timer) {
			ecore_timer_del(key_info.home_long_press_timer);
			key_info.home_long_press_timer = NULL;
		} else {
			key_info.homekey_count = 0;
			return ECORE_CALLBACK_RENEW;
		}

		key_info.home_multi_press_timer = ecore_timer_add(HOMEKEY_TIMER_SEC, _home_multi_press_timer_cb, NULL);
		if (!key_info.home_multi_press_timer) {
			_E("Critical! cannot add a timer for home multi press");
		}
		return ECORE_CALLBACK_RENEW;
	} else if (!strcmp(ev->keyname, KEY_PAUSE)) {
	} else if (!strcmp(ev->keyname, KEY_APPS)) {
		_D("App tray key is released");
	} else if (!strcmp(ev->keyname, KEY_TASKSWITCH)) {
		_D("Task switch key is released");
		_launch_taskmgr_cb(NULL);
	} else if (!strcmp(ev->keyname, KEY_WEBPAGE)) {
		_D("Web page key is released");
		process_mgr_must_open(APPID_BROWSER, NULL, NULL);
	} else if (!strcmp(ev->keyname, KEY_MAIL)) {
		_D("Mail key is released");
		process_mgr_must_open(APPID_EMAIL, NULL, NULL);
	} else if (!strcmp(ev->keyname, KEY_CONNECT)) {
		_D("Connect key is released");
		process_mgr_must_open(APPID_DIALER, NULL, NULL);
	} else if (!strcmp(ev->keyname, KEY_SEARCH)) {
		_D("Search key is released");
		if (_launch_search() < 0) {
			_E("Failed to launch the search");
		}
	} else if (!strcmp(ev->keyname, KEY_VOICE)) {
		_D("Voice key is released");
	}

	return ECORE_CALLBACK_RENEW;
}



static Eina_Bool _key_press_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Down *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_press_cb : %s Pressed", ev->keyname);

	/* Priority 1 : Cancel */
	/*              every reserved events have to be canceld when cancel key is pressed */
	if (!strcmp(ev->keyname, KEY_CANCEL)) {
		_D("Cancel button is pressed");
		key_info.cancel = EINA_TRUE;
		_cancel_key_events();
		return ECORE_CALLBACK_RENEW;
	}

	if (EINA_TRUE == key_info.cancel) {
		_D("CANCEL is on");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 2 : Check the lock status */
	if ((status_passive_get()->idle_lock_state == VCONFKEY_IDLE_LOCK)
		&& (status_active_get()->setappl_screen_lock_type_int > SETTING_SCREEN_LOCK_TYPE_NONE)) {
		_D("phone lock state, ignore key events.");
		_cancel_key_events();
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 3 : other keys */
	if (!strcmp(ev->keyname, KEY_SEND)) {
		_D("Launch calllog");
		process_mgr_must_open(APPID_CALLLOG, NULL, NULL);
	} else if(!strcmp(ev->keyname, KEY_CONFIG)) {
		_D("Launch camera");
		process_mgr_must_open(APPID_CAMERA, NULL, NULL);
	} else if (!strcmp(ev->keyname, KEY_HOME)) {
		_W("Home Key is pressed");
		if (key_info.home_long_press_timer) {
			ecore_timer_del(key_info.home_long_press_timer);
			key_info.home_long_press_timer = NULL;
		}

		key_info.homekey_count++;
		_W("homekey count : %d", key_info.homekey_count);

		if(key_info.home_multi_press_timer) {
			ecore_timer_del(key_info.home_multi_press_timer);
			key_info.home_multi_press_timer = NULL;
			_D("delete homekey timer");
		}

		_D("create long press timer");
		key_info.home_long_press_timer = ecore_timer_add(LONG_PRESS_TIMER_SEC, _launch_taskmgr_cb, NULL);
		if (!key_info.home_long_press_timer) {
			_E("Failed to add timer for long press detection");
		}
	} else if (!strcmp(ev->keyname, KEY_MEDIA)) {
		_D("Media key is pressed");
	} else if (!strcmp(ev->keyname, KEY_APPS)) {
		_D("App tray key is pressed");
	} else if (!strcmp(ev->keyname, KEY_TASKSWITCH)) {
		_D("Task switch key is pressed");
	} else if (!strcmp(ev->keyname, KEY_WEBPAGE)) {
		_D("Web page key is pressed");
	} else if (!strcmp(ev->keyname, KEY_MAIL)) {
		_D("Mail key is pressed");
	} else if (!strcmp(ev->keyname, KEY_SEARCH)) {
		_D("Search key is pressed");
	} else if (!strcmp(ev->keyname, KEY_VOICE)) {
		_D("Voice key is pressed");
	} else if (!strcmp(ev->keyname, KEY_CONNECT)) {
		_D("Connect key is pressed");
	}

	return ECORE_CALLBACK_RENEW;
}



void _media_key_event_cb(media_key_e key, media_key_event_e status, void *user_data)
{
	_D("MEDIA KEY EVENT : %d", key);
	if (MEDIA_KEY_STATUS_PRESSED == status) return;

	switch (key) {
	case MEDIA_KEY_PAUSE:
		_release_multimedia_key("KEY_PAUSECD");
		break;
	case MEDIA_KEY_PLAY:
		_release_multimedia_key("KEY_PLAYCD");
		break;
	case MEDIA_KEY_PLAYPAUSE:
		_release_multimedia_key("KEY_PLAYPAUSECD");
		break;
	default:
		_E("cannot reach here, key[%d]", key);
		break;
	}
}



void hw_key_create_window(void)
{
	int ret;
	Ecore_X_Atom atomNotiWindow;
	Ecore_X_Window keyrouter_notiwindow;

	key_info.win = ecore_x_window_input_new(0, 0, 0, 1, 1);
	if (!key_info.win) {
		_D("Failed to create hidden window");
		return;
	}
	ecore_x_event_mask_unset(key_info.win, ECORE_X_EVENT_MASK_NONE);
	ecore_x_icccm_title_set(key_info.win, "menudaemon,key,receiver");
	ecore_x_netwm_name_set(key_info.win, "menudaemon,key,receiver");
	ecore_x_netwm_pid_set(key_info.win, getpid());

	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_HOME, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_VOLUMEDOWN, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_VOLUMEUP, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_CONFIG, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_MEDIA, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_APPS, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_TASKSWITCH, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_WEBPAGE, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_MAIL, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_SEARCH, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_VOICE, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_CONNECT, SHARED_GRAB);
	utilx_grab_key(ecore_x_display_get(), key_info.win, KEY_POWER, SHARED_GRAB);

	key_info.key_up = ecore_event_handler_add(ECORE_EVENT_KEY_UP, _key_release_cb, NULL);
	if (!key_info.key_up)
		_E("Failed to register a key up event handler");

	key_info.key_down = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, _key_press_cb, NULL);
	if (!key_info.key_down)
		_E("Failed to register a key down event handler");

	/* Get notifwindow */
	atomNotiWindow = ecore_x_atom_get(STR_ATOM_KEYROUTER_NOTIWINDOW);
	ret = ecore_x_window_prop_window_get(ecore_x_window_root_first_get(), atomNotiWindow, &keyrouter_notiwindow, 1);
	if (ret > 0) {
		_D("Succeed to get keyrouter notiwindow ! ret = %d (win=0x%x)\n"
				, ret, keyrouter_notiwindow);
		ecore_x_window_sniff(keyrouter_notiwindow);
		key_info.keyrouter_notiwindow = keyrouter_notiwindow;
	} else {
		_E("Failed to get keyrouter notiwindow! ret = %d, atomNotiWindow = 0x%x, keyrouter_notiwindow = 0x%x"
				, ret, atomNotiWindow, keyrouter_notiwindow);
	}

	media_key_reserve(_media_key_event_cb, NULL);
}



void hw_key_destroy_window(void)
{
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_HOME);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_VOLUMEDOWN);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_VOLUMEUP);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_CONFIG);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_MEDIA);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_APPS);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_TASKSWITCH);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_WEBPAGE);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_MAIL);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_SEARCH);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_VOICE);
	utilx_ungrab_key(ecore_x_display_get(), key_info.win, KEY_CONNECT);

	if (key_info.key_up) {
		ecore_event_handler_del(key_info.key_up);
		key_info.key_up = NULL;
	}

	if (key_info.key_down) {
		ecore_event_handler_del(key_info.key_down);
		key_info.key_down = NULL;
	}

	ecore_x_window_delete_request_send(key_info.win);
	key_info.win = 0x0;

	media_key_release();
}

#elif HAVE_WAYLAND

#include <app.h>
#include <bundle.h>
#include <Elementary.h>
#include <Ecore.h>
#include <Ecore_Wayland.h>
#include <Ecore_Input.h>
#include <dd-deviced.h>
#include <syspopup_caller.h>
#include <vconf.h>
#include <system/media_key.h>
#include <aul.h>
#include <feedback.h>
#include <system_settings.h>

#include "hw_key.h"
#include "home_mgr.h"
#include "util.h"
#include "dbus_util.h"
#include "lock_mgr.h"
#include "status.h"
#include "process_mgr.h"
#include "lock_pwd_util.h"

#define APPID_CAMERA "org.tizen.camera-app"
#define APPID_CALLLOG "org.tizen.calllog"
#define APPID_MUSIC_PLAYER "org.tizen.music-player"
#define APPID_TASKMGR "org.tizen.task-mgr"
#define APPID_BROWSER "org.tizen.browser"
#define APPID_EMAIL "org.tizen.email"
#define APPID_DIALER "org.tizen.phone"

#define STR_ATOM_KEYROUTER_NOTIWINDOW "_KEYROUTER_NOTIWINDOW"

#define LONG_PRESS_TIMER_SEC 0.4
#define HOMEKEY_TIMER_SEC 0.2
#define CANCEL_KEY_TIMER_SEC 0.3


const char *key_name[KEY_NAME_MAX] = {
	"XF86AudioRaiseVolume",
	"XF86AudioLowerVolume",
	"XF86PowerOff",
	"XF86Menu",
	"XF86Home",
	"XF86Back",
	"XF86Camera",
	"XF86Camera_Full",
	"XF86Search",
	"XF86AudioPlay",
	"XF86AudioPause",
	"XF86AudioStop",
	"XF86AudioNext",
	"XF86AudioPrev",
	"XF86AudioRewind",
	"XF86AudioForward",
	"XF86AudioMedia",
	"XF86AudioPlayPause",
	"XF86AudioMute",
	"XF86AudioRecord",
	"Cancel",
	"XF86SoftKBD",
	"XF86QuickPanel",
	"XF86TaskPane",
	"XF86HomePage",
	"XF86WWW",
	"XF86Mail",
	"XF86ScreenSaver",
	"XF86MonBrightnessDown",
	"XF86MonBrightnessUp",
	"XF86Voice",
	"Hangul",
	"XF86Apps",
	"XF86Call",
	"XF86Game",
	"XF86VoiceWakeUp_LPSD",
	"XF86VoiceWakeUp",
	"KEY_NAME_MAX",
};


static struct {
	Ecore_Event_Handler *key_up;
	Ecore_Event_Handler *key_down;
	Ecore_Timer *home_long_press_timer;
	Ecore_Timer *home_multi_press_timer;
	Ecore_Timer *keygrab_timer;
	Eina_Bool cancel;
	int homekey_count;
} key_info = {
	.key_up = NULL,
	.key_down = NULL,
	.home_long_press_timer = NULL,
	.home_multi_press_timer = NULL,
	.keygrab_timer = NULL,
	.cancel = EINA_FALSE,
	.homekey_count = 0,
};




static void _cancel_key_events(void)
{
	key_info.homekey_count = 0;

	if (key_info.home_long_press_timer) {
		ecore_timer_del(key_info.home_long_press_timer);
		key_info.home_long_press_timer = NULL;
	}

	if(key_info.home_multi_press_timer) {
		ecore_timer_del(key_info.home_multi_press_timer);
		key_info.home_multi_press_timer = NULL;
	}
}



#define SERVICE_OPERATION_POPUP_SEARCH "http://samsung.com/appcontrol/operation/search"
#define SEARCH_PKG_NAME "org.tizen.sfinder"
static int _launch_search(void)
{
	app_control_h app_control;
	int ret = APP_CONTROL_ERROR_NONE;

	app_control_create(&app_control);
	app_control_set_operation(app_control, APP_CONTROL_OPERATION_DEFAULT);
	app_control_set_app_id(app_control, SEARCH_PKG_NAME);

	ret = app_control_send_launch_request(app_control, NULL, NULL);

	if(ret != APP_CONTROL_ERROR_NONE) {
		_E("Cannot launch search!! err[%d]", ret);
	}

	app_control_destroy(app_control);
	return ret;
}



static void _after_launch_taskmgr(int pid)
{
	if(0 < pid) {
		if(dbus_util_send_oomadj(pid, OOM_ADJ_VALUE_DEFAULT) < 0){
			_E("failed to send oom dbus signal");
		}
	}
}



static Eina_Bool _launch_taskmgr_cb(void* data)
{
	int val = -1;

	_D("Launch TASKMGR");

	key_info.home_long_press_timer = NULL;

	if (vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &val) < 0) {
		_E("Cannot get VCONFKEY for lock state");
	} else if (VCONFKEY_IDLE_LOCK == val) {
		_E("lock state, ignore home key long press..!!");
		return ECORE_CALLBACK_CANCEL;
	}

	process_mgr_must_launch(APPID_TASKMGR, NULL, NULL, NULL, _after_launch_taskmgr);

	return ECORE_CALLBACK_CANCEL;
}



static Eina_Bool _launch_by_home_key(void *data)
{
	int ret = 0;

	if (status_passive_get()->idle_lock_state > VCONFKEY_IDLE_UNLOCK) {
		return ECORE_CALLBACK_CANCEL;
	}

	ret = home_mgr_open_home(NULL);
	if(ret > 0) {
		dbus_util_send_home_raise_signal();
	}

	return ECORE_CALLBACK_CANCEL;
}



static Eina_Bool _home_multi_press_timer_cb(void *data)
{
	_W("homekey count[%d]", key_info.homekey_count);

	key_info.home_multi_press_timer = NULL;

	if(0 == key_info.homekey_count % 2) {
		key_info.homekey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	} else if(key_info.homekey_count >= 3) {
		key_info.homekey_count = 0;
		return ECORE_CALLBACK_CANCEL;
	}

	/* Single homekey operation */
	key_info.homekey_count = 0;
	_launch_by_home_key(data);

	return ECORE_CALLBACK_CANCEL;

}



static void _release_multimedia_key(const char *value)
{
	ret_if(NULL == value);
	_D("Multimedia key is released with %s", value);
	process_mgr_must_launch(APPID_MUSIC_PLAYER, "multimedia_key", value, NULL, NULL);
}



static Eina_Bool _key_release_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Up *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_release_cb : %s Released", ev->keyname);

	/* Priority 1 : Cancel event */
	if (!strcmp(ev->keyname, key_name[KEY_CANCEL])) {
		_D("CANCEL Key is released");
		key_info.cancel = EINA_FALSE;
		return ECORE_CALLBACK_RENEW;
	}

	if (EINA_TRUE == key_info.cancel) {
		_D("CANCEL is on");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 2 : Execute before checking the lock status */
	if (!strcmp(ev->keyname, key_name[KEY_MEDIA])) {
		_release_multimedia_key("KEY_PLAYCD");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 3 : Check the lock status */
	if ((status_passive_get()->idle_lock_state  == VCONFKEY_IDLE_LOCK)
		&& (status_active_get()->setappl_screen_lock_type_int > SETTING_SCREEN_LOCK_TYPE_NONE)) {
		if (!strcmp(ev->keyname, key_name[KEY_BACK])) {
			_D("Back key is released");
			lock_pwd_util_back_key_relased();
		} else {
			_D("phone lock state, ignore home key.");
		}
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 4 : These keys are only activated after checking the lock state */
#if 0
	if (!strcmp(ev->keyname, key_name[KEY_END])) {
	} else
#endif
   	if (!strcmp(ev->keyname, key_name[KEY_CONFIG])) {
	//} else if (!strcmp(ev->keyname, key_name[KEY_SEND])) {
	} else if (!strcmp(ev->keyname, key_name[KEY_HOME])) {
		_W("Home Key is released");

		syspopup_destroy_all();

		if(key_info.home_multi_press_timer) {
			_D("delete homekey timer");
			ecore_timer_del(key_info.home_multi_press_timer);
			key_info.home_multi_press_timer = NULL;
		}

		if (key_info.home_long_press_timer) {
			ecore_timer_del(key_info.home_long_press_timer);
			key_info.home_long_press_timer = NULL;
		} else {
			key_info.homekey_count = 0;
			return ECORE_CALLBACK_RENEW;
		}

		key_info.home_multi_press_timer = ecore_timer_add(HOMEKEY_TIMER_SEC, _home_multi_press_timer_cb, NULL);
		if (!key_info.home_multi_press_timer) {
			_E("Critical! cannot add a timer for home multi press");
		}
		return ECORE_CALLBACK_RENEW;
	//} else if (!strcmp(ev->keyname, key_name[KEY_PAUSE])) {
	} else if (!strcmp(ev->keyname, key_name[KEY_APPS])) {
		_D("App tray key is released");
	} else if (!strcmp(ev->keyname, key_name[KEY_TASKSWITCH])) {
		_D("Task switch key is released");
		_launch_taskmgr_cb(NULL);
	} else if (!strcmp(ev->keyname, key_name[KEY_WEBPAGE])) {
		_D("Web page key is released");
		process_mgr_must_open(APPID_BROWSER, NULL, NULL);
	} else if (!strcmp(ev->keyname, key_name[KEY_MAIL])) {
		_D("Mail key is released");
		process_mgr_must_open(APPID_EMAIL, NULL, NULL);
	} else if (!strcmp(ev->keyname, key_name[KEY_CONNECT])) {
		_D("Connect key is released");
		process_mgr_must_open(APPID_DIALER, NULL, NULL);
	} else if (!strcmp(ev->keyname, key_name[KEY_SEARCH])) {
		_D("Search key is released");
		if (_launch_search() < 0) {
			_E("Failed to launch the search");
		}
	} else if (!strcmp(ev->keyname, key_name[KEY_VOICE])) {
		_D("Voice key is released");
	}

	return ECORE_CALLBACK_RENEW;
}



static Eina_Bool _key_press_cb(void *data, int type, void *event)
{
	Evas_Event_Key_Down *ev = event;

	retv_if(!ev, ECORE_CALLBACK_RENEW);
	retv_if(!ev->keyname, ECORE_CALLBACK_RENEW);

	_D("_key_press_cb : %s Pressed", ev->keyname);

	/* Priority 1 : Cancel */
	/*              every reserved events have to be canceld when cancel key is pressed */
	if (!strcmp(ev->keyname, key_name[KEY_CANCEL])) {
		_D("Cancel button is pressed");
		key_info.cancel = EINA_TRUE;
		_cancel_key_events();
		return ECORE_CALLBACK_RENEW;
	}

	if (EINA_TRUE == key_info.cancel) {
		_D("CANCEL is on");
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 2 : Check the lock status */
	if ((status_passive_get()->idle_lock_state == VCONFKEY_IDLE_LOCK)
		&& (status_active_get()->setappl_screen_lock_type_int > SETTING_SCREEN_LOCK_TYPE_NONE)) {
		_D("phone lock state, ignore key events.");
		_cancel_key_events();
		return ECORE_CALLBACK_RENEW;
	}

	/* Priority 3 : other keys */
#if 0
	if (!strcmp(ev->keyname, key_name[KEY_SEND])) {
		_D("Launch calllog");
		process_mgr_must_open(APPID_CALLLOG, NULL, NULL);
	} else
#endif
	if(!strcmp(ev->keyname, key_name[KEY_CONFIG])) {
		_D("Launch camera");
		process_mgr_must_open(APPID_CAMERA, NULL, NULL);
	} else if (!strcmp(ev->keyname, key_name[KEY_HOME])) {
		_W("Home Key is pressed");
		if (key_info.home_long_press_timer) {
			ecore_timer_del(key_info.home_long_press_timer);
			key_info.home_long_press_timer = NULL;
		}

		key_info.homekey_count++;
		_W("homekey count : %d", key_info.homekey_count);

		if(key_info.home_multi_press_timer) {
			ecore_timer_del(key_info.home_multi_press_timer);
			key_info.home_multi_press_timer = NULL;
			_D("delete homekey timer");
		}

		_D("create long press timer");
		key_info.home_long_press_timer = ecore_timer_add(LONG_PRESS_TIMER_SEC, _launch_taskmgr_cb, NULL);
		if (!key_info.home_long_press_timer) {
			_E("Failed to add timer for long press detection");
		}
	} else if (!strcmp(ev->keyname, key_name[KEY_MEDIA])) {
		_D("Media key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_APPS])) {
		_D("App tray key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_TASKSWITCH])) {
		_D("Task switch key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_WEBPAGE])) {
		_D("Web page key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_MAIL])) {
		_D("Mail key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_SEARCH])) {
		_D("Search key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_VOICE])) {
		_D("Voice key is pressed");
	} else if (!strcmp(ev->keyname, key_name[KEY_CONNECT])) {
		_D("Connect key is pressed");
	}

	return ECORE_CALLBACK_RENEW;
}



static Eina_Bool __keygrab_timer_cb(void *data)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < KEY_NAME_MAX; i++) {
		_D("key grab : %s", key_name[i]);
		ret = ecore_wl_window_keygrab_set(NULL, key_name[i], 0, 0, 0, ECORE_WL_WINDOW_KEYGRAB_SHARED);
		//ret = ecore_wl_window_keygrab_set(NULL, key_name[i], 0, 0, 0,  ELM_WIN_KEYGRAB_SHARED);
		_D(" === ret : %d", ret);
	}

	key_info.key_up = ecore_event_handler_add(ECORE_EVENT_KEY_UP, _key_release_cb, NULL);
	if (!key_info.key_up) {
		_E("Failed to register a key up event handler");
	}

	key_info.key_down = ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, _key_press_cb, NULL);
	if (!key_info.key_down) {
		_E("Failed to register a key down event handler");
	}
}



void hw_key_create_window(void)
{
	if (key_info.keygrab_timer) {
		ecore_timer_del(key_info.keygrab_timer);
		key_info.keygrab_timer = NULL;
	}

	key_info.keygrab_timer = ecore_timer_add(1.0f, __keygrab_timer_cb, NULL);
	if (!key_info.keygrab_timer) {
		_E("Failed to add timer for keygrab");
	}
}



void hw_key_destroy_window(void)
{
	int i = 0;

	for (i = 0; i < KEY_NAME_MAX; i++) {
		ecore_wl_window_keygrab_unset(NULL, key_name[i], 0, 0);
	}

	if (key_info.keygrab_timer) {
		ecore_timer_del(key_info.keygrab_timer);
		key_info.keygrab_timer = NULL;
	}

	if (key_info.key_up) {
		ecore_event_handler_del(key_info.key_up);
		key_info.key_up = NULL;
	}

	if (key_info.key_down) {
		ecore_event_handler_del(key_info.key_down);
		key_info.key_down = NULL;
	}
}

#endif


// End of a file
