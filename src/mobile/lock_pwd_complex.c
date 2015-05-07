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
#include <Ecore.h>

#include "lock_mgr.h"
#include "util.h"
#include "lock_pwd_util.h"
#include "lock_pwd_complex.h"
#include "lock_pwd_control_panel.h"

#define IME_RESIZED 1
#define IME_DOWN 2

static struct _s_lock_pwd_complex {
	Evas_Object *pwd_complex_layout;
	Evas_Object *pwd_complex_entry;

	Eina_Bool is_blocked;
	Ecore_Timer *timer_pin;
	int pin_time_remain;
} s_lock_pwd_complex = {
	.pwd_complex_layout = NULL,
	.pwd_complex_entry = NULL,

	.is_blocked = EINA_FALSE,
	.timer_pin = NULL,
	.pin_time_remain = PASSWORD_BLOCK_SECONDS,
};




Eina_Bool lock_pwd_complex_is_blocked_get(void)
{
	return s_lock_pwd_complex.is_blocked;
}

static void _pwd_complex_layout_title_set(const char *title)
{
	ret_if(!s_lock_pwd_complex.pwd_complex_layout);
	ret_if(!title);

	elm_object_part_text_set(s_lock_pwd_complex.pwd_complex_layout, "title", title);
}



void lock_pwd_complex_entry_clear(void)
{
	ret_if(!s_lock_pwd_complex.pwd_complex_entry);

	elm_entry_entry_set(s_lock_pwd_complex.pwd_complex_entry, "");
}



static Eina_Bool _pwd_complex_entry_clear(void *data)
{
	lock_pwd_complex_entry_clear();
	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_complex_lock_time_init(void)
{
	if (vconf_set_str(VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR, "") < 0) {
		_E("Failed to set vconfkey : %s", VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR);
	}
}



static void _pwd_complex_lock_time_save(void)
{
	time_t cur_time = time(NULL);
	char buf[64] = { 0, };
	snprintf(buf, sizeof(buf), "%ld", cur_time);
	if (vconf_set_str(VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR, buf) < 0) {
		_E("Failed to set vconfkey : %s", VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR);
	}
}



static void _pwd_complex_event_correct(lock_pwd_event_e event)
{
	_D("%s", __func__);

	lock_pwd_util_win_hide();
	lock_pwd_complex_entry_clear();
	_pwd_complex_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));

	lock_mgr_idle_lock_state_set(VCONFKEY_IDLE_UNLOCK);
	lock_mgr_sound_play(LOCK_SOUND_UNLOCK);
}



static void _pwd_complex_event_incorrect(lock_pwd_event_e event)
{
	char temp_str[BUF_SIZE_256] = { 0, };
	char temp_left[BUF_SIZE_256] = { 0, };
	int remain_attempt = 0;

	remain_attempt = lock_pwd_verification_remain_attempt_get();
	_D("remain_attempt(%d)", remain_attempt);

	if (remain_attempt == 1) {
		strncpy(temp_left, _("IDS_IDLE_BODY_1_ATTEMPT_LEFT"), sizeof(temp_left));
		temp_left[sizeof(temp_left) - 1] = '\0';
	} else {
		snprintf(temp_left, sizeof(temp_left), _("IDS_IDLE_BODY_PD_ATTEMPTS_LEFT"), remain_attempt);
	}
	snprintf(temp_str, sizeof(temp_str), "%s<br>%s", _("IDS_COM_BODY_INCORRECT_PIN"), temp_left);
	_pwd_complex_layout_title_set(temp_str);

	ecore_timer_add(0.1, _pwd_complex_entry_clear, NULL);

	lock_pwd_verification_popup_create(event);
}



static Eina_Bool _wrong_pwd_wait_timer_cb(void *data)
{
	char try_again_buf[BUF_SIZE_256] = { 0, };
	char incorrect_pass_buf[BUF_SIZE_256] = { 0, };

	retv_if(!s_lock_pwd_complex.pwd_complex_layout, ECORE_CALLBACK_CANCEL);

	snprintf(try_again_buf, sizeof(try_again_buf), _("IDS_LCKSCN_POP_TRY_AGAIN_IN_PD_SECONDS"), s_lock_pwd_complex.pin_time_remain);
	snprintf(incorrect_pass_buf, sizeof(incorrect_pass_buf), "%s<br>%s", _("IDS_COM_BODY_INCORRECT_PIN"), try_again_buf);
	_pwd_complex_layout_title_set(incorrect_pass_buf);

	if (s_lock_pwd_complex.pin_time_remain == PASSWORD_BLOCK_SECONDS ||
			s_lock_pwd_complex.pin_time_remain > 0) {
		s_lock_pwd_complex.pin_time_remain--;
		return ECORE_CALLBACK_RENEW;
	} else {
		lock_pwd_complex_view_init();

		int lcd_state = lock_mgr_lcd_state_get();
		if (lcd_state == LCD_STATE_OFF) {
			lock_mgr_lockscreen_launch();
		}
	}

	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_complex_event_input_block(lock_pwd_event_e event)
{
	_D("%s", __func__);

	int block_sec = PASSWORD_BLOCK_SECONDS;
	char try_again_buf[BUF_SIZE_256] = { 0, };
	char incorrect_pass_buf[BUF_SIZE_256] = { 0, };

	ret_if(!s_lock_pwd_complex.pwd_complex_layout);

	_pwd_complex_lock_time_save();

	snprintf(try_again_buf, sizeof(try_again_buf), _("IDS_LCKSCN_POP_TRY_AGAIN_IN_PD_SECONDS"), block_sec);
	snprintf(incorrect_pass_buf, sizeof(incorrect_pass_buf), "%s<br>%s", _("IDS_COM_BODY_INCORRECT_PIN"), try_again_buf);
	_pwd_complex_layout_title_set(incorrect_pass_buf);

	s_lock_pwd_complex.is_blocked = EINA_TRUE;

	if (s_lock_pwd_complex.timer_pin) {
		ecore_timer_del(s_lock_pwd_complex.timer_pin);
		s_lock_pwd_complex.timer_pin = NULL;
	}

	s_lock_pwd_complex.timer_pin = ecore_timer_add(1.0, _wrong_pwd_wait_timer_cb, NULL);

	ecore_timer_add(0.1, _pwd_complex_entry_clear, NULL);

	lock_pwd_verification_popup_create(event);

	lock_pwd_control_panel_cancel_btn_enable_set(EINA_FALSE);
}



void lock_pwd_complex_event(lock_pwd_event_e event)
{
	switch(event) {
	case PWD_EVENT_CORRECT:
		_pwd_complex_event_correct(event);
		break;
	case PWD_EVENT_INCORRECT_WARNING:
	case PWD_EVENT_INCORRECT:
		_pwd_complex_event_incorrect(event);
		break;
	case PWD_EVENT_INPUT_BLOCK_WARNING:
	case PWD_EVENT_INPUT_BLOCK:
		_pwd_complex_event_input_block(event);
		break;
	case PWD_EVENT_EMPTY:
		break;
	case PWD_EVENT_OVER:
		break;
	default:
		break;
	}
}



static void _pwd_complex_enter_cb(void *data, Evas_Object *obj, void *event_info)
{
	ret_if(!obj);

	const char *password = elm_entry_entry_get(obj);

	lock_pwd_event_e pwd_event = lock_pwd_verification_verify(password);
	lock_pwd_complex_event(pwd_event);
}



Evas_Object *_pwd_complex_entry_create(void *data)
{
	Evas_Object *parent = NULL;
	Evas_Object *entry = NULL;

	parent = (Evas_Object *)data;
	goto_if(!parent, ERROR);

	entry = elm_entry_add(parent);
	goto_if(!entry, ERROR);

	elm_entry_single_line_set(entry, EINA_TRUE);
	elm_entry_password_set(entry, EINA_TRUE);
	elm_entry_entry_set(entry, "");
	elm_entry_cursor_end_set(entry);
	//elm_entry_scrollabled_set(entry, EINA_TRUE);
	elm_entry_cnp_mode_set(entry, ELM_CNP_MODE_PLAINTEXT);
	elm_entry_input_panel_layout_set(entry, ELM_INPUT_PANEL_LAYOUT_PASSWORD);
	elm_entry_input_panel_imdata_set(entry, "type=lockscreen", 15);
	elm_entry_input_panel_return_key_type_set(entry, ELM_INPUT_PANEL_RETURN_KEY_TYPE_DONE);

	evas_object_smart_callback_add(entry, "activated", _pwd_complex_enter_cb, NULL);

	evas_object_show(entry);

	return entry;

ERROR:
	_E("Failed to create entry");
	if (entry) {
		evas_object_del(entry);
		entry = NULL;
	}

	return NULL;
}



Evas_Object *lock_pwd_complex_layout_create(void *data)
{
	Evas_Object *parent = NULL;
	Evas_Object *pwd_complex_layout = NULL;
	Evas_Object *pwd_complex_entry = NULL;
	Evas_Object *pwd_control_panel = NULL;

	lock_pwd_verification_policy_create();

	parent = (Evas_Object *)data;
	retv_if(!parent, NULL);

	pwd_complex_layout = elm_layout_add(parent);
	goto_if(!pwd_complex_layout, ERROR);
	s_lock_pwd_complex.pwd_complex_layout = pwd_complex_layout;

	if (!elm_layout_file_set(pwd_complex_layout, LOCK_PWD_EDJE_FILE, "lock-complex-password")) {
		_E("Failed to set edje file : %s", LOCK_PWD_EDJE_FILE);
		goto ERROR;
	}

	evas_object_size_hint_weight_set(pwd_complex_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(pwd_complex_layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(pwd_complex_layout);

	/* create entry */
	pwd_complex_entry = _pwd_complex_entry_create(pwd_complex_layout);
	goto_if(!pwd_complex_entry, ERROR);
	s_lock_pwd_complex.pwd_complex_entry = pwd_complex_entry;

	elm_object_part_content_set(pwd_complex_layout, "entry", pwd_complex_entry);

	_pwd_complex_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));

	/* create control panel */
	pwd_control_panel = lock_pwd_control_panel_create(pwd_complex_layout);
	if (!pwd_control_panel) {
		_E("Failed to create password control panel");
	} else {
		elm_object_part_content_set(pwd_complex_layout, "control_panel", pwd_control_panel);
	}

	evas_object_show(pwd_complex_layout);

	return pwd_complex_layout;

ERROR:
	_E("Failed to create complex password layout");

	if (pwd_complex_entry) {
		evas_object_del(pwd_complex_entry);
		s_lock_pwd_complex.pwd_complex_entry = NULL;
	}

	if (pwd_complex_layout) {
		evas_object_del(pwd_complex_layout);
		s_lock_pwd_complex.pwd_complex_layout = NULL;
	}

	return NULL;
}


void lock_pwd_complex_layout_del(void)
{
	if (s_lock_pwd_complex.timer_pin) {
		ecore_timer_del(s_lock_pwd_complex.timer_pin);
		s_lock_pwd_complex.timer_pin = NULL;
	}

	if (s_lock_pwd_complex.pwd_complex_entry) {
		evas_object_del(s_lock_pwd_complex.pwd_complex_entry);
		s_lock_pwd_complex.pwd_complex_entry = NULL;
	}

	if (s_lock_pwd_complex.pwd_complex_layout) {
		evas_object_del(s_lock_pwd_complex.pwd_complex_layout);
		s_lock_pwd_complex.pwd_complex_layout = NULL;
	}
}



void lock_pwd_complex_view_init(void)
{
	_D("initialize complex password values");
	_pwd_complex_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));
	elm_object_signal_emit(s_lock_pwd_complex.pwd_complex_layout, "show_title", "title");
	s_lock_pwd_complex.is_blocked = EINA_FALSE;

	lock_pwd_complex_entry_clear();

	_pwd_complex_lock_time_init();

	lock_pwd_control_panel_cancel_btn_enable_set(EINA_TRUE);
}
