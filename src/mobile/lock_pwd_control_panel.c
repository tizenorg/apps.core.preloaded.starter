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

#include <app_control.h>
//#include <ui-gadget.h>

#include "lock_mgr.h"
#include "util.h"
#include "lock_pwd_util.h"
#include "lock_pwd_control_panel.h"

#define EMG_CALL_LABEL_STYLE_START "<style=far_shadow,bottom><shadow_color=#00000033><font_size=28><align=left><color=#FFFFFF><wrap=none>"
#define EMG_CALL_LABEL_STYLE_END "</wrap></color></align></font_size></shadow_color></style>"
#define EMG_BTN_WIDTH 280

static struct _s_lock_pwd_control_panel {
	Evas_Object *control_panel_layout;
	Evas_Object *emg_call_btn;
	Evas_Object *cancel_btn;
} s_lock_pwd_control_panel = {
	.control_panel_layout = NULL,
	.emg_call_btn = NULL,
	.cancel_btn = NULL,
};



static void _emg_call_btn_clicked_cb(void *data, Evas_Object *obj, const char *emission, const char *source)
{
	_D("%s", __func__);
	app_control_h service;
	Evas_Object *lock_pwd_win = NULL;

	lock_pwd_win = lock_pwd_util_win_get();
	ret_if(!lock_pwd_win);

	app_control_create(&service);
	ret_if(!service);

	lock_mgr_sound_play(LOCK_SOUND_TAP);

	if (APP_CONTROL_ERROR_NONE != app_control_set_operation(service, APP_CONTROL_OPERATION_DEFAULT)) {
		_E("Failed to set operation for app control handle");
		goto ERROR;
	}

	//@TODO: need to check appid
	if (APP_CONTROL_ERROR_NONE != app_control_set_app_id(service, "dialer-efl")) {
		_E("Failed to launch dialer-efl");
		goto ERROR;
	}

	app_control_add_extra_data(service, "emergency_dialer", "emergency");

	if (APP_CONTROL_ERROR_NONE != app_control_send_launch_request(service, NULL, NULL)) {
		_E("Failed to send launch request");
	}

	app_control_destroy(service);

	return;

ERROR:
	_E("Failed to launch emergency call");

	if (service) app_control_destroy(service);
}



static void _sliding_label_mode_set(Evas_Object *label)
{
	Evas_Object *label_edje = NULL;
	Evas_Object *tb = NULL;
	Evas_Coord tb_w = 0;

	ret_if(!label);

	elm_label_slide_mode_set(label, ELM_LABEL_SLIDE_MODE_NONE);

	label_edje = elm_layout_edje_get(label);
	ret_if(!label_edje);

	tb = (Evas_Object *)edje_object_part_object_get(label_edje, "elm.text");
	ret_if(!tb);

	evas_object_textblock_size_native_get(tb, &tb_w, NULL);
	_D("tb width(%d), label width(%f)", tb_w, ELM_SCALE_SIZE(EMG_BTN_WIDTH));

	if ((tb_w > 0) && (tb_w > ELM_SCALE_SIZE(EMG_BTN_WIDTH))) {
		elm_label_slide_mode_set(label, ELM_LABEL_SLIDE_MODE_AUTO);
	}

	elm_label_slide_go(label);
}


static Evas_Object *_sliding_label_create(Evas_Object *parent, const char *text)
{
	char buf[BUF_SIZE_512] = { 0, };
	char *markup_txt = NULL;
	Evas_Object *label = NULL;

	retv_if(!parent, NULL);

	label = elm_label_add(parent);
	retv_if(!label, NULL);

	markup_txt = elm_entry_utf8_to_markup(text);
	snprintf(buf, sizeof(buf), "%s%s%s", EMG_CALL_LABEL_STYLE_START, markup_txt, EMG_CALL_LABEL_STYLE_END);
	free(markup_txt);

	elm_object_style_set(label, "slide_short");
	elm_label_wrap_width_set(label, EMG_BTN_WIDTH);
	elm_label_slide_duration_set(label, 2);
	elm_object_text_set(label, buf);
	evas_object_show(label);

	_sliding_label_mode_set(label);

	return label;
}



static Evas_Object *_emg_call_btn_create(Evas_Object *parent)
{
	Evas_Object *btn = NULL;

	retv_if(!parent, NULL);

	btn = _sliding_label_create(parent, _("IDS_LCKSCN_BODY_EMERGENCY_CALL"));
	retv_if(!btn, NULL);

	elm_object_signal_callback_add(parent, "emg_button", "clicked", _emg_call_btn_clicked_cb, NULL);

	s_lock_pwd_control_panel.emg_call_btn = btn;

	return btn;
}



void lock_pwd_control_panel_cancel_btn_enable_set(Eina_Bool enable)
{
	ret_if(!s_lock_pwd_control_panel.control_panel_layout);

	if (enable) {
		elm_object_signal_emit(s_lock_pwd_control_panel.control_panel_layout, "button3,enable", "prog");
	} else {
		elm_object_signal_emit(s_lock_pwd_control_panel.control_panel_layout, "button3,disable", "prog");
	}
}



static void _cancel_btn_clicked_cb(void *data, Evas_Object *obj, void *event_info)
{
	_D("%s", __func__);
	lock_mgr_sound_play(LOCK_SOUND_TAP);

	lock_pwd_util_view_init();
}



static Evas_Object *_cancel_btn_create(Evas_Object *parent)
{
	Evas_Object *btn = NULL;

	retv_if(!parent, NULL);

	btn = elm_button_add(parent);
	retv_if(!btn, NULL);

	elm_theme_extension_add(NULL, LOCK_PWD_BTN_EDJE_FILE);

	elm_object_style_set(btn, "right_button");
	elm_object_text_set(btn, _("IDS_ST_BUTTON_CANCEL"));
	evas_object_smart_callback_add(btn, "clicked", (Evas_Smart_Cb)_cancel_btn_clicked_cb, NULL);

	evas_object_size_hint_weight_set(btn, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

	evas_object_show(btn);

	s_lock_pwd_control_panel.cancel_btn = btn;

	return btn;
}



Evas_Object *lock_pwd_control_panel_create(Evas_Object *parent)
{
	Evas_Object *control_panel_layout = NULL;
	Evas_Object *cancel_btn = NULL;
	Evas_Object *emg_call_btn = NULL;

	retv_if(!parent, NULL);

	control_panel_layout = elm_layout_add(parent);
	retv_if(!control_panel_layout, NULL);

	if (!elm_layout_file_set(control_panel_layout, LOCK_PWD_EDJE_FILE, "lock-control-panel")) {
		_E("Failed to set edje file : %s", LOCK_PWD_EDJE_FILE);
		goto ERROR;
	}
	s_lock_pwd_control_panel.control_panel_layout = control_panel_layout;

	evas_object_size_hint_weight_set(control_panel_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(control_panel_layout, EVAS_HINT_FILL, EVAS_HINT_FILL);

	/* create emg call button */
	emg_call_btn = _emg_call_btn_create(control_panel_layout);
	if (!emg_call_btn) {
		_E("Failed to create operator button");
	} else {
		elm_object_part_content_set(control_panel_layout, "button1", emg_call_btn);
	}

	/* create cancel button */
	cancel_btn = _cancel_btn_create(control_panel_layout);
	if (!cancel_btn) {
		_E("Failed to create cancel button");
	} else {
		elm_object_part_content_set(control_panel_layout, "button3", cancel_btn);
	}

	evas_object_show(control_panel_layout);

	return control_panel_layout;

ERROR:
	_E("Failed to create password control panel");

	if (control_panel_layout) {
		evas_object_del(control_panel_layout);
		s_lock_pwd_control_panel.control_panel_layout = NULL;
	}

	return NULL;
}

void lock_pwd_control_panel_del(void)
{
	if (s_lock_pwd_control_panel.cancel_btn) {
		evas_object_smart_callback_del(s_lock_pwd_control_panel.cancel_btn, "clicked", (Evas_Smart_Cb)_cancel_btn_clicked_cb);
		evas_object_del(s_lock_pwd_control_panel.cancel_btn);
		s_lock_pwd_control_panel.cancel_btn = NULL;
	}

	if (s_lock_pwd_control_panel.emg_call_btn) {
		elm_object_signal_callback_del(s_lock_pwd_control_panel.control_panel_layout, "emg_button", "clicked", _emg_call_btn_clicked_cb);
		evas_object_del(s_lock_pwd_control_panel.emg_call_btn);
		s_lock_pwd_control_panel.emg_call_btn = NULL;
	}

	if (s_lock_pwd_control_panel.control_panel_layout) {
		evas_object_del(s_lock_pwd_control_panel.control_panel_layout);
		s_lock_pwd_control_panel.control_panel_layout = NULL;
	}
}

void lock_pwd_control_panel_pause(void)
{
	if (s_lock_pwd_control_panel.emg_call_btn) {
		elm_label_slide_mode_set(s_lock_pwd_control_panel.emg_call_btn, ELM_LABEL_SLIDE_MODE_NONE);
		elm_label_slide_go(s_lock_pwd_control_panel.emg_call_btn);
	}
}

void lock_pwd_control_panel_resume(void)
{
	if (s_lock_pwd_control_panel.emg_call_btn) {
		elm_label_slide_mode_set(s_lock_pwd_control_panel.emg_call_btn, ELM_LABEL_SLIDE_MODE_AUTO);
		elm_label_slide_go(s_lock_pwd_control_panel.emg_call_btn);
	}
}
