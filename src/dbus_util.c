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

#include <E_DBus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "dbus_util.h"
#include "util.h"

#define DBUS_HOME_BUS_NAME "org.tizen.coreapps.home"
#define DBUS_HOME_RAISE_PATH "/Org/Tizen/Coreapps/home/raise"
#define DBUS_HOME_RAISE_INTERFACE DBUS_HOME_BUS_NAME".raise"
#define DBUS_HOME_RAISE_MEMBER "homeraise"

#define DBUS_REPLY_TIMEOUT (120 * 1000)

#define POWEROFF_BUS_NAME       "org.tizen.system.popup"
#define POWEROFF_OBJECT_PATH    "/Org/Tizen/System/Popup/Poweroff"
#define POWEROFF_INTERFACE_NAME POWEROFF_BUS_NAME".Poweroff"
#define METHOD_POWEROFF_NAME	"PopupLaunch"

#define CPU_BOOSTER_OBJECT_PATH DEVICED_OBJECT_PATH"/PmQos"
#define CPU_BOOSTER_INTERFACE	DEVICED_BUS_NAME".PmQos"
#define METHOD_CPU_BOOSTER		"AppLaunchHome"
#define DBUS_CPU_BOOSTER_SEC	200

#define METHOD_LOCK_PMQOS_NAME	"LockScreen"
#define DBUS_LOCK_PMQOS_SEC (2 * 1000)

static struct _info {
	DBusConnection *connection;
} s_info = {
	.connection = NULL,
};



static DBusConnection *_dbus_connection_get(void)
{
	DBusError derror;
	DBusConnection *connection = NULL;

	if (s_info.connection) {
		return s_info.connection;
	}

	_W("no connection for dbus. get dbus connection");

	dbus_error_init(&derror);
	connection = dbus_bus_get_private(DBUS_BUS_SYSTEM, &derror);
	if (!connection) {
		_E("Failed to get dbus connection:%s", derror.message);
		dbus_error_free(&derror);
		return NULL;
	}
	dbus_connection_setup_with_g_main(connection, NULL);
	dbus_error_free(&derror);

	s_info.connection = connection;

	return s_info.connection;
}



static int _append_variant(DBusMessageIter *iter, const char *sig, char *param[])
{
	char *ch;
	int i;
	int int_type;
	uint64_t int64_type;

	if (!sig || !param)
		return 0;

	for (ch = (char*)sig, i = 0; *ch != '\0'; ++i, ++ch) {
		switch (*ch) {
		case 'i':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_INT32, &int_type);
			break;
		case 'u':
			int_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT32, &int_type);
			break;
		case 't':
			int64_type = atoi(param[i]);
			dbus_message_iter_append_basic(iter, DBUS_TYPE_UINT64, &int64_type);
			break;
		case 's':
			dbus_message_iter_append_basic(iter, DBUS_TYPE_STRING, &param[i]);
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}



static DBusMessage *_invoke_dbus_method_sync(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn = NULL;
	DBusMessage *msg;
	DBusMessageIter iter;
	DBusMessage *reply;
	DBusError err;
	int r;

	conn = (DBusConnection *)_dbus_connection_get();
	if (!conn) {
		_E("dbus_bus_get error");
		return NULL;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)", path, interface, method);
		return NULL;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = _append_variant(&iter, sig, param);
	if (r < 0) {
		_E("append_variant error(%d)", r);
		dbus_message_unref(msg);
		return NULL;
	}

	dbus_error_init(&err);

	reply = dbus_connection_send_with_reply_and_block(conn, msg, DBUS_REPLY_TIMEOUT, &err);
	dbus_message_unref(msg);
	if (!reply) {
		_E("dbus_connection_send error(%s:%s)", err.name, err.message);
		dbus_error_free(&err);
		return NULL;
	}

	return reply;
}



static int _invoke_dbus_method_async(const char *dest, const char *path,
		const char *interface, const char *method,
		const char *sig, char *param[])
{
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter iter;
	int r;

	conn = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!conn) {
		_E("dbus_bus_get error");
		return 0;
	}

	msg = dbus_message_new_method_call(dest, path, interface, method);
	if (!msg) {
		_E("dbus_message_new_method_call(%s:%s-%s)", path, interface, method);
		return 0;
	}

	dbus_message_iter_init_append(msg, &iter);
	r = _append_variant(&iter, sig, param);
	if (r < 0) {
		_E("append_variant error(%d)", r);
		dbus_message_unref(msg);
		return 0;
	}

	r = dbus_connection_send(conn, msg, NULL);
	dbus_message_unref(msg);
	if (r != TRUE) {
		_E("dbus_connection_send error(%s:%s:%s-%s)", dest, path, interface, method);
		return 0;
	}

	_D("dbus_connection_send, ret=%d", r);
	return 1;
}



static int _dbus_message_send(const char *path, const char *interface, const char *member)
{
	int ret = 0;
	DBusMessage *msg = NULL;
	DBusConnection *conn = NULL;

	conn = (DBusConnection *)_dbus_connection_get();
	if (!conn) {
		_E("dbus_bus_get error");
		return -1;
	}

	msg = dbus_message_new_signal(path, interface, member);
	if (!msg) {
		_E("dbus_message_new_signal(%s:%s-%s)", path, interface, member);
		return -1;
	}

	ret = dbus_connection_send(conn, msg, NULL); //async call
	dbus_message_unref(msg);
	if (ret != TRUE) {
		_E("dbus_connection_send error(%s:%s-%s)", path, interface, member);
		return -ECOMM;
	}
	_D("dbus_connection_send, ret=%d", ret);
	return 0;
}



void dbus_util_send_home_raise_signal(void)
{
	int ret = 0;

	ret = _dbus_message_send(
			DBUS_HOME_RAISE_PATH,
			DBUS_HOME_RAISE_INTERFACE,
			DBUS_HOME_RAISE_MEMBER);
	_E("Sending HOME RAISE signal, result:%d", ret);
}



int dbus_util_send_oomadj(int pid, int oom_adj_value)
{
	DBusError err;
	DBusMessage *msg;
	char *pa[4];
	char buf1[BUF_SIZE_16];
	char buf2[BUF_SIZE_16];
	int ret, val;

	if(pid <= 0){
		_E("Pid is invalid");
		return -1;
	}

	snprintf(buf1, sizeof(buf1), "%d", pid);
	snprintf(buf2, sizeof(buf2), "%d", oom_adj_value);

	pa[0] = DEVICED_SET_METHOD;
	pa[1] = "2";
	pa[2] = buf1;
	pa[3] = buf2;

	msg = _invoke_dbus_method_sync(DEVICED_BUS_NAME, DEVICED_PATH, DEVICED_INTERFACE, DEVICED_SET_METHOD, "siss", pa);
	if (!msg)
		return -EBADMSG;

	dbus_error_init(&err);

	ret = dbus_message_get_args(msg, &err, DBUS_TYPE_INT32, &val, DBUS_TYPE_INVALID);
	if (!ret) {
		_E("no message : [%s:%s]", err.name, err.message);
		val = -EBADMSG;
	}

	dbus_message_unref(msg);
	dbus_error_free(&err);

	_D("%s-%s : %d", DEVICED_INTERFACE, pa[0], val);
	return val;
}



void dbus_util_send_cpu_booster_signal(void)
{
	int ret = 0;
	char *arr[1];
	char val[BUF_SIZE_32];

	snprintf(val, sizeof(val), "%d", DBUS_CPU_BOOSTER_SEC);
	arr[0] = val;

	ret = _invoke_dbus_method_async(DEVICED_BUS_NAME, CPU_BOOSTER_OBJECT_PATH, CPU_BOOSTER_INTERFACE,
			METHOD_CPU_BOOSTER, "i", arr);
	ret_if(!ret);

	_D("%s-%s", CPU_BOOSTER_INTERFACE, METHOD_CPU_BOOSTER);
}



void dbus_util_send_poweroff_signal(void)
{
	int ret = 0;

	ret = _invoke_dbus_method_async(POWEROFF_BUS_NAME, POWEROFF_OBJECT_PATH, POWEROFF_INTERFACE_NAME,
			METHOD_POWEROFF_NAME, NULL, NULL);
	ret_if(!ret);

	_D("%s-%s", POWEROFF_INTERFACE_NAME, METHOD_POWEROFF_NAME);
}



void dbus_util_send_lock_PmQos_signal(void)
{
	int ret = 0;

	char *arr[1];
	char val[BUF_SIZE_32];

	snprintf(val, sizeof(val), "%d", DBUS_LOCK_PMQOS_SEC);
	arr[0] = val;

	ret = _invoke_dbus_method_async(DEVICED_BUS_NAME, CPU_BOOSTER_OBJECT_PATH, CPU_BOOSTER_INTERFACE,
			METHOD_LOCK_PMQOS_NAME, "i", arr);
	ret_if(!ret);

	_D("%s-%s", CPU_BOOSTER_INTERFACE, METHOD_LOCK_PMQOS_NAME);
}



int dbus_util_receive_lcd_status(void (*changed_cb)(void *data, DBusMessage *msg), void *data)
{
	E_DBus_Connection *conn;
	E_DBus_Signal_Handler *handler;

	e_dbus_init();

	conn = e_dbus_bus_get(DBUS_BUS_SYSTEM);
	if (conn == NULL) {
		_E("e_dbus_bus_get error");
		return 0;
	}

	handler = e_dbus_signal_handler_add(conn, NULL, DISPLAY_OBJECT_PATH,
								DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_ON,
								changed_cb, data);
	if (handler == NULL) {
		_E("e_dbus_signal_handler_add error");
		return 0;
	}

	handler = e_dbus_signal_handler_add(conn, NULL, DISPLAY_OBJECT_PATH,
								DEVICED_INTERFACE_DISPLAY, MEMBER_LCD_OFF,
								changed_cb, data);
	if (handler == NULL) {
		_E("e_dbus_signal_handler_add error");
		return 0;
	}

	return 1;
}

char *dbus_util_msg_arg_get_str(DBusMessage *msg)
{
	int ret = 0;
	DBusError derror;
	const char *state = NULL;
	dbus_error_init(&derror);

	ret = dbus_message_get_args(msg, &derror, DBUS_TYPE_STRING, &state, DBUS_TYPE_INVALID);
	goto_if(!ret, ERROR);

	dbus_error_free(&derror);

	return strdup(state);

ERROR:
	_E("Failed to get reply (%s:%s)", derror.name, derror.message);
	dbus_error_free(&derror);

	return NULL;
}



