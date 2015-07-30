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

#ifndef __STATUS_H__
#define __STATUS_H__

#include <Eina.h>

#ifndef VCONFKEY_STARTER_RESERVED_APPS_STATUS
/*  2 digits for reserved apps & popup */
/* 0x01 : reserved apps */
/* 0x10 : reserved popup */
#define VCONFKEY_STARTER_RESERVED_APPS_STATUS "memory/starter/reserved_apps_status"
#endif

#define VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT      VCONFKEY_SETAPPL_PREFIX"/phone_lock_attempts_left"

#define STATUS_DEFAULT_LOCK_PKG_NAME "org.tizen.lockscreen"

typedef enum {
	STATUS_ACTIVE_KEY_INVALID = -1,
	STATUS_ACTIVE_KEY_PM_STATE = 0,
	STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME,
	STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT,
	STATUS_ACTIVE_KEY_STARTER_SEQUENCE,
	STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS,
	STATUS_ACTIVE_KEY_LANGSET,
	STATUS_ACTIVE_KEY_BOOT_ANIMATION_FINISHED,
	STATUS_ACTIVE_KEY_MAX,
} status_active_key_e;

struct status_active_s {
	Eina_List *list[STATUS_ACTIVE_KEY_MAX];
	char *setappl_selected_package_name;
	int setappl_screen_lock_type_int;
	char *langset;
	int pm_state;
	int starter_sequence;
	int sysman_power_off_status;
	int boot_animation_finished;
};

struct status_passive_s {
	int wms_wakeup_by_gesture_setting;
	int setappl_ambient_mode_bool;

	int pm_key_ignore;
	int pm_state;
	int call_state;
	int idle_lock_state;
	int setappl_password_attempts_left_int;
	int remote_lock_islocked;
	int setappl_psmode;
	int starter_reserved_apps_status;
	int setappl_sound_lock_bool;
	int setappl_motion_activation;
	int setappl_use_pick_up;

	char *setappl_3rd_lock_pkg_name_str;
};
typedef struct status_passive_s *status_passive_h;
typedef struct status_active_s *status_active_h;
typedef int (*status_active_cb)(status_active_key_e key, void *data);

extern status_active_h status_active_get(void);
extern status_passive_h status_passive_get(void);

int status_active_register_cb(status_active_key_e key, status_active_cb func, void *data);
int status_active_unregister_cb(status_active_key_e key, status_active_cb func);

extern int status_register(void);
extern void status_unregister(void);

#endif //__STATUS_H__
