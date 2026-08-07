/* khtpm_taskbar_plat_x11.c — X11 draw only; logic = khtpm_taskbar_core */
#ifndef _WIN32

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include "khtpm_taskbar_core.h"
#include "khtpm_taskbar_plat.h"

void ktb_plat_run_command(const char *cmd, const char *house_root) {
    if (!cmd || !cmd[0]) return;
    char portable[KTB_PATH_BUF];
    ktb_action_portable(cmd, portable, sizeof(portable));
    char sh[KTB_PATH_BUF * 2];
    /* quote so &.widgits is not backgrounded */
    snprintf(sh, sizeof(sh), "setsid nohup sh -c '%s' >/dev/null 2>&1 &", portable);
    (void)house_root;
    system(sh);
}

static unsigned long parse_color(Display *dpy, const char *name, unsigned long fallback) {
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XColor c;
    if (XAllocNamedColor(dpy, cmap, name, &c, &c)) return c.pixel;
    return fallback;
}

int ktb_plat_run(KtbState *s) {
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "taskbar: no display\n"); return 1; }
    int sw = DisplayWidth(dpy, DefaultScreen(dpy));
    int sh = DisplayHeight(dpy, DefaultScreen(dpy));
    unsigned long bg = parse_color(dpy, s->theme_bg, BlackPixel(dpy, DefaultScreen(dpy)));
    unsigned long fg = parse_color(dpy, s->theme_fg, WhitePixel(dpy, DefaultScreen(dpy)));

    XSetWindowAttributes swa;
    swa.override_redirect = True;
    swa.background_pixel = bg;
    swa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, DefaultScreen(dpy)),
                               0, sh - KTB_BAR_H, sw, KTB_BAR_H, 0,
                               CopyFromParent, InputOutput, CopyFromParent,
                               CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
    XMapRaised(dpy, win);
    GC gc = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc, fg);
    XSetBackground(dpy, gc, bg);

    int running = 1;
    while (running) {
        ktb_reload(s);
        bg = parse_color(dpy, s->theme_bg, bg);
        fg = parse_color(dpy, s->theme_fg, fg);
        XSetWindowBackground(dpy, win, bg);
        XSetForeground(dpy, gc, fg);
        XClearWindow(dpy, win);

        int close_x0 = ktb_close_x0(sw);
        int shortcuts_x0 = ktb_shortcuts_x0(sw, s->n_shortcuts);
        int tabs_right = shortcuts_x0 - 4;
        for (int i = 0; i < s->n_tabs; i++) {
            int x0 = i * KTB_TAB_W;
            if (x0 + 8 >= tabs_right) break;
            const char *cur = (i == s->tab_focus_idx) ? "[>]" : "[ ]";
            char lab[192];
            snprintf(lab, sizeof(lab), "%s %d. %s", cur, s->tabs[i].nav, s->tabs[i].entity);
            XDrawString(dpy, win, gc, x0 + 8, KTB_BAR_H / 2 + 4, lab, (int)strlen(lab));
        }
        if (s->nav_armed || s->digit_len > 0) {
            char arm[32];
            snprintf(arm, sizeof(arm), "[%s]", s->digit_len ? s->digit_buf : "NAV");
            XDrawString(dpy, win, gc, tabs_right - 80, KTB_BAR_H / 2 + 4, arm, (int)strlen(arm));
        }
        for (int i = 0; i < s->n_shortcuts; i++) {
            int sx0 = close_x0 - (i + 1) * KTB_SHORTCUT_W;
            XDrawRectangle(dpy, win, gc, sx0, 2, KTB_SHORTCUT_W - 4, KTB_BAR_H - 5);
            XDrawString(dpy, win, gc, sx0 + 8, KTB_BAR_H / 2 + 4,
                        s->shortcuts[i].glyph, (int)strlen(s->shortcuts[i].glyph));
        }
        XDrawRectangle(dpy, win, gc, close_x0, 2, KTB_CLOSE_W - 4, KTB_BAR_H - 5);
        XDrawString(dpy, win, gc, close_x0 + 14, KTB_BAR_H / 2 + 4, "X", 1);
        XFlush(dpy);

        fd_set fds;
        FD_ZERO(&fds);
        int xfd = ConnectionNumber(dpy);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, 400000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);

        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            if (ev.type == ButtonPress) {
                int x = ev.xbutton.x;
                if (x >= close_x0) {
                    ktb_quit_and_save(s);
                    running = 0;
                } else {
                    int si = ktb_shortcut_index_at_x(x, sw, s->n_shortcuts);
                    if (si >= 0)
                        ktb_plat_run_command(s->shortcuts[si].command, s->house_root);
                    else {
                        int ti = ktb_tab_index_at_x(x, s->n_tabs, tabs_right);
                        if (ti >= 0) ktb_activate_tab(s, ti);
                    }
                }
            } else if (ev.type == KeyPress) {
                KeySym ks = XLookupKeysym(&ev.xkey, 0);
                if (ks == XK_Left) ktb_focus_delta(s, -1);
                else if (ks == XK_Right) ktb_focus_delta(s, 1);
                else if (ks == XK_Return) ktb_digit_enter(s);
                else if (ks == XK_Escape) ktb_digit_clear(s);
                else if (ks == XK_BackSpace) ktb_digit_backspace(s);
                else {
                    char buf[8] = {0};
                    if (XLookupString(&ev.xkey, buf, sizeof(buf), &ks, NULL) > 0 &&
                        buf[0] >= '0' && buf[0] <= '9')
                        ktb_digit_push(s, buf[0]);
                }
            }
        }
    }
    XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

#endif
