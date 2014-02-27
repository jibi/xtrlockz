#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <pwd.h>
#include <string.h>
#include <crypt.h>
#include <unistd.h>
#include <math.h>
#include <shadow.h>

#include <X11/Xutil.h>

#define VERSION "2.0"
#define TIMEOUTPERATTEMPT 30000
#define MAXGOODWILL  (TIMEOUTPERATTEMPT*5)
#define INITIALGOODWILL MAXGOODWILL
#define GOODWILLPORTION 0.3

Display *display;
Window window, root;
struct passwd *pw;

int
check_password(const char *s) {
	int ok = !strcmp(crypt(s, pw->pw_passwd), pw->pw_passwd);

	if (!ok) {
		/* TODO: */
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
		fprintf(stderr,"xtrlockz (version %s): no arguments allowed\n",VERSION);
		exit(1);
	}

	errno = 0;
	pw    = getpwuid(getuid());

	if (!pw) {
		perror("password entry for uid not found");
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

	display= XOpenDisplay(0);

	if (!display) {
		fprintf(stderr,"xtrlockz (version %s): cannot open display\n", VERSION);
		exit(1);
	}

	attrib.override_redirect = True;
	window = XCreateWindow(display, DefaultRootWindow(display), 0, 0, 1, 1, 0,
			CopyFromParent, InputOnly, CopyFromParent, CWOverrideRedirect, &attrib);

	XSelectInput(display,window,KeyPressMask|KeyReleaseMask);
	XMapWindow(display,window);

	gs = 0;

	for (tvt=0 ; tvt<100; tvt++) {
		ret = XGrabKeyboard(display, window, False, GrabModeAsync, GrabModeAsync, CurrentTime);

		if (ret == GrabSuccess) {
			gs = 1;
			break;
		}

		usleep(10000);
	}

	if (gs == 0) {
		fprintf(stderr,"xtrlockz (version %s): cannot grab keyboard\n", VERSION);
		exit(1);
	}

	if (XGrabPointer(display, window, False, (KeyPressMask|KeyReleaseMask)&0, GrabModeAsync, GrabModeAsync, None, None, CurrentTime) != GrabSuccess) {
		XUngrabKeyboard(display,CurrentTime);
		fprintf(stderr,"xtrlockz (version %s): cannot grab pointer\n", VERSION);
		exit(1);
	}

	while (1) {
		XNextEvent(display,&ev);
		switch (ev.type) {
			case KeyPress:
				if (ev.xkey.time < timeout) {
					XBell(display,0); break;
				}

				clen = XLookupString(&ev.xkey,cbuf,9,&ks,0);

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

						XBell(display,0);
						rlen = 0;

						if (timeout) {
							goodwill += ev.xkey.time - timeout;
							if (goodwill > MAXGOODWILL) {
								goodwill= MAXGOODWILL;
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

