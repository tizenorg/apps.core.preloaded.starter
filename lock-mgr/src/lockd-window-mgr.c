/*
 * Copyright 2012  Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <Elementary.h>
#include <Ecore_X.h>
#include <utilX.h>
#include <vconf.h>
#include <bundle.h>
#include <appcore-efl.h>
#include <app.h>

#include "lockd-debug.h"
#include "lockd-window-mgr.h"

#define PACKAGE 		"starter"

struct _lockw_data {
	Ecore_X_Window input_x_window;

	Evas_Object *main_win;
	Evas_Object *main_layout;

	Ecore_X_Window lock_x_window;

	Ecore_Event_Handler *h_keydown;
	Ecore_Event_Handler *h_wincreate;
	Ecore_Event_Handler *h_winshow;

};

static Eina_Bool _lockd_window_key_down_cb(void *data, int type, void *event)
{
	LOCKD_DBG("Key Down CB.");

	return ECORE_CALLBACK_PASS_ON;
}

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

void
lockd_window_set_window_property(lockw_data * data, int lock_app_pid,
				 void *event)
{
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	lockw_data *lockw = (lockw_data *) data;
	int pid = 0;

	if (!lockw) {
		return;
	}
	LOCKD_DBG("%s, %d", __func__, __LINE__);

	user_window = get_user_created_window((Window) (e->win));

	ecore_x_netwm_pid_get(user_window, &pid);

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
		}
	}
}

void
lockd_window_set_window_effect(lockw_data * data, int lock_app_pid, void *event)
{
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	int pid = 0;

	user_window = get_user_created_window((Window) (e->win));
	ecore_x_netwm_pid_get(user_window, &pid);

	LOCKD_DBG("%s, %d", __func__, __LINE__);

	LOCKD_DBG("PID(%d) window created. (lock_app_pid : %d)\n", pid,
		  lock_app_pid);

	if (lock_app_pid == pid) {
		if (_lockd_window_check_validate_rect
		    (ecore_x_display_get(), user_window) == TRUE) {
			LOCKD_DBG
			    ("This is lock application. Disable window effect. win id : %x\n",
			     user_window);

			utilx_set_window_effect_state(ecore_x_display_get(),
						      user_window, 0);
		}
	}
}

void
lockd_window_mgr_ready_lock(void *data, lockw_data * lockw,
			    Eina_Bool(*create_cb) (void *, int, void *),
			    Eina_Bool(*show_cb) (void *, int, void *))
{
	Ecore_X_Window xwin;

	if (lockw == NULL) {
		LOCKD_ERR("lockw is NULL.");
		return;
	}

	lockw->h_wincreate =
	    ecore_event_handler_add(ECORE_X_EVENT_WINDOW_CREATE, create_cb,
				    data);
	lockw->h_winshow =
	    ecore_event_handler_add(ECORE_X_EVENT_WINDOW_SHOW, show_cb, data);

	xwin = lockw->input_x_window;
	utilx_grab_key(ecore_x_display_get(), xwin, KEY_SELECT, EXCLUSIVE_GRAB);

	lockw->h_keydown =
	    ecore_event_handler_add(ECORE_EVENT_KEY_DOWN,
				    _lockd_window_key_down_cb, NULL);
}

void lockd_window_mgr_finish_lock(lockw_data * lockw)
{
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

	xwin = lockw->input_x_window;
	utilx_ungrab_key(ecore_x_display_get(), xwin, KEY_SELECT);

	if (lockw->h_keydown != NULL) {
		ecore_event_handler_del(lockw->h_keydown);
		lockw->h_keydown = NULL;
	}
}

lockw_data *lockd_window_init(void)
{
	lockw_data *lockw = NULL;
	Ecore_X_Window input_x_window;
	Ecore_X_Window root_window;
	long pid;

	lockw = (lockw_data *) malloc(sizeof(lockw_data));
	memset(lockw, 0x0, sizeof(lockw_data));

	pid = getpid();

	input_x_window = ecore_x_window_input_new(0, 0, 0, 1, 1);
	ecore_x_icccm_title_set(input_x_window, "lock-daemon-input-window");
	ecore_x_netwm_name_set(input_x_window, "lock-daemon-input-window");
	ecore_x_netwm_pid_set(input_x_window, pid);
	LOCKD_DBG("Created input window : %p", input_x_window);
	lockw->input_x_window = input_x_window;

	root_window = ecore_x_window_root_first_get();
	ecore_x_window_client_sniff(root_window);

	return lockw;
}
