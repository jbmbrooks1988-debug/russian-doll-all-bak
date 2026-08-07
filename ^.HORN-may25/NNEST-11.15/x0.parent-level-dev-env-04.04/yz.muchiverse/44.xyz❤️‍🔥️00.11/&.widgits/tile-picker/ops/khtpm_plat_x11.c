/* khtpm_plat_x11.c — X11+GLX shim only; design = khtpm_core.
 * Linux primary builds should use: khtpm_main.c + khtpm_core.c + this file
 * instead of growing tp_desktop_window.c design forks.
 */
#ifndef _WIN32

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "khtpm_core.h"
#include "khtpm_plat.h"

#define POLL_MS 300

static Display *g_dpy;
static Window g_win;
static GLXContext g_ctx;
static GLuint g_tex;
static int g_has_tex;
static KhtpmEntity *g_e;
static int g_running;
static int g_drag, g_dx, g_dy, g_wx, g_wy;
static Colormap g_cmap;
static XVisualInfo *g_vi;

static void upload_tex(void) {
    if (!g_e || !g_e->sprite_pixels) { g_has_tex = 0; return; }
    if (g_tex) glDeleteTextures(1, &g_tex);
    glGenTextures(1, &g_tex);
    glBindTexture(GL_TEXTURE_2D, g_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_e->sprite_res, g_e->sprite_res, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_e->sprite_pixels);
    g_has_tex = 1;
}

static void shape_mask(void) {
    if (!g_dpy || !g_win || !g_e) return;
    int W = g_e->win_px, H = g_e->win_px;
    Pixmap mask = XCreatePixmap(g_dpy, g_win, W, H, 1);
    GC mgc = XCreateGC(g_dpy, mask, 0, NULL);
    XSetForeground(g_dpy, mgc, 0);
    XFillRectangle(g_dpy, mask, mgc, 0, 0, W, H);
    XSetForeground(g_dpy, mgc, 1);
    if (g_e->sprite_pixels && g_e->sprite_res > 0) {
        for (int y = 0; y < H; y++) {
            int sy = (y * g_e->sprite_res) / H;
            for (int x = 0; x < W; x++) {
                int sx = (x * g_e->sprite_res) / W;
                if (g_e->sprite_pixels[(sy * g_e->sprite_res + sx) * 4 + 3] > 127)
                    XDrawPoint(g_dpy, mask, mgc, x, y);
            }
        }
    } else {
        XFillRectangle(g_dpy, mask, mgc, 0, 0, W, H);
    }
    XShapeCombineMask(g_dpy, g_win, ShapeBounding, 0, 0, mask, ShapeSet);
    XFreeGC(g_dpy, mgc);
    XFreePixmap(g_dpy, mask);
}

static void render(void) {
    glXMakeCurrent(g_dpy, g_win, g_ctx);
    glViewport(0, 0, g_e->win_px, g_e->win_px);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (g_has_tex) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, 1);
        glTexCoord2f(0, 0); glVertex2f(-1, 1);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glBegin(GL_QUADS);
        glColor3f(0.15f, 0.35f, 0.55f);
        glVertex2f(-1, -1); glVertex2f(1, -1); glVertex2f(1, 1); glVertex2f(-1, 1);
        glEnd();
    }
    glXSwapBuffers(g_dpy, g_win);
}

/* Simple X11 popup using a transient menu window would be large;
 * for core path on Linux we draw menu via system(xmessage) is bad.
 * Use a minimal override_redirect list window. */
static void show_menu_x11(void) {
    if (!g_e || g_e->n_view <= 0) return;
    int row_h = 22, n = g_e->n_view;
    int mw = 220, mh = row_h * (n + 1) + 8;
    int mx = g_wx, my = g_wy + g_e->win_px + 4;
    Window mwnd = XCreateSimpleWindow(g_dpy, RootWindow(g_dpy, DefaultScreen(g_dpy)),
                                      mx, my, mw, mh, 1,
                                      BlackPixel(g_dpy, DefaultScreen(g_dpy)),
                                      WhitePixel(g_dpy, DefaultScreen(g_dpy)));
    XSetWindowAttributes swa;
    swa.override_redirect = True;
    XChangeWindowAttributes(g_dpy, mwnd, CWOverrideRedirect, &swa);
    XSelectInput(g_dpy, mwnd, ButtonPressMask | ExposureMask);
    XMapRaised(g_dpy, mwnd);
    GC gc = XCreateGC(g_dpy, mwnd, 0, NULL);
    XSetForeground(g_dpy, gc, BlackPixel(g_dpy, DefaultScreen(g_dpy)));
    for (;;) {
        XEvent ev;
        XNextEvent(g_dpy, &ev);
        if (ev.type == Expose) {
            char hdr[128];
            snprintf(hdr, sizeof(hdr), "-- %s --", g_e->full_id);
            XDrawString(g_dpy, mwnd, gc, 6, 16, hdr, (int)strlen(hdr));
            for (int i = 0; i < n; i++)
                XDrawString(g_dpy, mwnd, gc, 6, 16 + (i + 1) * row_h,
                            g_e->view[i].label, (int)strlen(g_e->view[i].label));
        } else if (ev.type == ButtonPress) {
            int row = (ev.xbutton.y - 8) / row_h - 1;
            XDestroyWindow(g_dpy, mwnd);
            XFreeGC(g_dpy, gc);
            if (row >= 0 && row < n) {
                char cmd[KHTPM_PATH_BUF];
                int act = khtpm_menu_apply(g_e, g_e->view[row].action, cmd, sizeof(cmd));
                if (act == KHTPM_ACT_CLOSE) { g_running = 0; return; }
                if (act == KHTPM_ACT_RAISE_MENU) { show_menu_x11(); return; }
                if (act == KHTPM_ACT_RUN && cmd[0]) {
                    char sh[KHTPM_PATH_BUF * 2];
                    snprintf(sh, sizeof(sh), "setsid nohup %s >/dev/null 2>&1 &", cmd);
                    system(sh);
                }
                if (act == KHTPM_ACT_OPEN_DIR) {
                    char sh[KHTPM_PATH_BUF * 2];
                    snprintf(sh, sizeof(sh), "xdg-open '%s' >/dev/null 2>&1 &", g_e->package_dir);
                    system(sh);
                }
            }
            return;
        }
    }
}

void khtpm_plat_ensure_taskbar(const char *house_root) {
    char pidpath[KHTPM_PATH_BUF];
    khtpm_path_join(pidpath, sizeof(pidpath), house_root, "#.desktop/livedesk_taskbar.pid");
    FILE *f = fopen(pidpath, "r");
    if (f) {
        int tpid = 0;
        if (fscanf(f, "%d", &tpid) == 1 && tpid > 1) {
            if (kill((pid_t)tpid, 0) == 0) { fclose(f); return; }
        }
        fclose(f);
    }
    char cmd[KHTPM_PATH_BUF * 2];
    snprintf(cmd, sizeof(cmd),
             "setsid nohup '%s/&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x' '%s' >/dev/null 2>&1 &",
             house_root, house_root);
    /* prefer relative when house is . */
    if (strcmp(house_root, ".") == 0)
        snprintf(cmd, sizeof(cmd),
                 "setsid nohup '&.widgits/livedesk-taskbar/ops/+x/tp_taskbar.+x' '.' >/dev/null 2>&1 &");
    system(cmd);
}

int khtpm_plat_run_entity(KhtpmEntity *e) {
    g_e = e;
    g_running = 1;
    g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) { fprintf(stderr, "khtpm: no display\n"); return 1; }

    static int attrs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 8,
                           GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8, None };
    g_vi = glXChooseVisual(g_dpy, DefaultScreen(g_dpy), attrs);
    if (!g_vi) { fprintf(stderr, "khtpm: no GLX visual\n"); return 1; }
    g_cmap = XCreateColormap(g_dpy, RootWindow(g_dpy, g_vi->screen), g_vi->visual, AllocNone);
    XSetWindowAttributes swa;
    swa.colormap = g_cmap;
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     ButtonMotionMask | StructureNotifyMask;
    swa.override_redirect = True;

    int x = 80, y = 80;
    khtpm_pos_read(e, &x, &y);
    int sw = DisplayWidth(g_dpy, DefaultScreen(g_dpy));
    int sh = DisplayHeight(g_dpy, DefaultScreen(g_dpy));
    khtpm_pos_clamp(&x, &y, e->win_px, sw, sh);
    g_wx = x; g_wy = y;

    g_win = XCreateWindow(g_dpy, RootWindow(g_dpy, g_vi->screen),
                          x, y, e->win_px, e->win_px, 0, g_vi->depth,
                          InputOutput, g_vi->visual,
                          CWColormap | CWBorderPixel | CWEventMask | CWOverrideRedirect, &swa);
    XStoreName(g_dpy, g_win, e->entity);
    g_ctx = glXCreateContext(g_dpy, g_vi, NULL, True);
    XMapRaised(g_dpy, g_win);
    glXMakeCurrent(g_dpy, g_win, g_ctx);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    upload_tex();
    shape_mask();
    render();
    khtpm_history(e, "plat_x11 open (core-driven)");

    struct timeval last;
    gettimeofday(&last, NULL);

    while (g_running) {
        while (XPending(g_dpy)) {
            XEvent ev;
            XNextEvent(g_dpy, &ev);
            if (ev.type == Expose) render();
            else if (ev.type == ButtonPress && ev.xbutton.button == 1) {
                g_drag = 1; g_dx = ev.xbutton.x_root - g_wx; g_dy = ev.xbutton.y_root - g_wy;
            } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
                if (g_drag) {
                    g_drag = 0;
                    khtpm_pos_clamp(&g_wx, &g_wy, e->win_px, sw, sh);
                    khtpm_pos_write(e, g_wx, g_wy);
                    XMoveWindow(g_dpy, g_win, g_wx, g_wy);
                }
            } else if (ev.type == MotionNotify && g_drag) {
                g_wx = ev.xmotion.x_root - g_dx;
                g_wy = ev.xmotion.y_root - g_dy;
                XMoveWindow(g_dpy, g_win, g_wx, g_wy);
            } else if (ev.type == ButtonPress && ev.xbutton.button == 3) {
                khtpm_menu_load(e);
                show_menu_x11();
            }
        }
        int raise = 0, open_menu = 0;
        if (khtpm_relay_poll(e, &raise, &open_menu)) g_running = 0;
        if (raise) { XRaiseWindow(g_dpy, g_win); XMapRaised(g_dpy, g_win); }
        if (open_menu) { khtpm_menu_load(e); show_menu_x11(); }
        if (!khtpm_package_exists(e)) g_running = 0;

        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - last.tv_sec) * 1000 + (now.tv_usec - last.tv_usec) / 1000;
        if (elapsed >= POLL_MS) {
            render();
            last = now;
        }
        fd_set fds;
        FD_ZERO(&fds);
        int xfd = ConnectionNumber(g_dpy);
        FD_SET(xfd, &fds);
        struct timeval tv = { 0, 50000 };
        select(xfd + 1, &fds, NULL, NULL, &tv);
    }

    glXMakeCurrent(g_dpy, None, NULL);
    glXDestroyContext(g_dpy, g_ctx);
    XDestroyWindow(g_dpy, g_win);
    XCloseDisplay(g_dpy);
    return 0;
}

#endif /* !_WIN32 */
