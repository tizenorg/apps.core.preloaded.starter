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

#ifndef __DBUS_UTIL_H__
#define __DBUS_UTIL_H__

#include <E_DBus.h>

#define OOM_ADJ_VALUE_HOMESCREEN 0

#ifdef TIZEN_PROFILE_MOBILE
#define OOM_ADJ_VALUE_DEFAULT   200
#else
#define OOM_ADJ_VALUE_DEFAULT   0
#endif

#define DEVICED_BUS_NAME       "org.tizen.system.deviced"
#define DEVICED_OBJECT_PATH    "/Org/Tizen/System/DeviceD"
#define DEVICED_INTERFACE_NAME DEVICED_BUS_NAME
#define DEVICED_PATH		DEVICED_OBJECT_PATH"/Process"
#define DEVICED_INTERFACE	DEVICED_INTERFACE_NAME".Process"
#define DEVICED_SET_METHOD	"oomadj_set"

#define DISPLAY_OBJECT_PATH     DEVICED_OBJECT_PATH"/Display"
#define DEVICED_INTERFACE_DISPLAY  DEVICED_INTERFACE_NAME".display"
#define MEMBER_LCD_ON			"LCDOn"
#define MEMBER_LCD_OFF			"LCDOff"

extern int dbus_util_send_oomadj(int pid, int oom_adj_value);
extern void dbus_util_send_cpu_booster_signal(void);
extern void dbus_util_send_poweroff_signal(void);
extern void dbus_util_send_lock_PmQos_signal(void);

extern int dbus_util_receive_lcd_status(void (*changed_cb)(void *data, DBusMessage *msg), void *data);
extern char *dbus_util_msg_arg_get_str(DBusMessage *msg);

#endif //__DBUS_UTIL_H__
