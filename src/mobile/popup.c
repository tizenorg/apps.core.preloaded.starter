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

#include "popup.h"
#include "util.h"

#define POPUP_DATA_KEY_WINDOW "__popup_window__"

static void _popup_destroy(Evas_Object *popup)
{
	Evas_Object *win = NULL;

	if (popup) {
		win = evas_object_data_del(popup, POPUP_DATA_KEY_WINDOW);
		evas_object_del(popup);

		if (win) {
			evas_object_del(win);
		}
	}
}

static Evas_Object *_window_create(void)
{
	Evas_Object *win = NULL;
	int win_w = 0, win_h = 0;

	win = elm_win_add(NULL, "STARTER-POPUP", ELM_WIN_DIALOG_BASIC);
	retv_if(!win, NULL);

	elm_win_title_set(win, "STARTER-POPUP");
	elm_win_alpha_set(win, EINA_TRUE);
	elm_win_borderless_set(win, EINA_TRUE);
	elm_win_autodel_set(win, EINA_TRUE);
	elm_win_raise(win);

	elm_win_screen_size_get(win, NULL, NULL, &win_w, &win_h);
	_D("win size : (%dx%d)", win_w, win_h);
	evas_object_resize(win, win_w, win_h);

	evas_object_show(win);

	return win;
}



static void _popup_btn_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
	Evas_Object *popup = (Evas_Object *)data;
	ret_if(!popup);

	_popup_destroy(popup);
}



Evas_Object *popup_create(const char *title, const char *text)
{
	Evas_Object *win = NULL;
	Evas_Object *popup = NULL;
	Evas_Object *btn = NULL;

	retv_if(!title, NULL);
	retv_if(!text, NULL);

	win = _window_create();
	goto_if(!win, ERROR);

	popup = elm_popup_add(win);
	goto_if(!popup, ERROR);

	elm_popup_orient_set(popup, ELM_POPUP_ORIENT_BOTTOM);
	evas_object_size_hint_weight_set(popup, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	elm_object_part_text_set(popup, "title,text", title);
	elm_object_text_set(popup, text);
	evas_object_data_set(popup, POPUP_DATA_KEY_WINDOW, win);

	/* ok button */
	btn = elm_button_add(popup);
	goto_if(!btn, ERROR);

	elm_object_style_set(btn, "popup");
	elm_object_text_set(btn, S_("IDS_COM_SK_CONFIRM"));
	elm_object_part_content_set(popup, "button1", btn);
	evas_object_smart_callback_add(btn, "clicked", _popup_btn_clicked_cb, popup);

	evas_object_show(popup);

	return popup;

ERROR:
	_E("Failed to create popup");

	_popup_destroy(popup);

	return NULL;
}

