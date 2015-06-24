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

#ifndef __HW_KEY_H__
#define __HW_KEY_H__

#ifdef HVAE_X11
#define	KEY_VOLUMEUP	"XF86AudioRaiseVolume"
#define	KEY_VOLUMEDOWN	"XF86AudioLowerVolume"
#define	KEY_HOME	"XF86Home"
#define	KEY_CONFIG	"XF86Camera_Full"
#define	KEY_SEARCH	"XF86Search"
#define	KEY_MEDIA	"XF86AudioMedia"
#define	KEY_TASKSWITCH	"XF86TaskPane"
#define	KEY_WEBPAGE	"XF86WWW"
#define	KEY_MAIL	"XF86Mail"
#define	KEY_VOICE	"XF86Voice"
#define	KEY_APPS	"XF86Apps"
#define	KEY_CONNECT	"XF86Call"
#define KEY_BACK	"XF86Back"

extern void hw_key_destroy_window(void);
extern void hw_key_create_window(void);
#endif //HAVE_X11

#endif

// End of a file
