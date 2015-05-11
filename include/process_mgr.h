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

#ifndef __PROCESS_MGR_H__
#define __PROCESS_MGR_H__

typedef struct _process_mgr_s *process_mgr_h;

typedef void (*after_func)(int pid);
typedef int (*change_func)(const char *, const char *, const char *, void *, void *);

void process_mgr_must_launch(const char *appid, const char *key, const char *value, change_func cfn, after_func afn);
void process_mgr_must_open(const char *appid, change_func cfn, after_func afn);
void process_mgr_must_syspopup_launch(const char *appid, const char *key, const char *value, change_func cfn, after_func afn);

extern void process_mgr_terminate_app(int lock_app_pid, int state);
extern void process_mgr_kill_app(int lock_app_pid);

extern int process_mgr_validate_app(int pid);
extern int process_mgr_validate_call(int pid);

extern int process_mgr_set_lock_priority(int pid);
extern int process_mgr_set_pwlock_priority(int pid);

#endif /* __PROCESS_MGR_H__ */
