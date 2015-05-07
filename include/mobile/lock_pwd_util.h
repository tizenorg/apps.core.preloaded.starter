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

#ifndef __LOCK_PWD_UTIL_H__
#define __LOCK_PWD_UTIL_H__

#define LOCK_PWD_EDJE_FILE "/usr/share/starter/res/edje/lock_pwd.edj"
#define LOCK_PWD_BTN_EDJE_FILE "/usr/share/starter/res/edje/lock_btn.edj"

int lock_pwd_util_win_width_get(void);
int lock_pwd_util_win_height_get(void);

void lock_pwd_util_create(Eina_Bool is_show);
void lock_pwd_util_del(void);

void lock_pwd_util_popup_create(char *title, char *text, Evas_Smart_Cb func, double timeout);

void lock_pwd_util_view_init(void);
void lock_pwd_util_back_key_relased(void);
Evas_Object *lock_pwd_util_win_get(void);
Eina_Bool lock_pwd_util_win_visible_get(void);
void lock_pwd_util_win_show(void);
void lock_pwd_util_win_hide(void);

#endif
