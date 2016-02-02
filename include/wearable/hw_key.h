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

#ifdef HAVE_WAYLAND
typedef enum {
	KEY_VOLUMEUP = 0,
	KEY_VOLUMEDOWN = 1,
	KEY_POWER = 2,
	KEY_MENU,
	KEY_HOME,
	KEY_BACK,
	KLEY_CAMERA,
	KEY_CONFIG,
	KEY_SEARCH,
	KEY_PLAYCD,
	KEY_PAUSECB,
	KEY_STOPCD,
	KEY_NEXTSONG,
	KEY_PRIVEIOUSSONG,
	KEY_REWIND,
	KEY_FASTFORWARD,
	KEY_MEDIA,
	KEY_PLAYPAUSE,
	KEY_MUTE,
	KEY_REC,
	KEY_CANCEL,
	KEY_SOFTBD,
	KEY_QUICKPANEL,
	KEY_TASKSWITCH,
	KEY_HOMEPAGE,
	KEY_WEBPAGE,
	KEY_MAIL,
	KEY_SCREENSAVER,
	KEY_BRIGHTNESSDOWN,
	KEY_BRIGHTNESSUP,
	KEY_VOICE,
	KEY_LANGUAGE,
	KEY_APPS,
	KEY_CONNECT,
	KEY_GAMEPLAY,
	KEY_VOICEWAKEUP_LPSD,
	KEY_VOICEWAKEUP,
	KEY_NAME_MAX,
} key_name_e;
#endif

extern void hw_key_destroy_window(void);
extern void hw_key_create_window(void);

// End of a file
