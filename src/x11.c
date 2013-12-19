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



#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WAYLAND
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#endif

#define DEFAULT_WINDOW_H 1280

void prop_string_set(const char *name, const char *value)
{
#ifndef WAYLAND
	Display *d;
	Atom a_name;
	Atom a_UTF8;
	XTextProperty xtp;

	if (name == NULL || value == NULL || value[0] == '\0')
		return;

	d = XOpenDisplay(NULL);
	if (d == NULL)
		return;

	a_name = XInternAtom(d, name, False);
	if (a_name == None)
		goto exit;

	a_UTF8 = XInternAtom(d, "UTF8_STRING", False);
	if (a_UTF8 == None)
		goto exit;

	xtp.value = (unsigned char *)value;
	xtp.format = 8;
	xtp.encoding = a_UTF8;
	xtp.nitems = strlen(value);

	XSetTextProperty(d, DefaultRootWindow(d), &xtp, a_name);

 exit:
	XCloseDisplay(d);
#endif
}

void prop_int_set(const char *name, unsigned int val)
{
#ifndef WAYLAND
	Display *d;
	Atom a_name;

	if (name == NULL)
		return;

	d = XOpenDisplay(NULL);
	if (d == NULL)
		return;

	a_name = XInternAtom(d, name, False);
	if (a_name == None)
		goto exit;

	XChangeProperty(d, DefaultRootWindow(d), a_name, XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)&val, 1);

 exit:
	XCloseDisplay(d);
#endif
}

void set_window_scale(void)
{
#ifndef WAYLAND
	double root_width = 0.0, root_height = 0.0;
	char buf[128] = { 0, };
	Display *disp;
	int screen_num;

	disp = XOpenDisplay(NULL);
	if (disp == NULL)
		return;

	screen_num = DefaultScreen(disp);

	root_width = DisplayWidth(disp, screen_num);
	root_height = DisplayHeight(disp, screen_num);

	XCloseDisplay(disp);

	snprintf(buf, sizeof(buf), "%lf", root_height / DEFAULT_WINDOW_H);

	if (root_width == 800 && root_height == 1280) {
		snprintf(buf, sizeof(buf), "0.71");
	}

	setenv("ELM_SCALE", buf, 1);
	setenv("SCALE_FACTOR", buf, 1);
#endif
}
