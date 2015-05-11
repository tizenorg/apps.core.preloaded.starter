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

#ifndef __LOCK_DAEMON_H__
#define __LOCK_DAEMON_H__

#include <Elementary.h>
#include <E_DBus.h>
#include <alarm.h>

#include "window_mgr.h"

#define _EDJ(x) elm_layout_edje_get(x)

#define LOCK_MGR_DEFAULT_PKG_NAME "org.tizen.lockscreen"

#ifdef TIZEN_BUILD_EMULATOR
#define LOCK_MGR_DEFAULT_BG_PATH "/opt/share/settings/Wallpapers/Default.jpg"
#else
#define LOCK_MGR_DEFAULT_BG_PATH "/opt/share/settings/Wallpapers/Lock_default.png"
#endif



typedef enum {
	LOCK_SOUND_LOCK,
	LOCK_SOUND_UNLOCK,
	LOCK_SOUND_BTN_KEY,
	LOCK_SOUND_TAP,
	LOCK_SOUND_MAX,
} lock_sound_type_e;

typedef enum {
	LCD_STATE_ON,
	LCD_STATE_OFF,
	LCD_STATE_MAX,
} lock_lcd_state_e;

int lock_mgr_lcd_state_get(void);
int lock_mgr_lock_pid_get(void);
void lock_mgr_sound_play(lock_sound_type_e type);

void lock_mgr_idle_lock_state_set(int lock_state);
Eina_Bool lock_mgr_lockscreen_launch(void);

int lock_mgr_daemon_start(void);
void lock_mgr_daemon_end(void);

#endif				/* __LOCK_DAEMON_H__ */
