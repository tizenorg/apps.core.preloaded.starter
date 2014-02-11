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



#include <ail.h>
#include <errno.h>
#include <Elementary.h>
#include <Ecore.h>
#include <Ecore_File.h>
#include <stdio.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <unistd.h>
#include <vconf.h>

/* For multi-user support */
#include <tzplatform_config.h>

#include "pkg_event.h"
#include "util.h"


#define CONF_FILE tzplatform_mkpath3(TZ_SYS_SHARE, "install-info", "desktop.conf")
#define BUFSZE 1024


extern int errno;
struct inotify_path
{
	int wd;
	char *path;
};

struct desktop_notifier s_desktop_notifier = {
	.number = 0,
	.ifd = 0,
	.handler = NULL,
};



struct inotify_path paths[CONF_PATH_NUMBER];


static Eina_Bool
directory_notify(void* data, Ecore_Fd_Handler* fd_handler)
{
	char *buf;
	ssize_t read_size, len, i = 0;
	int fd;

	fd = ecore_main_fd_handler_fd_get(fd_handler);
	_D("There are some modification, ifd [%d]", fd);
	if(fd < 0) {
		_E("Failed to get fd");
		return ECORE_CALLBACK_CANCEL;
	}

	if (ioctl(fd, FIONREAD, &read_size) < 0) {
		_E("Failed to get q size");
		return ECORE_CALLBACK_CANCEL;
	}

	if (read_size <= 0) {
		_E("Buffer is not ready!!!");
		return ECORE_CALLBACK_RENEW;
	}

	buf = malloc(read_size);
	if (!buf) {
		_E("Failed to allocate heap for event handling");
		return ECORE_CALLBACK_RENEW;
	}

	len = read(fd, buf, read_size);
	if (len < 0) {
		free(buf);
		return ECORE_CALLBACK_CANCEL;
	}
	buf[read_size - 1] = '\0';

	while (i < len) {
		struct inotify_event* event = (struct inotify_event*) &buf[i];
		char *str_potksed = "potksed.";
		char *package = NULL;
		ssize_t idx;
		int nev_name;

		nev_name = strlen(event->name) - 1;
		for (idx = 0; nev_name >= 0 && str_potksed[idx]; idx++) {
			if (event->name[nev_name] != str_potksed[idx]) {
				break;
			}
			nev_name --;
		}

		if (str_potksed[idx] != '\0' || nev_name < 0) {
			_D("This is not a desktop file : %s", event->name);
			i += sizeof(struct inotify_event) + event->len;
			continue;
		}

		package = strdup(event->name);
		break_if(NULL == package);

		package[nev_name + 1] = '\0';
		_D("Package : %s", package);

		if (event->mask & IN_CLOSE_WRITE || event->mask & IN_MOVED_TO) {
			ail_appinfo_h ai = NULL;
			ail_error_e ret;

			ret = ail_get_appinfo(package, &ai);
			if (AIL_ERROR_OK == ret || AIL_ERROR_NO_DATA == ret) {
				if (ai) ail_destroy_appinfo(ai);

				if (AIL_ERROR_NO_DATA == ret) {
					if (ail_desktop_add(package) < 0) {
						_D("Failed to add a new package (%s)", event->name);
					}
				} else if (AIL_ERROR_OK == ret) {
					if (ail_desktop_update(package) < 0) {
						_D("Failed to add a new package (%s)", event->name);
					}
				}
			} else {
				_E("Failed to get appinfo");
			}
		} else if (event->mask & IN_DELETE) {
			if (ail_desktop_remove(package) < 0) 
				_D("Failed to remove a package (%s)", event->name);
		} else {
			_D("this event is not dealt with inotify");
		}

		free(package);

		i += sizeof(struct inotify_event) + event->len;
	}

	free(buf);
	return ECORE_CALLBACK_RENEW;
}



static inline char *_ltrim(char *str)
{
	retv_if(NULL == str, NULL);
	while (*str && (*str == ' ' || *str == '\t' || *str == '\n')) str ++;
	return str;
}



static inline int _rtrim(char *str)
{
	int len;

	retv_if(NULL == str, 0);

	len = strlen(str);
	while (--len >= 0 && (str[len] == ' ' || str[len] == '\n' || str[len] == '\t')) {
		str[len] = '\0';
	}

	return len;
}



static int _retrieve_conf_path(struct inotify_path* paths)
{
	char *line = NULL;
	FILE *fp;
	size_t size = 0;
	ssize_t read;
	int i = 0;

	fp = fopen(CONF_FILE, "r");
	if (NULL == fp) {
		_E("%s", CONF_FILE);
		return -1;
	}

	while ((read = getline(&line, &size, fp)) != -1 && i < CONF_PATH_NUMBER - 1) {
		char *begin;

		if (size <= 0) break;

		begin = _ltrim(line);
		_rtrim(line);

		if (*begin == '#' || *begin == '\0') continue;

		paths[i].path = strdup(begin);
		i++;
	}

	if (line) free(line);
	paths[i].path = NULL;
	fclose(fp);

	return i;
}



static void _unretrieve_conf_path(struct inotify_path* paths, int number)
{
	register int i;

	for (i = 0; i < number; i ++) {
		if (paths[i].path) {
			free(paths[i].path);
			paths[i].path = NULL;
		}
	}
}



void pkg_event_init()
{
	int wd = 0;
	int i;

	s_desktop_notifier.ifd = inotify_init();
	if (s_desktop_notifier.ifd == -1) {
		_E("inotify_init error: %s", strerror(errno));
		return;
	}

	s_desktop_notifier.number = _retrieve_conf_path(paths);

	for (i = 0; i < CONF_PATH_NUMBER && paths[i].path; i++)
	{
		_D("Configuration file for desktop file monitoring [%s] is added", paths[i].path);
		if (access(paths[i].path, R_OK) != 0)
		{
			ecore_file_mkpath(paths[i].path);
			if (chmod(paths[i].path, 0777) == -1) {
				_E("cannot chmod %s", paths[i].path);
			}
		}

		wd = inotify_add_watch(s_desktop_notifier.ifd, paths[i].path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE);
		if (wd == -1) {
			_E("inotify_add_watch error: %s", strerror(errno));
			close(s_desktop_notifier.ifd);
			return;
		}

		paths[i].wd = wd;
	}

	s_desktop_notifier.handler = ecore_main_fd_handler_add(s_desktop_notifier.ifd, ECORE_FD_READ, directory_notify, NULL, NULL, NULL);
	if (!s_desktop_notifier.handler) {
		_E("cannot add handler for inotify");
	}
}



void pkg_event_fini(void)
{
	register int i;

	if (s_desktop_notifier.handler) {
		ecore_main_fd_handler_del(s_desktop_notifier.handler);
	}

	for (i = 0; i < CONF_PATH_NUMBER; i ++) {
		if (paths[i].wd) {
			if (inotify_rm_watch(s_desktop_notifier.ifd, paths[i].wd) < 0) {
				_E("Error: %s", strerror(errno));
			}
			paths[i].wd = 0;
		}
	}

	_unretrieve_conf_path(paths, s_desktop_notifier.number);

	if (s_desktop_notifier.ifd) {
		close(s_desktop_notifier.ifd);
		s_desktop_notifier.ifd = 0;
	}
}
