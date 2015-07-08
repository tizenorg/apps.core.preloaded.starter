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
#include <app_control.h>
#include <bundle.h>
#include <aul.h>
#include <security-manager.h>
#include <vconf.h>

#include "lock_mgr.h"
#include "util.h"
#include "status.h"
#include "lock_pwd_util.h"
#include "lock_pwd_simple.h"
#include "lock_pwd_control_panel.h"

#define DOT_TIME 1.5
#define CORRECT_TIME 0.2

static struct _s_lock_pwd_simple {
	Evas_Object *pwd_simple_layout;
	Ecore_Timer *timer_dot;

	Eina_Bool is_blocked;

	char pwd_simple[MAX_PASSWORD_NUM +1];
	int pwd_simple_length;

	Ecore_Timer *timer_correct;
	Ecore_Timer *timer_pin;
	int pin_time_remain;
} s_lock_pwd_simple = {
	.pwd_simple_layout = NULL,
	.timer_dot = NULL,

	.is_blocked = EINA_FALSE,

	.pwd_simple = { 0, },
	.pwd_simple_length = 0,

	.timer_correct = NULL,
	.timer_pin = NULL,
	.pin_time_remain = PASSWORD_BLOCK_SECONDS,
};



Eina_Bool lock_pwd_simple_is_blocked_get(void)
{
	return s_lock_pwd_simple.is_blocked;
}




static void _pwd_simple_layout_title_set(const char *title)
{
	ret_if(!s_lock_pwd_simple.pwd_simple_layout);
	ret_if(!title);
	elm_object_part_text_set(s_lock_pwd_simple.pwd_simple_layout, "title", title);
}



static void _pwd_simple_backspace(int length)
{
	char buf[BUF_SIZE_32] = { 0, };
	ret_if(!s_lock_pwd_simple.pwd_simple_layout);

	snprintf(buf, sizeof(buf), "dot_hide%d", length);
	elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, buf, "keyboard");
	if (s_lock_pwd_simple.timer_dot) {
		ecore_timer_del(s_lock_pwd_simple.timer_dot);
		s_lock_pwd_simple.timer_dot = NULL;
	}
}



static Eina_Bool _hide_dot_cb(void *data)
{
	char buf[BUF_SIZE_32] = { 0, };
	retv_if(!s_lock_pwd_simple.pwd_simple_layout, ECORE_CALLBACK_CANCEL);

	snprintf(buf, sizeof(buf), "dot_show%d", (int)data);
	elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, buf, "keyboard");
	s_lock_pwd_simple.timer_dot = NULL;

	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_simple_input(int length, const char *text)
{
	char part_buf[BUF_SIZE_32] = { 0, };
	char signal_buf[BUF_SIZE_32] = { 0, };
	ret_if(!s_lock_pwd_simple.pwd_simple_layout);

	lock_mgr_sound_play(LOCK_SOUND_BTN_KEY);

	snprintf(part_buf, sizeof(part_buf), "panel%d", length);
	elm_object_part_text_set(s_lock_pwd_simple.pwd_simple_layout, part_buf, text);

	snprintf(signal_buf, sizeof(signal_buf), "input_show%d", length);
	elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, signal_buf, "keyboard");

	if (length > 0) {
		snprintf(signal_buf, sizeof(signal_buf), "dot_show%d", length-1);
		elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, signal_buf, "keyboard");
	}

	if (s_lock_pwd_simple.timer_dot) {
		ecore_timer_del(s_lock_pwd_simple.timer_dot);
		s_lock_pwd_simple.timer_dot = NULL;
	}

	if (length < MAX_PASSWORD_NUM-1) {
		s_lock_pwd_simple.timer_dot = ecore_timer_add(DOT_TIME, _hide_dot_cb, (void *)length);
	}
}



static void _pwd_simple_keypad_process(void *data, Evas_Object *obj, const char *emission, const char *source)
{
	_D("%s", __func__);

	if (s_lock_pwd_simple.is_blocked) {
		_E("blocked");
		lock_mgr_sound_play(LOCK_SOUND_BTN_KEY);
		return;
	}

	if (!strncmp("Backspace", source, strlen("Backspace"))) {
		_E("Backspace");
		lock_mgr_sound_play(LOCK_SOUND_BTN_KEY);
		ret_if(s_lock_pwd_simple.pwd_simple_length <= 0);
		_pwd_simple_backspace(--s_lock_pwd_simple.pwd_simple_length);
	} else {
		if (s_lock_pwd_simple.pwd_simple_length >= MAX_PASSWORD_NUM) {
			_E("Too long");
			return;
		} else {
			s_lock_pwd_simple.pwd_simple[s_lock_pwd_simple.pwd_simple_length] = *source;
			_pwd_simple_input(s_lock_pwd_simple.pwd_simple_length++, source);
		}
	}

	if (s_lock_pwd_simple.pwd_simple_length == MAX_PASSWORD_NUM) {
		lock_pwd_event_e pwd_event = lock_pwd_verification_verify(s_lock_pwd_simple.pwd_simple);
		lock_pwd_simple_event(pwd_event);
	}
}



Evas_Object *lock_pwd_simple_layout_create(void *data)
{
	Evas_Object *parent = NULL;
	Evas_Object *pwd_simple_layout = NULL;
	Evas_Object *pwd_control_panel = NULL;

	lock_pwd_verification_policy_create();

	parent = (Evas_Object *)data;
	retv_if(!parent, NULL);

	pwd_simple_layout = elm_layout_add(parent);
	goto_if(!pwd_simple_layout, ERROR);
	s_lock_pwd_simple.pwd_simple_layout = pwd_simple_layout;

	if (!elm_layout_file_set(pwd_simple_layout, LOCK_PWD_EDJE_FILE, "lock-simple-password")) {
		_E("Failed to set edje file : %s", LOCK_PWD_EDJE_FILE);
		goto ERROR;
	}

	evas_object_size_hint_weight_set(pwd_simple_layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
	evas_object_size_hint_align_set(pwd_simple_layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_show(pwd_simple_layout);

	elm_object_signal_callback_add(pwd_simple_layout, "keypad_down_clicked", "*", _pwd_simple_keypad_process, NULL);
	_pwd_simple_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));

	/* create control panel */
	pwd_control_panel = lock_pwd_control_panel_create(pwd_simple_layout);
	if (!pwd_control_panel) {
		_E("Failed to create password control panel");
	} else {
		elm_object_part_content_set(pwd_simple_layout, "control_panel", pwd_control_panel);
	}

	return pwd_simple_layout;

ERROR:
	_E("Failed to create simple password layout");

	if (pwd_simple_layout) {
		evas_object_del(pwd_simple_layout);
		s_lock_pwd_simple.pwd_simple_layout = NULL;
	}

	return NULL;
}



void lock_pwd_simple_layout_del(void)
{
	if (s_lock_pwd_simple.pwd_simple_layout) {
		evas_object_del(s_lock_pwd_simple.pwd_simple_layout);
		s_lock_pwd_simple.pwd_simple_layout = NULL;
	}

	if (s_lock_pwd_simple.timer_dot) {
		ecore_timer_del(s_lock_pwd_simple.timer_dot);
		s_lock_pwd_simple.timer_dot = NULL;
	}

	if (s_lock_pwd_simple.timer_pin) {
		ecore_timer_del(s_lock_pwd_simple.timer_pin);
		s_lock_pwd_simple.timer_pin = NULL;
	}
}



void lock_pwd_simple_entry_clear(void)
{
	int i = 0;
	char buf[BUF_SIZE_32] = { 0, };

	ret_if(!s_lock_pwd_simple.pwd_simple_layout);

	if (s_lock_pwd_simple.timer_dot) {
		ecore_timer_del(s_lock_pwd_simple.timer_dot);
		s_lock_pwd_simple.timer_dot = NULL;
	}

	for (i = 0; i <= 3; i++) {
		snprintf(buf, sizeof(buf), "dot_hide%d", i);
		elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, buf, "keyboard");
	}
	s_lock_pwd_simple.pwd_simple_length = 0;
}



static Eina_Bool _pwd_simple_entry_clear(void *data)
{
	lock_pwd_simple_entry_clear();
	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_simple_event_incorrect(lock_pwd_event_e event)
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
	_pwd_simple_layout_title_set(temp_str);

	ecore_timer_add(0.1, _pwd_simple_entry_clear, NULL);

	lock_pwd_verification_popup_create(event);
}



static Eina_Bool _pwd_correct_timer_cb(void *data)
{
	lock_pwd_util_win_hide();
	lock_pwd_simple_entry_clear();
	_pwd_simple_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));

	lock_mgr_idle_lock_state_set(VCONFKEY_IDLE_UNLOCK);
	lock_mgr_sound_play(LOCK_SOUND_UNLOCK);

	s_lock_pwd_simple.timer_correct = NULL;

	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_simple_event_correct(lock_pwd_event_e event)
{
	if (s_lock_pwd_simple.timer_correct) {
		ecore_timer_del(s_lock_pwd_simple.timer_correct);
	}

	s_lock_pwd_simple.timer_correct = ecore_timer_add(CORRECT_TIME, _pwd_correct_timer_cb, NULL);
	if(!s_lock_pwd_simple.timer_correct) {
		_E("Failed to add tiemr for correct password event");
	}
}



static void _pwd_simple_lock_time_init(void)
{
	if (vconf_set_str(VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR, "") < 0) {
		_E("Failed to set vconfkey : %s", VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR);
	}
}



static void _pwd_simple_lock_time_save(void)
{
	time_t cur_time = time(NULL);
	char buf[64] = { 0, };
	snprintf(buf, sizeof(buf), "%ld", cur_time);
	if (vconf_set_str(VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR, buf) < 0) {
		_E("Failed to set vconfkey : %s", VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR);
	}
}



static Eina_Bool _wrong_pwd_wait_timer_cb(void *data)
{
	char try_again_buf[BUF_SIZE_256] = { 0, };
	char incorrect_pass_buf[BUF_SIZE_256] = { 0, };

	retv_if(!s_lock_pwd_simple.pwd_simple_layout, ECORE_CALLBACK_CANCEL);

	snprintf(try_again_buf, sizeof(try_again_buf), _("IDS_LCKSCN_POP_TRY_AGAIN_IN_PD_SECONDS"), s_lock_pwd_simple.pin_time_remain);
	snprintf(incorrect_pass_buf, sizeof(incorrect_pass_buf), "%s<br>%s", _("IDS_COM_BODY_INCORRECT_PIN"), try_again_buf);
	_pwd_simple_layout_title_set(incorrect_pass_buf);

	if (s_lock_pwd_simple.pin_time_remain == PASSWORD_BLOCK_SECONDS ||
			s_lock_pwd_simple.pin_time_remain > 0) {
		s_lock_pwd_simple.pin_time_remain--;
		return ECORE_CALLBACK_RENEW;
	} else {
		lock_pwd_simple_view_init();

		int lcd_state = lock_mgr_lcd_state_get();
		if (lcd_state == LCD_STATE_OFF) {
			if (!lock_mgr_lockscreen_launch()) {
				_E("Failed to launch lockscreen");
			}
		}
	}

	return ECORE_CALLBACK_CANCEL;
}



static void _pwd_simple_event_input_block(lock_pwd_event_e event)
{
	_D("%s", __func__);

	int block_sec = PASSWORD_BLOCK_SECONDS;
	char try_again_buf[200] = { 0, };
	char incorrect_pass_buf[200] = { 0, };

	ret_if(!s_lock_pwd_simple.pwd_simple_layout);

	_pwd_simple_lock_time_save();

	snprintf(try_again_buf, sizeof(try_again_buf), _("IDS_LCKSCN_POP_TRY_AGAIN_IN_PD_SECONDS"), block_sec);
	snprintf(incorrect_pass_buf, sizeof(incorrect_pass_buf), "%s<br>%s", _("IDS_COM_BODY_INCORRECT_PIN"), try_again_buf);
	_pwd_simple_layout_title_set(incorrect_pass_buf);

	s_lock_pwd_simple.is_blocked = EINA_TRUE;

	if (s_lock_pwd_simple.timer_pin) {
		ecore_timer_del(s_lock_pwd_simple.timer_pin);
		s_lock_pwd_simple.timer_pin = NULL;
	}

	s_lock_pwd_simple.timer_pin = ecore_timer_add(1.0, _wrong_pwd_wait_timer_cb, NULL);

	ecore_timer_add(0.1, _pwd_simple_entry_clear, NULL);

	lock_pwd_verification_popup_create(event);

	lock_pwd_control_panel_cancel_btn_enable_set(EINA_FALSE);
}



void lock_pwd_simple_event(lock_pwd_event_e event)
{
	switch(event) {
	case PWD_EVENT_CORRECT:
		_pwd_simple_event_correct(event);
		break;
	case PWD_EVENT_INCORRECT_WARNING:
	case PWD_EVENT_INCORRECT:
		_pwd_simple_event_incorrect(event);
		break;
	case PWD_EVENT_INPUT_BLOCK_WARNING:
	case PWD_EVENT_INPUT_BLOCK:
		_pwd_simple_event_input_block(event);
		break;
	case PWD_EVENT_EMPTY:
		break;
	case PWD_EVENT_OVER:
		break;
	default:
		break;
	}
}

void lock_pwd_simple_view_init(void)
{
	_D("initialize simpel password values");
	s_lock_pwd_simple.pin_time_remain = PASSWORD_BLOCK_SECONDS;

	_pwd_simple_layout_title_set(_("IDS_COM_BODY_ENTER_PIN"));
	elm_object_signal_emit(s_lock_pwd_simple.pwd_simple_layout, "show_title", "title");
	s_lock_pwd_simple.is_blocked = EINA_FALSE;

	lock_pwd_simple_entry_clear();

	_pwd_simple_lock_time_init();

	lock_pwd_control_panel_cancel_btn_enable_set(EINA_TRUE);
}
