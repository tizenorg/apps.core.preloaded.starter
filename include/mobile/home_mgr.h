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

#include <bundle.h>
#include <sys/types.h>
#include <stdbool.h>

extern int home_mgr_get_home_pid(void);
extern int home_mgr_get_volume_pid(void);
extern int home_mgr_get_indicator_pid(void);
extern int home_mgr_get_quickpanel_pid(void);

extern void home_mgr_init(void *data);
extern void home_mgr_fini(void);

void home_mgr_relaunch_homescreen(void);
void home_mgr_relaunch_volume(void);
void home_mgr_relaunch_indicator(void);
void home_mgr_relaunch_quickpanel(void);
extern int home_mgr_open_home(const char *pkgname);

// End of a file
