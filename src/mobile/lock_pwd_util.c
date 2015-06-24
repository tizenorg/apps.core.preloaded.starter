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

#include <system_settings.h>
#include <efl_extension.h>

#include "lock_mgr.h"
#include "util.h"
#include "status.h"
#include "status.h"
#include "lock_pwd_util.h"
#include "lock_pwd_simple.h"
#include "lock_pwd_complex.h"

static struct _s_lock_pwd_util {
	Evas_Object *lock_pwd_win;
	Evas_Object *conformant;
	Evas_Object *layout;
	Evas_Object *bg;

	int win_w;
	int win_h;
} s_lock_pwd_util = {
	.lock_pwd_win = NULL,
	.conformant = NULL,
	.layout = NULL,
	.bg = NULL,

	.win_w = 0,
	.win_h = 0,
};



int lock_pwd_util_win_width_get(void)
{
	return s_lock_pwd_util.win_w;
}



int lock_pwd_util_win_height_get(void)
{
	return s_lock_pwd_util.win_h;
}



static Evas_Object *_pwd_conformant_add(void *data)
{
	Evas_Object *win = NULL;
	Evas_Object *conformant = NULL;

	win = (Evas_Object *)data;
	retv_if(!win, NULL);

	conformant = elm_conformant_add(win);
	retv_if(!conformant, NULL);

	evas_object_size_hint_weight_set(conformant, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_win_resize_object_add(win, conformant);

	elm_win_indicator_mode_set(win, ELM_WIN_INDICATOR_SHOW);
	elm_win_indicator_opacity_set(win, ELM_WIN_INDICATOR_TRANSLUCENT);
	elm_win_conformant_set(win, EINA_TRUE);

	evas_object_show(conformant);

	return conformant;
}


void lock_pwd_util_bg_image_set(Evas_Object *bg, char *file)
{
	char *bg_path = NULL;
	int ret = 0;

	ret_if(!bg);

	if (file) {
		_D("lock bg : %s", file);
		if (!elm_bg_file_set(bg, file, NULL)) {
			_E("Failed to set BG : %s", file);
			goto ERROR;
		}
	} else {
		ret = system_settings_get_value_string(SYSTEM_SETTINGS_KEY_WALLPAPER_LOCK_SCREEN, &bg_path);
		if (SYSTEM_SETTINGS_ERROR_NONE != ret) {
			_E("Failed to get system setting value : %d", SYSTEM_SETTINGS_KEY_WALLPAPER_LOCK_SCREEN);
			goto ERROR;
		}
		_D("lock bg : %s", bg_path);

		if (!elm_bg_file_set(bg, bg_path, NULL)) {
			_E("Failed to set bg : %s", bg_path);
			goto ERROR;
		}

		if (bg_path) {
			free(bg_path);
			bg_path = NULL;
		}
	}

	return;

ERROR:
	if (bg_path) {
		free(bg_path);
		bg_path = NULL;
	}

	if (!elm_bg_file_set(bg, LOCK_MGR_DEFAULT_BG_PATH, NULL)) {
		_E("Failed to set default BG : %s. Retry to set default BG.", LOCK_MGR_DEFAULT_BG_PATH);
		if (!elm_bg_file_set(bg, LOCK_MGR_DEFAULT_BG_PATH, NULL)) {
			_E("Failed to set default BG : %s", LOCK_MGR_DEFAULT_BG_PATH);
		}
	}

	return;
}



static void _wallpaper_lock_screen_changed_cb(system_settings_key_e key, void *data)
{
	_D("%s", __func__);
	Evas_Object *bg = (Evas_Object *)data;
	ret_if(!bg);

	lock_pwd_util_bg_image_set(bg, NULL);
}



static Evas_Object *_pwd_bg_add(void *data)
{
	Evas_Object *bg = NULL;
	Evas_Object *parent = NULL;
	int ret = 0;

	parent = (Evas_Object *)data;
	retv_if(!parent, NULL);

	bg = elm_bg_add(parent);
	retv_if(!bg, NULL);

	elm_bg_option_set(bg, ELM_BG_OPTION_SCALE);
	elm_win_resize_object_add(parent, bg);
	lock_pwd_util_bg_image_set(bg, NULL);
	evas_object_show(bg);

	ret = system_settings_set_changed_cb(SYSTEM_SETTINGS_KEY_WALLPAPER_LOCK_SCREEN, _wallpaper_lock_screen_changed_cb, bg);
	if (SYSTEM_SETTINGS_ERROR_NONE != ret) {
		_E("Failed to register settings changed cb : %d", SYSTEM_SETTINGS_KEY_WALLPAPER_LOCK_SCREEN);
	}

	return bg;
}



static Evas_Object *_pwd_layout_create(void *data)
{
	Evas_Object *layout = NULL;
	Evas_Object *parent = NULL;

	parent = (Evas_Object *)data;
	retv_if(!parent, NULL);

	layout = elm_layout_add(parent);
	retv_if(!layout, NULL);

	if (!elm_layout_file_set(layout, LOCK_PWD_EDJE_FILE, "lock_pwd")) {
		_E("Failed to set edje file : %s", LOCK_PWD_EDJE_FILE);
		goto ERROR;
	}

	evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(layout, EVAS_HINT_FILL, EVAS_HINT_FILL);

	evas_object_show(layout);

	return layout;

ERROR:
	_E("Failed to create password layout");

	if (layout) {
		evas_object_del(layout);
		layout = NULL;
	}

	return NULL;
}



void lock_pwd_util_back_key_relased(void)
{
	_D("%s", __func__);

	ret_if(lock_pwd_simple_is_blocked_get());

	lock_mgr_sound_play(LOCK_SOUND_TAP);

	if (!lock_mgr_lockscreen_launch()) {
		_E("Failed to launch lockscreen");
	}

	lock_pwd_util_view_init();
}



void lock_pwd_util_create(Eina_Bool is_show)
{
	Evas_Object *win = NULL;
	Evas_Object *conformant = NULL;
	Evas_Object *bg = NULL;
	Evas_Object *layout = NULL;
	Evas_Object *pwd_layout = NULL;
	int lock_type = 0;
	int x = 0, y = 0, w = 0, h = 0;

	if (!s_lock_pwd_util.lock_pwd_win) {
		win = window_mgr_pwd_lock_win_create();
		ret_if(!win);
		s_lock_pwd_util.lock_pwd_win = win;

		elm_win_screen_size_get(win, &x, &y, &w, &h);
		_D("win size : %dx%d(%d, %d)", w, h, x, y);
		s_lock_pwd_util.win_w = w;
		s_lock_pwd_util.win_h = h;
	}

	conformant = _pwd_conformant_add(win);
	goto_if(!conformant, ERROR);
	s_lock_pwd_util.conformant = conformant;

	layout = _pwd_layout_create(conformant);
	goto_if(!layout, ERROR);
	s_lock_pwd_util.layout = layout;

	elm_object_content_set(conformant, layout);

	bg = _pwd_bg_add(win);
	goto_if(!bg, ERROR);
	s_lock_pwd_util.bg = bg;

	elm_object_part_content_set(layout, "sw.bg", bg);

	lock_type = status_active_get()->setappl_screen_lock_type_int;
	_D("lock type : %d", lock_type);

	switch(lock_type) {
	case SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD:
		pwd_layout = lock_pwd_simple_layout_create(layout);
		break;
	case SETTING_SCREEN_LOCK_TYPE_PASSWORD:
		pwd_layout = lock_pwd_complex_layout_create(layout);
		break;
	default:
		_E("lock type is not password : %d", lock_type);
		goto ERROR;
	}
	goto_if(!pwd_layout, ERROR);

	elm_object_part_content_set(layout, "sw.lock_pwd", pwd_layout);

	if (is_show) {
		evas_object_show(win);
	}

	return;

ERROR:
	_E("Failed to launch password lockscreen");

	lock_pwd_util_del();

	return;
}



void lock_pwd_util_del(void)
{
	int lock_type = status_active_get()->setappl_screen_lock_type_int;
	_D("lock type : %d", lock_type);

	if (lock_type == SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD) {
		lock_pwd_simple_layout_del();
	} else if (lock_type == SETTING_SCREEN_LOCK_TYPE_PASSWORD) {
		lock_pwd_complex_layout_del();
	}

	if (s_lock_pwd_util.layout) {
		evas_object_del(s_lock_pwd_util.layout);
		s_lock_pwd_util.layout = NULL;
	}

	if (s_lock_pwd_util.conformant) {
		evas_object_del(s_lock_pwd_util.conformant);
		s_lock_pwd_util.conformant = NULL;
	}

	if (s_lock_pwd_util.bg) {
		system_settings_unset_changed_cb(SYSTEM_SETTINGS_KEY_WALLPAPER_LOCK_SCREEN);
		evas_object_del(s_lock_pwd_util.bg);
		s_lock_pwd_util.bg = NULL;
	}

	if (s_lock_pwd_util.lock_pwd_win) {
		evas_object_del(s_lock_pwd_util.lock_pwd_win);
		s_lock_pwd_util.lock_pwd_win = NULL;
	}
}




static void _pwd_popup_cb(void *data, Evas_Object *obj, void *event_info)
{
	ret_if(!data);

	evas_object_del((Evas_Object *)data);
}



void lock_pwd_util_popup_create(char *title, char *text, Evas_Smart_Cb func, double timeout)
{
	Evas_Object *popup = NULL;
	Evas_Object *btn = NULL;

	ret_if(!s_lock_pwd_util.lock_pwd_win);

	popup = elm_popup_add(s_lock_pwd_util.lock_pwd_win);
	ret_if(!popup);

	elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	if (title) {
		elm_object_part_text_set(popup, "title,text", title);
	}

	if (text) {
		elm_object_text_set(popup, text);
	}

	btn = elm_button_add(popup);
	if (!btn) {
		_E("Failed to create lock popup button");
		evas_object_del(popup);
		return;
	}

	elm_object_style_set(btn, "popup");
	elm_object_text_set(btn, _("IDS_COM_BUTTON_OK_ABB"));
	elm_object_part_content_set(popup, "button1", btn);

	if (timeout > 0.0) {
		elm_popup_timeout_set(popup, timeout);
	}

	if (func) {
		evas_object_smart_callback_add(btn, "clicked", func, popup);
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, func, popup);
	} else {
		evas_object_smart_callback_add(btn, "clicked", _pwd_popup_cb, popup);
		eext_object_event_callback_add(popup, EEXT_CALLBACK_BACK, _pwd_popup_cb, popup);
	}

	evas_object_show(popup);

	return;
}



void lock_pwd_util_view_init(void)
{
	_D("initialize password lock values");
	int lock_type = 0;

	/* clear pwd lockscreen */
	lock_type = status_active_get()->setappl_screen_lock_type_int;
	if (lock_type == SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD) {
		lock_pwd_simple_view_init();
	} else if (lock_type == SETTING_SCREEN_LOCK_TYPE_PASSWORD) {
		lock_pwd_complex_view_init();
	}
}



Evas_Object *lock_pwd_util_win_get(void)
{
	return s_lock_pwd_util.lock_pwd_win;
}



Eina_Bool lock_pwd_util_win_visible_get(void)
{
	retv_if(!s_lock_pwd_util.lock_pwd_win, EINA_FALSE);
	return evas_object_visible_get(s_lock_pwd_util.lock_pwd_win);
}



void lock_pwd_util_win_show(void)
{
	ret_if(!s_lock_pwd_util.lock_pwd_win);
	evas_object_show(s_lock_pwd_util.lock_pwd_win);
}



void lock_pwd_util_win_hide(void)
{
	ret_if(!s_lock_pwd_util.lock_pwd_win);
	evas_object_hide(s_lock_pwd_util.lock_pwd_win);
}
