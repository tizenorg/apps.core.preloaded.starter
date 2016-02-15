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
#include <vconf.h>
#include <bundle.h>
#include <appcore-efl.h>
#include <app.h>

#ifdef HAVE_X11
#include <Ecore_X.h>
#include <utilX.h>
#include <ui-gadget.h>
#endif

#include "window_mgr.h"
#include "util.h"

#define STR_ATOM_PANEL_SCROLLABLE_STATE         "_E_MOVE_PANEL_SCROLLABLE_STATE"



#ifdef HAVE_X11
struct _lockw_data {
	Eina_Bool is_registered;
	Ecore_X_Window lock_x_window;

	Ecore_Event_Handler *h_wincreate;
	Ecore_Event_Handler *h_winshow;
	Ecore_Event_Handler *h_winhide;
};



static int _is_on_screen(Ecore_X_Display * dpy, Ecore_X_Window window)
{
	Ecore_X_Window root;
	Window child;
	Window win;

	int rel_x = 0;
	int rel_y = 0;
	int abs_x = 0;
	int abs_y = 0;

	unsigned int width = 0;
	unsigned int height = 0;
	unsigned int border = 0;
	unsigned int depth = 0;
	unsigned int root_w = 0;
	unsigned int root_h = 0;

	Eina_Bool ret = FALSE;

	root = ecore_x_window_root_first_get();
	XGetGeometry(dpy, root, &win, &rel_x, &rel_y, &root_w, &root_h, &border, &depth);
	_D("root rel_x[%d] rel_y[%d] border[%d] width[%d] height[%d]", rel_x, rel_y, border, root_w, root_h);

	if (XGetGeometry(dpy, window, &win, &rel_x, &rel_y, &width, &height, &border, &depth)) {
		if (XTranslateCoordinates(dpy, window, root, 0, 0, &abs_x, &abs_y, &child)) {
		    _D("abs_x[%d] abs_y[%d] border[%d] width[%d] height[%d]", abs_x, abs_y, border, width, height);
			if ((abs_x - border) >= root_w
				|| (abs_y - border) >= root_h
			    || (width + abs_x) <= 0 || (height + abs_y) <= 0)
			{
				ret = FALSE;
			} else {
				ret = (width == root_w) && (height == root_h);
			}
		}
	}

	return ret;
}



static Window _get_user_created_window(Window win)
{
	Atom type_ret = 0;
	int ret, size_ret = 0;
	unsigned long num_ret = 0, bytes = 0;
	unsigned char *prop_ret = NULL;
	unsigned int xid;
	Atom prop_user_created_win;

	prop_user_created_win = XInternAtom(ecore_x_display_get(), "_E_USER_CREATED_WINDOW", False);

	ret = XGetWindowProperty(ecore_x_display_get()
							, win, prop_user_created_win
							, 0L, 1L
							, False, 0
							, &type_ret, &size_ret
							, &num_ret, &bytes
							, &prop_ret);
	if (ret != Success) {
		if (prop_ret)
			XFree((void *) prop_ret);
		return win;
	} else if (!prop_ret) {
		return win;
	}

	memcpy(&xid, prop_ret, sizeof(unsigned int));
	XFree((void *)prop_ret);

	return xid;

}



int window_mgr_get_focus_window_pid(void)
{
	Ecore_X_Window x_win_focused = 0;
	int pid = 0;
	int ret = -1;

	_D("%s, %d", __func__, __LINE__);

	x_win_focused = ecore_x_window_focus_get();
	ret = ecore_x_netwm_pid_get(x_win_focused, &pid);
	if(ret != 1) {
		_E("Can't get pid for focus x window (%x)\n", x_win_focused);
		return -1;
	}
	_D("PID(%d) for focus x window (%x)\n", pid, x_win_focused);

	return pid;
}



static void _pwd_transient_set(Ecore_X_Window win, Ecore_X_Window for_win)
{
	_W("%p is transient for %p", win, for_win);

	ecore_x_icccm_transient_for_set(win, for_win);
}



static void _pwd_transient_unset(Ecore_X_Window xwin)
{
	ret_if(!xwin);

	_W("%p is not transient", xwin);
	ecore_x_icccm_transient_for_unset(xwin);
}



Eina_Bool window_mgr_pwd_transient_set(void *data)
{
	Evas_Object *pwd_win = NULL;
	Ecore_X_Window pwd_x_win;
	lockw_data *lockw = (lockw_data *) data;
	retv_if(!lockw, EINA_FALSE);

	pwd_win = lock_pwd_util_win_get();
	retv_if(!pwd_win, EINA_FALSE);

	pwd_x_win = elm_win_xwindow_get(pwd_win);
	retv_if(!pwd_x_win, EINA_FALSE);

	retv_if(!lockw->lock_x_window, EINA_FALSE);

	/* unset transient */
	_pwd_transient_unset(lockw->lock_x_window);

	/* set transient */
	_pwd_transient_set(lockw->lock_x_window, pwd_x_win);

	return EINA_TRUE;
}



Eina_Bool window_mgr_set_prop(lockw_data * data, int lock_app_pid, void *event)
{
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	lockw_data *lockw = (lockw_data *) data;
	int pid = 0;
	int ret = 0;

	retv_if(!lockw, EINA_FALSE);

	user_window = _get_user_created_window((Window) (e->win));

	ret = ecore_x_netwm_pid_get(user_window, &pid);
	retv_if(ret != 1, EINA_FALSE);

	_D("Check PID(%d) window. (lock_app_pid : %d)", pid, lock_app_pid);

	if (lock_app_pid == pid) {
		if (_is_on_screen(ecore_x_display_get(), user_window) == TRUE) {
			lockw->lock_x_window = user_window;
			/* window effect : fade in /out */
			ecore_x_icccm_name_class_set(user_window, "LOCK_SCREEN", "LOCK_SCREEN");
			ecore_x_netwm_window_type_set(user_window, ECORE_X_WINDOW_TYPE_NOTIFICATION);
			utilx_set_system_notification_level(ecore_x_display_get(), user_window, UTILX_NOTIFICATION_LEVEL_NORMAL);
			utilx_set_window_opaque_state(ecore_x_display_get(), user_window, UTILX_OPAQUE_STATE_ON);

			/* set transient */
			if (!window_mgr_pwd_transient_set(lockw)) {
				_E("Failed to set transient");
			}

			return EINA_TRUE;
		}
	}
	return EINA_FALSE;
}



Eina_Bool window_mgr_set_effect(lockw_data * data, int lock_app_pid, void *event)
{
	Ecore_X_Event_Window_Create *e = event;
	Ecore_X_Window user_window = 0;
	int pid = 0;
	int ret = 0;

	user_window = _get_user_created_window((Window) (e->win));
	ret = ecore_x_netwm_pid_get(user_window, &pid);
	retv_if(ret != 1, EINA_FALSE);

	if (lock_app_pid == pid) {
		if (_is_on_screen(ecore_x_display_get(), user_window) == TRUE) {
			Ecore_X_Atom ATOM_WINDOW_EFFECT_ENABLE = 0;
			unsigned int effect_state = 0;

			ATOM_WINDOW_EFFECT_ENABLE = ecore_x_atom_get("_NET_CM_WINDOW_EFFECT_ENABLE");
			if (ATOM_WINDOW_EFFECT_ENABLE) {
				ecore_x_window_prop_card32_set(user_window, ATOM_WINDOW_EFFECT_ENABLE, &effect_state, 1);
			} else {
				_E("ecore_x_atom_get() failed");
			}
			return EINA_TRUE;
		}
	}
	return EINA_FALSE;
}



void window_mgr_set_scroll_prop(lockw_data *data, int lock_type)
{
	lockw_data *lockw = (lockw_data *) data;
	Ecore_X_Atom ATOM_PANEL_SCROLLABLE_STATE = 0;
	unsigned int val[3] = { 0, };

	ret_if(!lockw);

	ATOM_PANEL_SCROLLABLE_STATE = ecore_x_atom_get(STR_ATOM_PANEL_SCROLLABLE_STATE);
	if (lock_type == SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD ||
			lock_type == SETTING_SCREEN_LOCK_TYPE_PASSWORD) {
		val[0] = 0; // always enable F
		val[1] = 0; // quickpanel enable F
		val[2] = 0; // apptray enable F
	} else {
		val[0] = 0; // always enable F
		val[1] = 1; // quickpanel enable T
		val[2] = 0; // apptray enable F
	}
	ecore_x_window_prop_card32_set(lockw->lock_x_window, ATOM_PANEL_SCROLLABLE_STATE, val, 3);
}



void window_mgr_register_event(void *data, lockw_data * lockw,
			    Eina_Bool (*create_cb) (void *, int, void *),
			    Eina_Bool (*show_cb) (void *, int, void *))
{
	Ecore_X_Window root_window;

	ret_if(!lockw);

	if (lockw->is_registered) {
		_E("Already register event cb");
		return;
	}

	/* For getting window x event */
	root_window = ecore_x_window_root_first_get();
	ecore_x_window_client_sniff(root_window);

	lockw->h_wincreate = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_CREATE, create_cb, data);
	lockw->h_winshow = ecore_event_handler_add(ECORE_X_EVENT_WINDOW_SHOW, show_cb, data);

	lockw->is_registered = EINA_TRUE;
}



static inline void _unregister_event(lockw_data *lockw)
{
	Ecore_X_Window root_window;

	/* unset transient */
	_pwd_transient_unset(lockw->lock_x_window);

	/* delete getting window x event */
	root_window = ecore_x_window_root_first_get();
	ecore_x_window_client_sniff(root_window);

	/* delete window create event handler */
	if (lockw->h_wincreate) {
		ecore_event_handler_del(lockw->h_wincreate);
		lockw->h_wincreate = NULL;
	}
	if (lockw->h_winshow) {
		ecore_event_handler_del(lockw->h_winshow);
		lockw->h_winshow = NULL;
	}
	if (lockw->h_winhide) {
		ecore_event_handler_del(lockw->h_winhide);
		lockw->h_winhide = NULL;
	}

	ecore_x_pointer_ungrab();

	lockw->is_registered = EINA_FALSE;
}



void window_mgr_unregister_event(lockw_data *lockw)
{
	ret_if(!lockw);

	if (!lockw->is_registered) {
		_E("event cb is not registered");
		return;
	}

	_unregister_event(lockw);
}



lockw_data *window_mgr_init(void)
{
	lockw_data *lockw = NULL;

	lockw = calloc(1, sizeof(*lockw));

	return lockw;
}



void window_mgr_fini(lockw_data *lockw)
{
	ret_if(!lockw);

	if (lockw->is_registered) {
		_unregister_event(lockw);
	}

	free(lockw);
}
#endif
