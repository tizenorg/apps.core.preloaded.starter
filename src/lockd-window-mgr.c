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



#include <Elementary.h>

#ifndef WAYLAND
#include <Ecore_X.h>
#include <utilX.h>
#endif

#include <ui-gadget.h>
#include <vconf.h>
#include <bundle.h>
#include <appcore-efl.h>
#include <app.h>

#include "lockd-debug.h"
#include "lockd-window-mgr.h"

#define PACKAGE 			"starter"
#define SOS_KEY_COUNT		3
#define SOS_KEY_INTERVAL	0.5

struct _lockw_data {
	Evas_Object *main_win;
	Evas_Object *main_layout;

	Ecore_X_Window lock_x_window;

	Ecore_Event_Handler *h_keydown;
	Ecore_Event_Handler *h_wincreate;
	Ecore_Event_Handler *h_winshow;

	Ecore_Timer *pTimerId;
	int volume_key_cnt;

	int phone_lock_state;
	int phone_lock_app_pid;
};

static Eina_Bool _lockd_window_key_down_cb(void *data, int type, void *event)
{
	Ecore_Event_Key *ev = event;

	LOCKD_DBG("Key Down CB : %s", ev->keyname);

	return ECORE_CALLBACK_PASS_ON;
}

#ifndef WAYLAND
static int
_lockd_window_check_validate_rect(Ecore_X_Display * dpy, Ecore_X_Window window)
{
	Ecore_X_Window root;
	Ecore_X_Window child;

	int rel_x = 0;
	int rel_y = 0;
	int abs_x = 0;
	int abs_y = 0;

	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int border = 0;
	unsigned int depth = 0;

	Eina_Bool ret = FALSE;

	root = ecore_x_window_root_first_get();

	if (XGetGeometry
	    (dpy, window, &root, &rel_x, &rel_y, &width, &height, &border,
	     &depth)) {
		if (XTranslateCoordinates
		    (dpy, window, root, 0, 0, &abs_x, &abs_y, &child)) {
			if ((abs_x - border) >= 480 || (abs_y - border) >= 800
			    || (width + abs_x) <= 0 || (height + abs_y) <= 0) {
				ret = FALSE;
			} else {
				ret = TRUE;
			}
		}
	}

	return ret;
}
#endif

#ifndef WAYLAND
static Window get_user_created_window(Window win)
{
	Atom type_ret = 0;
	int ret, size_ret = 0;
	unsigned long num_ret = 0, bytes = 0;
	unsigned char *prop_ret = NULL;
	unsigned int xid;
	Atom prop_user_created_win;

	prop_user_created_win =
	    XInternAtom(ecore_x_display_get(), "_E_USER_CREATED_WINDOW", False);

	ret =
	    XGetWindowProperty(ecore_x_display_get(), win,
			       prop_user_created_win, 0L, 1L, False, 0,
			       &type_ret, &size_ret, &num_ret, &bytes,
			       &prop_ret);

	if (ret != Success) {
		if (prop_ret)
			XFree((void *)prop_ret);
		return win;
	} else if (!prop_ret) {
		return win;
	}

	memcpy(&xid, prop_ret, sizeof(unsigned int));
	XFree((void *)prop_ret);

	return xid;

}
#endif

Eina_Bool
lockd_window_set_window_property(lockw_data * data, int lock_app_pid,
				 void *event)
{
#ifndef WAYLAND
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	lockw_data *lockw = (lockw_data *) data;
	int pid = 0;

	if (!lockw) {
		return EINA_FALSE;
	}
	LOCKD_DBG("%s, %d", __func__, __LINE__);

	user_window = get_user_created_window((Window) (e->win));

	int ret = ecore_x_netwm_pid_get(user_window, &pid);
	if(ret != 1) {
		return EINA_FALSE;
	}

	LOCKD_DBG("Check PID(%d) window. (lock_app_pid : %d)\n", pid,
		  lock_app_pid);

	if (lock_app_pid == pid) {
		if (_lockd_window_check_validate_rect
		    (ecore_x_display_get(), user_window) == TRUE) {
			lockw->lock_x_window = user_window;
			LOCKD_DBG
			    ("This is lock application. Set window property. win id : %x",
			     user_window);

			ecore_x_icccm_name_class_set(user_window, "LOCK_SCREEN",
						     "LOCK_SCREEN");
			ecore_x_netwm_window_type_set(user_window,
						      ECORE_X_WINDOW_TYPE_NOTIFICATION);
			utilx_set_system_notification_level(ecore_x_display_get
							    (), user_window,
							    UTILX_NOTIFICATION_LEVEL_NORMAL);
			utilx_set_window_opaque_state(ecore_x_display_get(),
						      user_window,
						      UTILX_OPAQUE_STATE_ON);
			return EINA_TRUE;
		}
	}
#endif

	return EINA_FALSE;
}

Eina_Bool
lockd_window_set_window_effect(lockw_data * data, int lock_app_pid, void *event)
{
#ifndef WAYLAND
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	int pid = 0;

	user_window = get_user_created_window((Window) (e->win));
	int ret = ecore_x_netwm_pid_get(user_window, &pid);
	if(ret != 1) {
		return EINA_FALSE;
	}

	LOCKD_DBG("%s, %d", __func__, __LINE__);

	LOCKD_DBG("PID(%d) window created. (lock_app_pid : %d)\n", pid,
		  lock_app_pid);

	if (lock_app_pid == pid) {
		if (_lockd_window_check_validate_rect
		    (ecore_x_display_get(), user_window) == TRUE) {
			LOCKD_DBG
			    ("This is lock application. Disable window effect. win id : %x\n",
			     user_window);

#if 0	//window effect is not available now
			utilx_set_window_effect_state(ecore_x_display_get(),
						      user_window, 0);
#endif
			return EINA_TRUE;
		}
	}
#endif

	return EINA_FALSE;
}

void lockd_window_set_phonelock_pid(lockw_data * data, int phone_lock_pid)
{
	lockw_data *lockw = (lockw_data *) data;

	if (!lockw) {
		return;
	}
	LOCKD_DBG("%s, %d", __func__, __LINE__);
	lockw->phone_lock_app_pid = phone_lock_pid;
	LOCKD_DBG("%s, %d, lockw->phone_lock_app_pid = %d", __func__, __LINE__,
		  lockw->phone_lock_app_pid);
}

void
lockd_window_mgr_ready_lock(void *data, lockw_data * lockw,
			    Eina_Bool(*create_cb) (void *, int, void *),
			    Eina_Bool(*show_cb) (void *, int, void *))
{
	if (lockw == NULL) {
		LOCKD_ERR("lockw is NULL.");
		return;
	}
#ifndef WAYLAND
	lockw->h_wincreate =
	    ecore_event_handler_add(ECORE_X_EVENT_WINDOW_CREATE, create_cb,
				    data);
	lockw->h_winshow =
	    ecore_event_handler_add(ECORE_X_EVENT_WINDOW_SHOW, show_cb, data);

	lockw->volume_key_cnt = 0;

	lockw->h_keydown =
	    ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
				    _lockd_window_key_down_cb, lockw);
#endif
}

void lockd_window_mgr_finish_lock(lockw_data * lockw)
{
#ifndef WAYLAND
	Ecore_X_Window xwin;

	if (lockw == NULL) {
		LOCKD_ERR("lockw is NULL.");
		return;
	}
	if (lockw->h_wincreate != NULL) {
		ecore_event_handler_del(lockw->h_wincreate);
		lockw->h_wincreate = NULL;
	}
	if (lockw->h_winshow != NULL) {
		ecore_event_handler_del(lockw->h_winshow);
		lockw->h_winshow = NULL;
	}

	ecore_x_pointer_ungrab();

	if (lockw->h_keydown != NULL) {
		ecore_event_handler_del(lockw->h_keydown);
		lockw->h_keydown = NULL;
	}
#endif
}

lockw_data *lockd_window_init(void)
{
	lockw_data *lockw = NULL;
	long pid;

	lockw = (lockw_data *) malloc(sizeof(lockw_data));
	memset(lockw, 0x0, sizeof(lockw_data));

	pid = getpid();

	return lockw;
}
