/*
 * X Transparent Lock
 *
 * Copyright (C)1993,1994 Ian Jackson
 * Copyright (C)2014 Jibi
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <crypt.h>
#include <unistd.h>
#include <math.h>
#include <shadow.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <X11/Xutil.h>

#define TIMEOUTPERATTEMPT 30000
#define MAXGOODWILL  (TIMEOUTPERATTEMPT * 5)
#define INITIALGOODWILL MAXGOODWILL
#define GOODWILLPORTION 0.3

Display *display;
Window window, root;
struct passwd *pw;

int
check_password(const char *s) {
	int ok = !strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd);

	if (!ok) {
		time_t cur_time;
		char *time_str;
		int fd;
		
		cur_time = time(NULL);
		time_str = ctime(&cur_time);

		if (!time_str) {
			return ok;
		}

		time_str[strlen(time_str) - 1] = '\x00';

		fd = open("/tmp/xtrlockz_fails", O_RDWR | O_CREAT | O_APPEND, 0775);

		if (fd == -1) {
			return ok;
		}

		dprintf(fd, "%s: failed login.\n", time_str);
		close(fd);
	}

	return ok;
}

int
main(int argc, char **argv){
	XEvent ev;
	KeySym ks;
	char cbuf[10], rbuf[128];
	int clen, rlen;
	long int goodwill, timeout;
	XSetWindowAttributes attrib;
	int ret;
	struct spwd *sp;
	int tvt, gs;

	rlen     = 0;
	goodwill = INITIALGOODWILL;
	timeout  = 0;

	if (argc != 1) {
		fprintf(stderr, "xtrlockz: no arguments allowed\n");
		exit(1);
	}

	errno = 0;
	pw    = getpwuid(getuid());

	if (!pw) {
		fprintf(stderr, "password entry for uid not found");
		exit(1);
	}

	sp = getspnam(pw->pw_name);

	if (sp) {
		pw->pw_passwd = sp->sp_pwdp;
	}

	endspent();

	setgid(getgid());
	setuid(getuid());

	if (strlen(pw->pw_passwd) < 13) {
		fprintf(stderr, "password entry has no pwd\n");
		exit(1);
	}

	display = XOpenDisplay(0);

	if (!display) {
		fprintf(stderr,"xtrlockz: cannot open display\n");
		exit(1);
	}

	attrib.override_redirect = True;
	window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0,
			CopyFromParent, InputOnly, CopyFromParent, CWOverrideRedirect, &attrib);

	XSelectInput(display,window,KeyPressMask|KeyReleaseMask);
	XMapWindow(display,window);

	gs = 0;

	for (tvt = 0; tvt < 100; tvt++) {
		ret = XGrabKeyboard(display, window, False, GrabModeAsync, GrabModeAsync, CurrentTime);

		if (ret == GrabSuccess) {
			gs = 1;
			break;
		}

		usleep(10000);
	}

	if (gs == 0) {
		fprintf(stderr,"xtrlockz: cannot grab keyboard\n");
		exit(1);
	}

	if (XGrabPointer(display, window, False, (KeyPressMask|KeyReleaseMask)&0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
		XUngrabKeyboard(display,CurrentTime);
		fprintf(stderr,"xtrlockz: cannot grab pointer\n");
		exit(1);
	}

	while (1) {
		XNextEvent(display,&ev);
		switch (ev.type) {
			case KeyPress:
				if (ev.xkey.time < timeout) {
					break;
				}

				clen = XLookupString(&ev.xkey, cbuf, 9, &ks, 0);

				switch (ks) {
					case XK_Escape:
					case XK_Clear:
						rlen=0;
						break;

					case XK_Delete:
					case XK_BackSpace:
						if (rlen>0) {
							rlen--;
						}

						break;

					case XK_Linefeed:
					case XK_Return:
						if (rlen==0) {
							break;
						}

						rbuf[rlen] = 0;

						if (check_password(rbuf)) {
							return 0;
						}

						rlen = 0;

						if (timeout) {
							goodwill += ev.xkey.time - timeout;
							if (goodwill > MAXGOODWILL) {
								goodwill = MAXGOODWILL;
							}
						}

						timeout  = -goodwill * GOODWILLPORTION;
						goodwill += timeout;
						timeout  += ev.xkey.time + TIMEOUTPERATTEMPT;
						break;
					default:
						if (clen != 1) break;

						if (rlen < (sizeof(rbuf) - 1)){
							rbuf[rlen] = cbuf[0];
							rlen++;
						}

						break;
				}
				break;
			default:
				break;
		}
	}

	return 0;
}

