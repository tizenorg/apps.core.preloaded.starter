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

#ifndef __LOCK_PWD_CONTROL_PANEL_H__
#define __LOCK_PWD_CONTROL_PANEL_H__

void lock_pwd_control_panel_emg_btn_text_update(void);
void lock_pwd_control_panel_cancel_btn_enable_set(Eina_Bool enable);

Evas_Object *lock_pwd_control_panel_create(Evas_Object *parent);
void lock_pwd_control_panel_del(void);

void lock_pwd_control_panel_pause(void);
void lock_pwd_control_panel_resume(void);
#endif
