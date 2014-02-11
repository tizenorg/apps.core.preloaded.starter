 /*
  * Copyright 2012  Samsung Electronics Co., Ltd
  *
  * Licensed under the Flora License, Version 1.1 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  *     http://floralicense.org/license/
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  */



#include <Elementary.h>

#include <vconf.h>
#include <vconf-keys.h>

#include <glib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>
#include <errno.h>

/* For multi-user support */
#include <tzplatform_config.h>

#include "lockd-debug.h"
#include "lock-daemon.h"
#include "lockd-process-mgr.h"
#include "lockd-window-mgr.h"
#include "starter-util.h"
#include "menu_daemon.h"

static int phone_lock_pid;

struct lockd_data {
	int lock_app_pid;
	int phone_lock_app_pid;
	int lock_type;
	Eina_Bool request_recovery;
	lockw_data *lockw;
	GPollFD *gpollfd;
};

#define PHLOCK_SOCK_PREFIX "/tmp/phlock"
#define PHLOCK_SOCK_MAXBUFF 65535
#define PHLOCK_APP_CMDLINE tzplatform_mkpath(TZ_SYS_RO_APP, "org.tizen.lockscreen/bin/lockscreen")
#define PHLOCK_UNLOCK_CMD "unlock"
#define PHLOCK_LAUNCH_CMD "launch_phone_lock"
#define LAUNCH_INTERVAL 100*1000

static int lockd_launch_app_lockscreen(struct lockd_data *lockd);

static void lockd_unlock_lockscreen(struct lockd_data *lockd);

static int _lockd_get_lock_type(void)
{
	int lock_type = 0;
	int ret = 0;

	vconf_get_int(VCONFKEY_SETAPPL_SCREEN_LOCK_TYPE_INT, &lock_type);

	if (lock_type == SETTING_SCREEN_LOCK_TYPE_PASSWORD ||
		lock_type == SETTING_SCREEN_LOCK_TYPE_SIMPLE_PASSWORD) {
		ret = 1;
	} else if (lock_type == SETTING_SCREEN_LOCK_TYPE_SWIPE ||
		lock_type == SETTING_SCREEN_LOCK_TYPE_MOTION) {
		ret = 0;
	} else {
		ret = 2;
	}

	LOCKD_DBG("_lockd_get_lock_type ret(%d), lock_type (%d)", ret, lock_type);

	return ret;
}

static void _lockd_notify_pm_state_cb(keynode_t * node, void *data)
{
	LOCKD_DBG("PM state Notification!!");

	struct lockd_data *lockd = (struct lockd_data *)data;
	int val = -1;

	if (lockd == NULL) {
		LOCKD_ERR("lockd is NULL");
		return;
	}

	if (vconf_get_int(VCONFKEY_PM_STATE, &val) < 0) {
		LOCKD_ERR("Cannot get VCONFKEY_PM_STATE");
		return;
	}

	if (val == VCONFKEY_PM_STATE_LCDOFF) {
		lockd->lock_type = _lockd_get_lock_type();
		lockd_launch_app_lockscreen(lockd);
	}
}

static void
_lockd_notify_lock_state_cb(keynode_t * node, void *data)
{
	LOCKD_DBG("lock state changed!!");

	struct lockd_data *lockd = (struct lockd_data *)data;
	int val = -1;

	if (lockd == NULL) {
		LOCKD_ERR("lockd is NULL");
		return;
	}

	if (vconf_get_int(VCONFKEY_IDLE_LOCK_STATE, &val) < 0) {
		LOCKD_ERR("Cannot get VCONFKEY_IDLE_LOCK_STATE");
		return;
	}

	if (val == VCONFKEY_IDLE_UNLOCK) {
		LOCKD_DBG("unlocked..!!");
		if (lockd->lock_app_pid != 0) {
			LOCKD_DBG("terminate lock app..!!");
			lockd_process_mgr_terminate_lock_app(lockd->lock_app_pid, 1);
		}
	}
}

static Eina_Bool lockd_set_lock_state_cb(void *data)
{
	LOCKD_DBG("%s, %d", __func__, __LINE__);
	vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, VCONFKEY_IDLE_LOCK);
	return ECORE_CALLBACK_CANCEL;
}

static void
_lockd_notify_phone_lock_verification_cb(keynode_t * node, void *data)
{
	LOCKD_DBG("%s, %d", __func__, __LINE__);

	struct lockd_data *lockd = (struct lockd_data *)data;
	int val = -1;

	if (lockd == NULL) {
		LOCKD_ERR("lockd is NULL");
		return;
	}

	if (vconf_get_bool(VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION, &val) < 0) {
		LOCKD_ERR("Cannot get %s", VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION);
		return;
	}

	if (val == TRUE) {
		lockd_window_mgr_finish_lock(lockd->lockw);
		vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, VCONFKEY_IDLE_UNLOCK);
	}
}

static int lockd_app_dead_cb(int pid, void *data)
{
	LOCKD_DBG("app dead cb call! (pid : %d)", pid);

	struct lockd_data *lockd = (struct lockd_data *)data;

	if (pid == lockd->lock_app_pid) {
		LOCKD_DBG("lock app(pid:%d) is destroyed.", pid);

		lockd_unlock_lockscreen(lockd);
	}

	menu_daemon_check_dead_signal(pid);

	return 0;

}

static Eina_Bool lockd_app_create_cb(void *data, int type, void *event)
{
	struct lockd_data *lockd = (struct lockd_data *)data;

	if (lockd == NULL) {
		return ECORE_CALLBACK_PASS_ON;
	}
	LOCKD_DBG("%s, %d", __func__, __LINE__);
	if (lockd_window_set_window_effect(lockd->lockw, lockd->lock_app_pid,
				       event) == EINA_TRUE) {
		if(lockd_window_set_window_property(lockd->lockw, lockd->lock_app_pid,
						 event) == EINA_FALSE) {
			LOCKD_ERR("window is not matched..!!");
		}
	}
	return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool lockd_app_show_cb(void *data, int type, void *event)
{
	struct lockd_data *lockd = (struct lockd_data *)data;

	if (lockd == NULL) {
		return EINA_TRUE;
	}
	LOCKD_DBG("%s, %d", __func__, __LINE__);
	if (lockd_window_set_window_property(lockd->lockw, lockd->lock_app_pid,
				event)) {
		ecore_idler_add(lockd_set_lock_state_cb, NULL);
	}
	return EINA_FALSE;
}

static int lockd_launch_app_lockscreen(struct lockd_data *lockd)
{
	LOCKD_DBG("launch app lock screen");

	int call_state = -1, phlock_state = -1, factory_mode = -1, test_mode = -1;
	int r = 0;

	WRITE_FILE_LOG("%s", "Launch lockscreen in starter");

	if (lockd_process_mgr_check_lock(lockd->lock_app_pid) == TRUE) {
		LOCKD_DBG("Lock Screen App is already running.");
		r = lockd_process_mgr_restart_lock(lockd->lock_type);
		if (r < 0) {
			LOCKD_DBG("Restarting Lock Screen App is fail [%d].", r);
			usleep(LAUNCH_INTERVAL);
		} else {
			LOCKD_DBG("Restarting Lock Screen App, pid[%d].", r);
			return 1;
		}
	}

	vconf_get_int(VCONFKEY_CALL_STATE, &call_state);
	if (call_state != VCONFKEY_CALL_OFF) {
		LOCKD_DBG
		    ("Current call state(%d) does not allow to launch lock screen.",
		     call_state);
		return 0;
	}

	lockd->lock_app_pid =
		lockd_process_mgr_start_lock(lockd, lockd_app_dead_cb,
				lockd->lock_type);
	if (lockd->lock_app_pid < 0)
		return 0;
	lockd_window_mgr_finish_lock(lockd->lockw);
	lockd_window_mgr_ready_lock(lockd, lockd->lockw, lockd_app_create_cb,
			lockd_app_show_cb);

	return 1;
}

static void lockd_unlock_lockscreen(struct lockd_data *lockd)
{
	LOCKD_DBG("unlock lock screen");
	lockd->lock_app_pid = 0;

	vconf_set_int(VCONFKEY_IDLE_LOCK_STATE, VCONFKEY_IDLE_UNLOCK);
	lockd_window_mgr_finish_lock(lockd->lockw);
}

inline static void lockd_set_sock_option(int fd, int cli)
{
	int size;
	int ret;
	struct timeval tv = { 1, 200 * 1000 };

	size = PHLOCK_SOCK_MAXBUFF;
	ret = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
	if(ret != 0)
		return;
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	if(ret != 0)
		return;
	if (cli) {
		ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		if(ret != 0)
			return;
	}
}

static int lockd_create_sock(void)
{
	struct sockaddr_un saddr;
	int fd;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		if (errno == EINVAL) {
			fd = socket(AF_UNIX, SOCK_STREAM, 0);
			if (fd < 0) {
				LOCKD_DBG
				    ("second chance - socket create error");
				return -1;
			}
		} else {
			LOCKD_DBG("socket error");
			return -1;
		}
	}

	bzero(&saddr, sizeof(saddr));
	saddr.sun_family = AF_UNIX;

	strncpy(saddr.sun_path, PHLOCK_SOCK_PREFIX, strlen(PHLOCK_SOCK_PREFIX));
	saddr.sun_path[strlen(PHLOCK_SOCK_PREFIX)] = 0;

	unlink(saddr.sun_path);

	if (bind(fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
		LOCKD_DBG("bind error");
		close(fd);
		return -1;
	}

	if (chmod(saddr.sun_path, (S_IRWXU | S_IRWXG | S_IRWXO)) < 0) {
		LOCKD_DBG("failed to change the socket permission");
		close(fd);
		return -1;
	}

	lockd_set_sock_option(fd, 0);

	if (listen(fd, 10) == -1) {
		LOCKD_DBG("listen error");
		close(fd);
		return -1;
	}

	return fd;
}

static gboolean lockd_glib_check(GSource * src)
{
	GSList *fd_list;
	GPollFD *tmp;

	fd_list = src->poll_fds;
	do {
		tmp = (GPollFD *) fd_list->data;
		if ((tmp->revents & (POLLIN | POLLPRI)))
			return TRUE;
		fd_list = fd_list->next;
	} while (fd_list);

	return FALSE;
}

static char *lockd_read_cmdline_from_proc(int pid)
{
	int memsize = 32;
	char path[32];
	char *cmdline = NULL, *tempptr = NULL;
	FILE *fp = NULL;

	snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);

	fp = fopen(path, "r");
	if (fp == NULL) {
		LOCKD_DBG("Cannot open cmdline on pid[%d]", pid);
		return NULL;
	}

	cmdline = malloc(32);
	if (cmdline == NULL) {
		LOCKD_DBG("%s", "Out of memory");
		fclose(fp);
		return NULL;
	}

	bzero(cmdline, memsize);
	if (fgets(cmdline, 32, fp) == NULL) {
		LOCKD_DBG("%s", "Cannot read cmdline");
		free(cmdline);
		fclose(fp);
		return NULL;
	}

	while (cmdline[memsize - 2] != 0) {
		cmdline[memsize - 1] = (char)fgetc(fp);
		tempptr = realloc(cmdline, memsize + 32);
		if (tempptr == NULL) {
			fclose(fp);
			LOCKD_DBG("%s", "Out of memory");
			return NULL;
		}
		cmdline = tempptr;
		bzero(cmdline + memsize, 32);
		fgets(cmdline + memsize, 32, fp);
		memsize += 32;
	}

	if (fp != NULL)
		fclose(fp);
	return cmdline;
}

static int lockd_sock_handler(void *data)
{
	int cl;
	int len;
	int sun_size;
	int clifd = -1;
	char cmd[PHLOCK_SOCK_MAXBUFF];
	char *cmdline = NULL;
	int val = -1;
	int fd = -1;
	int recovery_state = -1;
	GPollFD *gpollfd;

	struct ucred cr;
	struct sockaddr_un lockd_addr;
	struct lockd_data *lockd = (struct lockd_data *)data;

	if ((lockd == NULL) || (lockd->gpollfd == NULL)) {
		LOCKD_DBG("lockd->gpollfd is NULL");
		return -1;
	}
	gpollfd = (GPollFD *)lockd->gpollfd;
	fd = gpollfd->fd;

	cl = sizeof(cr);
	sun_size = sizeof(struct sockaddr_un);

	if ((clifd =
	     accept(fd, (struct sockaddr *)&lockd_addr,
		    (socklen_t *) & sun_size)) == -1) {
		if (errno != EINTR)
			LOCKD_DBG("accept error");
		return -1;
	}

	if (getsockopt(clifd, SOL_SOCKET, SO_PEERCRED, &cr, (socklen_t *) & cl)
	    < 0) {
		LOCKD_DBG("peer information error");
		close(clifd);
		return -1;
	}
	LOCKD_DBG("Peer's pid=%d, uid=%d, gid=%d\n", cr.pid, cr.uid, cr.gid);

	memset(cmd, 0, PHLOCK_SOCK_MAXBUFF);

	lockd_set_sock_option(clifd, 1);

	len = recv(clifd, cmd, PHLOCK_SOCK_MAXBUFF, 0);
	cmd[PHLOCK_SOCK_MAXBUFF - 1] = '\0';

	if (len != strlen(cmd)) {
		LOCKD_DBG("recv error %d %d", len, strlen(cmd));
		close(clifd);
		return -1;
	}

	LOCKD_DBG("cmd %s", cmd);

	cmdline = lockd_read_cmdline_from_proc(cr.pid);
	if (cmdline == NULL) {
		LOCKD_DBG("Error on opening /proc/%d/cmdline", cr.pid);
		close(clifd);
		return -1;
	}

	LOCKD_DBG("/proc/%d/cmdline : %s", cr.pid, cmdline);
	LOCKD_DBG("phone_lock_pid : %d vs cr.pid : %d", phone_lock_pid, cr.pid);

	if ((!strncmp(cmdline, PHLOCK_APP_CMDLINE, strlen(cmdline)))
	    && (!strncmp(cmd, PHLOCK_UNLOCK_CMD, strlen(cmd)))) {
		LOCKD_DBG("cmd is %s\n", PHLOCK_UNLOCK_CMD);

		if (phone_lock_pid == cr.pid) {
			LOCKD_DBG("pid [%d] %s is verified, unlock..!!\n", cr.pid,
				  cmdline);
			lockd_process_mgr_terminate_phone_lock(phone_lock_pid);
			phone_lock_pid = 0;
			vconf_set_bool(VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION, TRUE);
		}
	} else if (!strncmp(cmd, PHLOCK_LAUNCH_CMD, strlen(cmd))) {
		LOCKD_DBG("cmd is %s\n", PHLOCK_LAUNCH_CMD);
		if (_lockd_get_lock_type() == 1) {
			lockd->lock_type = 1;
			lockd_launch_app_lockscreen(lockd);
		}
	}

	if(cmdline != NULL) {
		free(cmdline);
		cmdline = NULL;
	}

	close(clifd);
	return 0;
}

static gboolean lockd_glib_handler(gpointer data)
{
	if (lockd_sock_handler(data) < 0) {
		LOCKD_DBG("lockd_sock_handler is failed..!!");
	}
	return TRUE;
}

static gboolean lockd_glib_dispatch(GSource * src, GSourceFunc callback,
				    gpointer data)
{
	callback(data);
	return TRUE;
}

static gboolean lockd_glib_prepare(GSource * src, gint * timeout)
{
	return FALSE;
}

static GSourceFuncs funcs = {
	.prepare = lockd_glib_prepare,
	.check = lockd_glib_check,
	.dispatch = lockd_glib_dispatch,
	.finalize = NULL
};

static int lockd_init_sock(struct lockd_data *lockd)
{
	int fd;
	GPollFD *gpollfd;
	GSource *src;
	int ret;

	fd = lockd_create_sock();
	if (fd < 0) {
		LOCKD_DBG("lock daemon create sock failed..!!");
	}

	src = g_source_new(&funcs, sizeof(GSource));

	gpollfd = (GPollFD *) g_malloc(sizeof(GPollFD));
	gpollfd->events = POLLIN;
	gpollfd->fd = fd;

	lockd->gpollfd = gpollfd;

	g_source_add_poll(src, lockd->gpollfd);
	g_source_set_callback(src, (GSourceFunc) lockd_glib_handler,
			      (gpointer) lockd, NULL);
	g_source_set_priority(src, G_PRIORITY_LOW);

	ret = g_source_attach(src, NULL);
	if (ret == 0)
		return -1;

	g_source_unref(src);

	return 0;
}

static void lockd_init_vconf(struct lockd_data *lockd)
{
	int val = -1;

	if (vconf_notify_key_changed
	    (VCONFKEY_PM_STATE, _lockd_notify_pm_state_cb, lockd) != 0) {
		LOCKD_ERR("Fail vconf_notify_key_changed : VCONFKEY_PM_STATE");
	}

	if (vconf_notify_key_changed
	    (VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION,
	     _lockd_notify_phone_lock_verification_cb, lockd) != 0) {
		if (vconf_get_bool(VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION, &val) < 0) {
			LOCKD_ERR
			    ("Cannot get %s", VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION);
			vconf_set_bool(VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION, 0);
			if (vconf_notify_key_changed
			    (VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION,
			     _lockd_notify_phone_lock_verification_cb,
			     lockd) != 0) {
				LOCKD_ERR
				    ("Fail vconf_notify_key_changed : %s", VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION);
			}
		} else {
			LOCKD_ERR
			    ("Fail vconf_notify_key_changed : %s", VCONFKEY_LOCKSCREEN_PHONE_LOCK_VERIFICATION);
		}
	}

	if (vconf_notify_key_changed
	    (VCONFKEY_IDLE_LOCK_STATE,
	     _lockd_notify_lock_state_cb,
	     lockd) != 0) {
		LOCKD_ERR
		    ("[Error] vconf notify : lock state");
	}
}

static void lockd_start_lock_daemon(void *data)
{
	struct lockd_data *lockd = NULL;
	int r = 0;

	lockd = (struct lockd_data *)data;

	if (!lockd) {
		return;
	}

	LOCKD_DBG("%s, %d", __func__, __LINE__);

	lockd_init_vconf(lockd);

	r = lockd_init_sock(lockd);
	if (r < 0) {
		LOCKD_DBG("lockd init socket failed: %d", r);
	}

	lockd->lockw = lockd_window_init();

	aul_listen_app_dead_signal(lockd_app_dead_cb, data);

	LOCKD_DBG("%s, %d", __func__, __LINE__);
}

int start_lock_daemon(int launch_lock)
{
	struct lockd_data *lockd = NULL;
	int val = -1;
	int recovery_state = -1;
	int first_boot = 0;
	int lock_type = 0;
	int ret = 0;

	LOCKD_DBG("%s, %d", __func__, __LINE__);

	lockd = (struct lockd_data *)malloc(sizeof(struct lockd_data));
	memset(lockd, 0x0, sizeof(struct lockd_data));
	lockd_start_lock_daemon(lockd);

	if (launch_lock == FALSE) {
		LOCKD_DBG("Don't launch lockscreen..");
		return 0;
	}

	if (vconf_get_bool(VCONFKEY_PWLOCK_FIRST_BOOT, &first_boot) < 0) {
		LOCKD_ERR("Cannot get %s vconfkey", VCONFKEY_PWLOCK_FIRST_BOOT);
	} else if (first_boot == 1) {
		LOCKD_DBG("first_boot : %d \n", first_boot);
		return 0;
	}

	lock_type = _lockd_get_lock_type();
	if (lock_type == 1) {
		lockd->request_recovery = FALSE;
	}
	lockd->lock_type = lock_type;
	ret = lockd_launch_app_lockscreen(lockd);
	return ret;
}
