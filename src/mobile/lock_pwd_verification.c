/*
 * Copyright (c) 2009-2014 Samsung Electronics Co., Ltd All Rights Reserved
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
#include <security-manager.h>
#include <vconf.h>
#include <vconf-keys.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "util.h"
#include "status.h"
#include "lock_mgr.h"
#include "lock_pwd_util.h"
#include "lock_pwd_verification.h"
#include "lock_pwd_simple.h"

#define PASSWORD_LENGTH_MIN 4
#define PASSWORD_LENGTH_MAX 16

typedef enum {
	NORMAL_PWD = 0,
	EMPTY_PWD = 1,
	OVERLENGTH_PWD = 2,
} lock_pwd_type;

static struct _s_lock_pwd_verification {
	unsigned int current_attempt;
	unsigned int remain_attempt;
	unsigned int max_attempt;
	unsigned int expire_sec;
	unsigned int incorrect_count;
} s_lock_pwd_verification = {
	.current_attempt = 0,
	.remain_attempt = 0,
	.max_attempt = 0,
	.expire_sec = 0,
	.incorrect_count = 0,
};



int lock_pwd_verification_current_attempt_get(void)
{
	return s_lock_pwd_verification.current_attempt;
}


int lock_pwd_verification_remain_attempt_get(void)
{
	return s_lock_pwd_verification.remain_attempt;
}


static Eina_Bool _pwd_verification_check_pwd(const char *str)
{
#if 0
	int ret = SECURITY_SERVER_API_ERROR_PASSWORD_MISMATCH;
	unsigned int current_attempt = 0;
	unsigned int max_attempt = 0;
	unsigned int expire_sec = 0;

	ret = security_server_chk_pwd(str, &current_attempt, &max_attempt, &expire_sec);
	_D("ret(%d), current_attempt(%d), max_attempt(%d), valid_sec(%d)", ret, current_attempt, max_attempt, expire_sec);

	s_lock_pwd_verification.current_attempt = current_attempt;
	s_lock_pwd_verification.max_attempt = max_attempt;
	s_lock_pwd_verification.expire_sec = expire_sec;

	switch(ret) {
	case SECURITY_SERVER_API_SUCCESS:
	case SECURITY_SERVER_API_ERROR_PASSWORD_EXPIRED:
		_E("Correct password");
		return EINA_TRUE;
	case SECURITY_SERVER_API_ERROR_PASSWORD_RETRY_TIMER:
		_E("Timer set! : not saved");
		break;
	default:
		_E("Incorrect password");
		break;
	}
#endif

	return EINA_FALSE;
}


static lock_pwd_type _pwd_verification_check_length(const char *str, int min, int max)
{
	int len = 0;

	retv_if(!str, EMPTY_PWD);

	len = strlen(str);
	retv_if(len == 0, EMPTY_PWD);

	retv_if(len < min || len > max, OVERLENGTH_PWD);

	return NORMAL_PWD;
}

static void _pwd_values_init(void)
{
	s_lock_pwd_verification.current_attempt = 0;
	s_lock_pwd_verification.remain_attempt = PASSWORD_ATTEMPTS_MAX_NUM;
	s_lock_pwd_verification.max_attempt = PASSWORD_ATTEMPTS_MAX_NUM;
	s_lock_pwd_verification.incorrect_count = 0;
}

lock_pwd_event_e lock_pwd_verification_verify(const char *password)
{
	lock_pwd_type pwd_type = NORMAL_PWD;

	retv_if(!password, PWD_EVENT_EMPTY);

	pwd_type = _pwd_verification_check_length(password, PASSWORD_LENGTH_MIN, PASSWORD_LENGTH_MAX);
	switch(pwd_type) {
	case NORMAL_PWD:
		if (_pwd_verification_check_pwd(password)) {
			_D("Correct Password");
			_pwd_values_init();
			return PWD_EVENT_CORRECT;
		} else {
			s_lock_pwd_verification.incorrect_count++;
			s_lock_pwd_verification.remain_attempt--;
			_D("incorrect_count(%d), remain_attempt(%d)", s_lock_pwd_verification.incorrect_count, s_lock_pwd_verification.remain_attempt);

			if (s_lock_pwd_verification.remain_attempt == 0) {
				_pwd_values_init();
				return PWD_EVENT_INPUT_BLOCK;
			} else {
				return PWD_EVENT_INCORRECT;
			}
		}
		break;
	case EMPTY_PWD:
		return PWD_EVENT_EMPTY;
		break;
	case OVERLENGTH_PWD:
		return PWD_EVENT_OVER;
		break;
	}

	return PWD_EVENT_INCORRECT;
}

void lock_pwd_verification_policy_create(void)
{
#if 0
	int ret = 0;
	unsigned int current_attempt = 0;
	unsigned int max_attempt = 0;
	unsigned int expire_sec = 0;

	ret = security_server_is_pwd_valid(&current_attempt, &max_attempt, &expire_sec);
	_D("policy status(%d), current_attempt(%d), max_attempt(%d)", ret, current_attempt, max_attempt);

	if (ret == SECURITY_SERVER_API_ERROR_NO_PASSWORD ||
			ret == SECURITY_SERVER_API_ERROR_PASSWORD_EXIST) {
		s_lock_pwd_verification.current_attempt = current_attempt;
		s_lock_pwd_verification.max_attempt = max_attempt;
		s_lock_pwd_verification.expire_sec = expire_sec;
	}

	s_lock_pwd_verification.remain_attempt = PASSWORD_ATTEMPTS_MAX_NUM;
	s_lock_pwd_verification.incorrect_count = 0;
#endif

	return;
}

void lock_pwd_verification_popup_create(lock_pwd_event_e event)
{
	char popup_text[BUF_SIZE_512] = { 0, };
	int remain_attempt = 0;
	int current_attempt = 0;

	current_attempt = lock_pwd_verification_current_attempt_get();
	remain_attempt = lock_pwd_verification_remain_attempt_get();
	_D("current_attemp(%d), remain_attempt(%d)", current_attempt, remain_attempt);

	switch(event) {
	case PWD_EVENT_INCORRECT_WARNING:
		snprintf(popup_text, sizeof(popup_text), _("IDS_LCKSCN_POP_YOU_HAVE_ATTEMPTED_TO_UNLOCK_THE_DEVICE_INCORRECTLY_P1SD_P1SD_TIMES_YOU_HAVE_P2SD_ATTEMPTS_LEFT_BEFORE_THE_DEVICE_IS_RESET_THE_DEVICE_IS_RESET_TO_FACTORY_MSG"), current_attempt, remain_attempt);
		lock_pwd_util_popup_create(NULL, popup_text, NULL, 0.0);
		break;
	case PWD_EVENT_INPUT_BLOCK:
		snprintf(popup_text, sizeof(popup_text), _("IDS_LCKSCN_POP_YOU_HAVE_MADE_P1SD_UNSUCCESSFUL_ATTEMPTS_TO_UNLOCK_YOUR_DEVICE_TRY_AGAIN_IN_P2SD_SECONDS"), PASSWORD_ATTEMPTS_MAX_NUM, PASSWORD_BLOCK_SECONDS);
		lock_pwd_util_popup_create(_("IDS_LCKSCN_HEADER_UNABLE_TO_UNLOCK_SCREEN_ABB"), popup_text, NULL, 15.0);
		break;
	case PWD_EVENT_INPUT_BLOCK_WARNING:
		snprintf(popup_text, sizeof(popup_text), _("IDS_LCKSCN_POP_YOU_HAVE_ATTEMPTED_TO_UNLOCK_THE_DEVICE_INCORRECTLY_P1SD_TIMES_YOU_HAVE_P2SD_ATTEMPTS_LEFT_BEFORE_THE_DEVICE_IS_RESET_TO_FACTORY_MSG"), current_attempt, remain_attempt);
		lock_pwd_util_popup_create(NULL, popup_text, NULL, 0.0);
		break;
	default:
		break;
	}
}
