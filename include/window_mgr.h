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

#ifndef __WINDOW_MGR_H__
#define __WINDOW_MGR_H__

typedef struct _lockw_data lockw_data;

int window_mgr_get_focus_window_pid(void);
Eina_Bool window_mgr_set_prop(lockw_data * data, int lock_app_pid, void *event);
Eina_Bool window_mgr_set_effect(lockw_data * data, int lock_app_pid, void *event);

void window_mgr_set_scroll_prop(lockw_data * data, int lock_type);
void window_mgr_register_event(void *data, lockw_data * lockw,
			    Eina_Bool (*create_cb) (void *, int, void *),
			    Eina_Bool (*show_cb) (void *, int, void *),
			    Eina_Bool (*hide_cb) (void *, int, void *));
void window_mgr_unregister_event(lockw_data * lockw);

lockw_data *window_mgr_init(void);
void window_mgr_fini(lockw_data *lockw);

Evas_Object *window_mgr_pwd_lock_win_create(void);

#endif				/* __WINDOW_MGR_H__ */
