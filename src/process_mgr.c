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
#include <aul.h>
#include <Ecore.h>
#include <syspopup_caller.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <bundle_internal.h>
#include <app_manager.h>
#include <app_control.h>

#include "process_mgr.h"
#include "util.h"
#include "dbus_util.h"

#define LOCKD_VOICE_CALL_PKG_NAME "org.tizen.call-ui"
#define LOCKD_VIDEO_CALL_PKG_NAME "org.tizen.vtmain"

#define NICE_VALUE_PWLOCK -5
#define NICE_VALUE_LOCKSCREEN -20



typedef struct _launch_info_s {
	char *appid;
	char *key;
	char *value;
	after_func afn;
	change_func cfn;
} launch_info_s;



static int _try_to_launch(const char *appid, const char *key, const char *value, after_func afn)
{
	int pid = -1;

	retv_if(!appid, -1);

	int ret = 0;
	app_control_h app_control_handle = NULL;
	app_context_h app_context_handle = NULL;

	ret = app_control_create(&app_control_handle);
	if (ret != APP_CONTROL_ERROR_NONE) {
		_E("Failed to create appcontrol");
		return -1;
	}

	ret = app_control_set_operation(app_control_handle, APP_CONTROL_OPERATION_DEFAULT);
	if (ret != APP_CONTROL_ERROR_NONE) {
		_E("Failed to set operation to appcontrol");
		app_control_destroy(app_control_handle);
		return -1;
	}

	if (key && value) {
		_D("key(%s), value(%s)", key, value);
		ret = app_control_add_extra_data(app_control_handle, key, value);
		if (ret != APP_CONTROL_ERROR_NONE) {
			_E("Failed to add extra data to appcontrol");
			app_control_destroy(app_control_handle);
			return -1;
		}
	}

	ret = app_control_set_app_id(app_control_handle, appid);
	if (ret != APP_CONTROL_ERROR_NONE) {
		_E("Failed to set app id to appcontrol");
		app_control_destroy(app_control_handle);
		return -1;
	}

	ret = app_control_send_launch_request(app_control_handle, NULL, NULL);
	if (ret != APP_CONTROL_ERROR_NONE) {
		_E("Failed to send launch request : %s", appid);
		app_control_destroy(app_control_handle);
		return -1;
	}

	app_control_destroy(app_control_handle);

	ret = app_manager_get_app_context(appid, &app_context_handle);
	if (ret != APP_MANAGER_ERROR_NONE) {
		_E("Failed to get app context : %s", appid);
		return -1;
	}

	ret = app_context_get_pid(app_context_handle, &pid);
	if (ret != APP_MANAGER_ERROR_NONE) {
		_E("Failed to get pid : %s", appid);
		app_context_destroy(app_context_handle);
		return -1;
	}

	app_context_destroy(app_context_handle);

	_D("Succeed to launch : %s(%d)", appid, pid);
	if (afn) afn(pid);

	return pid;
}



static Eina_Bool _must_launch_cb(void *data)
{
	launch_info_s *launch_info = data;
	int pid;

	retv_if(!launch_info, ECORE_CALLBACK_CANCEL);

	pid = _try_to_launch(launch_info->appid, launch_info->key, launch_info->value, launch_info->afn);
	if (pid > 0) {
		goto OUT;
	}

	retv_if(pid == AUL_R_ECOMM, ECORE_CALLBACK_RENEW);
	retv_if(pid == AUL_R_ETERMINATING, ECORE_CALLBACK_RENEW);
	if (pid == AUL_R_ERROR) {
		if (launch_info->cfn
			&& 0 == launch_info->cfn(launch_info->appid, launch_info->key, launch_info->value, (void *) launch_info->cfn, (void *) launch_info->afn))
		{
			_D("change func has set the next appid");
		} else {
			_E("change func has returned error");
		}
		goto OUT;
	}

	return ECORE_CALLBACK_RENEW;

OUT:
	free(launch_info->appid);
	free(launch_info->key);
	free(launch_info->value);
	free(launch_info);
	return ECORE_CALLBACK_CANCEL;
}



void process_mgr_must_launch(const char *appid, const char *key, const char *value, change_func cfn, after_func afn)
{
	Ecore_Timer *timer = NULL;
	launch_info_s *launch_info = NULL;
	int pid = -1;

	_D("Must launch : %s", appid);

	pid = _try_to_launch(appid, key, value, afn);
	if (pid > 0) return;

	_E("Failed the first try to launch %s", appid);

	launch_info = calloc(1, sizeof(launch_info_s));
	ret_if(!launch_info);

	if (appid) launch_info->appid = strdup(appid);
	if (key) launch_info->key = strdup(key);
	if (value) launch_info->value = strdup(value);
	launch_info->cfn = cfn;
	launch_info->afn = afn;

	timer = ecore_timer_add(0.1f, _must_launch_cb, launch_info);
	if (!timer) {
		_E("cannot add a timer for must_launch");
		free(launch_info->appid);
		free(launch_info->key);
		free(launch_info->value);
		free(launch_info);
	}
}



static int _try_to_open(const char *appid, after_func afn)
{
	int pid = -1;

	retv_if(!appid, -1);

	pid = aul_open_app(appid);
	if (pid > 0) {
		_D("Succeed to open %s", appid);
		if (afn) afn(pid);
	}

	return pid;
}



static Eina_Bool _must_open_cb(void *data)
{
	launch_info_s *launch_info = data;
	int pid;

	retv_if(!launch_info, ECORE_CALLBACK_CANCEL);

	pid = _try_to_open(launch_info->appid, launch_info->afn);
	if (pid > 0) {
		goto OUT;
	}

	retv_if(pid == AUL_R_ECOMM, ECORE_CALLBACK_RENEW);
	retv_if(pid == AUL_R_ETERMINATING, ECORE_CALLBACK_RENEW);
	if (pid == AUL_R_ERROR) {
		if (launch_info->cfn && 0 == launch_info->cfn(launch_info->appid, NULL, NULL, launch_info->cfn, launch_info->afn)) {
			_D("change func has set the next appid");
		} else {
			_E("change func has returned error");
		}
		goto OUT;
	}

	return ECORE_CALLBACK_RENEW;

OUT:
	free(launch_info->appid);
	free(launch_info);
	return ECORE_CALLBACK_CANCEL;
}



void process_mgr_must_open(const char *appid, change_func cfn, after_func afn)
{
	Ecore_Timer *timer = NULL;
	launch_info_s *launch_info = NULL;
	int pid = -1;

	_D("Must open %s", appid);

	pid = _try_to_open(appid, afn);
	if (pid > 0) return;

	_E("Failed the first try to open %s", appid);

	launch_info = calloc(1, sizeof(launch_info_s));
	ret_if(!launch_info);

	if (appid) launch_info->appid = strdup(appid);
	launch_info->cfn = cfn;
	launch_info->afn = afn;

	timer = ecore_timer_add(0.1f, _must_open_cb, launch_info);
	if (!timer) {
		_E("cannot add a timer for must_launch");
		free(launch_info->appid);
		free(launch_info);
	}
}



static int _try_to_syspopup_launch(const char *appid, const char *key, const char *value, after_func afn)
{
	int pid = -1;
	bundle *b = NULL;

	retv_if(!appid, -1);

	if (key) {
		b = bundle_create();
		bundle_add(b, key, value);
	}

	pid = aul_launch_app("org.tizen.volume", b);
	if (b) bundle_free(b);
	if (pid > 0) {
		_D("Succeed to launch %s", appid);
		if (afn) afn(pid);
	}

	return pid;
}



static Eina_Bool _must_syspopup_launch_cb(void *data)
{
	launch_info_s *launch_info = data;
	int pid;

	retv_if(!launch_info, ECORE_CALLBACK_CANCEL);

	pid = _try_to_syspopup_launch(launch_info->appid, launch_info->key, launch_info->value, launch_info->afn);
	if (pid > 0) {
		goto OUT;
	}

	retv_if(pid == AUL_R_ECOMM, ECORE_CALLBACK_RENEW);
	retv_if(pid == AUL_R_ETERMINATING, ECORE_CALLBACK_RENEW);
	if (pid == AUL_R_ERROR) {
		if (launch_info->cfn
			&& 0 == launch_info->cfn(launch_info->appid, launch_info->key, launch_info->value, launch_info->cfn, launch_info->afn)) {
			_D("change func has set the next appid");
		} else {
			_E("change func has returned error");
		}
		goto OUT;
	}

	return ECORE_CALLBACK_RENEW;

OUT:
	free(launch_info->appid);
	free(launch_info->key);
	free(launch_info->value);
	free(launch_info);
	return ECORE_CALLBACK_CANCEL;
}



void process_mgr_must_syspopup_launch(const char *appid, const char *key, const char *value, change_func cfn, after_func afn)
{
	Ecore_Timer *timer = NULL;
	launch_info_s *launch_info = NULL;
	int pid = -1;

	_D("Must launch %s", appid);

	pid = _try_to_syspopup_launch(appid, key, value, afn);
	if (pid > 0) return;

	_E("Failed the first try to launch %s", appid);

	launch_info = calloc(1, sizeof(launch_info_s));
	ret_if(!launch_info);

	if (appid) launch_info->appid = strdup(appid);
	if (key) launch_info->key = strdup(key);
	if (value) launch_info->value = strdup(value);
	launch_info->cfn = cfn;
	launch_info->afn = afn;

	timer = ecore_timer_add(0.1f, _must_syspopup_launch_cb, launch_info);
	if (!timer) {
		_E("cannot add a timer for must_launch");
		free(launch_info->appid);
		free(launch_info->key);
		free(launch_info->value);
		free(launch_info);
	}
}



static Eina_Bool _set_lock_priority_cb(void *data)
{
	int prio;

	prio = getpriority(PRIO_PROCESS, (pid_t)data);
	if (prio == NICE_VALUE_LOCKSCREEN) {
		_D("%s (%d: %d)\n", "setpriority Success", (pid_t)data, prio);
		return ECORE_CALLBACK_CANCEL;
	}

	if (setpriority(PRIO_PROCESS, (pid_t)data, NICE_VALUE_LOCKSCREEN) < 0 ) {
		_D("error : %d", errno);
		return ECORE_CALLBACK_CANCEL;
	}

	return ECORE_CALLBACK_RENEW;
}



static Eina_Bool _set_pwlock_priority_cb(void *data)
{
	int prio;

	prio = getpriority(PRIO_PROCESS, (pid_t)data);
	if (prio == NICE_VALUE_PWLOCK) {
		_D("%s (%d: %d)\n", "setpriority Success", (pid_t)data, prio);
		return ECORE_CALLBACK_CANCEL;
	}

	if (setpriority(PRIO_PROCESS, (pid_t)data, NICE_VALUE_PWLOCK) < 0 ) {
		_D("error : %d", errno);
		return ECORE_CALLBACK_CANCEL;
	}

	return ECORE_CALLBACK_RENEW;
}



int process_mgr_set_lock_priority(int pid)
{
	return !ecore_timer_add(1.0f, _set_lock_priority_cb, (void *)pid);
}



int process_mgr_set_pwlock_priority(int pid)
{
	return !ecore_timer_add(1.0f, _set_pwlock_priority_cb, (void *)pid);
}



void process_mgr_terminate_app(int pid, int state)
{
	_D("process_mgr_terminate_app,  state:%d\n", state);

	if (state == 1) {
		if (pid != 0) {
			_D("Terminate Lock app(pid : %d)", pid);
			aul_terminate_pid(pid);
		}
	}
}



extern int aul_kill_pid(int pid);
void process_mgr_kill_app(int pid)
{
	_D ("process_mgr_kill_app [pid:%d]..", pid);
	aul_kill_pid(pid);
}



int process_mgr_validate_app(int pid)
{
	char buf[BUF_SIZE_128] = {0, };

	/* Check pid is invalid. */
	if (aul_app_get_pkgname_bypid(pid, buf, sizeof(buf)) < 0) {
		_D("no such pkg by pid %d\n", pid);
	} else {
		_SECURE_D("appid = %s, pid = %d\n", buf, pid);
		if (aul_app_is_running(buf) == TRUE) {
			_D("%s [pid = %d] is running\n", buf, pid);
			return TRUE;
		} else {
			_SECURE_D("[pid = %d] is exist but %s is not running\n", pid, buf);
		}
	}

	return FALSE;
}



int process_mgr_validate_call(int pid)
{
	char buf[BUF_SIZE_128] = {0, };

	/* Check pid is invalid. */
	if (aul_app_get_pkgname_bypid(pid, buf, sizeof(buf)) < 0) {
		_D("no such pkg by pid %d", pid);
	} else {
		_SECURE_D("appid = %s, pid = %d", buf, pid);
		if ((!strncmp(buf, LOCKD_VOICE_CALL_PKG_NAME, strlen(buf)))
		    || (!strncmp(buf, LOCKD_VIDEO_CALL_PKG_NAME, strlen(buf)))) {
		    return TRUE;
		}
	}

	return FALSE;
}



