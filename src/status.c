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

#include <vconf.h>
#include <stdlib.h>
#include <string.h>

#include "status.h"
#include "util.h"

#define VCONFKEY_REMOTE_LOCK_ISLOCKED "db/private/org.tizen.wfmw/is_locked"
#define VCONFKEY_IDLE_SCREEN_SAFEMODE "memory/idle-screen/safemode"



typedef struct cb_info {
	status_active_cb func;
	void *data;
} cb_info_s;



static struct status_active_s s_status_active = {
	.list = {NULL, },
	.setappl_selected_package_name = NULL,
	.setappl_screen_lock_type_int = -1,
	.langset = NULL,
	.pm_state = -1,
	.starter_sequence = -1,
	.sysman_power_off_status = -1,
};



static struct status_passive_s s_status_passive = {
	.setup_wizard_state = -1,
	.wms_wakeup_by_gesture_setting = -1,
	.setup_wizard_first_boot = -1,

	.pm_key_ignore = -1,
	.call_state = -1,
	.idle_lock_state = -1,
	.setappl_password_attempts_left_int = -1,
	.remote_lock_islocked = -1,
	.setappl_psmode = -1,
	.starter_reserved_apps_status = -1,
	.setappl_sound_lock_bool = -1,
	.setappl_motion_activation = -1,
	.setappl_use_pick_up = -1,
	.setappl_accessibility_lock_time_int = -1,
	.idle_screen_safemode = -1,
	.boot_animation_finished = -1,
	.setappl_ambient_mode_bool = -1,

	.setappl_3rd_lock_pkg_name_str = NULL,
};



status_active_h status_active_get(void)
{
	return &s_status_active;
}



status_passive_h status_passive_get(void)
{
	return &s_status_passive;
}



int status_active_register_cb(status_active_key_e key, status_active_cb func, void *data)
{
	cb_info_s *info = NULL;

	retv_if(key <= STATUS_ACTIVE_KEY_INVALID, -1);
	retv_if(key >= STATUS_ACTIVE_KEY_MAX, -1);
	retv_if(!func, -1);

	info = calloc(1, sizeof(cb_info_s));
	retv_if(!info, -1);

	info->func = func;
	info->data = data;

	s_status_active.list[key] = eina_list_append(s_status_active.list[key], info);

	return 0;
}



int status_active_unregister_cb(status_active_key_e key, status_active_cb func)
{
	const Eina_List *l = NULL;
	const Eina_List *ln = NULL;
	cb_info_s *info = NULL;

	retv_if(key <= STATUS_ACTIVE_KEY_INVALID, -1);
	retv_if(key >= STATUS_ACTIVE_KEY_MAX, -1);
	retv_if(!func, -1);

	EINA_LIST_FOREACH_SAFE(s_status_active.list[key], l, ln, info) {
		if (func == info->func) {
			s_status_active.list[key] = eina_list_remove(s_status_active.list[key], info);
			free(info);
			return 0;
		}
	}

	_W("We cannot unregister the func. Because the list doesn't have it.");

	return 0;
}



static void _status_active_change_cb(keynode_t* node, void *data)
{
	const char *key_name = NULL;
	const Eina_List *l = NULL;
	cb_info_s *info = NULL;

	ret_if(!node);

	key_name = vconf_keynode_get_name(node);
	ret_if(!key_name);

	if (!strcmp(key_name, VCONFKEY_PM_STATE)) {
		s_status_active.pm_state = vconf_keynode_get_int(node);
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_PM_STATE], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_PM_STATE, info->data)) break;
		}
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME)) {
		char *tmp = vconf_keynode_get_str(node);
		free(s_status_active.setappl_selected_package_name);
		if (tmp) s_status_active.setappl_selected_package_name = strdup(tmp);
		else s_status_active.setappl_selected_package_name = NULL;
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_SETAPPL_SELECTED_PACKAGE_NAME, info->data)) break;
		}
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT)) {
		s_status_active.setappl_screen_lock_type_int = vconf_keynode_get_int(node);
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_SETAPPL_SCREEN_LOCK_TYPE_INT, info->data)) break;
		}
	} else if (!strcmp(key_name, VCONFKEY_STARTER_SEQUENCE)) {
		s_status_active.starter_sequence = vconf_keynode_get_int(node);
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_STARTER_SEQUENCE], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_STARTER_SEQUENCE, info->data)) break;
		}
	} else if (!strcmp(key_name, VCONFKEY_SYSMAN_POWER_OFF_STATUS)) {
		s_status_active.sysman_power_off_status = vconf_keynode_get_int(node);
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_SYSMAN_POWER_OFF_STATUS, info->data)) break;
		}
	} else if (!strcmp(key_name, VCONFKEY_LANGSET)) {
		char *tmp = vconf_keynode_get_str(node);
		free(s_status_active.langset);
		if (tmp) s_status_active.langset = strdup(tmp);
		else s_status_active.langset = NULL;
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_LANGSET], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_LANGSET, info->data)) break;
		}
#if 0
	} else if (!strcmp(key_name, )) {
		s_status_active. = vconf_keynode_get_int(node);
		EINA_LIST_FOREACH(s_status_active.list[STATUS_ACTIVE_KEY_], l, info) {
			continue_if(!info->func);
			if (0 == info->func(STATUS_ACTIVE_KEY_, info->data)) break;
		}
#endif
	}
}



static void _status_passive_change_cb(keynode_t* node, void *data)
{
	char *key_name = NULL;

	ret_if(!node);

	key_name = vconf_keynode_get_name(node);
	ret_if(!key_name);

#ifdef TIZEN_PROFILE_WEARABLE
	if (!strcmp(key_name, VCONFKEY_SETUP_WIZARD_STATE)) {
		s_status_passive.setup_wizard_state = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING)) {
		s_status_passive.wms_wakeup_by_gesture_setting = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_SETUP_WIZARD_FIRST_BOOT)) {
		s_status_passive.setup_wizard_first_boot = vconf_keynode_get_int(node);
	} else
#endif
	if (!strcmp(key_name, VCONFKEY_PM_KEY_IGNORE)) {
		s_status_passive.pm_key_ignore = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_CALL_STATE)) {
		s_status_passive.call_state = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_IDLE_LOCK_STATE)) {
		s_status_passive.idle_lock_state = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT)) {
		s_status_passive.setappl_password_attempts_left_int = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_REMOTE_LOCK_ISLOCKED)) {
		s_status_passive.remote_lock_islocked = vconf_keynode_get_bool(node);
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_PSMODE)) {
		s_status_passive.setappl_psmode = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_STARTER_RESERVED_APPS_STATUS)) {
		s_status_passive.starter_reserved_apps_status = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_IDLE_SCREEN_SAFEMODE)) {
		s_status_passive.idle_screen_safemode = vconf_keynode_get_int(node);
	} else if (!strcmp(key_name, VCONFKEY_BOOT_ANIMATION_FINISHED)) {
		s_status_passive.boot_animation_finished = vconf_keynode_get_int(node);
#if 0 //build error
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL)) {
		s_status_passive.setappl_ambient_mode_bool = vconf_keynode_get_int(node);
#endif
	} else if (!strcmp(key_name, VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR)) {
		char *tmp = vconf_keynode_get_str(node);
		free(s_status_passive.setappl_3rd_lock_pkg_name_str);
		if (tmp) s_status_passive.setappl_3rd_lock_pkg_name_str = strdup(tmp);
		else s_status_passive.setappl_3rd_lock_pkg_name_str = NULL;
#if 0
	} else if (!strcmp(key_name, )) {
		s_status_passive. = vconf_keynode_get_int(node);
#endif
	}
}



int status_register(void)
{
	_W("Register every events for Starter");

	/* Active events */
	if (vconf_notify_key_changed(VCONFKEY_PM_STATE, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_PM_STATE);
	} else if (vconf_get_int(VCONFKEY_PM_STATE, &s_status_active.pm_state) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_PM_STATE);
		s_status_active.pm_state = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME);
	} else if (!(s_status_active.setappl_selected_package_name = vconf_get_str(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME))) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME);
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT);
	} else if (vconf_get_int(VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT, &s_status_active.setappl_screen_lock_type_int) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT);
		s_status_active.setappl_screen_lock_type_int = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_STARTER_SEQUENCE, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_STARTER_SEQUENCE);
	} else if (vconf_get_int(VCONFKEY_STARTER_SEQUENCE, &s_status_active.starter_sequence) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_STARTER_SEQUENCE);
		s_status_active.starter_sequence = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	} else if (vconf_get_int(VCONFKEY_SYSMAN_POWER_OFF_STATUS, &s_status_active.sysman_power_off_status ) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
		s_status_active.sysman_power_off_status  = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_LANGSET, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_LANGSET);
	} else if (!(s_status_active.langset = vconf_get_str(VCONFKEY_LANGSET))) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_LANGSET);
	}

#if 0
	if (vconf_notify_key_changed(, _status_active_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", );
	} else if (vconf_get_int(, &s_status_active.) < 0) {
		_E("Failed to get vconfkey[%s]", );
		s_status_active. = -1;
	}
#endif

	/* Passive events */
#ifdef TIZEN_PROFILE_WEARABLE
	if (vconf_notify_key_changed(VCONFKEY_SETUP_WIZARD_STATE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETUP_WIZARD_STATE);
	} else if (vconf_get_int(VCONFKEY_SETUP_WIZARD_STATE, &s_status_passive.setup_wizard_state) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETUP_WIZARD_STATE);
		s_status_passive.setup_wizard_state = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING);
	} else if (vconf_get_int(VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING, &s_status_passive.wms_wakeup_by_gesture_setting) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING);
		s_status_passive.wms_wakeup_by_gesture_setting = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETUP_WIZARD_FIRST_BOOT, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETUP_WIZARD_FIRST_BOOT);
	} else if (vconf_get_int(VCONFKEY_SETUP_WIZARD_FIRST_BOOT, &s_status_passive.setup_wizard_first_boot) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETUP_WIZARD_FIRST_BOOT);
		s_status_passive.setup_wizard_first_boot = -1;
	}
#endif

	if (vconf_notify_key_changed(VCONFKEY_PM_KEY_IGNORE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_PM_KEY_IGNORE);
	} else if (vconf_get_int(VCONFKEY_PM_KEY_IGNORE, &s_status_passive.pm_key_ignore) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_PM_KEY_IGNORE);
		s_status_passive.pm_key_ignore = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_CALL_STATE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_CALL_STATE);
	} else if (vconf_get_int(VCONFKEY_CALL_STATE, &s_status_passive.call_state) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_CALL_STATE);
		s_status_passive.call_state = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_IDLE_LOCK_STATE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to regsiter add the callback for %s", VCONFKEY_IDLE_LOCK_STATE);
	} else if (vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &s_status_passive.idle_lock_state) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_IDLE_LOCK_STATE);
		s_status_passive.idle_lock_state = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT);
	} else if (vconf_get_int(VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT, &s_status_passive.setappl_password_attempts_left_int) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT);
		s_status_passive.setappl_password_attempts_left_int = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_REMOTE_LOCK_ISLOCKED, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_REMOTE_LOCK_ISLOCKED);
	} else if (vconf_get_bool(VCONFKEY_REMOTE_LOCK_ISLOCKED, &s_status_passive.remote_lock_islocked) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_REMOTE_LOCK_ISLOCKED);
		s_status_passive.remote_lock_islocked = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_PSMODE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_PSMODE);
	} else if (vconf_get_int(VCONFKEY_SETAPPL_PSMODE, &s_status_passive.setappl_psmode) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_PSMODE);
		s_status_passive.setappl_psmode = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_STARTER_RESERVED_APPS_STATUS, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_STARTER_RESERVED_APPS_STATUS);
	} else if (vconf_get_int(VCONFKEY_STARTER_RESERVED_APPS_STATUS, &s_status_passive.starter_reserved_apps_status) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_STARTER_RESERVED_APPS_STATUS);
		s_status_passive.starter_reserved_apps_status = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_SOUND_LOCK_BOOL, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_SOUND_LOCK_BOOL);
	} else if (vconf_get_bool(VCONFKEY_SETAPPL_SOUND_LOCK_BOOL, &s_status_passive.setappl_sound_lock_bool) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_SOUND_LOCK_BOOL);
		s_status_passive.setappl_sound_lock_bool = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_MOTION_ACTIVATION, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to regitster add the callback for %s", VCONFKEY_SETAPPL_MOTION_ACTIVATION);
	} else if (vconf_get_bool(VCONFKEY_SETAPPL_MOTION_ACTIVATION, &s_status_passive.setappl_motion_activation) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_MOTION_ACTIVATION);
		s_status_passive.setappl_motion_activation = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_USE_PICK_UP, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to regitster add the callback for %s", VCONFKEY_SETAPPL_USE_PICK_UP);
	} else if (vconf_get_bool(VCONFKEY_SETAPPL_USE_PICK_UP, &s_status_passive.setappl_use_pick_up) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_USE_PICK_UP);
		s_status_passive.setappl_use_pick_up = -1;
	}

#if 0 //build error
	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT);
	} else if (vconf_get_int(VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT, &s_status_passive.setappl_accessibility_lock_time_int) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT);
		s_status_passive.setappl_accessibility_lock_time_int = -1;
	}
#endif

	if (vconf_notify_key_changed(VCONFKEY_IDLE_SCREEN_SAFEMODE, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_IDLE_SCREEN_SAFEMODE);
	} else if (vconf_get_int(VCONFKEY_IDLE_SCREEN_SAFEMODE, &s_status_passive.idle_screen_safemode ) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_IDLE_SCREEN_SAFEMODE);
		s_status_passive.idle_screen_safemode  = -1;
	}

	if (vconf_notify_key_changed(VCONFKEY_BOOT_ANIMATION_FINISHED, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_BOOT_ANIMATION_FINISHED);
	} else if (vconf_get_int(VCONFKEY_BOOT_ANIMATION_FINISHED, &s_status_passive.boot_animation_finished) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_BOOT_ANIMATION_FINISHED);
		s_status_passive.boot_animation_finished = -1;
	}

#if 0 //build error
	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL);
	} else if (vconf_get_int(VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL, &s_status_passive.setappl_ambient_mode_bool) < 0) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL);
		s_status_passive.boot_animation_finished = -1;
	}
#endif

	if (vconf_notify_key_changed(VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR);
	} else if (!(s_status_passive.setappl_3rd_lock_pkg_name_str = vconf_get_str(VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR))) {
		_E("Failed to get vconfkey[%s]", VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR);
	}

#if 0
	if (vconf_notify_key_changed(, _status_passive_change_cb, NULL) < 0) {
		_E("Failed to register add the callback for %s", );
	} else if (vconf_get_int(, &s_status_passive.) < 0) {
		_E("Failed to get vconfkey[%s]", );
		s_status_passive. = -1;
	}
#endif

	return 0;
}



void status_unregister(void)
{
	/* Active events */
	if (vconf_ignore_key_changed(VCONFKEY_PM_STATE, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_PM_STATE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_SELECTED_PACKAGE_NAME);
	}
	free(s_status_active.setappl_selected_package_name);

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT);
	}

	if (vconf_ignore_key_changed(VCONFKEY_STARTER_SEQUENCE, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_STARTER_SEQUENCE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SYSMAN_POWER_OFF_STATUS, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SYSMAN_POWER_OFF_STATUS);
	}

	if (vconf_ignore_key_changed(VCONFKEY_LANGSET, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_LANGSET);
	}
	free(s_status_active.langset);

#if 0
	if (vconf_ignore_key_changed(, _status_active_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", );
	}
#endif

	/* Passive events */
#ifdef TIZEN_PROFILE_WEARABLE
	if (vconf_ignore_key_changed(VCONFKEY_SETUP_WIZARD_STATE, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETUP_WIZARD_STATE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_WMS_WAKEUP_BY_GESTURE_SETTING);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETUP_WIZARD_FIRST_BOOT, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETUP_WIZARD_FIRST_BOOT);
	}
#endif

	if (vconf_ignore_key_changed(VCONFKEY_PM_KEY_IGNORE, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_PM_KEY_IGNORE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_CALL_STATE, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_CALL_STATE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_IDLE_LOCK_STATE, _status_passive_change_cb) < 0) {
		_E("Faield to unregister the callback for %s", VCONFKEY_IDLE_LOCK_STATE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_PASSWORD_ATTEMPTS_LEFT_INT);
	}

	if (vconf_ignore_key_changed(VCONFKEY_REMOTE_LOCK_ISLOCKED, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_REMOTE_LOCK_ISLOCKED);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_PSMODE, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_PSMODE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_STARTER_RESERVED_APPS_STATUS, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_STARTER_RESERVED_APPS_STATUS);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_SOUND_LOCK_BOOL, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_SOUND_LOCK_BOOL);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_MOTION_ACTIVATION, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_MOTION_ACTIVATION);
	}

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_USE_PICK_UP, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_USE_PICK_UP);
	}

#if 0 //build error
	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_ACCESSIBILITY_LOCK_TIME_INT);
	}
#endif

	if (vconf_ignore_key_changed(VCONFKEY_IDLE_SCREEN_SAFEMODE, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_IDLE_SCREEN_SAFEMODE);
	}

	if (vconf_ignore_key_changed(VCONFKEY_BOOT_ANIMATION_FINISHED, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_BOOT_ANIMATION_FINISHED);
	}

#if 0 //build error
	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", VCONFKEY_SETAPPL_AMBIENT_MODE_BOOL);
	}
#endif

	if (vconf_ignore_key_changed(VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR, _status_passive_change_cb) < 0) {
		_E("Failed to unregister ther callback for %s", VCONFKEY_SETAPPL_3RD_LOCK_PKG_NAME_STR);
	}

#if 0
	if (vconf_ignore_key_changed(, _status_passive_change_cb) < 0) {
		_E("Failed to unregister the callback for %s", );
	}
#endif
}

