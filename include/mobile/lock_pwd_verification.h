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

#ifndef __LOCK_PWD_VERIFICATION_H__
#define __LOCK_PWD_VERIFICATION_H__

#include <vconf.h>

#define PASSWORD_ATTEMPTS_MAX_NUM 5
#define MAX_PASSWORD_NUM 4
#define INFINITE_ATTEMPT 0
#define PASSWORD_BLOCK_SECONDS 30
#define PASSWORD_TIMESTAMP_STR_LENGTH 512
#define VCONFKEY_SETAPPL_PASSWORD_TIMESTAMP_STR VCONFKEY_SETAPPL_PREFIX"/phone_lock_timestamp"

typedef enum {
	PWD_EVENT_CORRECT = 0,
	PWD_EVENT_INCORRECT_WARNING = 1,
	PWD_EVENT_INCORRECT,
	PWD_EVENT_INPUT_BLOCK_WARNING,
	PWD_EVENT_INPUT_BLOCK,
	PWD_EVENT_EMPTY,
	PWD_EVENT_OVER,
} lock_pwd_event_e;

typedef struct {
	unsigned int current_attempt;
	unsigned int block_attempt;
	unsigned int max_attempt;
	unsigned int expire_sec;
	unsigned int incorrect_count;
	void *data;
} lock_pwd_policy;

int lock_pwd_verification_current_attempt_get(void);
int lock_pwd_verification_remain_attempt_get(void);

lock_pwd_event_e lock_pwd_verification_verify(const char *password);
void lock_pwd_verification_policy_create(void);

void lock_pwd_verification_popup_create(lock_pwd_event_e event);

#endif
